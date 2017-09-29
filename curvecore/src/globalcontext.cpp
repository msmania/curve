#include <windows.h>
#include <atlbase.h>
#include <exdisp.h>
#include <mshtmhst.h>
#include <memory>
#include "resource.h"
#include "filemapping.h"
#include "synchronization.h"
#include "basewindow.h"
#include "olesite.h"
#include "eventsink.h"
#include "container.h"
#include "mainwindow.h"
#include "globalcontext.h"

void Log(LPCWSTR format, ...);

DWORD WINAPI GlobalContext::UIThreadStart(LPVOID lpParameter) {
  DWORD ret = 0;
  if (auto p = reinterpret_cast<GlobalContext*>(lpParameter)) {
    const auto flags = COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE;
    if (SUCCEEDED(CoInitializeEx(nullptr, flags))) {
      ret = p->UIThread();
      CoUninitialize();
    }
  }
  ExitThread(ret);}

GlobalContext::GlobalContext()
  : initOnce_(INIT_ONCE_STATIC_INIT),
    uiThread_(nullptr),
    uiThreadId_(0),
    waitUntilMainWindowReady_(/*manualReset*/TRUE,
                              /*initialState*/TRUE)
{}

DWORD WINAPI GlobalContext::UIThread() {
  const int initialWindowSize = 100;
  if (mainWindow_ = std::make_unique<MainWindow>()) {
    // Need WS_VISIBLE to capture an image from HDC
    if (mainWindow_->Create(L"Minibrowser2",
                            /*style*/WS_POPUP | WS_VISIBLE,
                            /*style_ex*/0,
                            0, 0, initialWindowSize, initialWindowSize,
                            /*parent*/nullptr,
                            /*menu*/nullptr)) {
      Log(L"MainWindow successfully created: %p\n", mainWindow_->hwnd());
      waitUntilMainWindowReady_.Signal();
      MSG msg = { 0 };
      while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    // When UI thread is termianted, CoUninitialize will unload ieframe.dll.
    // MainWindow holds WebBrowser objects via BrowserContainer.  If ieframe.dll
    // is unloaded before those objects are relesed, Access Violation will occur
    // when the process tries to release them.
    // To avoid that, the MainWindow instance must be destroyed when the UI thread
    // is terminated.
    mainWindow_.reset();
  }
  return 0;
}

bool GlobalContext::StartUIThread() {
  waitUntilMainWindowReady_.Reset();
  uiThread_ = CreateThread(/*lpThreadAttributes*/nullptr,
                           /*dwStackSize*/0,
                           UIThreadStart,
                           this,
                           /*dwCreationFlags*/0,
                           &uiThreadId_);
  if (uiThread_) {
    Log(L"Started UI thread %p tid=%d\n", uiThread_, uiThreadId_);
  }
  else {
    Log(L"Failed to start UI thread - %08x\n", GetLastError());
    waitUntilMainWindowReady_.Signal();
  }
  return !!uiThread_;
}

GlobalContext &GlobalContext::Instance() {
  static GlobalContext singleton;
  return singleton;
}

void GlobalContext::ReleaseAll() {
  RPCThreadEnd();

  if (mainWindow_ && mainWindow_->hwnd()) {
    // Not allowed to call DestroyWindow() from a different thread
    PostMessage(mainWindow_->hwnd(), WM_COMMAND, MAKELONG(ID_DESTROY, 0), 0);
  }

  if (uiThread_) {
    WaitForSingleObject(uiThread_, INFINITE);
    CloseHandle(uiThread_);
    uiThread_ = nullptr;
    uiThreadId_= 0;
  }
}

GlobalContext::~GlobalContext() {
  ReleaseAll();
}

void GlobalContext::RPCThreadStart() {
}

void GlobalContext::RPCThreadEnd() {
}

MainWindow &GlobalContext::GetMainWindow() {
  // Use InitOnceExecuteOnce for the greater safe
  // as this can be called from RPC method.
  InitOnceExecuteOnce(&initOnce_,
    [](PINIT_ONCE, PVOID Parameter, PVOID*) -> BOOL {
      BOOL ret = FALSE;
      if (auto p = reinterpret_cast<GlobalContext*>(Parameter)) {
        ret = p->StartUIThread();
      }
      return ret;
    },
    this, nullptr);

  waitUntilMainWindowReady_.Wait(INFINITE);
  return *mainWindow_;
}

FileMapping &GlobalContext::GetFileMapping() {
  return mapping_;
}

static RPC_STATUS GetRpcClientPid(DWORD &clientPid) {
  RPC_CALL_ATTRIBUTES attrib = { 0 };
  attrib.Version = RPC_CALL_ATTRIBUTES_VERSION;
  attrib.Flags = RPC_QUERY_CLIENT_PID;
  auto status = RpcServerInqCallAttributes(0, &attrib);
  if (status == RPC_S_OK) {
    clientPid = HandleToULong(attrib.ClientPID);
  }
  else {
    Log(L"RpcServerInqCallAttributes failed - %08x\n", status);
  }
  return status;
}

HRESULT GlobalContext::GenerateHandleForClient(HANDLE *sectionObject) const {
  DWORD clientPid = 0;
  RPC_STATUS status = GetRpcClientPid(clientPid);
  if (status != RPC_S_OK) {
    return status;
  }

  DWORD gle = 0;
  HANDLE clientProcess = OpenProcess(PROCESS_DUP_HANDLE,
                                     /*bInheritHandle*/FALSE,
                                     clientPid);
  if (clientProcess) {
    HANDLE duplicatedHandle = nullptr;
    if (DuplicateHandle(GetCurrentProcess(),
                        mapping_,
                        clientProcess,
                        &duplicatedHandle,
                        FILE_MAP_READ,
                        /*bInheritHandle*/FALSE,
                        /*dwOptions*/0)) {
      Log(L"Duplicated handle for Client (PID=%08x): %08x\n",
          clientPid,
          duplicatedHandle);
      *sectionObject = duplicatedHandle;
    }
    else {
      gle = GetLastError();
      Log(L"DuplicateHandle failed - %08x\n", gle);
    }
  }
  else {
    gle = GetLastError();
    Log(L"OpenProcess failed - %08x\n", gle);
  }
  return HRESULT_FROM_WIN32(gle);
}

HRESULT GlobalContext::EnsureFileMapping(LPCWSTR backFile, bool forceUpdate) {
  if (mapping_ && !forceUpdate)
    return S_OK;

  ULARGE_INTEGER maxMappingArea = { 0 };
  if (!backFile) {
    // 1MB max for mapping to the system page file
    maxMappingArea.LowPart = 1 << 20;
  }

  HRESULT hr = E_FAIL;
  if (mapping_.Create(backFile, /*sectionName*/nullptr, maxMappingArea)) {
    Log(L"Created SectionObject: %p on %s\n",
        HANDLE(mapping_),
        backFile ? backFile : L"PageFile");
    hr = S_OK;
  }
  else {
    hr = HRESULT_FROM_WIN32(GetLastError());
  }
  return hr;
}
