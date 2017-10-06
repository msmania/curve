#pragma once

#ifdef CURVECORE_EXPORTS
#define DLL_EXPORTIMPORT __declspec(dllexport)
#else
#define DLL_EXPORTIMPORT __declspec(dllimport)
#endif

namespace curve {

DLL_EXPORTIMPORT
void RunAsServer(LPCWSTR endpoint);

DLL_EXPORTIMPORT
HRESULT Navigate(LPCWSTR endpoint,
                  LPCWSTR url,
                  UINT viewWidth,
                  UINT viewHeight,
                  bool async);
DLL_EXPORTIMPORT
HRESULT Capture(LPCWSTR endpoint, WORD bitCount, LPCWSTR output);

DLL_EXPORTIMPORT
HRESULT EnsureFileMapping(LPCWSTR ep,
                          LPCWSTR backFile,
                          bool forceUpdate,
                          HANDLE *section);

DLL_EXPORTIMPORT
HRESULT Shutdown(LPCWSTR endpoint);

enum DiffAlgorithm : unsigned int {
  skipDiff = 0,
  averageDiff,
  maxDiff,
  minDiff,
  triangle,
  erosionDiff,
};

struct DiffInput {
  LPCWSTR endpoint1;
  LPCWSTR endpoint2;
  LPCWSTR url;
  DWORD waitInMilliseconds;
  UINT viewWidth;
  UINT viewHeight;
  LPCWSTR backFile1;
  LPCWSTR backFile2;
  DiffAlgorithm algo;
  LPCWSTR diffImage;
};

struct DiffOutput {
  double psnr_area_vs_smooth;
  double psnr_target_vs_area;
  double psnr_target_vs_smooth;
};

struct SimpleBitmap {
  WORD bitCount_;
  DWORD width_;
  DWORD height_;
  LPBYTE bits_;
  SimpleBitmap() : bitCount_(0), width_(0), height_(0), bits_(nullptr) {}
  SimpleBitmap(WORD bitCount, DWORD width, DWORD height, LPBYTE bits)
    : bitCount_(bitCount), width_(width), height_(height), bits_(bits) {}
  DWORD GetLineSize() const {
    return static_cast<DWORD>((width_ * bitCount_ + 31) / 32) * 4;
  }
};

DLL_EXPORTIMPORT
HRESULT DiffImage(const DiffInput &input, DiffOutput &output);

DLL_EXPORTIMPORT
void BatchRun(std::istream &is,
              LPCWSTR endpoint1,
              LPCWSTR endpoint2,
              LPCWSTR backFile1,
              LPCWSTR backFile2);

} // namespace curve
