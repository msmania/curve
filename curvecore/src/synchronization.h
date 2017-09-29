class Event {
private:
  HANDLE h_;

public:
  Event(BOOL manualReset, BOOL initialState);
  ~Event();
  BOOL Signal();
  BOOL Reset();
  DWORD Wait(DWORD dwMilliseconds);
};

class CriticalSectionHelper {
private:
  CRITICAL_SECTION &cs_;

public:
  CriticalSectionHelper(CRITICAL_SECTION &);
  ~CriticalSectionHelper();
};