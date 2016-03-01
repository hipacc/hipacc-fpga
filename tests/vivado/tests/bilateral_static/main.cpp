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
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "hipacc.hpp"

//#define TEST

// variables set by Makefile
#define SIGMA_D 3
#define SIGMA_R 16
#define WIDTH 1024
#define HEIGHT 1024
#define EPS 0.02f
#define CONSTANT 1.0f

#ifdef TEST
#define FREEIMAGE
#include <FreeImage.h>
#define WIDTH 512
#define HEIGHT 512
#endif

using namespace hipacc;
using namespace hipacc::math;


// get time in milliseconds
double time_ms () {
    struct timeval tv;
    gettimeofday (&tv, NULL);

    return ((double)(tv.tv_sec) * 1e+3 + (double)(tv.tv_usec) * 1e-3);
}


class BilateralFilterMask : public Kernel<uchar> {
    private:
        Accessor<uchar> &in;
        Mask<float> &mask;
        Domain &dom;
        int sigma_d, sigma_r;

    public:
        BilateralFilterMask(IterationSpace<uchar> &iter, Accessor<uchar> &in,
                Mask<float> &mask, Domain &dom, int sigma_d, int sigma_r) :
            Kernel(iter),
            in(in),
            mask(mask),
            dom(dom),
            sigma_d(sigma_d),
            sigma_r(sigma_r)
        { add_accessor(&in); }

        void kernel() {
            int c_r = 0.5f/(SIGMA_R*SIGMA_R);//(sigma_r*sigma_r);//1.0f/(2.0f*sigma_r*sigma_r);
            float d = 0.0f;
            float p = 0.0f;

            iterate(dom, [&] () -> void {
                    float diff = (in(dom) - in());
                    float s = expf(-c_r * diff*diff) * mask(dom);
                    d += s;
                    p += s * in(dom);
                    });

            output() = (uchar)((p+0.5f)/d);
        }
};


int main(int argc, const char **argv) {
    double time0, time1, dt;
    const int sigma_d = SIGMA_D;
    const int sigma_r = SIGMA_R;
    float timing = 0.0f;

    const int width = WIDTH;
    const int height = HEIGHT;

#ifdef FREEIMAGE
    // Initialize
    FreeImage_Initialise();

    // Load image
    FIBITMAP* img = FreeImage_Load(FIF_PNG, "lenna.png");

    // Convert to grayscale
    FIBITMAP* gray;
    if (img != NULL) {
      std::cout << "FreeImage: Successfully opened image lenna.png" << std::endl;
    
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
          std::cerr << "FreeImage: Unknown image format" << std::endl;
          return 1;
      }
    } else {
      std::cerr << "FreeImage: Failed to open image lenna.png" << std::endl;
      return 1;
    }

    // Get image bits
    uchar *host_in = FreeImage_GetBits(gray);
#else
    uchar *host_in = (uchar *)malloc(sizeof(uchar)*width*height);

    // initialize data
    #define DELTA 0.001f
    for (int y=0; y<height; ++y) {
        for (int x=0; x<width; ++x) {
            host_in[y*width + x] = (uchar) (x*height + y) * DELTA;
        }
    }
#endif

    // host memory for image of width x height pixels
    uchar *host_out = (uchar *)malloc(sizeof(uchar)*width*height);

    const float filter_mask[sigma_d][sigma_d] = {
        #if SIGMA_D==3
        { 0.018316f, 0.135335f, 0.018316f },
        { 0.135335f, 1.000000f, 0.135335f },
        { 0.018316f, 0.135335f, 0.018316f }
        #endif
        #if SIGMA_D==5
        { 0.018316f, 0.082085f, 0.135335f, 0.082085f, 0.018316f },
        { 0.082085f, 0.367879f, 0.606531f, 0.367879f, 0.082085f },
        { 0.135335f, 0.606531f, 1.000000f, 0.606531f, 0.135335f },
        { 0.082085f, 0.367879f, 0.606531f, 0.367879f, 0.082085f },
        { 0.018316f, 0.082085f, 0.135335f, 0.082085f, 0.018316f }
        #endif
        #if SIGMA_D==7
        { 0.018316f, 0.055638f, 0.108368f, 0.135335f, 0.108368f, 0.055638f, 0.018316f },
        { 0.055638f, 0.169013f, 0.329193f, 0.411112f, 0.329193f, 0.169013f, 0.055638f },
        { 0.108368f, 0.329193f, 0.641180f, 0.800737f, 0.641180f, 0.329193f, 0.108368f },
        { 0.135335f, 0.411112f, 0.800737f, 1.000000f, 0.800737f, 0.411112f, 0.135335f },
        { 0.108368f, 0.329193f, 0.641180f, 0.800737f, 0.641180f, 0.329193f, 0.108368f },
        { 0.055638f, 0.169013f, 0.329193f, 0.411112f, 0.329193f, 0.169013f, 0.055638f },
        { 0.018316f, 0.055638f, 0.108368f, 0.135335f, 0.108368f, 0.055638f, 0.018316f }
        #endif
        #if SIGMA_D==13
        { 0.018316f, 0.033746f, 0.055638f, 0.082085f, 0.108368f, 0.128022f, 0.135335f, 0.128022f, 0.108368f, 0.082085f, 0.055638f, 0.033746f, 0.018316f },
        { 0.033746f, 0.062177f, 0.102512f, 0.151240f, 0.199666f, 0.235877f, 0.249352f, 0.235877f, 0.199666f, 0.151240f, 0.102512f, 0.062177f, 0.033746f },
        { 0.055638f, 0.102512f, 0.169013f, 0.249352f, 0.329193f, 0.388896f, 0.411112f, 0.388896f, 0.329193f, 0.249352f, 0.169013f, 0.102512f, 0.055638f },
        { 0.082085f, 0.151240f, 0.249352f, 0.367879f, 0.485672f, 0.573753f, 0.606531f, 0.573753f, 0.485672f, 0.367879f, 0.249352f, 0.151240f, 0.082085f },
        { 0.108368f, 0.199666f, 0.329193f, 0.485672f, 0.641180f, 0.757465f, 0.800737f, 0.757465f, 0.641180f, 0.485672f, 0.329193f, 0.199666f, 0.108368f },
        { 0.128022f, 0.235877f, 0.388896f, 0.573753f, 0.757465f, 0.894839f, 0.945959f, 0.894839f, 0.757465f, 0.573753f, 0.388896f, 0.235877f, 0.128022f },
        { 0.135335f, 0.249352f, 0.411112f, 0.606531f, 0.800737f, 0.945959f, 1.000000f, 0.945959f, 0.800737f, 0.606531f, 0.411112f, 0.249352f, 0.135335f },
        { 0.128022f, 0.235877f, 0.388896f, 0.573753f, 0.757465f, 0.894839f, 0.945959f, 0.894839f, 0.757465f, 0.573753f, 0.388896f, 0.235877f, 0.128022f },
        { 0.108368f, 0.199666f, 0.329193f, 0.485672f, 0.641180f, 0.757465f, 0.800737f, 0.757465f, 0.641180f, 0.485672f, 0.329193f, 0.199666f, 0.108368f },
        { 0.082085f, 0.151240f, 0.249352f, 0.367879f, 0.485672f, 0.573753f, 0.606531f, 0.573753f, 0.485672f, 0.367879f, 0.249352f, 0.151240f, 0.082085f },
        { 0.055638f, 0.102512f, 0.169013f, 0.249352f, 0.329193f, 0.388896f, 0.411112f, 0.388896f, 0.329193f, 0.249352f, 0.169013f, 0.102512f, 0.055638f },
        { 0.033746f, 0.062177f, 0.102512f, 0.151240f, 0.199666f, 0.235877f, 0.249352f, 0.235877f, 0.199666f, 0.151240f, 0.102512f, 0.062177f, 0.033746f },
        { 0.018316f, 0.033746f, 0.055638f, 0.082085f, 0.108368f, 0.128022f, 0.135335f, 0.128022f, 0.108368f, 0.082085f, 0.055638f, 0.033746f, 0.018316f }
        #endif
    };
    Mask<float> mask(filter_mask);

    // define Domain for blur filter
    Domain dom(sigma_d, sigma_d);

    // input and output image of width x height pixels
    Image<uchar> in(width, height);
    Image<uchar> out(width, height);

    // iteration space
    IterationSpace<uchar> iter(out);

    in = host_in;

    fprintf(stderr, "Calculating HIPAcc bilateral filter ...\n");

    // Image only
    // Boundary::CLAMP
    BoundaryCondition<uchar> BcInClamp(in, mask, Boundary::CLAMP);
    Accessor<uchar> AccInClamp(BcInClamp);
    BilateralFilterMask BF(iter, AccInClamp, mask, dom, sigma_d, sigma_r);

    BF.execute();
    timing = hipacc_last_kernel_timing();

    fprintf(stderr, "Hipacc: %.3f ms, %.3f Mpixel/s\n\n", timing, (width*height/timing)/1000);

    // get results
    host_out = out.data();

#ifdef FREEIMAGE
    // Save grayscale image
    FIBITMAP* outimg = FreeImage_ConvertFromRawBits(host_out, width, height, width,
                                                   8, 255, 255, 255, FALSE);
    if (FreeImage_Save(FIF_PNG, outimg, "filtered.png")) {
      std::cout << "FreeImage: Successfully saved image filtered.png" << std::endl;
    } else {
      std::cerr << "FreeImage: Failed to save image filtered.png" << std::endl;
    }

    // Cleanup
    FreeImage_Unload(outimg);
    FreeImage_Unload(gray);
#else
    // memory cleanup
    //free(host_in);
#endif
    //free(host_out);

    return EXIT_SUCCESS;
}

