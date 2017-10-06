#include <windows.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <fstream>
#include "blob.h"
#include "bitmap.h"
#include "curvecore.h"

void Log(LPCWSTR format, ...);

static bool is_near(double d1, double d2) {
  return std::abs(d1 - d2) < 1e-4;
}

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

// https://github.com/opencv/opencv/blob/3.3.0/samples/cpp/tutorial_code/gpu/gpu-basics-similarity/gpu-basics-similarity.cpp
static double getPSNR(const cv::Mat &I1, const cv::Mat &I2) {
  cv::Mat s1;
  cv::absdiff(I1, I2, s1);       // |I1 - I2|
  s1.convertTo(s1, CV_32F);  // cannot make a square on 8 bits
  s1 = s1.mul(s1);           // |I1 - I2|^2

  cv::Scalar s = sum(s1);         // sum elements per channel

  double sse = s.val[0] + s.val[1] + s.val[2]; // sum channels

  if (sse <= 1e-10) // for small values return zero
    return 0;
  else {
    double mse = sse / (double)(I1.channels() * I1.total());
    double psnr = 10.0 * log10((255 * 255) / mse);
    return psnr;
  }
}

static double getPSNR(const cv::Mat &I) {
  cv::Mat I_32f;
  I.convertTo(I_32f, CV_32F);
  I_32f = I_32f.mul(I_32f);
  double sse = cv::sum(I_32f).val[0];
  if (sse <= 1e-10)
    return 0;
  else {
    double mse = sse / I.total();
    double psnr = 10.0 * log10((255 * 255) / mse);
    // Log(L"%f %f %f\n", sse, mse, psnr);
    return psnr;
  }
}

static cv::Mat GenerateRedBlueDiffImage(const cv::Mat &im1, const cv::Mat &im2) {
  static INIT_ONCE initOnce = INIT_ONCE_STATIC_INIT;
  static cv::Mat lut(1, 256, CV_8UC3);
  InitOnceExecuteOnce(&initOnce,
    [](PINIT_ONCE, PVOID Parameter, PVOID*) -> BOOL {
      for (int i = 0; i < 128; ++i) {
        lut.at<cv::Vec3b>(0, i)[0] = i * 2;
        lut.at<cv::Vec3b>(0, i)[1] = i * 2;
        lut.at<cv::Vec3b>(0, i)[2] = 255;
        lut.at<cv::Vec3b>(0, i + 128)[0] = 255;
        lut.at<cv::Vec3b>(0, i + 128)[1] = 255 - i * 2;
        lut.at<cv::Vec3b>(0, i + 128)[2] = 255 - i * 2;
      }
      return TRUE;
    },
    /*Parameter*/nullptr,
    /*Context*/nullptr);

  cv::Mat im_diff16, im_diff8, im_diff_color;
  cv::subtract(im1, im2, im_diff16, cv::noArray(), CV_16S);
  im_diff16.convertTo(im_diff8, CV_8U, .5, 128);

  cv::merge(std::vector<cv::Mat>{im_diff8, im_diff8, im_diff8},
            im_diff_color);
  cv::LUT(im_diff_color, lut, im_diff_color);
  return im_diff_color;
}

static bool GrayscaleDiffOpenCV(curve::DiffAlgorithm algo,
                                curve::SimpleBitmap &im_smaller,
                                curve::SimpleBitmap &im_bigger,
                                curve::DiffOutput &result,
                                LPCSTR diffImagePath) {
  cv::Mat im1(im_smaller.height_,
              im_smaller.width_,
              CV_8UC1,
              im_smaller.bits_,
              im_smaller.GetLineSize()),
          im2(im_bigger.height_,
              im_bigger.width_,
              CV_8UC1,
              im_bigger.bits_,
              im_bigger.GetLineSize()),
          im_diff, im_resize_area, im_resize_linear;

  cv::resize(im1, im_resize_linear, im2.size(), 0, 0, cv::INTER_LINEAR);

  if (algo == curve::triangle) {
    cv::resize(im1, im_resize_area, im2.size(), 0, 0, cv::INTER_AREA);
    result.psnr_area_vs_smooth = getPSNR(im_resize_area, im_resize_linear);
    result.psnr_target_vs_area = getPSNR(im2, im_resize_area);
    result.psnr_target_vs_smooth = getPSNR(im2, im_resize_linear);
    if (diffImagePath) {
      im_diff = GenerateRedBlueDiffImage(im2, im_resize_area);
    }
  }
  else if (algo == curve::erosionDiff) {
    const int erosion_size = 2;
    static const auto erosion_area =
      cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                cv::Size(2 * erosion_size + 1,
                                         2 * erosion_size + 1),
                                cv::Point(erosion_size, erosion_size));

    cv::absdiff(im2, im_resize_linear, im_diff);
    cv::erode(im_diff, im_diff, erosion_area);
    result.psnr_area_vs_smooth
      = result.psnr_target_vs_area
      = result.psnr_target_vs_smooth
      = getPSNR(im_diff);
  }

  if (diffImagePath && im_diff.cols > 0) {
    cv::flip(im_diff, im_diff, 0);
    cv::imwrite(diffImagePath, im_diff);
  }
  return true;
}

Blob toString(LPCWSTR wideString);

bool GrayscaleDiff(curve::DiffAlgorithm algo,
                   curve::SimpleBitmap &image1,
                   curve::SimpleBitmap &image2,
                   curve::DiffOutput &result,
                   LPCWSTR diffImagePath) {
  if (image1.bitCount_ != 8 || image2.bitCount_ != 8
      || image1.width_ == 0 || image1.height_ == 0
      || image2.width_ == 0 || image2.height_ == 0) {
    return false;
  }

  // Make sure the 1st data is smaller than the second one
  if (image1.width_ <= image2.width_
      && image1.height_ <= image2.height_) {
    ; // ok
  }
  else if (image1.width_ >= image2.width_
           && image1.height_ >= image2.height_) {
    std::swap(image1, image2);
  }
  else {
    return false;
  }

  if (algo == curve::triangle
      || algo == curve::erosionDiff) {
    auto path_ascii = toString(diffImagePath);
    return GrayscaleDiffOpenCV(algo,
                               image1,
                               image2,
                               result,
                               path_ascii.As<char>());
  }

  const auto lineSize1 = image1.GetLineSize();
  const auto lineSize2 = image2.GetLineSize();

  DIB diffBitmap;
  if (diffImagePath)
    diffBitmap = CreateRedBlueBitmap(image1.width_, image1.height_);

  double ret = 0;
  int scaleX = image2.width_ / image1.width_;
  int scaleY = image2.height_ / image1.height_;
  for (DWORD y = 0; y < image1.height_; ++y) {
    int y2 = y * scaleY;
    for (DWORD x = 0; x < image1.width_; ++x) {
      double diff = 0;
      switch (algo) {
      case curve::averageDiff:
        for (int i = 0; i < scaleX; ++i) {
          diff += image2.bits_[y2 * lineSize2 + x * scaleX + i];
        }
        diff = diff / scaleX - image1.bits_[y * lineSize1 + x];
        break;
      case curve::maxDiff:
        for (int i = 0; i < scaleX; ++i) {
          double d = image1.bits_[y * lineSize1 + x]
                     - image2.bits_[y2 * lineSize2 + x * scaleX + i];
          if (std::abs(d) > std::abs(diff))
            diff = d;
        }
        break;
      case curve::minDiff:
        diff = DBL_MAX;
        for (int i = 0; i < scaleX; ++i) {
          double d = image1.bits_[y * lineSize1 + x]
                     - image2.bits_[y2 * lineSize2 + x * scaleX + i];
          if (std::abs(d) < std::abs(diff))
            diff = d;
        }
        break;
      default:
        diff = image2.bits_[y2 * lineSize2 + x * scaleX]
               - image1.bits_[y * lineSize1 + x];
        break;
      }
      ret += (diff * diff);

      if (diffBitmap) {
        diffBitmap.GetBits()[y * lineSize1 + x] =
          static_cast<BYTE>(static_cast<char>(diff));
      }
    }
  }

  if (diffImagePath && diffBitmap) {
    std::ofstream os(diffImagePath, std::ios::binary);
    if (os.is_open()) {
      diffBitmap.Save(os);
    }
  }

  result.psnr_area_vs_smooth
    = result.psnr_target_vs_area
    = result.psnr_target_vs_smooth = std::sqrt(ret);
  return true;
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

  curve::SimpleBitmap image1(8, 5, 3, bitmap_5x3),
                      image2(8, 10, 6, bitmap_10x6);

  const auto algo = curve::averageDiff;
  curve::DiffOutput output;
  auto &output_d = output.psnr_area_vs_smooth;

  assert(GrayscaleDiff(algo, image1, image2, output, nullptr));
  assert(output_d == 0);
  assert(GrayscaleDiff(algo, image2, image1, output, nullptr));
  assert(output_d == 0);

  bitmap_10x6[0] = 0xf1;
  assert(GrayscaleDiff(algo, image1, image2, output, nullptr));
  assert(is_near(output_d, .5));
  double d_copy = output_d;
  assert(GrayscaleDiff(algo, image2, image1, output, nullptr));
  assert(output_d == d_copy);

  bitmap_10x6[1] = 0xef;
  assert(GrayscaleDiff(algo, image1, image2, output, nullptr));
  assert(is_near(output_d, .0));
  d_copy = output_d;
  assert(GrayscaleDiff(algo, image2, image1, output, nullptr));
  assert(output_d == d_copy);

  bitmap_10x6[12 * 4 + 1] = 0xf1;
  bitmap_10x6[12 * 4 + 2] = 0xf3;
  assert(GrayscaleDiff(algo, image1, image2, output, nullptr));
  assert(is_near(output_d, 1.5811)); // sqrt(0.5^2 + 0.15^2)
  d_copy = output_d;
  assert(GrayscaleDiff(algo, image2, image1, output, nullptr));
  assert(output_d == d_copy);
}
