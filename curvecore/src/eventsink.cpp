#include <windows.h>
#include <atlbase.h>
#include <exdispid.h>
#include <exdisp.h>
#include "resource.h"
#include "eventsink.h"

void Log(LPCWSTR format, ...);

HRESULT EventSink::Connect(REFIID iid, IUnknown *objectToConnect) {
  HRESULT hr = E_NOINTERFACE;
  if (CComQIPtr<IConnectionPointContainer> cpc = objectToConnect) {
    CComPtr<IConnectionPoint> cp;
    hr = cpc->FindConnectionPoint(iid, &cp);
    if (SUCCEEDED(hr)) {
      DWORD cookie;
      hr = cp->Advise(this, &cookie);
    }
  }
  return hr;
}

EventSink::EventSink(HWND container)
  : ref_(1), container_(container)
{}

EventSink::~EventSink() {
  Log(L"%s\n", __FUNCTIONW__);
}

#pragma warning(push)
#pragma warning(disable : 4838)
// warning C4838: conversion from 'DWORD' to 'int' requires a narrowing conversion
STDMETHODIMP EventSink::QueryInterface(REFIID riid, void **ppvObject) {
  const QITAB QITable[] = {
    QITABENT(EventSink, IDispatch),
    { 0 },
  };
  return QISearch(this, QITable, riid, ppvObject);
}
#pragma warning(pop)

STDMETHODIMP_(ULONG) EventSink::AddRef() {
  return InterlockedIncrement(&ref_);
}

STDMETHODIMP_(ULONG) EventSink::Release() {
  auto cref = InterlockedDecrement(&ref_);
  if (cref == 0) {
    delete this;
  }
  return cref;
}

STDMETHODIMP EventSink::GetTypeInfoCount(__RPC__out UINT *pctinfo) {
  return E_NOTIMPL;
}

STDMETHODIMP EventSink::GetTypeInfo(UINT iTInfo,
                                    LCID lcid,
                                    __RPC__deref_out_opt ITypeInfo **ppTInfo) {
  return E_NOTIMPL;
}

STDMETHODIMP EventSink::GetIDsOfNames(__RPC__in REFIID riid,
                                      __RPC__in_ecount_full(cNames) LPOLESTR *rgszNames,
                                      __RPC__in_range(0, 16384) UINT cNames,
                                      LCID lcid,
                                      __RPC__out_ecount_full(cNames) DISPID *rgDispId) {
  return E_NOTIMPL;
}

STDMETHODIMP EventSink::Invoke(_In_  DISPID dispIdMember,
                               _In_  REFIID riid,
                               _In_  LCID lcid,
                               _In_  WORD wFlags,
                               _In_  DISPPARAMS *pDispParams,
                               _Out_opt_  VARIANT *pVarResult,
                               _Out_opt_  EXCEPINFO *pExcepInfo,
                               _Out_opt_  UINT *puArgErr) {
  HRESULT hr = S_OK;
  switch (dispIdMember) {
  case DISPID_DOCUMENTCOMPLETE:
    if (pDispParams->cArgs == 2
        && pDispParams->rgvarg[0].vt == (VT_VARIANT | VT_BYREF)
        && pDispParams->rgvarg[0].pvarVal
        && pDispParams->rgvarg[0].pvarVal->vt == VT_BSTR
        && pDispParams->rgvarg[1].vt == VT_DISPATCH) {
      if (CComQIPtr<IWebBrowser2> wb = pDispParams->rgvarg[1].pdispVal) {
        Log(L"Received DWebBrowserEvents2.DocumentComplete: %s\n",
            pDispParams->rgvarg[0].pvarVal->bstrVal);
        PostMessage(GetParent(container_),
                    WM_COMMAND,
                    MAKELONG(ID_DOCUMENTCOMPLETE, 0),
                    0);
      }
    }
    break;
  default:
    hr = E_NOTIMPL;
    break;
  }
  return hr;
}
