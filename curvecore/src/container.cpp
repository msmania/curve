#include <windows.h>
#include <atlbase.h>
#include <exdisp.h>
#include <mshtmhst.h>
#include "basewindow.h"
#include "olesite.h"
#include "eventsink.h"
#include "container.h"

void Log(LPCWSTR format, ...);

HRESULT BrowserContainer::ActivateBrowser() {
  site_.Attach(new OleSite(hwnd()));
  if (!site_) {
    return E_POINTER;
  }

  HRESULT hr = wb_.CoCreateInstance(CLSID_WebBrowser);
  if (SUCCEEDED(hr)) {
    wb_->put_Silent(VARIANT_TRUE);

    CComPtr<IOleObject> ole;
    hr = wb_.QueryInterface(&ole);
    if (SUCCEEDED(hr)) {
      hr = OleSetContainedObject(ole, TRUE);
      if (SUCCEEDED(hr)) {
        hr = ole->SetClientSite(site_);
        if (SUCCEEDED(hr)) {
          RECT rc;
          GetClientRect(hwnd(), &rc);
          hr = ole->DoVerb(OLEIVERB_INPLACEACTIVATE,
                           /*lpmsg*/nullptr,
                           site_,
                           /*lindex*/0,
                           hwnd(),
                           &rc);
        }
      }
    }
  }
  return hr;
}

HRESULT BrowserContainer::ConnectEventSink() {
  if (!events_) {
    events_.Attach(new EventSink(hwnd()));
  }
  return events_ ? events_->Connect(DIID_DWebBrowserEvents2, wb_)
                 : E_POINTER;
}

void BrowserContainer::OnDestroy() {
  if (CComQIPtr<IOleObject> ole = wb_) {
    ole->SetClientSite(nullptr);
  }
}

void BrowserContainer::Resize() {
  if (CComQIPtr<IOleInPlaceObject> inplace = wb_) {
    RECT clientArea;
    GetClientRect(hwnd(), &clientArea);
    inplace->SetObjectRects(&clientArea, &clientArea);
  }
}

BrowserContainer::~BrowserContainer() {
  Log(L"> %s\n", __FUNCTIONW__);
  Log(L"  OleSite      = %p\n", static_cast<LPVOID>(site_));
  Log(L"  IWebBrowser2 = %p\n", static_cast<LPVOID>(wb_));
}

LPCWSTR BrowserContainer::ClassName() const {
  return L"Minibrowser2 Container";
}

IWebBrowser2 *BrowserContainer::GetBrowser() {
  return wb_;
}

LRESULT BrowserContainer::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  LRESULT ret = 0;
  switch (uMsg) {
  case WM_CREATE:
    if (FAILED(ActivateBrowser())
        || FAILED(ConnectEventSink())) {
      ret = -1;
    }
    break;
  case WM_DESTROY:
    OnDestroy();
    break;
  case WM_SIZE:
    Resize();
    break;
  default:
    ret = DefWindowProc(hwnd(), uMsg, wParam, lParam);
  }
  return ret;
}
