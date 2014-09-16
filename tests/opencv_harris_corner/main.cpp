//
// Copyright (c) 2013, University of Erlangen-Nuremberg
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <iostream>
#include <numeric>
#include <vector>

#ifdef OpenCV
#include "opencv2/opencv.hpp"
#endif

#include "hipacc.hpp"

// variables set by Makefile
//#define SIZE_X 7
//#define SIZE_Y 7
//#define WIDTH 4096
//#define HEIGHT 4096
#define USE_LAMBDA
//#define NO_SEP
//#define USE_FREEIMAGE

#ifdef USE_FREEIMAGE
#include <FreeImage.h>
#endif

using namespace hipacc;


// get time in milliseconds
double time_ms () {
    struct timeval tv;
    gettimeofday (&tv, NULL);

    return ((double)(tv.tv_sec) * 1e+3 + (double)(tv.tv_usec) * 1e-3);
}


// Harris Corner filter in HIPAcc
#ifdef NO_SEP
class Deriv1D : public Kernel<float> {
  private:
    Accessor<uchar> &input;
    Mask<float> &mask;

  public:
    Deriv1D(IterationSpace<float> &iter, Accessor<uchar> &input,
            Mask<float> &mask)
          : Kernel(iter), input(input), mask(mask) {
      addAccessor(&input);
    }

    void kernel() {
      float sum = 0.0f;
      sum += convolve(mask, Reduce::SUM, [&] () -> float {
          return input(mask) * mask();
      });
      output() = sum*sum;
    }
};
#else
class Deriv1DCol : public Kernel<float> {
  private:
    Accessor<uchar> &input;
    Mask<float> &mask;
    const int size;

  public:
    Deriv1DCol(IterationSpace<float> &iter, Accessor<uchar> &input,
            Mask<float> &mask, const int size)
          : Kernel(iter),
            input(input),
            mask(mask),
            size(size) {
      addAccessor(&input);
    }

    void kernel() {
      float sum = 0.0f;
      sum += convolve(mask, Reduce::SUM, [&] () -> float {
          return input(mask) * mask();
      });
      output() = sum;
    }
};
class Deriv1DRow : public Kernel<float> {
  private:
    Accessor<float> &input;
    Mask<float> &mask;
    const int size;

  public:
    Deriv1DRow(IterationSpace<float> &iter, Accessor<float> &input,
            Mask<float> &mask, const int size)
          : Kernel(iter),
            input(input),
            mask(mask),
            size(size) {
      addAccessor(&input);
    }

    void kernel() {
      float sum = 0.0f;
      sum += convolve(mask, Reduce::SUM, [&] () -> float {
          return input(mask) * mask();
      });
      output() = sum*sum;
    }
};
#endif

class Deriv2D : public Kernel<float> {
  private:
    Accessor<uchar> &input;
    Domain &dom;
    Mask<float> &mask1;
    Mask<float> &mask2;

  public:
    Deriv2D(IterationSpace<float> &iter, Accessor<uchar> &input, Domain &dom,
            Mask<float> &mask1, Mask<float> &mask2)
          : Kernel(iter),
            input(input),
            dom(dom),
            mask1(mask1),
            mask2(mask2) {
      addAccessor(&input);
    }

    void kernel() {
      float sum1 = 0, sum2 = 0;
      iterate(dom, [&] () -> void {
        uchar val = input(dom);
        sum1 += val * mask1(dom);
        sum2 += val * mask2(dom);
      });
      output() = sum1 * sum2;
    }
};

class GaussianBlurFilterMaskRow : public Kernel<float> {
  private:
    Accessor<float> &input;
    Mask<float> &mask;
    const int size;

  public:
    GaussianBlurFilterMaskRow(IterationSpace<float> &iter,
          Accessor<float> &input, Mask<float> &mask, const int size)
        : Kernel(iter),
          input(input),
          mask(mask),
          size(size) {
      addAccessor(&input);
    }

    #ifdef USE_LAMBDA
    void kernel() {
      output() = convolve(mask, Reduce::SUM, [&] () -> float {
          return mask() * input(mask);
      });
    }
    #else
    void kernel() {
      const int anchor = size >> 1;
      float sum = 0;

      for (int xf = -anchor; xf<=anchor; xf++) {
        sum += mask(xf, 0)*input(xf, 0);
      }

      output() = sum;
    }
    #endif
};

class GaussianBlurFilterMaskColumn : public Kernel<float> {
  private:
    Accessor<float> &input;
    Mask<float> &mask;
    const int size;

  public:
    GaussianBlurFilterMaskColumn(IterationSpace<float> &iter,
          Accessor<float> &input, Mask<float> &mask, const int size)
        : Kernel(iter),
          input(input),
          mask(mask),
          size(size) {
      addAccessor(&input);
    }

    #ifdef USE_LAMBDA
    void kernel() {
      output() = convolve(mask, Reduce::SUM, [&] () -> float {
          return mask() * input(mask);
      }) + 0.5f;
    }
    #else
    void kernel() {
      const int anchor = size >> 1;
      float sum = 0.5f;

      for (int yf = -anchor; yf<=anchor; yf++) {
        sum += mask(0, yf)*input(0, yf);
      }

      output() = (uchar) (sum);
    }
    #endif
};

class HarrisCorner : public Kernel<float> {
  private:
    Accessor<float> &Dx;
    Accessor<float> &Dy;
    Accessor<float> &Dxy;
    float k;

  public:
    HarrisCorner(IterationSpace<float> &iter, Accessor<float> &Dx,
            Accessor<float> &Dy, Accessor<float> &Dxy, float k)
          : Kernel(iter),
            Dx(Dx),
            Dy(Dy),
            Dxy(Dxy),
            k(k) {
      addAccessor(&Dx);
      addAccessor(&Dy);
      addAccessor(&Dxy);
    }

    void kernel() {
      float x = Dx();
      float y = Dy();
      float xy = Dxy();
      output() = ((x * y) - (xy * xy))      /* det   */
                 - (k * (x + y) * (x + y)); /* trace */
    }
};


/*************************************************************************
 * Main function                                                         *
 *************************************************************************/
int main(int argc, const char **argv) {
    float k = 0.04f;
    float threshold = 20000.0f;

    if (argc > 2) {
      k = atof(argv[1]);
      threshold = atof(argv[2]);
    }

    #ifdef USE_FREEIMAGE
    FreeImage_Initialise();

    FIBITMAP* img = FreeImage_Load(FIF_PNG, "lenna.png");

    FIBITMAP* gray;
    if (img != NULL) {
      std::cerr << "Successfully opened image 'lenna.png'" << std::endl;

      FREE_IMAGE_COLOR_TYPE type = FreeImage_GetColorType(img);

      switch (type) {
        case FIC_MINISBLACK:
          gray = img;
          break;
        case FIC_RGB:
        case FIC_RGBALPHA:
          gray = FreeImage_ConvertToGreyscale(img);
          FreeImage_Unload(img);
          break;
        default:
          std::cerr << "Unknown image format" << std::endl;
          exit(1);
      }
    }

    const int width = FreeImage_GetWidth(gray);
    const int height = FreeImage_GetHeight(gray);
    #else
    const int width = WIDTH;
    const int height = HEIGHT;
    #endif

    const int size_x = SIZE_X;
    const int size_y = SIZE_Y;
    std::vector<float> timings;
    float timing = 0.0f;

    // input and output image of width x height pixels
    Image<uchar> IN(width, height);
    Image<float> OUT(width, height);
    Image<float> DX(width, height);
    Image<float> DY(width, height);
    Image<float> DXY(width, height);
    Image<float> TMP(width, height);

    // host memory for image of width x height pixels
    #ifdef USE_FREEIMAGE
    uchar *input = FreeImage_GetBits(gray);
    #else
    uchar *input = (uchar *)malloc(sizeof(uchar)*width*height);
    #endif

    // initialize data
    for (int y=0; y<height; ++y) {
      for (int x=0; x<width; ++x) {
        #ifndef USE_FREEIMAGE
        input[y*width + x] = (char)(y*width + x) % 256;
        #endif
      }
    }

    #ifndef OpenCV
    // only filter kernel sizes 3x3, 5x5, and 7x7 implemented
    if (size_x != size_y || !(size_x == 3 || size_x == 5 || size_x == 7)) {
        fprintf(stderr, "Wrong filter kernel size. Currently supported values: 3x3, 5x5, and 7x7!\n");
        exit(EXIT_FAILURE);
    }

    // convolution filter mask
    const float filter_x[1][SIZE_X] = {
        #if SIZE_X == 3
        { 0.238994f, 0.522011f, 0.238994f }
        #endif
        #if SIZE_X == 5
        { 0.070766f, 0.244460f, 0.369546f, 0.244460f, 0.070766f }
        #endif
        #if SIZE_X == 7
        { 0.028995f, 0.103818f, 0.223173f, 0.288026f, 0.223173f, 0.103818f, 0.028995f }
        #endif
    };
    const float filter_y[SIZE_Y][1] = {
        #if SIZE_Y == 3
        { 0.238994f }, { 0.522011f }, { 0.238994f }
        #endif
        #if SIZE_Y == 5
        { 0.070766f }, { 0.244460f }, { 0.369546f }, { 0.244460f }, { 0.070766f }
        #endif
        #if SIZE_Y == 7
        { 0.028995f }, { 0.103818f }, { 0.223173f }, { 0.288026f }, { 0.223173f }, { 0.103818f }, { 0.028995f }
        #endif
    };

    const float mask_x[3][3] = { { -0.166666667f,          0.0f,  0.166666667f },
                                 { -0.166666667f,          0.0f,  0.166666667f },
                                 { -0.166666667f,          0.0f,  0.166666667f } };
    const float mask_y[3][3] = { { -0.166666667f, -0.166666667f, -0.166666667f },
                                 {          0.0f,          0.0f,          0.0f },
                                 {  0.166666667f,  0.166666667f,  0.166666667f } };
    Mask<float> MX(mask_x);
    Mask<float> MY(mask_y);

    #ifndef NO_SEP
    const float mask_vx[1][3] = { { 0.166666667f,     0.166666667f,     0.166666667f } };
    const float mask_vy[3][1] = { { 0.166666667f }, { 0.166666667f }, { 0.166666667f } };
    const float mask_mx[1][3] = { {        -1.0f,             0.0f,             1.0f } };
    const float mask_my[3][1] = { {        -1.0f }, {         0.0f }, {         1.0f } };
    Mask<float> MXX(mask_mx);
    Mask<float> MXY(mask_vy);
    Mask<float> MYX(mask_vx);
    Mask<float> MYY(mask_my);
    #endif

    Mask<float> GX(filter_x);
    Mask<float> GY(filter_y);

    IterationSpace<float> IsOut(OUT);
    IterationSpace<float> IsDx(DX);
    IterationSpace<float> IsDy(DY);
    IterationSpace<float> IsDxy(DXY);
    IterationSpace<float> IsTmp(TMP);

    IN = input;

    fprintf(stderr, "Calculating HIPAcc Harris Corner filter ...\n");

    BoundaryCondition<uchar> BcInClamp(IN, 3, 3, Boundary::CLAMP);
    Accessor<uchar> AccInClamp(BcInClamp);

    #ifdef NO_SEP
    Deriv1D D1dx(IsDx, AccInClamp, MX);
    D1dx.execute();
    timing = hipaccGetLastKernelTiming();
    timings.push_back(timing);

    Deriv1D D1dy(IsDy, AccInClamp, MY);
    D1dy.execute();
    timing = hipaccGetLastKernelTiming();
    timings.push_back(timing);
    #else
    BoundaryCondition<float> BcTmpDcClamp(TMP, 1, 3, Boundary::CLAMP);
    Accessor<float> AccTmpDcClamp(BcTmpDcClamp);

    Deriv1DCol D1dxc(IsTmp, AccInClamp, MXX, 3);
    D1dxc.execute();
    timing = hipaccGetLastKernelTiming();
    timings.push_back(timing);

    Deriv1DRow D1dxr(IsDx, AccTmpDcClamp, MXY, 3);
    D1dxr.execute();
    timing = hipaccGetLastKernelTiming();
    timings.push_back(timing);

    Deriv1DCol D1dyc(IsTmp, AccInClamp, MYX, 3);
    D1dyc.execute();
    timing = hipaccGetLastKernelTiming();
    timings.push_back(timing);

    Deriv1DRow D1dyr(IsDy, AccTmpDcClamp, MYY, 3);
    D1dyr.execute();
    timing = hipaccGetLastKernelTiming();
    timings.push_back(timing);
    #endif

    Domain dom(3, 3);
    Deriv2D D2dxy(IsDxy, AccInClamp, dom, MX, MY);
    D2dxy.execute();
    timing = hipaccGetLastKernelTiming();
    timings.push_back(timing);

    BoundaryCondition<float> BcTmpClamp(TMP, 1, size_y, Boundary::CLAMP);
    Accessor<float> AccTmpClamp(BcTmpClamp);

    BoundaryCondition<float> BcInClampDx(DX, size_x, 1, Boundary::CLAMP);
    Accessor<float> AccInClampDx(BcInClampDx);
    GaussianBlurFilterMaskRow GRDx(IsTmp, AccInClampDx, GX, size_x);
    GaussianBlurFilterMaskColumn GCDx(IsDx, AccTmpClamp, GY, size_y);
    GRDx.execute();
    timing = hipaccGetLastKernelTiming();
    GCDx.execute();
    timing += hipaccGetLastKernelTiming();
    timings.push_back(timing);

    BoundaryCondition<float> BcInClampDy(DY, size_x, 1, Boundary::CLAMP);
    Accessor<float> AccInClampDy(BcInClampDy);
    GaussianBlurFilterMaskRow GRDy(IsTmp, AccInClampDy, GX, size_x);
    GaussianBlurFilterMaskColumn GCDy(IsDy, AccTmpClamp, GY, size_y);
    GRDy.execute();
    timing = hipaccGetLastKernelTiming();
    GCDy.execute();
    timing += hipaccGetLastKernelTiming();
    timings.push_back(timing);

    BoundaryCondition<float> BcInClampDxy(DXY, size_x, 1, Boundary::CLAMP);
    Accessor<float> AccInClampDxy(BcInClampDxy);
    GaussianBlurFilterMaskRow GRDxy(IsTmp, AccInClampDxy, GX, size_x);
    GaussianBlurFilterMaskColumn GCDxy(IsDxy, AccTmpClamp, GY, size_y);
    GRDxy.execute();
    timing = hipaccGetLastKernelTiming();
    GCDxy.execute();
    timing += hipaccGetLastKernelTiming();
    timings.push_back(timing);

    Accessor<float> AccDx(DX);
    Accessor<float> AccDy(DY);
    Accessor<float> AccDxy(DXY);
    HarrisCorner HC(IsOut, AccDx, AccDy, AccDxy, k);
    HC.execute();
    timing = hipaccGetLastKernelTiming();
    timings.push_back(timing);

    // get pointer to result data
    float *output = OUT.getData();

    for (int x = 0; x < width; ++x) {
      for (int y = 0; y < height; y++) {
        int pos = y*width+x;
        if (output[pos] > threshold) {
          for (int i = -5; i <= 5; ++i) {
            if (x+i >= 0 && x+i < width)
              input[pos+i] = 255;
          }
          for (int i = -5; i <= 5; ++i) {
            if (y+i > 0 && y+i < height)
              input[pos+(i*width)] = 255;
          }
        }
      }
    }
    #endif

    #ifdef OpenCV
    fprintf(stderr, "\nCalculating OpenCV Harris Corner filter on the CPU ...\n");

    cv::Mat cv_data_in(height, width, CV_8UC1, input);
    cv::Mat dst, dst_norm, dst_norm_scaled;
    dst = cv::Mat::zeros(cv_data_in.size(), CV_32FC1);

    // Detector parameters
    int blockSize = 2;
    int apertureSize = SIZE_X;
    threshold = 200.0f;

    double min_dt = DBL_MAX;
    for (int nt=0; nt<10; nt++) {
      timing = time_ms();

      // Detecting corners
      cv::cornerHarris(cv_data_in, dst,
                       blockSize, apertureSize, k, cv::BORDER_DEFAULT);

      // Normalizing
      cv::normalize(dst, dst_norm, 0, 255, cv::NORM_MINMAX, CV_32FC1, cv::Mat());
      cv::convertScaleAbs(dst_norm, dst_norm_scaled);

      double dt = time_ms() - timing;
      if (dt < min_dt) min_dt = dt;
    }
    timings.push_back(min_dt);

    for (int y = 0; y < dst_norm.rows; y++) {
      for (int x = 0; x < dst_norm.cols; x++) {
        if ((int)dst_norm.at<float>(y,x) > threshold) {
          int pos = y*width+x;
          for (int i = -5; i <= 5; ++i) {
            if (x+i >= 0 && x+i < width)
              input[pos+i] = 255;
          }
          for (int i = -5; i <= 5; ++i) {
            if (y+i > 0 && y+i < height)
              input[pos+(i*width)] = 255;
          }
        }
      }
    }
    #endif

    timing = std::accumulate(timings.begin(), timings.end(), 0.0f);
    fprintf(stderr, "Harris Corner: %.3f ms, %.3f Mpixel/s\n", timing, (width*height/timing)/1000);

    #ifdef USE_FREEIMAGE
    FIBITMAP* out = FreeImage_ConvertFromRawBits(input, width, height, width,
                                                 8, 255, 255, 255, FALSE);
    FreeImage_Save(FIF_PNG, out, "filtered.png");
    #endif

    // memory cleanup
    #ifdef USE_FREEIMAGE
    FreeImage_Unload(out);
    FreeImage_Unload(gray);
    #else
    free(input);
    #endif

    return EXIT_SUCCESS;
}

