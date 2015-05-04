//
// Copyright (c) 2014, Saarland University
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

#ifndef __HIPACC_CPU_HPP__
#define __HIPACC_CPU_HPP__

#include <string.h>
#include <iostream>

#include <hls_stream.h>
#include <ap_int.h>

#include "hipacc_base.hpp"

class HipaccContext : public HipaccContextBase {
    public:
        static HipaccContext &getInstance() {
            static HipaccContext instance;

            return instance;
        }
};

long start_time = 0L;
long end_time = 0L;

void hipaccStartTiming() {
    start_time = getMicroTime();
}

void hipaccStopTiming() {
    end_time = getMicroTime();
    last_gpu_timing = (end_time - start_time) * 1.0e-3f;

    std::cerr << "<HIPACC:> Kernel timing: "
              << last_gpu_timing << "(ms)" << std::endl;
}


// Create image to store size information
template<typename T>
HipaccImage hipaccCreateMemory(T *host_mem, int width, int height) {
    HipaccContext &Ctx = HipaccContext::getInstance();

    HipaccImage img = HipaccImage(width, height, width, 0, sizeof(T), (void *)NULL);
    Ctx.add_image(img);

    return img;
}


// Release image
void hipaccReleaseMemory(HipaccImage &img) {
    HipaccContext &Ctx = HipaccContext::getInstance();
    Ctx.del_image(img);
}


template<typename T>
T hipaccReverseBits(T in) {
  T out = 0;
  T one = 1;
  size_t size = sizeof(T) * 8;
  for (int i = 0; i < size; ++i) {
    if (i < size/2) {
      out |= (in & (one << i)) << (size - 1 - (2*i));
    } else {
      out |= (in & (one << i)) >> (size - 1 - (2*(size - 1 - i)));
    }
  }
  return out;
}


// Write to stream
template<typename T1, typename T2>
void hipaccWriteMemory(HipaccImage &img, hls::stream<T1> &s, T2 *host_mem) {
    int width = img.width;
    int height = img.height;

    for (size_t i=0; i<width*height; ++i) {
        T1 data = host_mem[i];
        s << data;
    }
}


// Read from stream
template<typename T1, typename T2>
void hipaccReadMemory(hls::stream<T1> &s, T2 *host_mem, HipaccImage &img) {
    int width = img.width;
    int height = img.height;

    for (size_t i=0; i<width*height; ++i) {
        T1 data;
        s >> data;
        host_mem[i] = data;
    }
}


// Write to stream
// T1 is ap_uint<32>
// T2 is uint (representing uchar4)
template<int BW, typename T2>
void hipaccWriteMemory(HipaccImage &img, hls::stream<ap_uint<BW> > &s, T2 *host_mem) {
    int width = img.width;
    int height = img.height;
    int vect = BW/8/sizeof(T2);

    for (size_t y=0; y<height; ++y) {
        for (size_t x=0; x<width; x+=vect) {
            size_t i = (y*width)+x;
            ap_uint<BW> data;
            if (vect > 1) {
                for (size_t v=0; v<vect; ++v) {
                    if (x+v >= width) {
                        // padding
                        data(v*BW/vect, ((v+1)*BW/vect)-1) = (T2)0;
                    } else {
                        data(v*BW/vect, ((v+1)*BW/vect)-1) = host_mem[i+v];
                    }
                }
                s << data;
            } else {
                data = host_mem[i];
                s << hipaccReverseBits<ap_uint<BW> >(data);
            }
        }
    }
}


// Read from stream
// T1 is ap_uint<32>
// T2 is uint (representing uchar4)
template<int BW, typename T2>
void hipaccReadMemory(hls::stream<ap_uint<BW> > &s, T2 *host_mem, HipaccImage &img) {
    int width = img.width;
    int height = img.height;
    int vect = BW/8/sizeof(T2);

    for (size_t y=0; y<height; ++y) {
        for (size_t x=0; x<width; x+=vect) {
            size_t i = (y*width)+x;
            ap_uint<BW> data;
            s >> data;
            if (vect > 1) {
                ap_uint<BW> temp = data;
                for (size_t v=0; v<vect; ++v) {
                    if (x+v < width) {
                        host_mem[i+v] = temp(v*BW/vect, ((v+1)*BW/vect)-1);
                    }
                }
            } else {
                ap_uint<BW> temp = hipaccReverseBits<ap_uint<BW> >(data);
                host_mem[i] = *(T2*)&temp;
            }
        }
    }
}


// Copy from stream to stream
void hipaccCopyMemory(HipaccImage &src, HipaccImage &dst) {
    assert(false && "Copy stream not implemented yet");
}


// Infer non-const Domain from non-const Mask
template<typename T>
void hipaccWriteDomainFromMask(HipaccImage &dom, T* host_mem) {
    assert(false && "Only const masks and domains are supported yet");
}


#endif  // __HIPACC_CPU_HPP__

