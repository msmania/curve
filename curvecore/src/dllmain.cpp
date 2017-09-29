#include <windows.h>
#include <stdio.h>

void Log(LPCWSTR format, ...) {
  va_list v;
  va_start(v, format);
  vwprintf(format, v);
  va_end(v);
}

void __RPC_FAR * __RPC_USER midl_user_allocate(size_t len) {
  return(malloc(len));
}

void __RPC_USER midl_user_free(void __RPC_FAR * ptr) {
  free(ptr);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL,
                    DWORD fdwReason,
                    LPVOID) {
  return TRUE;
}
