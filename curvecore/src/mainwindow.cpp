#include <windows.h>
#include <windowsx.h>
#include <atlbase.h>
#include <exdisp.h>
#include <mshtmhst.h>
#include <fstream>
#include <memory>
#include "resource.h"
#include "filemapping.h"
#include "blob.h"
#include "bitmap.h"
#include "synchronization.h"
#include "basewindow.h"
#include "olesite.h"
#include "eventsink.h"
#include "container.h"
#include "mainwindow.h"
#include "globalcontext.h"

void Log(LPCWSTR format, ...);

MainWindow::Command::Command()
  : waitUntilCommandIsDone_(/*manualReset*/TRUE,
                            /*initialState*/TRUE),
    result_(S_OK),
    done_(true),
    async_(false) {
  InitializeCriticalSection(&exclusiveAccess_);
}

MainWindow::Command::~Command() {
  end();
  DeleteCriticalSection(&exclusiveAccess_);
}

void MainWindow::Command::set_result(HRESULT hr) {
  result_ = hr;
}

bool MainWindow::Command::is_done() const {
  return done_;
}

bool MainWindow::Command::is_async() const {
  return async_;
}

WORD MainWindow::Command::get_bitcount() const {
  return bitCount_;
}

BSTR MainWindow::Command::get_url() {
  return targetUrl_;
}

LPCWSTR MainWindow::Command::get_captureLocal() const {
  return captureLocal_;
}

void MainWindow::Command::get_size(UINT &width, UINT &height) const {
  width = width_;
  height = height_;
}

void MainWindow::Command::set_size(UINT width, UINT height) {
  width_ = width;
  height_ = height;
}

bool MainWindow::Command::begin_navigate(LPCWSTR url, boolean async) {
  CriticalSectionHelper cs(exclusiveAccess_);
  if (!done_) return false;

  targetUrl_ = url;
  async_ = !!async;

  done_ = false;
  result_ = S_OK;
  waitUntilCommandIsDone_.Reset();
  return true;
}

bool MainWindow::Command::begin_capture(WORD bitCount,
                                        LPCWSTR saveOnServer) {
  CriticalSectionHelper cs(exclusiveAccess_);
  if (!done_) return false;

  bitCount_ = bitCount;
  captureLocal_ = saveOnServer;

  done_ = false;
  result_ = S_OK;
  waitUntilCommandIsDone_.Reset();
  return true;
}

void MainWindow::Command::end() {
  CriticalSectionHelper cs(exclusiveAccess_);
  if (!done_) {
    done_ = true;
    waitUntilCommandIsDone_.Signal();
  }
}

HRESULT MainWindow::Command::wait() {
  auto waitResult = waitUntilCommandIsDone_.Wait(timeoutNavigationAndCaptureInMSec);
  if (waitResult != WAIT_OBJECT_0) {
    Log(L"Giving up the opration - %08x\n", waitResult);
    result_ = HRESULT_FROM_WIN32(waitResult);

    // Force end the command when we give up.
    end();
  }
  return result_;
}

bool MainWindow::InitChildControls() {
  RECT parentRect;
  GetClientRect(hwnd(), &parentRect);
  container_.Create(L"Container",
                    WS_VISIBLE | WS_CHILD,
                    /*style_ex*/0,
                    0, 0,
                    parentRect.right - parentRect.left,
                    parentRect.bottom - parentRect.top,
                    hwnd(),
                    /*menu*/nullptr);
  return !!container_.hwnd();
}

void MainWindow::Resize() {
  RECT clientSize;
  GetClientRect(hwnd(), &clientSize);
  if (container_.hwnd()) {
    MoveWindow(container_.hwnd(),
               0, 0,
               clientSize.right - clientSize.left,
               clientSize.bottom - clientSize.top,
               /*bRepaint*/FALSE);
  }
}

bool MainWindow::OleDraw(LPCWSTR localFile, WORD bitCount) {
  bool ret = false;
  if (CComPtr<IWebBrowser2> wb = container_.GetBrowser()) {
    long width, height;
    if (SUCCEEDED(wb->get_Width(&width))
        && width > 0
        && SUCCEEDED(wb->get_Height(&height))
        && height > 0) {
      RECT scrollerRect;
      SetRect(&scrollerRect, 0, 0, width, height);
      if (auto memDC = SafeDC::CreateMemDC(hwnd())) {
        auto &section = GlobalContext::Instance().GetFileMapping();
        if (auto dib = DIB::CreateNew(memDC,
                                      bitCount,
                                      width,
                                      height,
                                      section,
                                      /*initWithGrayscaleTable*/true)) {
          SelectBitmap(memDC, dib);
          HRESULT hr = ::OleDraw(wb, DVASPECT_CONTENT, memDC, &scrollerRect);
          if (SUCCEEDED(hr)) {
            if (localFile) {
              std::ofstream os(localFile, std::ios::binary);
              if (os.is_open()) {
                ret = !!dib.Save(os);
              }
            }
            else {
              ret = true;
            }
          }
          else {
            Log(L"OleDraw failed - %08x\n", hr);
          }
        }
      }
    }
  }
  return ret;
}

bool MainWindow::Capture(WORD bitCount, LPCWSTR localFile) {
  bool ret = false;
  if (CComPtr<IWebBrowser2> wb = container_.GetBrowser()) {
    HWND targetWindow;
    long width, height;
    if (SUCCEEDED(IUnknown_GetWindow(wb, &targetWindow))
        && SUCCEEDED(wb->get_Width(&width))
        && width > 0
        && SUCCEEDED(wb->get_Height(&height))
        && height > 0) {
      if (HDC target = GetDC(targetWindow)) {
        auto &section = GlobalContext::Instance().GetFileMapping();
        DWORD uw = width, uh = height;
        DIB dib;
        if (bitCount == 8) {
          dib = DIB::CaptureFromHDC(target, 32, uw, uh, /*section*/nullptr);
          dib.ConvertToGrayscale(target, section);
        }
        else {
          dib = DIB::CaptureFromHDC(target, bitCount, uw, uh, section);
        }
        if (dib) {
          command_.set_size(uw, uh);
          if (localFile) {
            std::ofstream os(localFile, std::ios::binary);
            if (os.is_open()) {
              ret = !!dib.Save(os);
            }
          }
          else {
            ret = true;
          }
        }
        ReleaseDC(targetWindow, target);
      }
    }
  }
  return ret;
}

MainWindow::MainWindow()
  : BaseWindow<MainWindow>()
{}

LPCWSTR MainWindow::ClassName() const {
  return L"Minibrowser2 MainWindow";
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM w, LPARAM l) {
  LRESULT ret = 0;
  switch (msg) {
  case WM_CREATE:
    if (!InitChildControls()) {
      return -1;
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  case WM_SIZE:
    Resize();
    break;
  case WM_COMMAND:
    switch (LOWORD(w)) {
    case ID_BROWSE:
      if (CComPtr<IWebBrowser> wb = container_.GetBrowser()) {
        Log(L"Start navigation to %s\n", command_.get_url());
        HRESULT hr = wb->Navigate(command_.get_url(),
                                  /*Flags*/nullptr,
                                  /*TargetFrameName*/nullptr,
                                  /*PostData*/nullptr,
                                  /*Headers*/nullptr);
        command_.set_result(hr);
        if (command_.is_async()) {
          command_.end();
        }
      }
      break;
    case ID_DOCUMENTCOMPLETE:
      if (!command_.is_async()) {
        command_.end();
      }
      break;
    case ID_SCREENSHOT:
      if (Capture(command_.get_bitcount(),
                  command_.get_captureLocal())) {
        Log(L"Screenshot captured.\n");
      }
      else {
        // ScreenShot() can fail for a number of reasons.
        // Just putting E_FAIL.
        command_.set_result(E_FAIL);
      }
      command_.end();
      break;
    case ID_DESTROY:
      DestroyWindow(hwnd());
      break;
    default:
      ret = DefWindowProc(hwnd(), msg, w, l);
      break;
    }
    break;
  default:
    ret = DefWindowProc(hwnd(), msg, w, l);
    break;
  }
  return ret;
}

HRESULT MainWindow::StartNavigate(LPCWSTR url,
                                  UINT viewWidth,
                                  UINT viewHeight,
                                  boolean async) {
  if (command_.begin_navigate(url, async)) {
    MoveWindow(hwnd(), 0, 0, viewWidth, viewHeight, /*bRepaint*/FALSE);
    PostMessage(hwnd(), WM_COMMAND, MAKELONG(ID_BROWSE, 0), 0);
    return command_.wait();
  }
  else {
    Log(L"Another command is in progress.  Aborting the request.\n");
    return E_PENDING;
  }
}

HRESULT MainWindow::StartCapture(WORD bitCount,
                                 UINT &width,
                                 UINT &height,
                                 LPCWSTR saveOnServer) {
  if (command_.begin_capture(bitCount, saveOnServer)) {
    PostMessage(hwnd(), WM_COMMAND, MAKELONG(ID_SCREENSHOT, 0), 0);
    HRESULT hr = command_.wait();
    command_.get_size(/*out*/width, /*out*/height);
    return hr;
  }
  else {
    Log(L"Another command is in progress.  Aborting the request.\n");
    return E_PENDING;
  }

}
