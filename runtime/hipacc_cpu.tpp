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

#ifndef __HIPACC_CPU_TPP__
#define __HIPACC_CPU_TPP__


template<typename T>
HipaccImage createImage(T *host_mem, void *mem, size_t width, size_t height, size_t stride, size_t alignment, hipaccMemoryType mem_type) {
    HipaccImage img = std::make_shared<HipaccImageCPU>(width, height, stride, alignment, sizeof(T), mem, mem_type);
    hipaccWriteMemory(img, host_mem ? host_mem : (T*)img->host);

    return img;
}


// Allocate memory with alignment specified
template<typename T>
HipaccImage hipaccCreateMemory(T *host_mem, size_t width, size_t height, size_t alignment) {
    // alignment has to be a multiple of sizeof(T)
    alignment = (int)ceilf((float)alignment/sizeof(T)) * sizeof(T);
    int stride = (int)ceilf((float)(width)/(alignment/sizeof(T))) * (alignment/sizeof(T));

    T *mem = new T[stride*height];
    return createImage(host_mem, (void *)mem, width, height, stride, alignment);
}


// Allocate memory without any alignment considerations
template<typename T>
HipaccImage hipaccCreateMemory(T *host_mem, size_t width, size_t height) {
    T *mem = new T[width*height];
    return createImage(host_mem, (void *)mem, width, height, width, 0);
}


// Write to memory
template<typename T>
void hipaccWriteMemory(HipaccImage &img, T *host_mem) {
    if (host_mem == nullptr) return;

    size_t width  = img->width;
    size_t height = img->height;
    size_t stride = img->stride;

    if ((char *)host_mem != img->host)
        std::copy(host_mem, host_mem + width*height, (T*)img->host);

    if (stride > width) {
        for (size_t i=0; i<height; ++i) {
            std::memcpy(&((T*)img->mem)[i*stride], &host_mem[i*width], sizeof(T)*width);
        }
    } else {
        std::memcpy(img->mem, host_mem, sizeof(T)*width*height);
    }
}


// Read from memory
template<typename T>
T *hipaccReadMemory(const HipaccImage &img) {
    size_t width  = img->width;
    size_t height = img->height;
    size_t stride = img->stride;

    if (stride > width) {
        for (size_t i=0; i<height; ++i) {
            std::memcpy(&((T*)img->host)[i*width], &((T*)img->mem)[i*stride], sizeof(T)*width);
        }
    } else {
        std::memcpy((T*)img->host, img->mem, sizeof(T)*width*height);
    }

    return (T*)img->host;
}


// Infer non-const Domain from non-const Mask
template<typename T>
void hipaccWriteDomainFromMask(HipaccImage &dom, T* host_mem) {
    size_t size = dom->width * dom->height;
    uchar *dom_mem = new uchar[size];

    for (size_t i=0; i<size; ++i) {
        dom_mem[i] = (host_mem[i] == T(0) ? 0 : 1);
    }

    hipaccWriteMemory(dom, dom_mem);

    delete[] dom_mem;
}


#endif  // __HIPACC_CPU_TPP__

