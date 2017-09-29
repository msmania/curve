#include <windows.h>

LONG GetMagic(HDC dc) {
  LONG ret = 1;
  if (auto gdi32 = LoadLibrary(L"gdi32.dll")) {
    if (auto f = reinterpret_cast<LONG (WINAPI*)(HDC)>(
                   GetProcAddress(gdi32, "GetDCDpiScaleValue"))) {
      ret = f(dc);
    }
    FreeLibrary(gdi32);
  }
  return ret;
}
