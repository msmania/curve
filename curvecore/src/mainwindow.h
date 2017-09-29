class MainWindow : public BaseWindow<MainWindow> {
private:
  BrowserContainer container_;

  class Command {
  private:
    const DWORD timeoutNavigationAndCaptureInMSec = 5000;
    CRITICAL_SECTION exclusiveAccess_;
    Event waitUntilCommandIsDone_;
    HRESULT result_;
    bool done_;

    // Command-specific arguments
    bool async_;
    WORD bitCount_;
    UINT width_;
    UINT height_;
    CComBSTR targetUrl_;
    LPCWSTR captureLocal_;

  public:
    Command();
    ~Command();

    void set_result(HRESULT hr);
    bool is_done() const;

    bool is_async() const;
    WORD get_bitcount() const;
    BSTR get_url();
    LPCWSTR get_captureLocal() const;
    void get_size(UINT &width, UINT &height) const;
    void set_size(UINT width, UINT height);

    bool begin_navigate(LPCWSTR url, boolean async);
    bool begin_capture(WORD bitCount, LPCWSTR saveOnServer);
    void end();
    HRESULT wait();
  };

  Command command_;

  bool InitChildControls();
  void Resize();
  bool OleDraw(LPCWSTR localFile, WORD bitCount);
  bool Capture(WORD bitCount, LPCWSTR localFile);

public:
  MainWindow();
  LPCWSTR ClassName() const;
  LRESULT HandleMessage(UINT msg, WPARAM w, LPARAM l);
  HRESULT StartNavigate(LPCWSTR url,
                        UINT viewWidth,
                        UINT viewHeight,
                        boolean async);
  HRESULT StartCapture(WORD bitCount,
                       UINT &width,
                       UINT &height,
                       LPCWSTR saveOnServer);
};
