class BrowserContainer : public BaseWindow<BrowserContainer> {
private:
  CComPtr<OleSite> site_;
  CComPtr<EventSink> events_;
  CComPtr<IWebBrowser2> wb_;

  HRESULT ActivateBrowser();
  HRESULT ConnectEventSink();
  void OnDestroy();
  void Resize();

public:
  ~BrowserContainer();
  LPCWSTR ClassName() const;
  IWebBrowser2 *GetBrowser();
  LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
