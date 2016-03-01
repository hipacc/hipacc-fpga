//
// Copyright (c) 2013, University of Erlangen-Nuremberg
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include <iostream>
#include <vector>
#include <numeric>

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "hipacc.hpp"


//#define TEST


// variables set by Makefile
#ifdef TEST
#define WIDTH  24
#define HEIGHT 24
#else
#define WIDTH  1024
#define HEIGHT 1024
#endif
#define SIZE_X 3
#define SIZE_Y 3


using namespace hipacc;


// Harris Corner filter in HIPAcc
class Sobel : public Kernel<short> {
  private:
    Accessor<uchar> &Input;
    Mask<char> &cMask;
    Domain &dom;

  public:
    Sobel(IterationSpace<short> &IS,
            Accessor<uchar> &Input, Mask<char> &cMask, Domain &dom)
          : Kernel(IS),
            Input(Input),
            cMask(cMask),
            dom(dom) {
      add_accessor(&Input);
    }

    void kernel() {
      short sum = 0;
      sum += reduce(dom, Reduce::SUM, [&] () -> short {
          return Input(dom) * cMask(dom);
      });
      output() = sum / 6;
    }
};

class Square1 : public Kernel<short> {
  private:
    Accessor<short> &Input;

  public:
    Square1(IterationSpace<short> &IS,
            Accessor<short> &Input)
          : Kernel(IS),
            Input(Input) {
      add_accessor(&Input);
    }

    void kernel() {
      short in = Input();
      output() = in * in;
    }
};
class Square2 : public Kernel<short> {
  private:
    Accessor<short> &Input1;
    Accessor<short> &Input2;

  public:
    Square2(IterationSpace<short> &IS,
            Accessor<short> &Input1,
            Accessor<short> &Input2)
          : Kernel(IS),
            Input1(Input1),
            Input2(Input2) {
      add_accessor(&Input1);
      add_accessor(&Input2);
    }

    void kernel() {
      output() = Input1() * Input2();
    }
};

class Gauss : public Kernel<short> {
  private:
    Accessor<short> &Input;
    Mask<uchar> &cMask;

  public:
    Gauss(IterationSpace<short> &IS,
            Accessor<short> &Input, Mask<uchar> &cMask)
          : Kernel(IS),
            Input(Input),
            cMask(cMask) {
      add_accessor(&Input);
    }

    void kernel() {
      int sum = 0;
      sum += convolve(cMask, Reduce::SUM, [&] () -> float {
          return Input(cMask) * cMask();
      });
      output() = sum / 16;
    }
};

class HarrisCorner : public Kernel<uchar> {
  private:
    Accessor<short> &Dx;
    Accessor<short> &Dy;
    Accessor<short> &Dxy;
    float k;
    float threshold;

  public:
    HarrisCorner(IterationSpace<uchar> &IS,
            Accessor<short> &Dx, Accessor<short> &Dy, Accessor<short> &Dxy,
            float k, float threshold)
          : Kernel(IS),
            Dx(Dx),
            Dy(Dy),
            Dxy(Dxy),
            k(k),
            threshold(threshold) {
      add_accessor(&Dx);
      add_accessor(&Dy);
      add_accessor(&Dxy);
    }

    void kernel() {
      int x = Dx();
      int y = Dy();
      int xy = Dxy();
      float R = 0;
      R = ((x * y) - (xy * xy)) /* det   */
          - (k * (x + y) * (x + y)); /* trace */
      uchar out = 0;
      if (R > threshold)
        out = 1;
      output() = out;
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

    const int width = WIDTH;
    const int height = HEIGHT;

    const int size_x = SIZE_X;
    const int size_y = SIZE_Y;
    double timing = 0.0;

    // host memory for image of of width x height pixels
    uchar host_in[WIDTH*HEIGHT] = { 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 };
    uchar *host_out = (uchar *)malloc(sizeof(uchar)*width*height);

    // initialize data
    //for (int y=0; y<height; ++y) {
    //  for (int x=0; x<width; ++x) {
    //    host_out[y*width + x] = 0;
    //  }
    //}

    // input and output image of width x height pixels
    Image<uchar> IN(WIDTH, HEIGHT);
    Image<uchar> OUT(WIDTH, HEIGHT);
    Image<short> DX(WIDTH, HEIGHT);
    Image<short> DY(WIDTH, HEIGHT);
    Image<short> DXY(WIDTH, HEIGHT);
    Image<short> SX(WIDTH, HEIGHT);
    Image<short> SY(WIDTH, HEIGHT);
    Image<short> SXY(WIDTH, HEIGHT);

    // convolution filter mask
    const uchar filter_xy[SIZE_Y][SIZE_X] = {
        #if SIZE_X == 3
        { 1, 2, 1 },
        { 2, 4, 2 },
        { 1, 2, 1 }
        //{ 0.057118f, 0.124758f, 0.057118f },
        //{ 0.124758f, 0.272496f, 0.124758f },
        //{ 0.057118f, 0.124758f, 0.057118f }
        #endif
        #if SIZE_X == 5
        { 0.005008f, 0.017300f, 0.026151f, 0.017300f, 0.005008f },
        { 0.017300f, 0.059761f, 0.090339f, 0.059761f, 0.017300f },
        { 0.026151f, 0.090339f, 0.136565f, 0.090339f, 0.026151f },
        { 0.017300f, 0.059761f, 0.090339f, 0.059761f, 0.017300f },
        { 0.005008f, 0.017300f, 0.026151f, 0.017300f, 0.005008f }
        #endif
        #if SIZE_X == 7
        { 0.000841, 0.003010, 0.006471, 0.008351, 0.006471, 0.003010, 0.000841 },
        { 0.003010, 0.010778, 0.023169, 0.029902, 0.023169, 0.010778, 0.003010 },
        { 0.006471, 0.023169, 0.049806, 0.064280, 0.049806, 0.023169, 0.006471 },
        { 0.008351, 0.029902, 0.064280, 0.082959, 0.064280, 0.029902, 0.008351 },
        { 0.006471, 0.023169, 0.049806, 0.064280, 0.049806, 0.023169, 0.006471 },
        { 0.003010, 0.010778, 0.023169, 0.029902, 0.023169, 0.010778, 0.003010 },
        { 0.000841, 0.003010, 0.006471, 0.008351, 0.006471, 0.003010, 0.000841 }
        #endif
    };

    const  char mask_x[3][3] = { {-1,  0,  1},//{-0.166666667f,          0.0f,  0.166666667f},
                                 {-1,  0,  1},//{-0.166666667f,          0.0f,  0.166666667f},
                                 {-1,  0,  1} };//{-0.166666667f,          0.0f,  0.166666667f} };
    const  char mask_y[3][3] = { {-1, -1, -1},//{-0.166666667f, -0.166666667f, -0.166666667f},
                                 { 0,  0,  0},//{         0.0f,          0.0f,          0.0f},
                                 { 1,  1,  1} };//{ 0.166666667f,  0.166666667f,  0.166666667f} };

    Mask<uchar> G(filter_xy);
    Mask<char> MX(mask_x);
    Mask<char> MY(mask_y);
    Domain DomX(MX);
    Domain DomY(MY);

    IterationSpace<uchar> IsOut(OUT);
    IterationSpace<short> IsDx(DX);
    IterationSpace<short> IsDy(DY);
    IterationSpace<short> IsDxy(DXY);
    IterationSpace<short> IsSx(SX);
    IterationSpace<short> IsSy(SY);
    IterationSpace<short> IsSxy(SXY);

    IN = host_in;
    OUT = host_out;

    BoundaryCondition<uchar> BcInClamp(IN, MX, Boundary::CLAMP);
    Accessor<uchar> AccInClamp(BcInClamp);

    Sobel DerivX(IsDx, AccInClamp, MX, DomX);
    DerivX.execute();
    timing += hipacc_last_kernel_timing();

    Sobel DerivY(IsDy, AccInClamp, MY, DomY);
    DerivY.execute();
    timing += hipacc_last_kernel_timing();


    Accessor<short> AccDx(DX);
    Accessor<short> AccDy(DY);


    Square1 SquareX(IsSx, AccDx);
    SquareX.execute();
    timing += hipacc_last_kernel_timing();

    Square1 SquareY(IsSy, AccDy);
    SquareY.execute();
    timing += hipacc_last_kernel_timing();

    Square2 SquareXY(IsSxy, AccDx, AccDy);
    SquareXY.execute();
    timing += hipacc_last_kernel_timing();


    BoundaryCondition<short> BcInClampSx(SX, G, Boundary::CLAMP);
    Accessor<short> AccInClampSx(BcInClampSx);
    BoundaryCondition<short> BcInClampSy(SY, G, Boundary::CLAMP);
    Accessor<short> AccInClampSy(BcInClampSy);
    BoundaryCondition<short> BcInClampSxy(SXY, G, Boundary::CLAMP);
    Accessor<short> AccInClampSxy(BcInClampSxy);


    Gauss GaussX(IsDx, AccInClampSx, G);
    GaussX.execute();
    timing += hipacc_last_kernel_timing();

    Gauss GaussY(IsDy, AccInClampSy, G);
    GaussY.execute();
    timing += hipacc_last_kernel_timing();

    Gauss GaussXY(IsDxy, AccInClampSxy, G);
    GaussXY.execute();
    timing += hipacc_last_kernel_timing();


    Accessor<short> AccDxy(DXY);


    HarrisCorner HC(IsOut, AccDx, AccDy, AccDxy, k, threshold);
    HC.execute();
    timing += hipacc_last_kernel_timing();

    // get results
    host_out = OUT.data();
    fprintf(stdout,"<HIPACC:> Overall time: %fms\n", timing);

#ifdef TEST
    int i,j;
    for(i = 0; i < HEIGHT; i++) {
        for(j = 0; j < WIDTH; j++) {
            //fprintf(stdout,"%d ", host_out[i*WIDTH+j]);
            if (host_out[i*WIDTH+j] == 1) {
                fprintf(stdout,"X ");
            } else {
                fprintf(stdout,"- ");
            }
        }
        fprintf(stdout,"\n");
    }
#endif

    //free(host_in);
    //free(host_out);

    return EXIT_SUCCESS;
}

