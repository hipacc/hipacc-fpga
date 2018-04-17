//
// Copyright (c) 2012, University of Erlangen-Nuremberg
// Copyright (c) 2012, Siemens AG
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

#include <iostream>
#include <vector>

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

using namespace hipacc;
using namespace hipacc::math;


// Sobel filter in HIPAcc
class SobelFilter : public Kernel<short> {
  private:
    Accessor<uchar> &Input;
    Mask<char> &cMask;
    Domain &Dom; 

  public:
    SobelFilter(IterationSpace<short> &IS, Accessor<uchar> &Input,
                Mask<char> &cMask, Domain &Dom)
      : Kernel(IS),
        Input(Input),
        cMask(cMask),
        Dom(Dom) {
      add_accessor(&Input);
    }

    void kernel() {
      short sum = reduce(Dom, Reduce::SUM, [&] () -> short {
                    return cMask(Dom) * Input(Dom);
                  });
      output() = sum;
    }
};

class Combine : public Kernel<uchar> {
  private:
    Accessor<short> &Dx;
    Accessor<short> &Dy;

  public:
    Combine(IterationSpace<uchar> &IS,
            Accessor<short> &Dx,
            Accessor<short> &Dy)
      : Kernel(IS),
        Dx(Dx),
        Dy(Dy) {
      add_accessor(&Dx);
      add_accessor(&Dy);
    }

    void kernel() {
      float dx = Dx();
      float dy = Dy();
      uchar res = sqrt(((dx * dx) + (dy * dy)));
      output() = res;
    }
};


/*************************************************************************
 * Main function                                                         *
 *************************************************************************/
int main(int argc, const char **argv) {
    double time0, time1, dt, min_dt;
    const int width = WIDTH;
    const int height = HEIGHT;
    std::vector<float> timings;
    float timing = 0.0f;

    // convolution filter mask
    const char mask_x[3][3] = {
        {   -1,   0,   1 },
        {   -2,   0,   2 },
        {   -1,   0,   1 }
    };
    const char mask_y[3][3] = {
        {   -1,  -2,  -1 },
        {    0,   0,   0 },
        {    1,   2,   1 }
    };

    // host memory for image of width x height pixels
    uchar tmp[WIDTH*HEIGHT] = {
             0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,
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
    uchar *host_in = (uchar*)tmp;
    uchar *host_out = (uchar*)malloc(width*height);

    // input and output image of width x height pixels
    Image<uchar> IN(width, height);
    Image<uchar> OUT(width, height);
    Image<short> Dx(width, height);
    Image<short> Dy(width, height);

    // filter mask
    Mask<char> MX(mask_x);
    Mask<char> MY(mask_y);
    Domain DX(MX);
    Domain DY(MY);

    IterationSpace<short> IsDx(Dx);
    IterationSpace<short> IsDy(Dy);
    IterationSpace<uchar> IsOut(OUT);

    IN = host_in;
    OUT = host_out;

    BoundaryCondition<uchar> BcInClamp(IN, MX, Boundary::CLAMP);
    Accessor<uchar> AccInClamp(BcInClamp);

    SobelFilter Sx(IsDx, AccInClamp, MX, DX);
    Sx.execute();
    timing += hipacc_last_kernel_timing();

    SobelFilter Sy(IsDy, AccInClamp, MY, DY);
    Sy.execute();
    timing += hipacc_last_kernel_timing();

    Accessor<short> AccDx(Dx);
    Accessor<short> AccDy(Dy);
    Combine C(IsOut, AccDx, AccDy);
    C.execute();
    timing += hipacc_last_kernel_timing();

    // get results
    host_out = OUT.data();

    fprintf(stdout, "Execution time: %f ms\n", timing);

#ifdef TEST
    int i,j;
    for(i = 0; i < HEIGHT; i++) {
        for(j = 0; j < WIDTH; j++) {
            fprintf(stdout,"%d ", host_out[i*WIDTH+j]);
        }
        fprintf(stdout,"\n");
    }
#endif

    // memory cleanup
    //free(host_in);
    //free(host_out);

    return EXIT_SUCCESS;
}

