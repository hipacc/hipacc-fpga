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

#ifndef __PYRAMID_HPP__
#define __PYRAMID_HPP__

#include "types.hpp"
#include "image.hpp"

#include <vector>
#include <functional>

namespace hipacc {

// forward declaration
template <typename data_t>
class Traversal;
class Recursion;

class PyramidBase {
  template <typename data_t>
  friend class Traversal;
  friend class Recursion;

  private:
    const int depth_;
    int level_;

    void setLevel(int level) {
      level_ = level;
    }

    void increment() {
      ++level_;
    }

    void decrement() {
      --level_;
    }

  public:
    PyramidBase(const int depth)
        : depth_(depth), level_(0) {
    }

    int getDepth() {
      return depth_;
    }

    int getLevel() {
      return level_;
    }

    bool isTopLevel() {
      return level_ == 0;
    }

    bool isBottomLevel() {
      return level_ == depth_-1;
    }
};


template<typename data_t>
class Pyramid : public PyramidBase {
  private:
    std::vector<Image<data_t> > imgs_;

  public:
    Pyramid(Image<data_t> &img, const int depth)
        : PyramidBase(depth) {
      imgs_.push_back(img);
      int height = img.getHeight()/2;
      int width = img.getWidth()/2;
      for (int i = 1; i < depth; ++i) {
        assert(width * height > 0 && "Pyramid stages to deep for image size.");
        imgs_.push_back(Image<data_t>(width, height));
        height /= 2;
        width /= 2;
      }
    }

    Image<data_t> &operator()(int relative) {
      assert(getLevel() + relative >= 0 &&
             getLevel() + relative < imgs_.size() &&
             "Accessed pyramid stage is out of bounds.");
      return imgs_.at(getLevel() + relative);
    }
};


std::function<void()> gTraverse;
std::vector<PyramidBase*> gPyramids;


template <typename data_t>
class Traversal {
  public:
    Traversal(const std::function<void()> func) {
      gTraverse = func;
      gPyramids.clear();
    }

    void add(Pyramid<data_t> &p) {
      p.setLevel(0);
      gPyramids.push_back((PyramidBase*)&p);
    }

    void run() {
      gTraverse();
    }
};


class Recursion {
  public:
    Recursion() {
      for (std::vector<PyramidBase*>::iterator it = gPyramids.begin();
           it != gPyramids.end(); ++it) {
        (*it)->increment();
      }
    }

    ~Recursion() {
      for (std::vector<PyramidBase*>::iterator it = gPyramids.begin();
           it != gPyramids.end(); ++it) {
        (*it)->decrement();
      }
    }

    void run(int loop) {
      for (int i = 0; i < loop; i++) {
        gTraverse();
      }
    }
};


template <typename data_t>
void traverse(Pyramid<data_t> &p0, Pyramid<data_t> &p1,
              const std::function<void()> func) {
    assert(p0.getDepth() == p1.getDepth() &&
           "Pyramid depths do not match.");

    Traversal<data_t> t(func);
    t.add(p0);
    t.add(p1);
    t.run();
}


template <typename data_t>
void traverse(Pyramid<data_t> &p0, Pyramid<data_t> &p1, Pyramid<data_t> &p2,
              const std::function<void()> func) {
    assert(p0.getDepth() == p1.getDepth() &&
           p1.getDepth() == p2.getDepth() &&
           "Pyramid depths do not match.");

    Traversal<data_t> t(func);
    t.add(p0);
    t.add(p1);
    t.add(p2);
    t.run();
}


template <typename data_t>
void traverse(Pyramid<data_t> &p0, Pyramid<data_t> &p1, Pyramid<data_t> &p2,
              Pyramid<data_t> &p3, const std::function<void()> func) {
    assert(p0.getDepth() == p1.getDepth() &&
           p1.getDepth() == p2.getDepth() &&
           p2.getDepth() == p3.getDepth() &&
           "Pyramid depths do not match.");

    Traversal<data_t> t(func);
    t.add(p0);
    t.add(p1);
    t.add(p2);
    t.add(p3);
    t.run();
}


template <typename data_t>
void traverse(Pyramid<data_t> &p0, Pyramid<data_t> &p1, Pyramid<data_t> &p2,
              Pyramid<data_t> &p3, Pyramid<data_t> &p4,
              const std::function<void()> func) {
    assert(p0.getDepth() == p1.getDepth() &&
           p1.getDepth() == p2.getDepth() &&
           p2.getDepth() == p3.getDepth() &&
           p3.getDepth() == p4.getDepth() &&
           "Pyramid depths do not match.");

    Traversal<data_t> t(func);
    t.add(p0);
    t.add(p1);
    t.add(p2);
    t.add(p3);
    t.add(p4);
    t.run();
}


void traverse(unsigned int loop=1) {
  assert(!gPyramids.empty() &&
         "Traverse recursion called outside of traverse.");

  if (!gPyramids.at(0)->isBottomLevel()) {
    Recursion r;
    r.run(loop);
  }
}

} // end namespace hipacc

#endif // __PYRAMID_HPP__

