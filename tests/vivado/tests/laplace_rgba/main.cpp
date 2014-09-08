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

#define TEST

// variables set by Makefile
//#define SIZE_X 1
//#define SIZE_Y 1
#ifdef TEST
#define WIDTH  (24/4)
#define HEIGHT 24
#else
#define WIDTH  (4096/4)
#define HEIGHT 1024
#endif

using namespace hipacc;
using namespace hipacc::math;


// Laplace filter in HIPAcc
class LaplaceFilter : public Kernel<uchar4> {
    private:
        Accessor<uchar4> &Input;
        Domain &cDom;
        Mask<int> &cMask;

    public:
        LaplaceFilter(IterationSpace<uchar4> &IS, Accessor<uchar4>
                &Input, Domain &cDom, Mask<int> &cMask) :
            Kernel(IS),
            Input(Input),
            cDom(cDom),
            cMask(cMask)
        { addAccessor(&Input); }

        void kernel() {
            int4 sum = reduce(cDom, HipaccSUM, [&] () -> int4 {
                    return cMask(cDom) * convert_int4(Input(cDom));
                    });
            sum = min(sum, 255);
            sum = max(sum, 0);
            output() = convert_uchar4(sum);
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
    const
    #if SIZE_X == 1
    #define SIZE 3
    #elif SIZE_X == 3
    #define SIZE 3
    #else
    #define SIZE 5
    #endif
    int mask[SIZE][SIZE] = {
        #if SIZE_X==1
        { 0,  1,  0 },
        { 1, -4,  1 },
        { 0,  1,  0 }
        #endif
        #if SIZE_X==3
        { 1,  1,  1 },
        { 1, -8,  1 },
        { 1,  1,  1 }
        #endif
        #if SIZE_X==5
        { 1,   1,   1,   1,   1 },
        { 1,   1,   1,   1,   1 },
        { 1,   1, -24,   1,   1 },
        { 1,   1,   1,   1,   1 },
        { 1,   1,   1,   1,   1 }
        #endif
    };

    // host memory for image of width x height pixels
    uchar tmp[WIDTH*HEIGHT*4] = {
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
    uchar4 *host_in = (uchar4*)tmp;
    uchar4 *host_out = (uchar4 *)malloc(sizeof(uchar4)*width*height);

    // input and output image of width x height pixels
    Image<uchar4> IN(width, height);
    Image<uchar4> OUT(width, height);

    // filter mask
    Mask<int> M(mask);

    // filter domain
    Domain D(M);

    IterationSpace<uchar4> IsOut(OUT);

    IN = host_in;
    OUT = host_out;


    // BOUNDARY_CLAMP
    BoundaryCondition<uchar4> BcInClamp(IN, M, BOUNDARY_CLAMP);
    Accessor<uchar4> AccInClamp(BcInClamp);
    LaplaceFilter LFC(IsOut, AccInClamp, D, M);

    LFC.execute();

    // get results
    host_out = OUT.getData();

#ifdef TEST
    int i,j;
    for(i = 0; i < HEIGHT; i++) {
        for(j = 0; j < WIDTH; j++) {
            fprintf(stdout,"%s %s %s %s ",
              host_out[i*WIDTH+j].x == 255 ? "X" : "-",
              host_out[i*WIDTH+j].y == 255 ? "X" : "-",
              host_out[i*WIDTH+j].z == 255 ? "X" : "-",
              host_out[i*WIDTH+j].w == 255 ? "X" : "-");
        }
        fprintf(stdout,"\n");
    }
#endif

    // memory cleanup
    //free(host_in);
    free(host_out);

    return EXIT_SUCCESS;
}

