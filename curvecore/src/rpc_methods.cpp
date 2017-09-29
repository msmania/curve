#include <windows.h>
#include <atlbase.h>
#include <exdisp.h>
#include <mshtmhst.h>
#include <memory>
#include <curve_rpc.h>
#include "filemapping.h"
#include "synchronization.h"
#include "basewindow.h"
#include "olesite.h"
#include "eventsink.h"
#include "container.h"
#include "mainwindow.h"
#include "globalcontext.h"

void Log(LPCWSTR format, ...);

class RpcThreadLock {
public:
  RpcThreadLock() {
    GlobalContext::Instance().RPCThreadStart();
  }
  ~RpcThreadLock() {
    GlobalContext::Instance().RPCThreadEnd();
  }
};

extern "C" {

void s_Shutdown(handle_t IDL_handle) {
  RpcThreadLock lock;
  Log(L"Start: Shutdown()\n");

  GlobalContext::Instance().ReleaseAll();

  RPC_STATUS status;
  status = RpcMgmtStopServerListening(/*Binding*/nullptr);
  if (status) {
    Log(L"RpcMgmtStopServerListening failed - %08x\n", status);
    exit(status);
  }

  status = RpcServerUnregisterIf(/*IfSpec*/nullptr,
                                 /*MgrTypeUuid*/nullptr,
                                 /*WaitForCallsToComplete*/FALSE);
  if (status) {
    Log(L"RpcServerUnregisterIf failed - %08x\n", status);
    exit(status);
  }
}

HRESULT s_Navigate(handle_t IDL_handle,
                   const wchar_t *url,
                   unsigned int viewWidth,
                   unsigned int viewHeight,
                   boolean async) {
  RpcThreadLock lock;
  Log(L"Start: Navigate(%s, %u, %u, %s)\n",
      url,
      viewWidth,
      viewHeight,
      async ? L"async" : L"sync");
  auto &mainWindow = GlobalContext::Instance().GetMainWindow();
  return mainWindow.StartNavigate(url, viewWidth, viewHeight, async);
}

HRESULT s_Capture(handle_t IDL_handle,
                  unsigned short bitCount,
                  unsigned int *width,
                  unsigned int *height,
                  const wchar_t *saveOnServer) {
  RpcThreadLock lock;
  Log(L"Start: Capture(%s)\n", saveOnServer);
  auto &mainWindow = GlobalContext::Instance().GetMainWindow();
  return mainWindow.StartCapture(bitCount, *width, *height, saveOnServer);
}

HRESULT s_EnsureFileMapping(handle_t IDL_handle,
                            const wchar_t *filepath,
                            boolean forceUpdate,
                            unsigned long *handleForClient) {
  RpcThreadLock lock;
  Log(L"Start: EnsureMappingFile(%s)\n", filepath);

  auto &instance = GlobalContext::Instance();
  HRESULT hr = instance.EnsureFileMapping(filepath, !!forceUpdate);
  if (SUCCEEDED(hr)) {
    HANDLE h = nullptr;
    hr = instance.GenerateHandleForClient(&h);
    *handleForClient = HandleToULong(h);
  }
  return hr;
}

}
