class GlobalContext {
private:
  static DWORD WINAPI UIThreadStart(LPVOID lpParameter);

  // Persistent across RPC calls
  INIT_ONCE initOnce_;
  HANDLE uiThread_;
  DWORD uiThreadId_;
  Event waitUntilMainWindowReady_;
  std::unique_ptr<MainWindow> mainWindow_;

  FileMapping mapping_;

  // Updated per RPC call

  GlobalContext();

  DWORD WINAPI UIThread();
  bool StartUIThread();

public:
  static GlobalContext &Instance();

  ~GlobalContext();
  void ReleaseAll();
  void RPCThreadStart();
  void RPCThreadEnd();
  MainWindow &GetMainWindow();
  FileMapping &GetFileMapping();
  HRESULT GenerateHandleForClient(HANDLE *sectionObject) const;
  HRESULT EnsureFileMapping(LPCWSTR backFile, bool forceUpdate);
};
