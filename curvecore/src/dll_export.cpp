#define RPC_USE_NATIVE_WCHAR
#include <windows.h>
#include <atlbase.h>
#include <assert.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <curve_rpc.h>
#include "filemapping.h"
#include "blob.h"
#include "diff.h"
#include "curvecore.h"

void Log(LPCWSTR format, ...);
bool GrayscaleDiffOpenCV(curve::SimpleBitmap &image1,
                         curve::SimpleBitmap &image2,
                         curve::DiffOutput &result,
                         LPCSTR diffOutput1,
                         LPCSTR diffOutput2);

class RpcClientBinding {
private:
  RPC_WSTR bindStr_;
  handle_t handle_;

  RPC_STATUS Bind(LPCWSTR endpoint) {
    Release();

    WCHAR ProtocolSequence[] = L"ncalrpc";
    CComBSTR endpointCopy = endpoint;

    RPC_STATUS status = RpcStringBindingCompose(/*ObjUuid*/nullptr,
                                                ProtocolSequence,
                                                /*NetworkAddr*/nullptr,
                                                endpointCopy,
                                                /*Options*/nullptr,
                                                &bindStr_);
    if (status == RPC_S_OK) {
      status = RpcBindingFromStringBinding(bindStr_, &handle_);
    }
    return status;
  }

  void Release() {
    if (handle_) {
      RpcBindingFree(&handle_);
      handle_ = nullptr;
    }
    if (bindStr_) {
      RpcStringFree(&bindStr_);
      bindStr_ = nullptr;
    }
  }

public:
  RpcClientBinding(LPCWSTR endpoint)
    : bindStr_(nullptr), handle_(nullptr) {
    if (Bind(endpoint) != RPC_S_OK) {
      Release();
    }
  }

  ~RpcClientBinding() {
    Release();
  }

  operator handle_t() {
    return handle_;
  }
  LPCWSTR bindName() const {
    return bindStr_;
  }
};

template <class F>
HRESULT ExceptionSafe(F stub) {
  HRESULT ret;
  RpcTryExcept{
    ret = stub();
  }
  RpcExcept(1) {
    ret = HRESULT_FROM_WIN32(RpcExceptionCode());
    Log(L"Rpc exception - %08x\n", ret);
  }
  RpcEndExcept
  return ret;
}

static bool EnsureFile(LPCWSTR filepath, SIZE_T filesize) {
  HANDLE h = CreateFile(filepath,
                        GENERIC_WRITE | GENERIC_READ,
                        /*dwShareMode*/0,
                        /*lpSecurityAttributes*/nullptr,
                        CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL,
                        /*hTemplateFile*/nullptr);
  bool ret = false;
  if (h != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER ll;
    ll.QuadPart = filesize - 1;
    ret = !!SetFilePointerEx(h, ll, /*lpNewFilePointer*/nullptr, FILE_BEGIN);
    if (ret) {
      char c = 0;
      DWORD bytesWritten;
      ret = !!WriteFile(h, &c, 1, &bytesWritten, /*lpOverlapped*/nullptr);
    }
    else {
      Log(L"SetFilePointerEx failed - %08x\n", GetLastError());
    }
    CloseHandle(h);
  }
  else if (GetLastError() == ERROR_FILE_EXISTS) {
    ret = true;
  }
  else {
    Log(L"CreateFile(%s) failed - %08x\n", filepath, GetLastError());
  }
  return ret;
}

static Blob toString(LPCWSTR wideString) {
  Blob blob;
  auto len = WideCharToMultiByte(CP_OEMCP,
                                 /*dwFlags*/0,
                                 wideString,
                                 /*cchWideChar*/-1,
                                 /*lpMultiByteStr*/nullptr,
                                 /*cbMultiByte*/0,
                                 /*lpDefaultChar*/nullptr,
                                 /*lpUsedDefaultChar*/nullptr);
  if (len > 0 && blob.Alloc(len)) {
    WideCharToMultiByte(CP_OEMCP,
                        0,
                        wideString,
                        -1,
                        blob.As<char>(),
                        blob.Size(),
                        nullptr,
                        nullptr);
  }
  return blob;
}

static Blob toWideString(LPCSTR string) {
  Blob blob;
  auto len = MultiByteToWideChar(CP_OEMCP,
                                 /*dwFlags*/0,
                                 string,
                                 /*cbMultiByte*/-1,
                                 /*lpWideCharStr*/nullptr,
                                 /*cchWideChar*/0);
  if (len > 0 && blob.Alloc(len * sizeof(WCHAR))) {
    MultiByteToWideChar(CP_OEMCP,
                        0,
                        string,
                        -1,
                        blob.As<WCHAR>(),
                        len);
  }
  return blob;
}

static HRESULT NavigateAndCapture(RpcClientBinding &cl1,
                                  RpcClientBinding &cl2,
                                  LPCWSTR url,
                                  UINT viewWidth,
                                  UINT viewHeight,
                                  DWORD wait,
                                  curve::SimpleBitmap &outCapturedSize1,
                                  curve::SimpleBitmap &outCapturedSize2) {
  HRESULT hr = ExceptionSafe([&]() {
    return c_Navigate(cl1, url, viewWidth, viewHeight, /*async*/true);
  });
  if (FAILED(hr)) goto cleanup;
  hr = ExceptionSafe([&]() {
    return c_Navigate(cl2, url, viewWidth, viewHeight, /*async*/false);
  });
  if (FAILED(hr)) goto cleanup;

  Sleep(wait);

  WORD bitCount =
    outCapturedSize1.bitCount_ =
    outCapturedSize2.bitCount_ = 8; // Capture as a grayscale image
  UINT width, height;
  hr = ExceptionSafe([&]() {
    return c_Capture(cl1,
                     bitCount,
                     &width,
                     &height,
                     /*saveOnServer*/nullptr);
  });
  if (FAILED(hr)) goto cleanup;
  outCapturedSize1.width_ = width;
  outCapturedSize1.height_ = height;
  // Log(L"Cap from %s: %d x %d\n", cl1.bindName(), width, height);

  hr = ExceptionSafe([&]() {
    return c_Capture(cl2,
                     bitCount,
                     &width,
                     &height,
                     /*saveOnServer*/nullptr);
  });
  if (FAILED(hr)) goto cleanup;
  outCapturedSize2.width_ = width;
  outCapturedSize2.height_ = height;
  // Log(L"Cap from %s: %d x %d\n", cl2.bindName(), width, height);

cleanup:
  return hr;
}

namespace curve {

void RunAsServer(LPCWSTR endpoint) {
  const DWORD MinimumCallThreads = 1;
  WCHAR ProtocolSequence[] = L"ncalrpc";
  CComBSTR endpointBuffer(endpoint);
  RPC_STATUS status;
  status = RpcServerUseProtseqEp(ProtocolSequence,
                                 RPC_C_LISTEN_MAX_CALLS_DEFAULT,
                                 endpointBuffer,
                                 /*SecurityDescriptor*/nullptr); 
  if (status) {
    Log(L"RpcServerUseProtseqEp failed - %08x\n", status);
    exit(status);
  }

  status = RpcServerRegisterIf(s_curve_v1_0_s_ifspec,
                               /*MgrTypeUuid*/nullptr,
                               /*MgrEpv*/nullptr);
  if (status) {
    Log(L"RpcServerRegisterIf failed - %08x\n", status);
    exit(status);
  }

  Log(L"Start listening on %s:%s...\n", ProtocolSequence, endpoint);
  status = RpcServerListen(MinimumCallThreads,
                           RPC_C_LISTEN_MAX_CALLS_DEFAULT,
                           /*DontWait*/FALSE);
  if (status) {
    Log(L"RpcServerListen failed - %08x\n", status);
    exit(status);
  }

  Log(L"RPC server terminated\n");
}

HRESULT Navigate(LPCWSTR endpoint,
                 LPCWSTR url,
                 UINT viewWidth,
                 UINT viewHeight,
                 bool async) {
  RpcClientBinding client(endpoint);
  return ExceptionSafe([&]() {
    return c_Navigate(client, url, viewWidth, viewHeight, async);
  });
}

HRESULT Capture(LPCWSTR endpoint, WORD bitCount, LPCWSTR output) {
  RpcClientBinding client(endpoint);
  return ExceptionSafe([&]() {
    UINT width, height;
    return c_Capture(client, bitCount, &width, &height, output);
  });
}

HRESULT EnsureFileMapping(LPCWSTR ep,
                          LPCWSTR backFile,
                          bool forceUpdate,
                          HANDLE *section) {
  RpcClientBinding client(ep);
  return ExceptionSafe([&]() {
    DWORD dw;
    HRESULT hr = c_EnsureFileMapping(client, backFile, forceUpdate, &dw);
    if (SUCCEEDED(hr) && ULongToHandle(dw)) {
      if (section) {
        *section = ULongToHandle(dw);
      }
      else {
        assert(CloseHandle(ULongToHandle(dw)));
      }
    }
    return hr;
  });
}

HRESULT Shutdown(LPCWSTR endpoint) {
  RpcClientBinding client(endpoint);
  return ExceptionSafe([&]() {
    /*void*/c_Shutdown(client);
    return S_OK;
  });
}

HRESULT DiffImage(const DiffInput &input, DiffOutput &output) {
  const SIZE_T defaultSize = 1 << 26; // Use 64MB as a new backfile
  if (!EnsureFile(input.backFile1, defaultSize)
      || !EnsureFile(input.backFile2, defaultSize)) {
    return E_FAIL;
  }

  RpcClientBinding cl1(input.endpoint1);
  RpcClientBinding cl2(input.endpoint2);
  FileMapping map1, map2;
  SimpleBitmap image1, image2;

  HRESULT hr = ExceptionSafe([&]() {
    DWORD h;
    HRESULT hr = c_EnsureFileMapping(cl1,
                                     input.backFile1,
                                     /*forceUpdate*/false,
                                     &h);
    if (SUCCEEDED(hr))
      map1.Attach(ULongToHandle(h));
    return hr;
  });
  if (FAILED(hr)) goto cleanup;

  hr = ExceptionSafe([&]() {
    DWORD h;
    HRESULT hr = c_EnsureFileMapping(cl2,
                                     input.backFile2,
                                     /*forceUpdate*/false,
                                     &h);
    if (SUCCEEDED(hr))
      map2.Attach(ULongToHandle(h));
    return hr;
  });
  if (FAILED(hr)) goto cleanup;

  hr = NavigateAndCapture(cl1, cl2,
                          input.url,
                          input.viewWidth,
                          input.viewHeight,
                          input.waitInMilliseconds,
                          image1, image2);
  if (SUCCEEDED(hr)) {
    auto view1 = map1.CreateMappedView(FILE_MAP_READ, 0);
    auto view2 = map2.CreateMappedView(FILE_MAP_READ, 0);
    image1.bits_ = view1;
    image2.bits_ = view2;
    auto str1 = toString(input.diffOutput1);
    auto str2 = toString(input.diffOutput2);
    auto result = GrayscaleDiffOpenCV(image1, image2,
                                      output,
                                      str1.As<char>(),
                                      str2.As<char>());
    hr = result ? S_OK : E_FAIL;
  }

cleanup:
  return hr;
}

enum InputColumn : int {
  colId = 0,
  colUrl,
  colWait,
  colWidth,
  colHeight,
  colDiff1,
  colDiff2,
  colMax
};

void BatchRun(std::istream &is,
              LPCWSTR endpoint1,
              LPCWSTR endpoint2,
              LPCWSTR backFile1,
              LPCWSTR backFile2) {
  const SIZE_T defaultSize = 1 << 26; // Use 64MB as a new backfile
  if (!EnsureFile(backFile1, defaultSize)
      || !EnsureFile(backFile2, defaultSize)) {
    return;
  }

  RpcClientBinding cl1(endpoint1);
  RpcClientBinding cl2(endpoint2);

  FileMapping map1;
  HRESULT hr = ExceptionSafe([&]() {
    DWORD h;
    HRESULT hr = c_EnsureFileMapping(cl1,
                                     backFile1,
                                     /*forceUpdate*/false,
                                     &h);
    if (SUCCEEDED(hr))
      map1.Attach(ULongToHandle(h));
    return hr;
  });
  if (FAILED(hr)) return;

  FileMapping map2;
  hr = ExceptionSafe([&]() {
    DWORD h;
    HRESULT hr = c_EnsureFileMapping(cl2,
                                     backFile2,
                                     /*forceUpdate*/false,
                                     &h);
    if (SUCCEEDED(hr))
      map2.Attach(ULongToHandle(h));
    return hr;
  });
  if (FAILED(hr)) return;

  auto view1 = map1.CreateMappedView(FILE_MAP_READ, 0);
  auto view2 = map2.CreateMappedView(FILE_MAP_READ, 0);

  std::string line;
  for (int i = 0; std::getline(is, line); ++i) {
    if (line.size() > 0
        && line[0] != '#'
        && line[0] != ';') {
      std::istringstream iss(line);
      std::vector<std::string> cols;
      for (std::string token; std::getline(iss, token, '\t'); )
        cols.push_back(token);

      if (cols.size() > colHeight) {
        const auto urlBlob = toWideString(cols[colUrl].c_str());
        const auto url = urlBlob.As<WCHAR>();
        const auto diff1 = cols.size() > colDiff1 && cols[colDiff1].size() > 0
                         ? cols[colDiff1].c_str() : nullptr;
        const auto diff2 = cols.size() > colDiff2 && cols[colDiff2].size() > 0
                         ? cols[colDiff2].c_str() : nullptr;
        SimpleBitmap image1, image2;
        if (SUCCEEDED(NavigateAndCapture(cl1, cl2,
                                         url,
                                         atoi(cols[colWidth].c_str()),
                                         atoi(cols[colHeight].c_str()),
                                         atoi(cols[colWait].c_str()),
                                         image1, image2))) {
          image1.bits_ = view1;
          image2.bits_ = view2;
          DiffOutput output;
          if (GrayscaleDiffOpenCV(image1, image2, output, diff1, diff2)) {
            Log(L"%hs\t%hs\t%f\t%f\n",
                cols[colId].c_str(),
                cols[colUrl].c_str(),
                output.psnr_area,
                output.psnr_smooth);
          }
        }
      }
      else {
        Log(L"E> Skipping invalid line#%d\n", i);
      }
    }
  }
}

} // namespace curve