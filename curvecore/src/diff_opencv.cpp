#include <windows.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include "blob.h"
#include "bitmap.h"
#include "curvecore.h"

void Log(LPCWSTR format, ...);

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

bool GrayscaleDiffOpenCV(curve::SimpleBitmap &image1,
                         curve::SimpleBitmap &image2,
                         curve::DiffOutput &result,
                         LPCSTR diffOutput1,
                         LPCSTR diffOutput2) {
  if (image1.bitCount_ != 8 || image2.bitCount_ != 8
      || image1.width_ == 0 || image1.height_ == 0
      || image2.width_ == 0 || image2.height_ == 0) {
    return false;
  }

  cv::Mat im1(image1.height_,
              image1.width_,
              CV_8UC1,
              image1.bits_,
              image1.GetLineSize());
  cv::Mat im2(image2.height_,
              image2.width_,
              CV_8UC1,
              image2.bits_,
              image2.GetLineSize());

  // Make sure the 1st data is smaller than the second one
  if (image1.width_ <= image2.width_
      && image1.height_ <= image2.height_) {
    ; // ok
  }
  else if (image1.width_ >= image2.width_
           && image1.height_ >= image2.height_) {
    cv::swap(im1, im2);
  }
  else {
    return false;
  }

  // for testing
  // cv::resize(im2, im2, cv::Size(), 2, 2, cv::INTER_LANCZOS4);

  cv::Mat im_resize_area, im_resize_smooth;
  cv::resize(im1, im_resize_area, im2.size(), 0, 0, cv::INTER_AREA);
  cv::resize(im1, im_resize_smooth, im2.size(), 0, 0, cv::INTER_LINEAR);

  result.psnr_area = getPSNR(im2, im_resize_area);
  result.psnr_smooth = getPSNR(im2, im_resize_smooth);
  if (diffOutput1) {
    auto diff = GenerateRedBlueDiffImage(im2, im_resize_area);
    cv::flip(diff, diff, 0);
    cv::imwrite(diffOutput1, diff);
  }
  if (diffOutput2) {
    auto diff = GenerateRedBlueDiffImage(im2, im_resize_smooth);
    cv::flip(diff, diff, 0);
    cv::imwrite(diffOutput2, diff);
  }
  return true;
}
