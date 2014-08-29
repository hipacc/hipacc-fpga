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

//
// The optical flow implementation at hand uses the Census Transform:
//
// Stein, Fridtjof. "Efficient computation of optical flow using the census
// transform." Pattern Recognition. Springer Berlin Heidelberg, 2004. 79-86.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "hipacc.hpp"

#define TEST

#ifdef TEST
#define WIDTH  24
#define HEIGHT 24
#else
#define WIDTH  512
#define HEIGHT 512
#endif
#define WINDOW_SIZE_X 30
#define WINDOW_SIZE_Y 30
#define EPSILON 30

using namespace hipacc;


class GaussianBlurFilter : public Kernel<uchar> {
    private:
        Accessor<uchar> &input;
        Mask<float> &mask;

    public:
        GaussianBlurFilter(IterationSpace<uchar> &iter, Accessor<uchar> &input,
                Mask<float> &mask) :
            Kernel(iter),
            input(input),
            mask(mask)
        { addAccessor(&input); }

        void kernel() {
            output() = (uchar)(convolve(mask, HipaccSUM, [&] () -> float {
                    return mask() * input(mask);
                    }) + 0.5f);
        }
};

inline uint ctn_t32(uchar data, uchar central, uint prev_result) {
    if (data > central + EPSILON) {
        return (prev_result << 2) | 0x01;
    } else if (data < central - EPSILON) {
        return (prev_result << 2) | 0x02;
    } else {
        return (prev_result << 2);
    }
}

class SignatureKernel : public Kernel<uint> {
    private:
        Accessor<uchar> &input;
        Domain &dom;

    public:
        SignatureKernel(IterationSpace<uint> &iter, Accessor<uchar> &input,
                        Domain &dom) :
            Kernel(iter),
            input(input),
            dom(dom)
        { addAccessor(&input); }

        void kernel() {
            // Census Transformation
            uchar z = input();
            uint c = 0u;
            iterate(dom, [&] () {
              c = ctn_t32(input(dom), z, c);
            });

            output() = c;
        }
};

class VectorKernel : public Kernel<int> {
    private:
        Accessor<uint> &sig1, &sig2;
        Domain &dom;

    public:
        VectorKernel(IterationSpace<int> &iter, Accessor<uint> &sig1,
                Accessor<uint> &sig2, Domain &dom) :
            Kernel(iter),
            sig1(sig1),
            sig2(sig2),
            dom(dom)
        {
            addAccessor(&sig1);
            addAccessor(&sig2);
        }

        void kernel() {
            int vec_found = 0;
            int mem_loc = 0;

            uint reference = sig1();

            iterate(dom, [&] () -> void {
                    if (sig2(dom) == reference) {
                        vec_found++;
                        // encode ix and iy as upper and lower half-word of
                        // mem_loc
                        mem_loc = (dom.getX() << 16) | (dom.getY() & 0xffff);
                    }
                    });

            // save the vector, if exactly one was found
            if (vec_found!=1) {
                mem_loc = 0;
            }

            output() = mem_loc;
        }
};


int main(int argc, const char **argv) {
    float timing = 0, fps_timing = 0;
    const int width = WIDTH;
    const int height = HEIGHT;

    uchar host_in1[WIDTH*HEIGHT] = {
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
    uchar host_in2[WIDTH*HEIGHT] = {
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
             0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,
             0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,
             0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,
             0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,
             0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,
             0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,
             0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,
             0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 ,
            255,255, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,255,255,255,255,255,255,255,255, 0 , 0 , 0 , 0 , 0 , 0 };
    uchar *host_out = (uchar *)malloc(sizeof(uchar)*width*height);

    // images
    Image<uchar> prev(WIDTH, HEIGHT);
    Image<uchar> img(WIDTH, HEIGHT);
    Image<uchar> filter_img(WIDTH, HEIGHT);
    Image<uint> prev_signature(WIDTH, HEIGHT);
    Image<uint> img_signature(WIDTH, HEIGHT);


    // filter mask
    const float filter_mask[3][3] = {
        { 0.057118f, 0.124758f, 0.057118f },
        { 0.124758f, 0.272496f, 0.124758f },
        { 0.057118f, 0.124758f, 0.057118f }
    };
    Mask<float> mask(filter_mask);

    // domain for vector kernel
    Domain dom(WINDOW_SIZE_X/2, WINDOW_SIZE_Y/2);
    // do not process the center pixel
    dom(0,0) = 0;

    // domain for signature kernel
    const uchar sig_coef[9][9] = {
      { 1, 0, 0, 0, 1, 0, 0, 0, 1 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 1, 0, 1, 0, 1, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 1, 0, 0, 0, 1, 0, 0, 0, 1 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 1, 0, 1, 0, 1, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 1, 0, 0, 0, 1, 0, 0, 0, 1 }};
    Domain sig_dom(sig_coef);

    // vector image
    Image<int> img_vec(WIDTH, HEIGHT);
    int *vecs = (int *) malloc(img_vec.getWidth() * img_vec.getHeight() * sizeof(int));
    //BoundaryCondition<uint> bound_img_sig(img_signature, dom, BOUNDARY_CONSTANT, 0);
    //Accessor<uint> acc_img_sig(bound_img_sig);
    Accessor<uint> acc_img_sig(img_signature);
    //BoundaryCondition<uint> bound_prev_sig(prev_signature, dom, BOUNDARY_CONSTANT, 0);
    //Accessor<uint> acc_prev_sig(bound_prev_sig);
    Accessor<uint> acc_prev_sig(prev_signature);
    IterationSpace<int> iter_vec(img_vec);
    VectorKernel vector_kernel(iter_vec, acc_img_sig, acc_prev_sig, dom);

    prev = host_in1;
    img = host_in2;

    // filter previous image/frame
    BoundaryCondition<uchar> bound_prev(prev, mask, BOUNDARY_CLAMP);
    Accessor<uchar> acc_prev(bound_prev);
    IterationSpace<uchar> iter_blur(filter_img);
    GaussianBlurFilter blur_prev(iter_blur, acc_prev, mask);
    blur_prev.execute();


    // generate signature for first image/frame
    BoundaryCondition<uchar> bound_fil(filter_img, sig_dom, BOUNDARY_CLAMP);
    Accessor<uchar> acc_fil(bound_fil);
    IterationSpace<uint> iter_prev_sig(prev_signature);
    SignatureKernel sig_prev(iter_prev_sig, acc_fil, sig_dom);
    sig_prev.execute();


#ifdef GPU
    while(true) {
#endif
      // filter frame
      BoundaryCondition<uchar> bound_img(img, mask, BOUNDARY_CLAMP);
      Accessor<uchar> acc_img(bound_img);
      GaussianBlurFilter blur_img(iter_blur, acc_img, mask);

      blur_img.execute();
      timing = hipaccGetLastKernelTiming();
      fps_timing = timing;
      fprintf(stdout, "HIPAcc blur filter: %.3f ms\n", timing);

      // generate signature for frame
      IterationSpace<uint> iter_img_sig(img_signature);
      SignatureKernel sig_img(iter_img_sig, acc_fil, sig_dom);
      sig_img.execute();
      timing = hipaccGetLastKernelTiming();
      fps_timing += timing;
      fprintf(stdout, "HIPAcc signature kernel: %.3f ms\n", timing);

      // perform matching
      vector_kernel.execute();
      timing = hipaccGetLastKernelTiming();
      fps_timing += timing;
      fprintf(stdout, "HIPAcc vector kernel: %.3f ms\n", timing);
      vecs = img_vec.getData();

      // fps time
      fprintf(stdout, "<HIPACC:> optical flow: %.3f ms, %f fps\n", fps_timing, 1000.0f/fps_timing);

#ifdef TEST
      for (int y=0; y<img_vec.getHeight(); y++) {
          for (int x=0; x<img_vec.getWidth(); x++) {
              if (vecs[x + y*img_vec.getWidth()]!=0) {
                  int loc = vecs[x + y*img_vec.getWidth()];
                  int high = loc >> 16;
                  int low = (loc & 0xffff);
                  if (low >> 15) low |= 0xffff0000;
                  fprintf(stdout, "(%d, %d) ---> (%d, %d)\n", x + high, y + low, x, y);
              }
          }
      }
#endif

#ifdef GPU
       prev = img;
    } // end while true
#endif

    return EXIT_SUCCESS;
}

