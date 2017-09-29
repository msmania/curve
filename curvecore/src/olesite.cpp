#include <windows.h>
#include <atlbase.h>
#include <exdisp.h>
#include <mshtmhst.h>
#include "olesite.h"

void Log(LPCWSTR format, ...);

OleSite::OleSite(HWND hwnd) : ref_(1), hwnd_(hwnd)
{}

OleSite::~OleSite() {
  Log(L"%s\n", __FUNCTIONW__);
}

#pragma warning(push)
#pragma warning(disable : 4838)
// warning C4838: conversion from 'DWORD' to 'int' requires a narrowing conversion
STDMETHODIMP OleSite::QueryInterface(REFIID riid, void **ppvObject) {
  const QITAB QITable[] = {
    QITABENT(OleSite, IOleClientSite),
    QITABENT(OleSite, IOleInPlaceSite),
    QITABENT(OleSite, IDocHostUIHandler),
    { 0 },
  };
  return QISearch(this, QITable, riid, ppvObject);
}
#pragma warning(pop)

STDMETHODIMP_(ULONG) OleSite::AddRef() {
  return InterlockedIncrement(&ref_);
}

STDMETHODIMP_(ULONG) OleSite::Release() {
  auto cref = InterlockedDecrement(&ref_);
  if (cref == 0) {
    delete this;
  }
  return cref;
}

STDMETHODIMP OleSite::SaveObject() {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::GetMoniker(DWORD dwAssign,
                                 DWORD dwWhichMoniker,
                                 __RPC__deref_out_opt IMoniker **ppmk) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::GetContainer(__RPC__deref_out_opt IOleContainer **ppContainer) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::ShowObject() {
  return S_OK;
}

STDMETHODIMP OleSite::OnShowWindow(BOOL fShow) {
  return S_OK;
}

STDMETHODIMP OleSite::RequestNewObjectLayout() {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::GetWindow(__RPC__deref_out_opt HWND *phwnd) {
  *phwnd = hwnd_;
  return S_OK;
}

STDMETHODIMP OleSite::ContextSensitiveHelp(
  BOOL fEnterMode) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::CanInPlaceActivate() {
  return S_OK;
}

STDMETHODIMP OleSite::OnInPlaceActivate() {
  return S_OK;
}

STDMETHODIMP OleSite::OnUIActivate() {
  return S_OK;
}

STDMETHODIMP OleSite::GetWindowContext(__RPC__deref_out_opt IOleInPlaceFrame **ppFrame,
                                       __RPC__deref_out_opt IOleInPlaceUIWindow **ppDoc,
                                       __RPC__out LPRECT lprcPosRect,
                                       __RPC__out LPRECT lprcClipRect,
                                       __RPC__inout LPOLEINPLACEFRAMEINFO lpFrameInfo) {
  if (ppFrame != nullptr) *ppFrame = nullptr;
  if (ppDoc != nullptr) *ppDoc = nullptr;
  if (lprcPosRect)
    GetClientRect(hwnd_, lprcPosRect);

  if (lprcClipRect)
    GetClientRect(hwnd_, lprcClipRect);

  if (lpFrameInfo) {
    lpFrameInfo->fMDIApp = false;
    lpFrameInfo->hwndFrame = hwnd_;
    lpFrameInfo->haccel = nullptr;
    lpFrameInfo->cAccelEntries = 0;
  }

  return S_OK;
}

STDMETHODIMP OleSite::Scroll(SIZE scrollExtant) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::OnUIDeactivate(BOOL fUndoable) {
  return S_OK;
}

STDMETHODIMP OleSite::OnInPlaceDeactivate() {
  return S_OK;
}

STDMETHODIMP OleSite::DiscardUndoState() {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::DeactivateAndUndo() {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::OnPosRectChange(__RPC__in LPCRECT lprcPosRect) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::ShowContextMenu(_In_ DWORD dwID,
                                      _In_ POINT *ppt,
                                      _In_ IUnknown *pcmdtReserved,
                                      _In_ IDispatch *pdispReserved) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::GetHostInfo(_Inout_ DOCHOSTUIINFO *pInfo) {
  pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
  pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER;
  return S_OK;
}

STDMETHODIMP OleSite::ShowUI(DWORD dwID,
                             _In_ IOleInPlaceActiveObject *pActiveObject,
                             _In_ IOleCommandTarget *pCommandTarget,
                             _In_ IOleInPlaceFrame *pFrame,
                             _In_ IOleInPlaceUIWindow *pDoc) {
  return S_OK;
}

STDMETHODIMP OleSite::HideUI() {
  return S_OK;
}

STDMETHODIMP OleSite::UpdateUI() {
  return S_OK;
}

STDMETHODIMP OleSite::EnableModeless(BOOL fEnable) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::OnDocWindowActivate(BOOL fActivate) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::OnFrameWindowActivate(BOOL fActivate) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::ResizeBorder(_In_ LPCRECT prcBorder,
                                   _In_ IOleInPlaceUIWindow *pUIWindow,
                                   _In_ BOOL fRameWindow) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::TranslateAccelerator(LPMSG lpMsg,
                                           const GUID *pguidCmdGroup,
                                           DWORD nCmdID) {
  return E_NOTIMPL;
}
STDMETHODIMP OleSite::GetOptionKeyPath(_Out_ LPOLESTR *pchKey,
                                DWORD dw) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::GetDropTarget(_In_ IDropTarget *pDropTarget,
                                    _Outptr_ IDropTarget **ppDropTarget) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::GetExternal(_Outptr_result_maybenull_ IDispatch **ppDispatch) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::TranslateUrl(DWORD dwTranslate,
                                   _In_ LPWSTR pchURLIn,
                                   _Outptr_ LPWSTR *ppchURLOut) {
  return E_NOTIMPL;
}

STDMETHODIMP OleSite::FilterDataObject(_In_ IDataObject *pDO,
                                       _Outptr_result_maybenull_ IDataObject **ppDORet) {
  return E_NOTIMPL;
}
