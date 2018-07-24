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
#ifndef __HIPACC_VIVADO_RED_HPP__
#define __HIPACC_VIVADO_RED_HPP__


//*********************************************************************************************************************
// Histogram
//*********************************************************************************************************************
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int NUM_BINS, typename OUT, typename IN, class ReduceKernel, class BinningKernel>
void processHistogramBinningPut(
    hls::stream<IN> &in_s,
    OUT _lmem[NUM_BINS],
    const int &width,
    const int &height,
    ReduceKernel &kernel_reduce,
    BinningKernel  &kernel_binning
    )
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);
  PRAGMA_HLS(HLS DEPENDENCE variable=_lmem intra RAW false)
  PRAGMA_HLS(HLS DEPENDENCE variable=_lmem inter false)

  // initialize
  for(int x = 0; x < NUM_BINS; x++){
    PRAGMA_HLS(HLS pipeline ii=II_TARGET)
    _lmem[x] = 0;
  }

  // Map
  uint old_key = NUM_BINS + 1;
  OUT old_acc = 0;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)

      const IN pixel = in_s.read();
      kernel_binning(x, y, NUM_BINS, pixel);
      OUT bin_val = kernel_binning.value;
      const uint new_key = kernel_binning.key;

      assert(new_key < NUM_BINS);

      OUT hist_acc = _lmem[new_key];
      if (new_key == old_key) hist_acc = old_acc;
      const OUT new_acc = kernel_reduce(hist_acc, bin_val);

      // update the pipeline regs
      _lmem[old_key] = old_acc;
      old_acc = new_acc;
      old_key = new_key;
    }
  _lmem[old_key] = old_acc;
}


template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int NUM_BINS, int VECT, typename INT, typename OUT, int BW_IN, class ReduceKernel, class BinningKernel>
void processHistogramBinningPutVECT(
    hls::stream< ap_uint<BW_IN> > &in_s,
    OUT _lmem[NUM_BINS],
    const int &width,
    const int &height,
    ReduceKernel &kernel_reduce,
    BinningKernel  &kernel_binning
    )
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  // initialize
  OUT _lmem_temp[VECT][NUM_BINS]; // parallel histograms
  PRAGMA_HLS(HLS array_partition variable=_lmem_temp dim=1)
  PRAGMA_HLS(HLS DEPENDENCE variable=_lmem_temp intra RAW false)
  PRAGMA_HLS(HLS DEPENDENCE variable=_lmem_temp inter false)

  for(int x = 0; x < NUM_BINS; x++){
    PRAGMA_HLS(HLS pipeline ii=II_TARGET)
    for(int i = 0; i < VECT; i++){
      PRAGMA_HLS(HLS unroll)
      _lmem_temp[i][x] = 0;
    }
  }

  // Map
  uint old_key[VECT];
  OUT  old_acc[VECT];
  PRAGMA_HLS(HLS array_partition variable=old_key dim=0)
  PRAGMA_HLS(HLS array_partition variable=old_acc dim=0)
  for(int i = 0; i < VECT; i++){
    PRAGMA_HLS(HLS unroll)
    old_key[i] = NUM_BINS + 1;
    old_acc[i] = 0;
  }

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; x+=VECT) {
      PRAGMA_HLS(HLS loop_flatten)
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const ap_uint<BW_IN> new_databeat = in_s.read();
      for(int i = 0; i < VECT; i++){
        PRAGMA_HLS(HLS unroll)

        // calculate new histogram value (acc) with forwarding
        float pixel = new_databeat(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1);
        kernel_binning(x, y, NUM_BINS, pixel);
        OUT bin_val = kernel_binning.value;
        uint new_key = kernel_binning.key;

        assert(new_key < NUM_BINS);

        OUT hist_acc = _lmem_temp[i][new_key];
        if (new_key == old_key[i]) hist_acc = old_acc[i];

        const OUT new_acc = kernel_reduce(hist_acc, bin_val);

        // update the histogram and the pipeline regs
        _lmem_temp[i][old_key[i]] = old_acc[i];
        old_acc[i] = new_acc;
        old_key[i] = new_key;
      }
    }
  for(int i = 0; i < VECT; i++){
    PRAGMA_HLS(HLS unroll)
    _lmem_temp[i][old_key[i]] = old_acc[i];
  }

  // Reduce
  for(int x = 0; x < NUM_BINS; x++){
    PRAGMA_HLS(HLS pipeline ii=II_TARGET)
    OUT hist_val = _lmem_temp[0][x];
    for(int i = 1; i < VECT; i++){
      PRAGMA_HLS(HLS unroll)
      hist_val += _lmem_temp[i][x];
    }
    _lmem[x] = hist_val;
  }
}


template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int NUM_BINS, int VECT, typename INT, typename OUT, int BW_IN, class ReduceKernel, class BinningKernel>
void processHistogramBinningPutVECTF(
    hls::stream< ap_uint<BW_IN> > &in_s,
    OUT _lmem[NUM_BINS],
    const int &width,
    const int &height,
    ReduceKernel &kernel_reduce,
    BinningKernel  &kernel_binning
    )
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  // initialize
  OUT _lmem_temp[VECT][NUM_BINS]; // parallel histograms
  PRAGMA_HLS(HLS array_partition variable=_lmem_temp dim=1)
  PRAGMA_HLS(HLS DEPENDENCE variable=_lmem_temp intra RAW false)
  PRAGMA_HLS(HLS DEPENDENCE variable=_lmem_temp inter false)

  for(int x = 0; x < NUM_BINS; x++){
    PRAGMA_HLS(HLS pipeline ii=II_TARGET)
    for(int i = 0; i < VECT; i++){
      PRAGMA_HLS(HLS unroll)
      _lmem_temp[i][x] = 0;
    }
  }

  // Map
  uint old_key[VECT];
  OUT  old_acc[VECT];
  PRAGMA_HLS(HLS array_partition variable=old_key dim=0)
  PRAGMA_HLS(HLS array_partition variable=old_acc dim=0)
  for(int i = 0; i < VECT; i++){
    PRAGMA_HLS(HLS unroll)
    old_key[i] = NUM_BINS + 1;
    old_acc[i] = 0;
  }

  for (uint y = 0; y < height; ++y)
    for (uint x = 0; x < width; x+=VECT) {
      PRAGMA_HLS(HLS loop_flatten)
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const ap_uint<BW_IN> new_databeat = in_s.read();
      for(int i = 0; i < VECT; i++){
        PRAGMA_HLS(HLS unroll)

        // calculate new histogram value (acc) with forwarding
        float pixel = i2f(new_databeat(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1));
        kernel_binning(x, y, NUM_BINS, pixel);
        OUT bin_val = kernel_binning.value;
        uint new_key = kernel_binning.key;

        assert(new_key < NUM_BINS);

        OUT hist_acc = _lmem_temp[i][new_key];
        if (new_key == old_key[i]) hist_acc = old_acc[i];

        const OUT new_acc = kernel_reduce(hist_acc, bin_val);

        // update the histogram and the pipeline regs
        _lmem_temp[i][old_key[i]] = old_acc[i];
        old_acc[i] = new_acc;
        old_key[i] = new_key;
      }
    }
  for(int i = 0; i < VECT; i++){
    PRAGMA_HLS(HLS unroll)
    _lmem_temp[i][old_key[i]] = old_acc[i];
  }

  // Reduce
  for(int x = 0; x < NUM_BINS; x++){
    PRAGMA_HLS(HLS pipeline ii=II_TARGET)
    OUT hist_val = _lmem_temp[0][x];
    for(int i = 1; i < VECT; i++){
      PRAGMA_HLS(HLS unroll)
      hist_val += _lmem_temp[i][x];
    }
    _lmem[x] = hist_val;
  }
}


//*********************************************************************************************************************
// Global Reduce
//*********************************************************************************************************************
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, typename DTYPE, class ReduceKernel>
void processReduce2D(
    hls::stream<DTYPE> &in_s,
    DTYPE &out,
    const int &width,
    const int &height,
    ReduceKernel &kernel
    )
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  DTYPE result = in_s.read();
  for (int clockTick = 1; clockTick < height * width; ++clockTick){
    PRAGMA_HLS(HLS pipeline ii=II_TARGET)
    const DTYPE pixel = in_s.read();
    result = kernel(result, pixel);
  }
  out = result;
}


// one cycle-pipelined reduce of SIZE elements to 1
template<int SIZE, typename DTYPE, class ReduceKernel>
DTYPE parallel_reduce(
   DTYPE in[SIZE],
   ReduceKernel &kernel
   )
{
    DTYPE result = in[0];
    for(int i = 1; i < SIZE; i++){
      PRAGMA_HLS(HLS unroll)
      result = kernel(result, in[i]);
    }
    return result;
}


template<int SIZE, int BLOCK, typename DTYPE, class ReduceKernel>
DTYPE prefix_reduce_balanced(
        DTYPE prefix_sum,
        DTYPE in[16],
        ReduceKernel &kernel
        )
{
  DTYPE level_prev[SIZE + 1];
  PRAGMA_HLS(HLS array_partition variable=level_prev dim=0)
  PRAGMA_HLS(HLS array_partition variable=in dim=0)

  // initialize
  level_prev[0] = prefix_sum;
  for(int i = 0; i < SIZE; i++){
      PRAGMA_HLS(HLS unroll)
      level_prev[i + 1] = in[i];
  }

  for(int level_size = SIZE + 1;
          level_size > BLOCK;
          level_size = (level_size / BLOCK) + (level_size % BLOCK)){
    PRAGMA_HLS(HLS unroll)

    int offset = (level_size % BLOCK); // remainders to the next level (wires)
    for (int i = offset; i < level_size; i += BLOCK){
      PRAGMA_HLS(HLS unroll)
      level_prev[offset + (i / BLOCK)] = parallel_reduce<BLOCK, DTYPE, ReduceKernel>(&(level_prev[i]), kernel);
    }
  }

  // TODO: could be smaller than block
  DTYPE result = parallel_reduce<BLOCK, DTYPE, ReduceKernel>(&(level_prev[0]), kernel);
  return result;
}


// parallel reduce with a starting point input
template<int SIZE, typename DTYPE, class ReduceKernel>
DTYPE prefix_reduce(
   DTYPE prefix_sum,
   DTYPE in[SIZE],
   ReduceKernel &kernel
   )
{
    DTYPE result = prefix_sum;
    for(int i = 0; i < SIZE; i++){
      PRAGMA_HLS(HLS unroll)
      result = kernel(result, in[i]);
    }
    return result;
}


template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int VECT, typename DTYPE, int BW_IN, class ReduceKernel>
void processReduce2DVECT(
    hls::stream<ap_uint<BW_IN> > &in_s,
    DTYPE &out,
    const int &width,
    const int &height,
    ReduceKernel &kernel
    )
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);
  const int stride = ((width - 1) / VECT ) + 1;

  DTYPE result;
  DTYPE pixel_temp[VECT];
  PRAGMA_HLS(HLS array_partition variable=pixel_temp dim=0)
  for (int clockTick = 0; clockTick < height * stride; clockTick++){
    PRAGMA_HLS(HLS pipeline ii=II_TARGET)

    const ap_uint<BW_IN> pixel_db = in_s.read();
    for(int i = 0; i < VECT; i++){
      PRAGMA_HLS(HLS unroll)
      pixel_temp[i] = pixel_db(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1);
    }

    if(clockTick == 0) result = pixel_temp[0];

    //result = prefix_reduce<VECT, DTYPE>(result, pixel_temp, kernel);
    result = prefix_reduce_balanced<VECT, 4, DTYPE>(result, pixel_temp, kernel);
  }
  out = result;
}

#endif
