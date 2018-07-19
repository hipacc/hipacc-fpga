//
// Copyright (c) 2012, University of Erlangen-Nuremberg
// Copyright (c) 2012, Siemens AG
// Copyright (c) 2010, ARM Limited
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

#ifndef __KERNEL_HPP__
#define __KERNEL_HPP__

#include <vector>

#include "iterationspace.hpp"

namespace hipacc {

int64_t hipacc_time_micro() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

enum class Reduce : uint8_t {
    SUM = 0,
    MIN,
    MAX,
    PROD,
    MEDIAN
};

template<typename data_t, typename bin_t = data_t>
class Kernel {
    private:
        const IterationSpace<data_t> &iteration_space_;
        Accessor<data_t> output_;
        std::vector<AccessorBase *> inputs_;
        data_t reduction_result_;
        bin_t bin_val_;
        unsigned int bin_idx_;
        unsigned int num_bins_;
        bool executed_ = false;
        bool reduced_ = false;
        bool break_iteration;

    public:
        explicit Kernel(IterationSpace<data_t> &iteration_space) :
            iteration_space_(iteration_space),
            output_(iteration_space.img,
                    iteration_space.width(), iteration_space.height(),
                    iteration_space.offset_x(), iteration_space.offset_y())
        {}

        virtual ~Kernel() {}
        virtual void kernel() = 0;
        virtual bin_t reduce(bin_t left, bin_t right) const {
            assert(false && "No reduce method specified");
        }
        virtual void binning(unsigned int x, unsigned int y, data_t pixel) {
            assert(false && "No binning method specified");
        }

        void add_accessor(AccessorBase *acc) { inputs_.push_back(acc); }

        void execute() {
            if (!executed_) {
                auto end  = iteration_space_.end();
                auto iter = iteration_space_.begin();
                // register input & output accessors
                for (auto acc : inputs_)
                    acc->set_iterator(&iter);
                output_.set_iterator(&iter);

                // apply kernel for whole iteration space
                auto start_time = hipacc_time_micro();
                while (iter != end) {
                    kernel();
                    ++iter;
                }
                auto end_time = hipacc_time_micro();
                hipacc_last_timing = (float)(end_time - start_time)/1000.0f;

                // de-register input & output accessors
                for (auto acc : inputs_)
                    acc->set_iterator(nullptr);
                output_.set_iterator(nullptr);

                executed_ = true;
            }
        }

        void reduce() {
            if (!executed_)
                execute();

            if (!reduced_) {
                auto end  = iteration_space_.end();
                auto iter = iteration_space_.begin();

                // register output accessor
                output_.set_iterator(&iter);

                // first element
                data_t result = output_();

                // apply reduction for remaining iteration space
                while (++iter != end)
                    result = reduce(result, output_());

                // de-register output accessor
                output_.set_iterator(nullptr);

                reduction_result_ = result;

                reduced_ = true;
            }
        }

        data_t reduced_data() {
            // apply reduction
            reduce();

            return reduction_result_;
        }

        bin_t* binned_data(const unsigned int bin_size) {
            if (!executed_)
                execute();

            num_bins_ = bin_size;

            auto end  = iteration_space_.end();
            auto iter = iteration_space_.begin();

            // register output accessor
            output_.set_iterator(&iter);

            bin_t *binned_result = new bin_t[bin_size]();

            // apply binning for whole iteration space
            while (iter != end) {
                binning(x(), y(), output_());

                assert(bin_idx_ < bin_size && "Bin index out of range");

                binned_result[bin_idx_] = reduce(binned_result[bin_idx_], bin_val_);

                ++iter;
            }

            // de-register output accessor
            output_.set_iterator(nullptr);

            return binned_result;
        }


        // access output image
        data_t &output() {
            return output_();
        }

        // access output bin
        bin_t &bin(const unsigned int idx) {
            bin_idx_ = idx;
            return bin_val_;
        }


        // low-level access functions
        data_t &output_at(const int xf, const int yf) {
            return output_.pixel_at(xf, yf);
        }

        int x() const {
            assert(output_.EI!=ElementIterator() && "ElementIterator not set!");
            return output_.x();
        }

        int y() const {
            assert(output_.EI!=ElementIterator() && "ElementIterator not set!");
            return output_.y();
        }

        unsigned int num_bins() const {
          return num_bins_;
        }

        // built-in functions: convolve, iterate, and reduce
        template <typename data_m, typename Function>
        auto convolve(Mask<data_m> &mask, Reduce mode, const Function& fun) -> decltype(fun());
        template <typename Function>
        auto reduce(Domain &domain, Reduce mode, const Function &fun) -> decltype(fun());
        template <typename Function>
        void iterate(Domain &domain, const Function &fun);
        void break_iterate() {
          break_iteration = true;
        }
};


template <typename data_t, typename bin_t> template <typename data_m, typename Function>
auto Kernel<data_t, bin_t>::convolve(Mask<data_m> &mask, Reduce mode, const Function& fun) -> decltype(fun()) {
    break_iteration = false;
    auto end  = mask.end();
    auto iter = mask.begin();

    // register mask
    mask.set_iterator(&iter);

    // initialize result - calculate first iteration
    auto result = fun();

    // advance iterator and apply kernel to remaining iteration space
    while (++iter != end && !break_iteration) {
        switch (mode) {
            case Reduce::SUM:    result += fun();                                    break;
            case Reduce::MIN:    result  = hipacc::math::min(fun(), result);         break;
            case Reduce::MAX:    result  = hipacc::math::max(fun(), result);         break;
            case Reduce::PROD:   result *= fun();                                    break;
            case Reduce::MEDIAN: assert(0 && "Reduce::MEDIAN not implemented yet!"); break;
        }
    }

    // de-register mask
    mask.set_iterator(nullptr);

    return result;
}


template <typename data_t, typename bin_t> template <typename Function>
auto Kernel<data_t, bin_t>::reduce(Domain &domain, Reduce mode, const Function &fun) -> decltype(fun()) {
    break_iteration = false;
    auto end  = domain.end();
    auto iter = domain.begin();

    // register domain
    domain.set_iterator(&iter);

    // initialize result - calculate first iteration
    auto result = fun();

    // advance iterator and apply kernel to remaining iteration space
    while (++iter != end && !break_iteration) {
        switch (mode) {
            case Reduce::SUM:    result += fun();                                    break;
            case Reduce::MIN:    result  = hipacc::math::min(fun(), result);         break;
            case Reduce::MAX:    result  = hipacc::math::max(fun(), result);         break;
            case Reduce::PROD:   result *= fun();                                    break;
            case Reduce::MEDIAN: assert(0 && "Reduce::MEDIAN not implemented yet!"); break;
        }
    }

    // de-register domain
    domain.set_iterator(nullptr);

    return result;
}


template <typename data_t, typename bin_t> template <typename Function>
void Kernel<data_t, bin_t>::iterate(Domain &domain, const Function &fun) {
    break_iteration = false;
    auto end  = domain.end();
    auto iter = domain.begin();

    // register domain
    domain.set_iterator(&iter);

    // advance iterator and apply kernel to iteration space
    while (iter != end && !break_iteration) {
        fun();
        ++iter;
    }

    // de-register domain
    domain.set_iterator(nullptr);
}
} // end namespace hipacc

#endif // __KERNEL_HPP__

