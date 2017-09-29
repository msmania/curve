enum DiffAlgorithm : unsigned int {
  skipDiff = 0,
  averageDiff,
  maxDiff,
  minDiff,
};

double GrayscaleDiff(DiffAlgorithm algo,
                     DWORD width1,
                     DWORD height1,
                     LPCBYTE bits1,
                     DWORD width2,
                     DWORD height2,
                     LPCBYTE bits2,
                     LPCWSTR diffBitmap);
