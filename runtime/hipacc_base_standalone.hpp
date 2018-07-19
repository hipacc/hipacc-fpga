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


// This is the standalone (header-only) Hipacc base runtime


#include "hipacc_base.hpp"


#ifndef __HIPACC_BASE_STANDALONE_HPP__
#define __HIPACC_BASE_STANDALONE_HPP__


float last_gpu_timing = 0.0f;

// get GPU timing of last executed Kernel in ms
float hipacc_last_kernel_timing() {
    return last_gpu_timing;
}

int64_t hipacc_time_micro() {
#ifdef VIVADO_SYNTHESIS
    // Vivado HLS does not support chrono
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000000LL + ts.tv_nsec / 1000LL;
#else // VIVADO_SYNTHESIS
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
#endif
}


unsigned int nextPow2(unsigned int x) {
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    ++x;

    // get at least the warp size
    if (x < 32) x = 32;

    return x;
}


HipaccImageBase::HipaccImageBase(size_t width, size_t height, size_t stride,
    size_t alignment, size_t pixel_size, void *mem, hipaccMemoryType mem_type)
    : width(width), height(height), stride(stride), alignment(alignment),
      pixel_size(pixel_size), mem(mem), mem_type(mem_type),
      host(new char[width*height*pixel_size]) {
    std::fill(host, host + width*height*pixel_size, 0);
}

HipaccImageBase::~HipaccImageBase() {
    delete[] host;
}

bool HipaccImageBase::operator==(const HipaccImageBase &other) const {
    return mem == other.mem;
}


HipaccAccessor::HipaccAccessor(HipaccImage img, size_t width, size_t height, int32_t offset_x, int32_t offset_y)
    : img(img), width(width), height(height), offset_x(offset_x), offset_y(offset_y) {}

HipaccAccessor::HipaccAccessor(HipaccImage img)
    : img(img), width(img->width), height(img->height), offset_x(0), offset_y(0) {}


HipaccContextBase::HipaccContextBase() {}


hipacc_launch_info::hipacc_launch_info(int size_x, int size_y, int is_width, int is_height, int
        offset_x, int offset_y, int pixels_per_thread, int simd_width) :
    size_x(size_x), size_y(size_y), is_width(is_width),
    is_height(is_height), offset_x(offset_x), offset_y(offset_y), pixels_per_thread(pixels_per_thread), simd_width(simd_width),
    bh_start_left(0), bh_start_right(0), bh_start_top(0),
    bh_start_bottom(0), bh_fall_back(0) {}

hipacc_launch_info::hipacc_launch_info(int size_x, int size_y, HipaccAccessor &Acc, int pixels_per_thread, int simd_width) :
    size_x(size_x), size_y(size_y), is_width(Acc.width),
    is_height(Acc.height), offset_x(Acc.offset_x), offset_y(Acc.offset_y),
    pixels_per_thread(pixels_per_thread), simd_width(simd_width),
    bh_start_left(0), bh_start_right(0), bh_start_top(0),
    bh_start_bottom(0), bh_fall_back(0) {}


hipacc_smem_info::hipacc_smem_info(int size_x, int size_y, int pixel_size) :
    size_x(size_x), size_y(size_y), pixel_size(pixel_size) {}


HipaccPyramid::HipaccPyramid(const int depth)
    : depth_(depth), level_(0), bound_(false) {
}

void HipaccPyramid::add(const HipaccImage &img) {
    imgs_.push_back(img);
}

HipaccImage& HipaccPyramid::operator()(int relative) {
    assert(level_ + relative >= 0 && level_ + relative < (int)imgs_.size() &&
           "Accessed pyramid stage is out of bounds.");
    return imgs_.at(level_+relative);
}

int HipaccPyramid::depth() const {
    return depth_;
}

int HipaccPyramid::level() const {
    return level_;
}

bool HipaccPyramid::is_top_level() const {
    return level_ == 0;
}

bool HipaccPyramid::is_bottom_level() const {
    return level_ == depth_-1;
}

void HipaccPyramid::swap(HipaccPyramid &other) {
    std::vector<HipaccImage> tmp = other.imgs_;
    other.imgs_ = this->imgs_;
    this->imgs_ = tmp;
}

bool HipaccPyramid::bind() {
    if (!bound_) {
        bound_ = true;
        level_ = 0;
        return true;
    } else {
        return false;
    }
}

void HipaccPyramid::unbind() {
    bound_ = false;
}


std::vector<const std::function<void()>*> hipaccTraverseFunc;
std::vector<std::vector<HipaccPyramid*>>  hipaccPyramids;


void hipaccTraverse(HipaccPyramid &p0, const std::function<void()> func) {
    assert(p0.bind() && "Pyramid already bound to another traversal.");

    std::vector<HipaccPyramid*> pyrs;
    pyrs.push_back(&p0);

    hipaccPyramids.push_back(pyrs);
    hipaccTraverseFunc.push_back(&func);

    (*hipaccTraverseFunc.back())();

    hipaccTraverseFunc.pop_back();
    hipaccPyramids.pop_back();

    p0.unbind();
}


void hipaccTraverse(HipaccPyramid &p0, HipaccPyramid &p1,
                    const std::function<void()> func) {
    assert(p0.depth_ == p1.depth_ &&
           "Pyramid depths do not match.");

    assert(p0.bind() && "Pyramid already bound to another traversal.");
    assert(p1.bind() && "Pyramid already bound to another traversal.");

    std::vector<HipaccPyramid*> pyrs;
    pyrs.push_back(&p0);
    pyrs.push_back(&p1);

    hipaccPyramids.push_back(pyrs);
    hipaccTraverseFunc.push_back(&func);

    (*hipaccTraverseFunc.back())();

    hipaccTraverseFunc.pop_back();
    hipaccPyramids.pop_back();

    p0.unbind();
    p1.unbind();
}


void hipaccTraverse(HipaccPyramid &p0, HipaccPyramid &p1, HipaccPyramid &p2,
                    const std::function<void()> func) {
    assert(p0.depth_ == p1.depth_ &&
           p1.depth_ == p2.depth_ &&
           "Pyramid depths do not match.");

    assert(p0.bind() && "Pyramid already bound to another traversal.");
    assert(p1.bind() && "Pyramid already bound to another traversal.");
    assert(p2.bind() && "Pyramid already bound to another traversal.");

    std::vector<HipaccPyramid*> pyrs;
    pyrs.push_back(&p0);
    pyrs.push_back(&p1);
    pyrs.push_back(&p2);

    hipaccPyramids.push_back(pyrs);
    hipaccTraverseFunc.push_back(&func);

    (*hipaccTraverseFunc.back())();

    hipaccTraverseFunc.pop_back();
    hipaccPyramids.pop_back();

    p0.unbind();
    p1.unbind();
    p2.unbind();
}


void hipaccTraverse(HipaccPyramid &p0, HipaccPyramid &p1, HipaccPyramid &p2,
                    HipaccPyramid &p3, const std::function<void()> func) {
    assert(p0.depth_ == p1.depth_ &&
           p1.depth_ == p2.depth_ &&
           p2.depth_ == p3.depth_ &&
           "Pyramid depths do not match.");

    assert(p0.bind() && "Pyramid already bound to another traversal.");
    assert(p1.bind() && "Pyramid already bound to another traversal.");
    assert(p2.bind() && "Pyramid already bound to another traversal.");
    assert(p3.bind() && "Pyramid already bound to another traversal.");

    std::vector<HipaccPyramid*> pyrs;
    pyrs.push_back(&p0);
    pyrs.push_back(&p1);
    pyrs.push_back(&p2);
    pyrs.push_back(&p3);

    hipaccPyramids.push_back(pyrs);
    hipaccTraverseFunc.push_back(&func);

    (*hipaccTraverseFunc.back())();

    hipaccTraverseFunc.pop_back();
    hipaccPyramids.pop_back();

    p0.unbind();
    p1.unbind();
    p2.unbind();
    p3.unbind();
}


void hipaccTraverse(HipaccPyramid &p0, HipaccPyramid &p1, HipaccPyramid &p2,
                    HipaccPyramid &p3, HipaccPyramid &p4,
                    const std::function<void()> func) {
    assert(p0.depth_ == p1.depth_ &&
           p1.depth_ == p2.depth_ &&
           p2.depth_ == p3.depth_ &&
           p3.depth_ == p4.depth_ &&
           "Pyramid depths do not match.");

    assert(p0.bind() && "Pyramid already bound to another traversal.");
    assert(p1.bind() && "Pyramid already bound to another traversal.");
    assert(p2.bind() && "Pyramid already bound to another traversal.");
    assert(p3.bind() && "Pyramid already bound to another traversal.");
    assert(p4.bind() && "Pyramid already bound to another traversal.");

    std::vector<HipaccPyramid*> pyrs;
    pyrs.push_back(&p0);
    pyrs.push_back(&p1);
    pyrs.push_back(&p2);
    pyrs.push_back(&p3);
    pyrs.push_back(&p4);

    hipaccPyramids.push_back(pyrs);
    hipaccTraverseFunc.push_back(&func);

    (*hipaccTraverseFunc.back())();

    hipaccTraverseFunc.pop_back();
    hipaccPyramids.pop_back();

    p0.unbind();
    p1.unbind();
    p2.unbind();
    p3.unbind();
    p4.unbind();
}


void hipaccTraverse(std::vector<HipaccPyramid*> pyrs,
                    const std::function<void()> func) {
    for (size_t i=0; i<pyrs.size(); ++i) {
        if (i < pyrs.size() - 1) {
            assert(pyrs[i]->depth_ == pyrs[i+1]->depth_ && "Pyramid depths do not match.");
        }
        assert(pyrs[i]->bind() && "Pyramid already bound to another traversal.");
    }

    hipaccPyramids.push_back(pyrs);
    hipaccTraverseFunc.push_back(&func);

    (*hipaccTraverseFunc.back())();

    hipaccTraverseFunc.pop_back();
    hipaccPyramids.pop_back();

    for (auto pyr : pyrs)
        pyr->unbind();
}


void hipaccTraverse(unsigned int loop,
                    const std::function<void()> func) {
    assert(!hipaccPyramids.empty() && "Traverse recursion called outside of traverse.");

    std::vector<HipaccPyramid*> pyrs = hipaccPyramids.back();

    if (!pyrs.at(0)->is_bottom_level()) {
        for (auto pyr : pyrs)
            ++pyr->level_;

        for (size_t i=0; i<loop; i++) {
            (*hipaccTraverseFunc.back())();
            if (i < loop-1) {
                func();
            }
        }

        for (auto pyr : pyrs)
            --pyr->level_;
    }
}


#endif // __HIPACC_BASE_STANDALONE_HPP__

