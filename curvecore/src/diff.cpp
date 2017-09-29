#include <windows.h>
#include <assert.h>
#include <algorithm>
#include <fstream>
#include "diff.h"
#include "blob.h"
#include "bitmap.h"

void Log(LPCWSTR format, ...);

static DIB CreateRedBlueBitmap(LONG width, LONG height) {
  auto dib = DIB::CreateNew(/*dc*/nullptr,
                            /*bitCount*/8,
                            width, height,
                            /*section*/nullptr,
                            /*initWithGrayscaleTable*/false);
  auto colorTable = dib.GetColorTable();
  for (int i = 0; i <= 0x7f; ++i) {
    auto &rgb = colorTable[i];
    rgb.rgbRed = 255;
    rgb.rgbGreen = rgb.rgbBlue = (127 - i) * 2;
    rgb.rgbReserved = 0;
  }
  for (int i = 0xff; i >= 0x80; --i) {
    auto &rgb = colorTable[i];
    rgb.rgbBlue = 255;
    rgb.rgbGreen = rgb.rgbRed = (i - 128) * 2;
    rgb.rgbReserved = 0;
  }
  return dib;
}

double GrayscaleDiff(DiffAlgorithm algo,
                     DWORD width1,
                     DWORD height1,
                     LPCBYTE bits1,
                     DWORD width2,
                     DWORD height2,
                     LPCBYTE bits2,
                     LPCWSTR diffOutput) {
  if (width1 <= width2 && height1 <= height2) {
    ; // ok
  }
  else if (width1 >= width2 && height1 >= height2) {
    // Make sure the 1st data is smaller than the second one
    std::swap(width1, width2);
    std::swap(height1, height2);
    std::swap(bits1, bits2);
  }
  else {
    return DBL_MAX;
  }

  const auto lineSize1 = int((width1 * 8 + 31) / 32) * 4;
  const auto lineSize2 = int((width2 * 8 + 31) / 32) * 4;

  DIB diffBitmap;
  if (diffOutput)
    diffBitmap = CreateRedBlueBitmap(width1, height1);

  double ret = 0;
  int scaleX = width2 / width1;
  int scaleY = height2 / height1;
  for (DWORD y = 0; y < height1; ++y) {
    int y2 = y * scaleY;
    for (DWORD x = 0; x < width1; ++x) {
      double diff = 0;
      switch (algo) {
      case averageDiff:
        for (int i = 0; i < scaleX; ++i) {
          diff += bits2[y2 * lineSize2 + x * scaleX + i];
        }
        diff = diff / scaleX - bits1[y * lineSize1 + x];
        break;
      case maxDiff:
        for (int i = 0; i < scaleX; ++i) {
          double d = bits1[y * lineSize1 + x]
                     - bits2[y2 * lineSize2 + x * scaleX + i];
          if (std::abs(d) > std::abs(diff))
            diff = d;
        }
        break;
      case minDiff:
        diff = DBL_MAX;
        for (int i = 0; i < scaleX; ++i) {
          double d = bits1[y * lineSize1 + x]
                     - bits2[y2 * lineSize2 + x * scaleX + i];
          if (std::abs(d) < std::abs(diff))
            diff = d;
        }
        break;
      default:
        diff = bits2[y2 * lineSize2 + x * scaleX] - bits1[y * lineSize1 + x];
        break;
      }
      ret += (diff * diff);

      if (diffBitmap) {
        diffBitmap.GetBits()[y * lineSize1 + x] =
          static_cast<BYTE>(static_cast<char>(diff));
      }
    }
  }

  if (diffOutput && diffBitmap) {
    std::ofstream os(diffOutput, std::ios::binary);
    if (os.is_open()) {
      diffBitmap.Save(os);
    }
  }
  return std::sqrt(ret);
}

static bool is_near(double d1, double d2) {
  return std::abs(d1 - d2) < 1e-4;
}

void Test_GrayscaleDiff() {
  BYTE bitmap_5x3[] = {
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00,
  };
  BYTE bitmap_10x6[] = {
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x01, 0x07,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x02, 0x08,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x03, 0x09,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x04, 0x0a,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x05, 0x0b,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x06, 0x0c,
  };

  assert(GrayscaleDiff(averageDiff, 5, 3, bitmap_5x3, 10, 6, bitmap_10x6, nullptr) == 0);
  assert(GrayscaleDiff(averageDiff, 10, 6, bitmap_10x6, 5, 3, bitmap_5x3, nullptr) == 0);

  bitmap_10x6[0] = 0xf1;
  auto d = GrayscaleDiff(averageDiff, 5, 3, bitmap_5x3, 10, 6, bitmap_10x6, nullptr);
  assert(is_near(d, .5));
  assert(GrayscaleDiff(averageDiff, 10, 6, bitmap_10x6, 5, 3, bitmap_5x3, nullptr) == d);

  bitmap_10x6[1] = 0xef;
  d = GrayscaleDiff(averageDiff, 5, 3, bitmap_5x3, 10, 6, bitmap_10x6, nullptr);
  assert(is_near(d, .0));
  assert(GrayscaleDiff(averageDiff, 10, 6, bitmap_10x6, 5, 3, bitmap_5x3, nullptr) == d);

  bitmap_10x6[12 * 4 + 1] = 0xf1;
  bitmap_10x6[12 * 4 + 2] = 0xf3;
  d = GrayscaleDiff(averageDiff, 5, 3, bitmap_5x3, 10, 6, bitmap_10x6, nullptr);
  assert(is_near(d, 1.5811)); // sqrt(0.5^2 + 0.15^2)
  assert(GrayscaleDiff(averageDiff, 10, 6, bitmap_10x6, 5, 3, bitmap_5x3, nullptr) == d);

  return;
}