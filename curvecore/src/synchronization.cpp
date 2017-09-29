#include <windows.h>
#include "synchronization.h"

Event::Event(BOOL manualReset, BOOL initialState) : h_(nullptr) {
  h_ = CreateEvent(/*lpEventAttributes*/nullptr,
                   /*bManualReset*/manualReset,
                   /*bInitialState*/initialState,
                   /*lpName*/nullptr);
}

Event::~Event() {
  if (h_) {
    WaitForSingleObject(h_, INFINITE);
    CloseHandle(h_);
    h_ = nullptr;
  }
}

BOOL Event::Signal() {
  return SetEvent(h_);
}

BOOL Event::Reset() {
  return ResetEvent(h_);
}

DWORD Event::Wait(DWORD dwMilliseconds) {
  return WaitForSingleObject(h_, dwMilliseconds);
}

CriticalSectionHelper::CriticalSectionHelper(CRITICAL_SECTION &cs)
  : cs_(cs) {
  EnterCriticalSection(&cs_);
}

CriticalSectionHelper::~CriticalSectionHelper() {
  LeaveCriticalSection(&cs_);
}
