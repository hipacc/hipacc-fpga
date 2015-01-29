//*********************************************************************************************************************
// VERSION 1
// Author:    Nikolas Apelt 
// File:      Filter.hpp
// Purpose:   A C++ image processing library for Vivado HLS
//*********************************************************************************************************************
// VERSION 2  -- Version 1 had some pretty fancy stuff in it, which was unfortunately eradicated
//               when Xilinx introduced Vivado HLS 14.2, which rendered most of the things rather non-functional
//               Version 2 is so far fully functional across all versions of Vivado HLS until 14.2
//               Plus, Version 2 introduces the upsample and downsample methods for pyramid designs.
// Author:    moritz.schmid@fau.de
// File:      Filter.hpp
// Purpose:   A C++ image processing library for Vivado HLS
//*********************************************************************************************************************
#pragma once

#include <ap_int.h>
#include <hls_stream.h>
#include <assert.h>
#include <typeinfo>
#include <iostream>
#define ASSERTION_CHECK

#define RADIUS (KERNEL_SIZE/2)
#define GROUP_DELAY RADIUS 
#define SWIN_Y KERNEL_SIZE
#define SWIN_X ((RADIUS>VECT) ? ((RADIUS%VECT)?RADIUS/VECT+1:RADIUS/VECT)*2+1 : KERNEL_SIZE)
#define SRADIUS_X (SWIN_X/2) 
//((RADIUS % VECT) ? RADIUS/VECT + 1 : RADIUS/VECT)*2+1
#define GROUP_DELAY_X ((int)SWIN_X/2)
#define GROUP_DELAY_Y (SWIN_Y/2)
#define WIDTH (sizeof(INT)*8)
#define INT_I_WIDTH (sizeof(INT_I)*8)
#define INT_O_WIDTH (sizeof(INT_O)*8)
#define O_WIDTH (sizeof(OUT)*8)
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

// Apron is the overlap for local operators
// The radius of the kernel is smaller than or equal to the vector size : APRON=1
// The radius is larger than the vector size: APRON = ceil(RADIUS/VECT) 
#define APRON ((RADIUS<=VECT) ? 1 : ((RADIUS%VECT)?RADIUS/VECT+1:RADIUS/VECT))
//#define APRON ((RADIUS>VECT) ? ((RADIUS%VECT)?RADIUS/VECT+1:RADIUS/VECT))

// Overlap to the left in one vector that is not part of the APRON, i.e., APRON starts after these elements
#define LEFT ((RADIUS%VECT) ? (VECT-(RADIUS%VECT)) : 0)
// Overlap to the RIGHT that are part of the APRON
#define RIGHT ((RADIUS%VECT) ? (RADIUS%VECT) : VECT)

#define GDELAY_X     (KERNEL_SIZE_X/2)
#define GDELAY_Y     (KERNEL_SIZE_Y/2)

// Oliver's VECT alternatives
#define I_WIDTH_V         (BW_IN/VECT)
#define O_WIDTH_V         (BW_OUT/VECT)
#define KERNEL_SIZE_X_V   ((GDELAY_X>VECT) ? ((GDELAY_X%VECT)?GDELAY_X/VECT+1:GDELAY_X/VECT)*2+1 : KERNEL_SIZE_X)
#define GDELAY_X_V        (KERNEL_SIZE_X_V/2)

#ifndef _BORDERPADDING_
#define _BORDERPADDING_
// Border Handling Enums
struct BorderPadding 
{
public:
    enum values {
    	BORDER_CONST,
    	BORDER_CLAMP,
    	BORDER_MIRROR,
    	BORDER_MIRROR_101
    };

    typedef void isBorderMode;
};

int getNewCoords(int i, int kernel, int offset, int col, int width, const enum BorderPadding::values borderPadding)
{
#pragma HLS INLINE
  if(col >= offset && col < kernel-1){
    int border = kernel-1 - col;
    if(i >= border)
      return i;
    else{
      // where is the border of the image in the window?
      // what is the current distance to the border
      int d2b = border - i;
      switch (borderPadding){
        case BorderPadding::BORDER_CLAMP:
          return border;
        case BorderPadding::BORDER_MIRROR:
          return border + d2b - 1;
        case BorderPadding::BORDER_MIRROR_101:
          return border+d2b;
        case BorderPadding::BORDER_CONST:
          return -1;
          default:
          return -1;
      }
    }
  }
  else if (col >= width){
    // how many pixels does the window exceed the image
    int overlap = col - (width-1);
    // where is the image border?
    int border = kernel - overlap -1;
    if(i <= border)
      return i;
    else
    {
      // what is the distance to the border?
      int d2b = i - border;  
      switch (borderPadding){
        case BorderPadding::BORDER_CLAMP:
          return border;
        case BorderPadding::BORDER_MIRROR:
          return border - d2b + 1; 
        case BorderPadding::BORDER_MIRROR_101:
          return border - d2b;
        case BorderPadding::BORDER_CONST:
          return -1;
          default:
          return -1;
      }
    }
  }
  else
    return i;
}

#endif

// conversion function for floats stored in integers
float i2f(int arg)
{
  union {
    float f;
    int i;
  } single_cast;
  single_cast.i = arg;
  return single_cast.f;
}

// conversion function to store a float in an integer
int f2i(float arg)
{
  union {
    float f;
    int i;
  } single_cast;
  single_cast.f = arg;
  return single_cast.i;
}

//*********************************************************************************************************************
// LOCAL OPERATORS OLP 
//*********************************************************************************************************************
// MAX_WIDTH : iterations in x-direction
// MAX_HEIGHT : iterations in y-direction
template<int II_TARGET, int VECT, int PART_L, int PART_R, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN>
void distribute2FIXED(
    hls::stream<IN> &data_in, 
    hls::stream<IN> &data_out0, 
    hls::stream<IN> &data_out1,
    const int &width,
    const int &height)
{
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH); assert( height <= MAX_HEIGHT );
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif
  
  IN temp;
  int part = MAX_WIDTH/2; 
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width+(2*APRON*VECT); col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col<width){  
        data_in >> temp;
        if(col >= 0 & col < PART_L-APRON)
          data_out0 << temp;
        else
          if(col >= PART_R-APRON & col < PART_R+APRON){
            data_out0 << temp;
            data_out1 << temp;
          }
        else
          data_out1 << temp;
      }
    }
  }
}
  
template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN>
void distribute2(
    hls::stream<IN> &data_in, 
    hls::stream<IN> &data_out0, 
    hls::stream<IN> &data_out1,
    const int &width,
    const int &height)
{
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH); assert( height <= MAX_HEIGHT );
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif
  
  IN temp;
  int part = MAX_WIDTH/2; 
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width+(2*APRON*VECT); col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col<width){  
        data_in >> temp;
        if(col >= 0 & col < part-APRON)
          data_out0 << temp;
        else
          if(col >= part-APRON & col < part+APRON){
            data_out0 << temp;
            data_out1 << temp;
          }
        else
          data_out1 << temp;
      }
    }
  }
}

// MAX_WIDTH : iterations in x-direction
// MAX_HEIGHT : iterations in y-direction
template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN>
void distribute4(
    hls::stream<IN> &data_in, 
    hls::stream<IN> &data_out0, 
    hls::stream<IN> &data_out1, 
    hls::stream<IN> &data_out2, 
    hls::stream<IN> &data_out3,
    const int &width,
    const int &height)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif
  
  int part = width/4;
  #ifdef ASSERTION_CHECK
    assert( part <= MAX_WIDTH/4 ); 
  #endif
  
  IN temp;
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width+2*APRON*VECT; col++){

      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col < width){
        data_in >> temp;
        
        if(col >= 0 & col < part-APRON){
          data_out0 << temp;
        }
        else if(col >= part-APRON & col < part+APRON){
          data_out0 << temp;
          data_out1 << temp;
        }
        else if(col >= part+APRON & col < (2*part)-APRON){
          data_out1 << temp;
        }
        else if(col >= (2*part)-APRON & col < (2*part)+APRON){
          data_out1 << temp;
          data_out2 << temp;
        }
        else if(col >= (2*part)+APRON & col < (3*part)-APRON){
          data_out2 << temp;
        }
        else if(col >= (3*part)-APRON & col < (3*part)+APRON){
          data_out2 << temp;
          data_out3 << temp;
        }
        else{
          data_out3 << temp;
        }
      }
    }
  }
}

// MAX_WIDTH : iterations in x-direction
// MAX_HEIGHT : iterations in y-direction
template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN>
void distribute8(
    hls::stream<IN> &data_in, 
    hls::stream<IN> &data_out0, 
    hls::stream<IN> &data_out1, 
    hls::stream<IN> &data_out2, 
    hls::stream<IN> &data_out3,
    hls::stream<IN> &data_out4, 
    hls::stream<IN> &data_out5, 
    hls::stream<IN> &data_out6, 
    hls::stream<IN> &data_out7,
    const int &width,
    const int &height)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif
  
  int part = width/8;
  #ifdef ASSERTION_CHECK
    assert( part <= MAX_WIDTH/8 ); 
  #endif
  
  IN temp;
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width+2*APRON*VECT; col++){

      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col < width){
        data_in >> temp;
        
        if(col >= 0 & col < part-APRON){
          data_out0 << temp;
        }
        else if(col >= part-APRON & col < part+APRON){
          data_out0 << temp;
          data_out1 << temp;
        }
        else if(col >= part+APRON & col < (2*part)-APRON){
          data_out1 << temp;
        }
        else if(col >= (2*part)-APRON & col < (2*part)+APRON){
          data_out1 << temp;
          data_out2 << temp;
        }
        else if(col >= (2*part)+APRON & col < (3*part)-APRON){
          data_out2 << temp;
        }
        else if(col >= (3*part)-APRON & col < (3*part)+APRON){
          data_out2 << temp;
          data_out3 << temp;
        }
        else if(col >= (3*part)+APRON & col < (4*part)-APRON){
          data_out3 << temp;
        }
        else if(col >= (4*part)-APRON & col < (4*part)+APRON){
          data_out3 << temp;
          data_out4 << temp;
        }
        else if(col >= (4*part)+APRON & col < (5*part)-APRON){
          data_out4 << temp;
        }
        else if(col >= (5*part)-APRON & col < (5*part)+APRON){
          data_out4 << temp;
          data_out5 << temp;
        }
        else if(col >= (5*part)+APRON & col < (6*part)-APRON){
          data_out5 << temp;
        }
        else if(col >= (6*part)-APRON & col < (6*part)+APRON){
          data_out5 << temp;
          data_out6 << temp;
        }
        else if(col >= (6*part)+APRON & col < (7*part)-APRON){
          data_out6 << temp;
        }
        else if(col >= (7*part)-APRON & col < (7*part)+APRON){
          data_out6 << temp;
          data_out7 << temp;
        }
        else{
          data_out7 << temp;
        }
      }
    }
  }
}

// MAX_WIDTH : iterations in x-direction
// MAX_HEIGHT : iterations in y-direction
template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN>
void distribute16(
    hls::stream<IN> &data_in, 
    hls::stream<IN> &data_out0, 
    hls::stream<IN> &data_out1, 
    hls::stream<IN> &data_out2, 
    hls::stream<IN> &data_out3,
    hls::stream<IN> &data_out4, 
    hls::stream<IN> &data_out5, 
    hls::stream<IN> &data_out6, 
    hls::stream<IN> &data_out7,
    hls::stream<IN> &data_out8, 
    hls::stream<IN> &data_out9, 
    hls::stream<IN> &data_out10, 
    hls::stream<IN> &data_out11,
    hls::stream<IN> &data_out12, 
    hls::stream<IN> &data_out13, 
    hls::stream<IN> &data_out14, 
    hls::stream<IN> &data_out15,
    const int &width,
    const int &height)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif
  
  int part = width/16;
  #ifdef ASSERTION_CHECK
    assert( part <= MAX_WIDTH/16 ); 
  #endif
  
  IN temp;
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width+2*APRON*VECT; col++){

      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col < width){
        data_in >> temp;
        
        if(col >= 0 & col < part-APRON){
          if(row == 0) std::cout << col << "single 0" << std::endl;
          data_out0 << temp;
        }
        else if(col >= part-APRON & col < part+APRON){
          if(row == 0) std::cout << col << "double 0" << std::endl;
          data_out0 << temp;
          data_out1 << temp;
        }
        else if(col >= part+APRON & col < (2*part)-APRON){
          if(row == 0) std::cout << col << "single 1" << std::endl;
          data_out1 << temp;
        }
        else if(col >= (2*part)-APRON & col < (2*part)+APRON){
          if(row == 0) std::cout << col << "double 1" << std::endl;
          data_out1 << temp;
          data_out2 << temp;
        }
        else if(col >= (2*part)+APRON & col < (3*part)-APRON){
          if(row == 0) std::cout << col << "single 2" << std::endl;
          data_out2 << temp;
        }
        else if(col >= (3*part)-APRON & col < (3*part)+APRON){
          if(row == 0) std::cout << col << "double 2" << std::endl;
          data_out2 << temp;
          data_out3 << temp;
        }
        else if(col >= (3*part)+APRON & col < (4*part)-APRON){
          if(row == 0) std::cout << col << "single 3" << std::endl;
          data_out3 << temp;
        }
        else if(col >= (4*part)-APRON & col < (4*part)+APRON){
          if(row == 0) std::cout << col << "double 3" << std::endl;
          data_out3 << temp;
          data_out4 << temp;
        }
        else if(col >= (4*part)+APRON & col < (5*part)-APRON){
          if(row == 0) std::cout << col << "single 4" << std::endl;
          data_out4 << temp;
        }
        else if(col >= (5*part)-APRON & col < (5*part)+APRON){
          if(row == 0) std::cout << col << "double 4" << std::endl;
          data_out4 << temp;
          data_out5 << temp;
        }
        else if(col >= (5*part)+APRON & col < (6*part)-APRON){
          if(row == 0) std::cout << col << "single 5" << std::endl;
          data_out5 << temp;
        }
        else if(col >= (6*part)-APRON & col < (6*part)+APRON){
          if(row == 0) std::cout << col << "double 5" << std::endl;
          data_out5 << temp;
          data_out6 << temp;
        }
        else if(col >= (6*part)+APRON & col < (7*part)-APRON){
          if(row == 0) std::cout << col << "single 6" << std::endl;
          data_out6 << temp;
        }
        else if(col >= (7*part)-APRON & col < (7*part)+APRON){
          if(row == 0) std::cout << col << "double 6" << std::endl;
          data_out6 << temp;
          data_out7 << temp;
        }
        else if(col >= (7*part)+APRON & col < (8*part)-APRON){
          if(row == 0) std::cout << col << "single 7" << std::endl;
          data_out7 << temp;
        }
        else if(col >= (8*part)-APRON & col < (8*part)+APRON){
          if(row == 0) std::cout << col << "double 7" << std::endl;
          data_out7 << temp;
          data_out8 << temp;
        }
        else if(col >= (8*part)+APRON & col < (9*part)-APRON){
          if(row == 0) std::cout << col << "single 8" << std::endl;
          data_out8 << temp;
        }
        else if(col >= (9*part)-APRON & col < (9*part)+APRON){
          if(row == 0) std::cout << col << "double 8" << std::endl;
          data_out8 << temp;
          data_out9 << temp;
        }
        else if(col >= (9*part)+APRON & col < (10*part)-APRON){
          if(row == 0) std::cout << col << "single 9" << std::endl;
          data_out9 << temp;
        }
        else if(col >= (10*part)-APRON & col < (10*part)+APRON){
          if(row == 0) std::cout << col << "double 9" << std::endl;
          data_out9 << temp;
          data_out10 << temp;
        }
        else if(col >= (10*part)+APRON & col < (11*part)-APRON){
          if(row == 0) std::cout << col << "single 10" << std::endl;
          data_out10 << temp;
        }
        else if(col >= (11*part)-APRON & col < (11*part)+APRON){
          if(row == 0) std::cout << col << "double 10" << std::endl;
          data_out10 << temp;
          data_out11 << temp;
        }
        else if(col >= (11*part)+APRON & col < (12*part)-APRON){
          if(row == 0) std::cout << col << "single 11" << std::endl;
          data_out11 << temp;
        }
        else if(col >= (12*part)-APRON & col < (12*part)+APRON){
          if(row == 0) std::cout << col << "double 11" << std::endl;
          data_out11 << temp;
          data_out12 << temp;
        }
        else if(col >= (12*part)+APRON & col < (13*part)-APRON){
          if(row == 0) std::cout << col << "single 12" << std::endl;
          data_out12 << temp;
        }
        else if(col >= (13*part)-APRON & col < (13*part)+APRON){
          if(row == 0) std::cout << col << "double 12" << std::endl;
          data_out12 << temp;
          data_out13 << temp;
        }
        else if(col >= (13*part)+APRON & col < (14*part)-APRON){
          if(row == 0) std::cout << col << "single 13" << std::endl;
          data_out13 << temp;
        }
        else if(col >= (14*part)-APRON & col < (14*part)+APRON){
          if(row == 0) std::cout << col << "double 13" << std::endl;
          data_out13 << temp;
          data_out14 << temp;
        }
        else if(col >= (14*part)+APRON & col < (15*part)-APRON){
          if(row == 0) std::cout << col << "single 14" << std::endl;
          data_out14 << temp;
        }
        else if(col >= (15*part)-APRON & col < (15*part)+APRON){
          if(row == 0) std::cout << col << "double 14" << std::endl;
          data_out14 << temp;
          data_out15 << temp;
        }
        else{
          if(row == 0) std::cout << col << "single 15" << std::endl;
          data_out15 << temp;
        }
      }
    }
  }
}

// SPLIT aggregate data types from MAJOR to MINOR
// Left split has overhang to the right
template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename INT>
void split_L(
    hls::stream<IN> &data_in, 
    hls::stream<INT> &data_out,
    const int &width,
    const int &height)
{
  assert( width <= MAX_WIDTH+APRON ); assert( height <= MAX_HEIGHT );
  assert( (KERNEL_SIZE % 2) == 1 );

  IN temp;
  INT _temp;
  std::cout << "width: " << WIDTH << std::endl;
  for(int row = 0; row < height; row++){
    for(int col = 0; col < width+(2*APRON*VECT); col++){
   	  PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      if(col < width+(APRON*VECT)){
	      //read at the beginning of a vector cycle
        if(col%VECT==0 ){
          data_in >> temp;
          //if(row == 0) std::cout << "READ " << col << ": " << temp << std::endl;
        }

        //always convert
        _temp = temp((col%VECT)*WIDTH,((col%VECT)+1)*WIDTH-1);
        //if(row == 0) std::cout << "CONVERT " << col << ": " << _temp << std::endl;

        // only assign output if data is valid
        if(col < width+(APRON-1)*VECT+RIGHT){
          data_out << _temp;
          //if(row == 0) std::cout << "WRITE " << col << ": " << _temp << std::endl;
        }
      }
    }
  }
}

template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename INT>
void splitF_L(
    hls::stream<IN> &data_in, 
    hls::stream<INT> &data_out,
    const int &width,
    const int &height)
{
  assert( width <= MAX_WIDTH+APRON ); assert( height <= MAX_HEIGHT );
  assert( (KERNEL_SIZE % 2) == 1 );

  IN temp;
  INT _temp;
  std::cout << "width: " << WIDTH << std::endl;
  for(int row = 0; row < height; row++){
    for(int col = 0; col < width+(2*APRON*VECT); col++){
   	  PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      if(col < width+(APRON*VECT)){
	      //read at the beginning of a vector cycle
        if(col%VECT==0 ){
          data_in >> temp;
          //if(row == 0) std::cout << "READ " << col << ": " << temp << std::endl;
        }

        //always convert
        _temp = i2f(temp((col%VECT)*WIDTH,((col%VECT)+1)*WIDTH-1));
        //if(row == 0) std::cout << "CONVERT " << col << ": " << _temp << std::endl;

        // only assign output if data is valid
        if(col < width+(APRON-1)*VECT+RIGHT){
          data_out << _temp;
          //if(row == 0) std::cout << "WRITE " << col << ": " << _temp << std::endl;
        }
      }
    }
  }
}

// Center split has overhang to the left and right
template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename INT>
void split_C(
    hls::stream<IN> &data_in, 
    hls::stream<INT> &data_out,
    const int &width,
    const int &height)
{
  
  assert( width <= MAX_WIDTH+(2*APRON) ); assert( height <= MAX_HEIGHT );
  assert( (KERNEL_SIZE % 2) == 1 );

  IN temp;
  INT _temp;
  for(int row = 0; row < height; row++){
    for(int col = 0; col < width+(2*APRON*VECT); col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      if(col%VECT==0){
        data_in >> temp;
        //if(row == 0) std::cout << "READ " << col << ": " << temp << std::endl;
      }
      _temp = temp(((col%VECT)+1)*WIDTH-1,(col%VECT)*WIDTH);
      //_temp = temp((col%VECT)*WIDTH,((col%VECT)+1)*WIDTH-1);
      //if(row == 0) std::cout << "CONVERT " << col << ": " << _temp << std::endl;

      if(col >= LEFT & col < width+(2*APRON-1)*VECT+RIGHT){
        data_out << _temp;
        //if(row == 0) std::cout << "WRITE " << col << ": " << _temp << std::endl;
      } 
    }
  }
} 

// Center split has overhang to the left and right
template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename INT>
void splitF_C(
    hls::stream<IN> &data_in, 
    hls::stream<INT> &data_out,
    const int &width,
    const int &height)
{
  
  assert( width <= MAX_WIDTH+(2*APRON) ); assert( height <= MAX_HEIGHT );
  assert( (KERNEL_SIZE % 2) == 1 );

  IN temp;
  INT _temp;
  for(int row = 0; row < height; row++){
    for(int col = 0; col < width+(2*APRON*VECT); col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      if(col%VECT==0){
        data_in >> temp;
        //if(row == 0) std::cout << "READ " << col << ": " << temp << std::endl;
      }
      _temp = i2f(temp(((col%VECT)+1)*WIDTH-1,(col%VECT)*WIDTH));
      //_temp = temp((col%VECT)*WIDTH,((col%VECT)+1)*WIDTH-1);
      //if(row == 0) std::cout << "CONVERT " << col << ": " << _temp << std::endl;

      if(col >= LEFT & col < width+(2*APRON-1)*VECT+RIGHT){
        data_out << _temp;
        //if(row == 0) std::cout << "WRITE " << col << ": " << _temp << std::endl;
      } 
    }
  }
} 

// Center split has overhang to the left and right
template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename INT>
void splitF_R(
    hls::stream<IN> &data_in, 
    hls::stream<INT> &data_out,
    const int &width,
    const int &height)
{
  
  assert( width <= MAX_WIDTH+APRON ); assert( height <= MAX_HEIGHT );
  assert( (KERNEL_SIZE % 2) == 1 );

  IN temp;
  INT _temp;

  for(int row = 0; row < height; row++){
    // COLUMNS are SIZE_X/(OLP*VECT)+APRON  
    for(int col = 0; col < width+(2*APRON*VECT); col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col<width+(APRON*VECT)){
        if(col%VECT==0){
          data_in >> temp;
          if(row == 0) std::cout << "READ " << col << ": " << std::hex << temp << std::endl;
        }
        
        _temp = i2f(temp(((col%VECT)+1)*WIDTH-1,(col%VECT)*WIDTH));
        //_temp = temp((col%VECT)*WIDTH,((col%VECT)+1)*WIDTH-1);
        if(row == 0) std::cout << "CONVERT " << col << ": " << std::hex << _temp << std::endl;

        if(col>=LEFT & col < width+(APRON*VECT)){
    	    data_out << _temp;
          if(row == 0) std::cout << "WRITE " << col << ": " << std::hex << _temp << std::endl;
        }
      }
    }
  }
}

// Center split has overhang to the left and right
template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename INT>
void split_R(
    hls::stream<IN> &data_in, 
    hls::stream<INT> &data_out,
    const int &width,
    const int &height)
{
  
  assert( width <= MAX_WIDTH+APRON ); assert( height <= MAX_HEIGHT );
  assert( (KERNEL_SIZE % 2) == 1 );

  IN temp;
  INT _temp;

  for(int row = 0; row < height; row++){
    // COLUMNS are SIZE_X/(OLP*VECT)+APRON  
    for(int col = 0; col < width+(2*APRON*VECT); col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col<width+(APRON*VECT)){
        if(col%VECT==0){
          data_in >> temp;
          if(row == 0) std::cout << "READ " << col << ": " << std::hex << temp << std::endl;
        }
        
        _temp = temp(((col%VECT)+1)*WIDTH-1,(col%VECT)*WIDTH);
        //_temp = temp((col%VECT)*WIDTH,((col%VECT)+1)*WIDTH-1);
        if(row == 0) std::cout << "CONVERT " << col << ": " << std::hex << _temp << std::endl;

        if(col>=LEFT & col < width+(APRON*VECT)){
    	    data_out << _temp;
          if(row == 0) std::cout << "WRITE " << col << ": " << std::hex << _temp << std::endl;
        }
      }
    }
  }
}


template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename INT, typename OUT>
void assemble(
    hls::stream<INT> &data_in, 
    hls::stream<OUT> &data_out,
    const int &width,
    const int &height)
{
  
  assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
  assert( (KERNEL_SIZE % 2) == 1 );

  OUT temp;
  INT _temp;

  for(int row = 0; row < height; row++){
    for(int col = 0; col < width+(2*APRON*VECT); col++){
    	PRAGMA_HLS(HLS pipeline ii=II_TARGET)
    	#pragma HLS INLINE region
      if(col < width){
        data_in >> _temp;
        temp(((col%VECT)*WIDTH),((col%VECT)+1)*WIDTH-1) = _temp;
        if(col%VECT == VECT-1)
          data_out << temp;
      }
    }
  }
}

template<int II_TARGET, int VECT, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename INT, typename OUT>
void assembleF(
    hls::stream<INT> &data_in, 
    hls::stream<OUT> &data_out,
    const int &width,
    const int &height)
{
  
  assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
  assert( (KERNEL_SIZE % 2) == 1 );

  OUT temp;
  INT _temp;

  for(int row = 0; row < height; row++){
    for(int col = 0; col < width+(2*APRON*VECT); col++){
    	PRAGMA_HLS(HLS pipeline ii=II_TARGET)
    	#pragma HLS INLINE region
      if(col < width){
        data_in >> _temp;
        temp(((col%VECT)*WIDTH),((col%VECT)+1)*WIDTH-1) = f2i(_temp);
        if(col%VECT == VECT-1)
          data_out << temp;
      }
    }
  }
}


// normal processing, one input, one output stream
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processL(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH + GROUP_DELAY ); assert( height <= MAX_HEIGHT + GROUP_DELAY);
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif

  IN lineBuff[KERNEL_SIZE-1][MAX_WIDTH+GROUP_DELAY];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  OUT out_pixel;
  IN temp_lb, in_pixel;
  int i, j ,row, col;

  process_main_loop:
  for (int row = 0; row < height+GROUP_DELAY; row++) {
    for (int col = 0; col < width+(2*GROUP_DELAY); col++) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width+GROUP_DELAY & row < height){
        in_s >> in_pixel;
       // if(row==0) std::cout << col << ": " << in_pixel << std::endl;
      //}

      //**********************************************************
      // UPDATE THE WINDOW
      //**********************************************************
      //if (col < width+GROUP_DELAY & row < height+GROUP_DELAY){
        for(i = 0; i < KERNEL_SIZE; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      //}

      //**********************************************************
      // UPDATE THE LINE BUFFER
      //**********************************************************
      //if (col < width & row < height+GROUP_DELAY){
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE-1] = lineBuff[i][col];
          } else {
            temp_lb = lineBuff[i][col];
            win_tmp[i][KERNEL_SIZE-1] = temp_lb;
            lineBuff[i-1][col] = temp_lb;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        if (KERNEL_SIZE > 1) {
          lineBuff[KERNEL_SIZE-2][col] = in_pixel;
        }
        win_tmp[KERNEL_SIZE-1][KERNEL_SIZE-1] = in_pixel;
      }

      //**********************************************************
      // HANDLE BORDERS 
      //**********************************************************
      // X-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int jx = getNewCoords(j,KERNEL_SIZE,GROUP_DELAY,col,width+GROUP_DELAY,borderPadding);
          win[i][j] = win_tmp[i][jx];
        }
      }
      // Y-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int ix = getNewCoords(i,KERNEL_SIZE,GROUP_DELAY,row,height,borderPadding);
          win[i][j] = win[ix][j];
        }
      }

      //**********************************************************
      // FILTER COMPUTATION AND OUTPUT ASSIGNMENT 
      //**********************************************************
      // Do the filtering
      if (row >= GROUP_DELAY && (col >= GROUP_DELAY & col < width+GROUP_DELAY)){
        out_pixel = filter(win);
        out_s.write(out_pixel);
      }
    }
  }
}

// normal processing, one input, one output stream
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processC(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH + (2*GROUP_DELAY) ); assert( height <= MAX_HEIGHT + GROUP_DELAY);
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif

  IN lineBuff[KERNEL_SIZE-1][MAX_WIDTH+2*GROUP_DELAY];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  OUT out_pixel;
  IN temp_lb, in_pixel;
  int i, j ,row, col;

  process_main_loop:
  for (int row = 0; row < height+GROUP_DELAY; row++) {
    for (int col = 0; col < width+(2*GROUP_DELAY); col++) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width+(2*GROUP_DELAY) & row < height){
        in_s >> in_pixel;
      //}

      //**********************************************************
      // UPDATE THE WINDOW
      //**********************************************************
      //if (col < width+GROUP_DELAY & row < height+GROUP_DELAY){
        for(i = 0; i < KERNEL_SIZE; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      //}

      //**********************************************************
      // UPDATE THE LINE BUFFER
      //**********************************************************
      //if (col < width & row < height+GROUP_DELAY){
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE-1] = lineBuff[i][col];
          } else {
            temp_lb = lineBuff[i][col];
            win_tmp[i][KERNEL_SIZE-1] = temp_lb;
            lineBuff[i-1][col] = temp_lb;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        if (KERNEL_SIZE > 1) {
          lineBuff[KERNEL_SIZE-2][col] = in_pixel;
        }
        win_tmp[KERNEL_SIZE-1][KERNEL_SIZE-1] = in_pixel;
      }

      //**********************************************************
      // HANDLE BORDERS 
      //**********************************************************
      // X-DIRECTION
      /*
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int jx = getNewCoords(j,KERNEL_SIZE,GROUP_DELAY,col,width+(2*GROUP_DELAY),borderPadding);
          win[i][j] = win_tmp[i][jx];
        }
      }
      */
      // Y-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int ix = getNewCoords(i,KERNEL_SIZE,GROUP_DELAY,row,height,borderPadding);
          win[i][j] = win_tmp[ix][j];
        }
      }

      //**********************************************************
      // FILTER COMPUTATION AND OUTPUT ASSIGNMENT 
      //**********************************************************
      // Do the filtering
      if (row >= GROUP_DELAY && col >= (2*GROUP_DELAY)){
        out_pixel = filter(win);
        out_s.write(out_pixel);
      }
    }
  }
}

// normal processing, one input, one output stream
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processR(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT);
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif

  IN lineBuff[KERNEL_SIZE-1][MAX_WIDTH+2*GROUP_DELAY];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  OUT out_pixel;
  IN temp_lb, in_pixel;
  int i, j ,row, col;

  process_main_loop:
  for (int row = 0; row < height+GROUP_DELAY; row++) {
    for (int col = 0; col < width+(2*GROUP_DELAY); col++) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width+GROUP_DELAY & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      //**********************************************************
      if (col < width+(2*GROUP_DELAY) & row < height+GROUP_DELAY){
        for(i = 0; i < KERNEL_SIZE; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      //}

      //**********************************************************
      // UPDATE THE LINE BUFFER
      //**********************************************************
      //if (col < width & row < height+GROUP_DELAY){
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE-1] = lineBuff[i][col];
          } else {
            temp_lb = lineBuff[i][col];
            win_tmp[i][KERNEL_SIZE-1] = temp_lb;
            lineBuff[i-1][col] = temp_lb;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        if (KERNEL_SIZE > 1) {
          lineBuff[KERNEL_SIZE-2][col] = in_pixel;
        }
        win_tmp[KERNEL_SIZE-1][KERNEL_SIZE-1] = in_pixel;
      }

      //**********************************************************
      // HANDLE BORDERS 
      //**********************************************************
      // X-DIRECTION
      
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int jx = getNewCoords(j,KERNEL_SIZE,GROUP_DELAY,col,width+(2*GROUP_DELAY),borderPadding);
          win[i][j] = win_tmp[i][jx];
        }
      }
      
      // Y-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int ix = getNewCoords(i,KERNEL_SIZE,GROUP_DELAY,row,height,borderPadding);
          win[i][j] = win[ix][j];
        }
      }

      //**********************************************************
      // FILTER COMPUTATION AND OUTPUT ASSIGNMENT 
      //**********************************************************
      // Do the filtering
      if (row >= GROUP_DELAY && col >= (2*GROUP_DELAY)){
        out_pixel = filter(win);
        out_s.write(out_pixel);
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, typename IN, typename OUT>
void collect4(
    hls::stream<IN> &in0_s,
    hls::stream<IN> &in1_s,
    hls::stream<IN> &in2_s,
    hls::stream<IN> &in3_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
  #endif
  
  int part = width/4;
  #ifdef ASSERTION_CHECK
    assert( part <= MAX_WIDTH/4 ); 
  #endif
  
  OUT temp;
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width; col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col >= 0 & col < part){
        in0_s >> temp;
        //if(row == 0) std::cout << col << ": single 1" << std::endl;
      }
      else if(col >= part & col < (2*part)){
        in1_s >> temp;
        //if(row == 0) std::cout << col << ": single 2" << std::endl;
      }
      else if(col >= (2*part) & col < (3*part)){
        in2_s >> temp;
        //if(row == 0) std::cout << col << ": single 3" << std::endl;
      }
      else{
        in3_s >> temp;
        //if(row == 0) std::cout << col << ": single 4" << std::endl;
      }
      out_s << temp;
    }
  }
}

template<int II_TARGET, int PART_0, int PART_1, int MAX_WIDTH, int MAX_HEIGHT, typename IN, typename OUT>
void collect2FIXED(
    hls::stream<IN> &in0_s,
    hls::stream<IN> &in1_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
  #endif
  
  //int part = width/2;
  //#ifdef ASSERTION_CHECK
  //  assert( part <= MAX_WIDTH/2 ); 
  //#endif
  
  OUT temp;
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width; col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col >= 0 & col < PART_0)
        in0_s >> temp;
      else 
        in1_s >> temp;
      out_s << temp;
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, typename IN, typename OUT>
void collect2(
    hls::stream<IN> &in0_s,
    hls::stream<IN> &in1_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
  #endif
  
  int part = width/2;
  #ifdef ASSERTION_CHECK
    assert( part <= MAX_WIDTH/2 ); 
  #endif
  
  OUT temp;
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width; col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col >= 0 & col < part)
        in0_s >> temp;
      else 
        in1_s >> temp;
      out_s << temp;
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, typename IN, typename OUT>
void collect8(
    hls::stream<IN> &in0_s,
    hls::stream<IN> &in1_s,
    hls::stream<IN> &in2_s,
    hls::stream<IN> &in3_s,
    hls::stream<IN> &in4_s,
    hls::stream<IN> &in5_s,
    hls::stream<IN> &in6_s,
    hls::stream<IN> &in7_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
  #endif
  
  int part = width/8;
  #ifdef ASSERTION_CHECK
    assert( part <= MAX_WIDTH/8 ); 
  #endif
  
  OUT temp;
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width; col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col >= 0 & col < part){
        in0_s >> temp;
      }
      else if(col >= part & col < (2*part)){
        in1_s >> temp;
      }
      else if(col >= (2*part) & col < (3*part)){
        in2_s >> temp;
      }
      else if(col >= (3*part) & col < (4*part)){
        in3_s >> temp;
      }
      else if(col >= (4*part) & col < (5*part)){
        in4_s >> temp;
      }
      else if(col >= (5*part) & col < (6*part)){
        in5_s >> temp;
      }
      else if(col >= (6*part) & col < (7*part)){
        in6_s >> temp;
      }
      else{
        in7_s >> temp;
      }
      out_s << temp;
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, typename IN, typename OUT>
void collect16(
    hls::stream<IN> &in0_s,
    hls::stream<IN> &in1_s,
    hls::stream<IN> &in2_s,
    hls::stream<IN> &in3_s,
    hls::stream<IN> &in4_s,
    hls::stream<IN> &in5_s,
    hls::stream<IN> &in6_s,
    hls::stream<IN> &in7_s,
    hls::stream<IN> &in8_s,
    hls::stream<IN> &in9_s,
    hls::stream<IN> &in10_s,
    hls::stream<IN> &in11_s,
    hls::stream<IN> &in12_s,
    hls::stream<IN> &in13_s,
    hls::stream<IN> &in14_s,
    hls::stream<IN> &in15_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
  #endif
  
  int part = width/16;
  #ifdef ASSERTION_CHECK
    assert( part <= MAX_WIDTH/16 ); 
  #endif
  
  OUT temp;
  for(int row = 0; row < height; row++){  
    for(int col = 0; col < width; col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      if(col >= 0 & col < part){
        in0_s >> temp;
      }
      else if(col >= part & col < (2*part)){
        in1_s >> temp;
      }
      else if(col >= (2*part) & col < (3*part)){
        in2_s >> temp;
      }
      else if(col >= (3*part) & col < (4*part)){
        in3_s >> temp;
      }
      else if(col >= (4*part) & col < (5*part)){
        in4_s >> temp;
      }
      else if(col >= (5*part) & col < (6*part)){
        in5_s >> temp;
      }
      else if(col >= (6*part) & col < (7*part)){
        in6_s >> temp;
      }
      else if(col >= (7*part) & col < (8*part)){
        in7_s >> temp;
      }
      else if(col >= (8*part) & col < (9*part)){
        in8_s >> temp;
      }
      else if(col >= (9*part) & col < (10*part)){
        in9_s >> temp;
      }
      else if(col >= (10*part) & col < (11*part)){
        in10_s >> temp;
      }
      else if(col >= (11*part) & col < (12*part)){
        in11_s >> temp;
      }
      else if(col >= (12*part) & col < (13*part)){
        in12_s >> temp;
      }
      else if(col >= (13*part) & col < (14*part)){
        in13_s >> temp;
      }
      else if(col >= (14*part) & col < (15*part)){
        in14_s >> temp;
      }
      else{
        in15_s >> temp;
      }
      out_s << temp;
    }
  }
}


//*********************************************************************************************************************
// LOCAL OPERATORS VECTOR
//*********************************************************************************************************************
struct winPos
{
  int win;
  int pos;
};
typedef struct winPos winPos;

winPos getWinCoords(int i, int kernel_x, int kernel, int vect, int offset, int col, int width, const enum BorderPadding::values borderPadding)
{
#pragma HLS INLINE
  winPos temp;
  
  int win = i/vect;

  // this will exhibit problems
  if(col >= offset && col < kernel-1){
    int borderWin = kernel_x-1 - col;
    if(win >= borderWin){
      //we are fine
      temp.win = win;
      temp.pos = i%vect;
    // else
    }else{
      int border = borderWin*vect;
      int d2b = border-i;
      int pos = 0;
      switch (borderPadding){
        case BorderPadding::BORDER_CLAMP:
          temp.pos = border%vect;
          break;
        case BorderPadding::BORDER_MIRROR:
          temp.pos = (border + d2b - 1)%vect;
          break;
        case BorderPadding::BORDER_MIRROR_101:
          temp.pos = (border+d2b)%vect;
          break;
        case BorderPadding::BORDER_CONST:
          temp.pos = 0;
          break;
          default:
          temp.pos = 0;
          break;
      }
      temp.win = borderWin+d2b/vect;
    }
  }
  // we are at the right border
  else if(col >= width){
    int overlap = col - (width-1);
    int border = kernel - overlap - 1;
    // if we are inside the defined region
    if(win <= border){
      //we are fine
      temp.win = win;
      temp.pos = i%vect;
    }
    else{
      border = (border+1)*vect-1;
      int d2b = i - border;
      switch (borderPadding){
        case BorderPadding::BORDER_CLAMP:
          temp.pos = border%vect;
          break;
        case BorderPadding::BORDER_MIRROR:
          temp.pos = (border - d2b + 1)%vect;
          break;
        case BorderPadding::BORDER_MIRROR_101:
          temp.pos = (border - d2b)%vect;
          break;
        case BorderPadding::BORDER_CONST:
          temp.pos = 0;
          break;
          default:
          temp.pos = 0;
          break;
      }
      temp.win = win-1;
    }    
  }else{
    temp.win = win;
    temp.pos = i%vect;
  }
  return temp;
}


//*********************************************************************************************************************
// VECTOR FLOAT 
//*********************************************************************************************************************
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processVECT2F(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
    assert( width <= MAX_WIDTH); assert( height <= MAX_HEIGHT);
    assert(VECT==2);
  
  IN lineBuff[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  INT win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  INT win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete

  IN in_pixel, out_pixel, temp_lb;

  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }
      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width && row < height + GROUP_DELAY_Y)
      {
       for(i = 0; i < SWIN_Y; i++){
       #pragma HLS unroll
         for(j = 0; j < SWIN_X-1; j++){
           win_tmp[i][j] = win_tmp[i][j+1];
         }
       }
     

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win_tmp[i][SWIN_X-1] = lineBuff[i][col];
         } else {
           temp_lb = lineBuff[i][col];
           win_tmp[i][SWIN_X-1] = temp_lb;
           lineBuff[i-1][col] = temp_lb;
         }
       }
       
       if (SWIN_Y > 1) {
         lineBuff[SWIN_Y-2][col] = in_pixel;
       }
       win_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel;
     

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win[i][j] = win_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win[i][j] = win[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 = getWinCoords(j,KERNEL_SIZE, SWIN_X, VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 = getWinCoords(j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1[i][j] = i2f(win[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1));
           win2[i][j] = i2f(win[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1));
         }
       }
     }
      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
      {
        INT outp1 = filter(win1);
        INT outp2 = filter(win2);
        out_pixel(0,WIDTH-1) = f2i(outp1);
        out_pixel(WIDTH,2*WIDTH-1) = f2i(outp2); 
        out_s << out_pixel; 
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processVECT4F(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{

  // TODO fix this
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert(VECT==4);
  
  IN lineBuff[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  INT win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  INT win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  INT win3[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win3 dim=0 complete
  INT win4[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win4 dim=0 complete

  IN in_pixel, out_pixel, temp_lb;

  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

     //**********************************************************
     // UPDATE THE WINDOW
     // The line buffer behaves normally
     //**********************************************************
     if (col < width && row < height + GROUP_DELAY_Y){
       for(i = 0; i < SWIN_Y; i++){
       #pragma HLS unroll
         for(j = 0; j < SWIN_X-1; j++){
           win_tmp[i][j] = win_tmp[i][j+1];
         }
       }
     

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win_tmp[i][SWIN_X-1] = lineBuff[i][col];
         } else {
           temp_lb = lineBuff[i][col];
           win_tmp[i][SWIN_X-1] = temp_lb;
           lineBuff[i-1][col] = temp_lb;
         }
       }
       if (SWIN_Y > 1) {
         lineBuff[SWIN_Y-2][col] = in_pixel;
       }
       win_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel;
     

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win[i][j] = win_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win[i][j] = win[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 = getWinCoords(j,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 = getWinCoords(j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 = getWinCoords(j+2,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 = getWinCoords(j+3,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1[i][j] = i2f(win[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1));
           win2[i][j] = i2f(win[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1));
           win3[i][j] = i2f(win[i][j3.win]((j3.pos)*WIDTH,(j3.pos+1)*WIDTH-1));
           win4[i][j] = i2f(win[i][j4.win]((j4.pos)*WIDTH,(j4.pos+1)*WIDTH-1));
         }
       }
     }
     //**********************************************************
     // DO CALCULATIONS
     //**********************************************************
     if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
     {
       INT outp1 = filter(win1);
       INT outp2 = filter(win2);
       INT outp3 = filter(win3);
       INT outp4 = filter(win4);
       out_pixel(0,WIDTH-1)         = f2i(outp1);
       out_pixel(WIDTH,2*WIDTH-1)   = f2i(outp2); 
       out_pixel(2*WIDTH,3*WIDTH-1) = f2i(outp3); 
       out_pixel(3*WIDTH,4*WIDTH-1) = f2i(outp4); 
       out_s << out_pixel; 
     }
    }
  }
}

//*********************************************************************************************************************
// VECTOR INT 
//*********************************************************************************************************************
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processVECT2(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  //#ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert(VECT==2);
  //#endif
  
  IN lineBuff[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  INT win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  INT win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete

  IN in_pixel, out_pixel, temp_lb;

  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width & row < height + GROUP_DELAY_Y)
     {
       for(i = 0; i < SWIN_Y; i++){
       #pragma HLS unroll
         for(j = 0; j < SWIN_X-1; j++){
           win_tmp[i][j] = win_tmp[i][j+1];
         }
       }
     

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win_tmp[i][SWIN_X-1] = lineBuff[i][col];
         } else {
           temp_lb = lineBuff[i][col];
           win_tmp[i][SWIN_X-1] = temp_lb;
           lineBuff[i-1][col] = temp_lb;
         }
       }
       if (SWIN_Y > 1) {
         lineBuff[SWIN_Y-2][col] = in_pixel;
       }
       win_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel;
     

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win[i][j] = win_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win[i][j] = win[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 = getWinCoords(j,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 = getWinCoords(j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1[i][j] = win[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1);
           win2[i][j] = win[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1);
         }
       }
     }
      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
      {
        INT outp[VECT];
        outp[0] = filter(win1);
        outp[1] = filter(win2);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*WIDTH,(i+1)*WIDTH-1) = outp[i];
        out_s << out_pixel; 
      }
    }
  }
}
  
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processVECT3(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert(VECT==3);
  #endif
  
  IN lineBuff[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  INT win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  INT win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  INT win3[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win3 dim=0 complete

  IN in_pixel, out_pixel, temp_lb;

  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < height+GROUP_DELAY_Y; row++){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < width+GROUP_DELAY_X; col++){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width && row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width && row < height + GROUP_DELAY_Y)
     {
       for(i = 0; i < SWIN_Y; i++){
       #pragma HLS unroll
         for(j = 0; j < SWIN_X-1; j++){
           win_tmp[i][j] = win_tmp[i][j+1];
         }
       }
     

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win_tmp[i][SWIN_X-1] = lineBuff[i][col];
         } else {
           temp_lb = lineBuff[i][col];
           win_tmp[i][SWIN_X-1] = temp_lb;
           lineBuff[i-1][col] = temp_lb;
         }
       }
       if (SWIN_Y > 1) {
         lineBuff[SWIN_Y-2][col] = in_pixel;
       }
       win_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel;
     

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win[i][j] = win_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win[i][j] = win[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 = getWinCoords(j,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 = getWinCoords(j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 = getWinCoords(j+2,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1[i][j] = win[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1);
           win2[i][j] = win[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1);
           win3[i][j] = win[i][j3.win]((j3.pos)*WIDTH,(j3.pos+1)*WIDTH-1);
         }
       }
     }
      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
      {
        INT outp[VECT];
        outp[0] = filter(win1);
        outp[1] = filter(win2);
        outp[2] = filter(win3);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*WIDTH,(i+1)*WIDTH-1) = outp[i];
        out_s << out_pixel; 
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processMISOVECT4(
    hls::stream<IN> &in1_s, 
    hls::stream<IN> &in2_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert(VECT==4);
  
  IN lineBuff1[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff1 dim=1 complete
  IN win1[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  IN win1_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win1_tmp dim=0 complete

  IN lineBuff2[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff2 dim=1 complete
  IN win2[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  IN win2_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win2_tmp dim=0 complete
  
  INT win1_1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1_1 dim=0 complete
  INT win1_2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1_2 dim=0 complete
  INT win1_3[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1_3 dim=0 complete
  INT win1_4[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1_4 dim=0 complete
  
  INT win2_1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2_1 dim=0 complete
  INT win2_2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2_2 dim=0 complete
  INT win2_3[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2_3 dim=0 complete
  INT win2_4[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2_4 dim=0 complete

  IN temp_lb;
  IN in_pixel1, temp_lb1;
  IN in_pixel2, temp_lb2;
  OUT out_pixel;

  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in1_s >> in_pixel1;
        in2_s >> in_pixel2;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width & row < height + GROUP_DELAY_Y)
     {
       for(i = 0; i < SWIN_Y; i++){
       #pragma HLS unroll
         for(j = 0; j < SWIN_X-1; j++){
           win1_tmp[i][j] = win1_tmp[i][j+1];
           win2_tmp[i][j] = win2_tmp[i][j+1];
         }
       }
     

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win1_tmp[i][SWIN_X-1] = lineBuff1[i][col];
           win2_tmp[i][SWIN_X-1] = lineBuff2[i][col];
         } else {
           temp_lb1 = lineBuff1[i][col];
           temp_lb2 = lineBuff2[i][col];
           win1_tmp[i][SWIN_X-1] = temp_lb1;
           win2_tmp[i][SWIN_X-1] = temp_lb2;
           lineBuff1[i-1][col] = temp_lb1;
           lineBuff2[i-1][col] = temp_lb2;
         }
       }
       if (SWIN_Y > 1) {
         lineBuff1[SWIN_Y-2][col] = in_pixel1;
         lineBuff2[SWIN_Y-2][col] = in_pixel2;
       }
       win1_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel1;
       win2_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel2;
     

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win1[i][j] = win1_tmp[i][jx];
           win2[i][j] = win2_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win1[i][j] = win1[iy][j];
            win2[i][j] = win2[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j  ,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j2 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+2,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+3,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1_1[i][j] = win1[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1);
           win1_2[i][j] = win1[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1);
           win1_3[i][j] = win1[i][j3.win]((j3.pos)*WIDTH,(j3.pos+1)*WIDTH-1);
           win1_4[i][j] = win1[i][j4.win]((j4.pos)*WIDTH,(j4.pos+1)*WIDTH-1);
           
           win2_1[i][j] = win2[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1);
           win2_2[i][j] = win2[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1);
           win2_3[i][j] = win2[i][j3.win]((j3.pos)*WIDTH,(j3.pos+1)*WIDTH-1);
           win2_4[i][j] = win2[i][j4.win]((j4.pos)*WIDTH,(j4.pos+1)*WIDTH-1);
         }
       }
     }
      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
      {
        INT outp[VECT];
        outp[0] = filter(win1_1,win2_1);
        outp[1] = filter(win1_2,win2_2);
        outp[2] = filter(win1_3,win2_3);
        outp[3] = filter(win1_4,win2_4);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*WIDTH,(i+1)*WIDTH-1) = outp[i];
        out_s << out_pixel; 
      }
    }
  }
}


  
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processVECT4(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert(VECT==4);
  
  IN lineBuff[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  INT win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  INT win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  INT win3[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win3 dim=0 complete
  INT win4[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win4 dim=0 complete

  IN in_pixel, temp_lb;
  OUT out_pixel;
  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width & row < height + GROUP_DELAY_Y)
     {
       for(i = 0; i < SWIN_Y; i++){
       #pragma HLS unroll
         for(j = 0; j < SWIN_X-1; j++){
           win_tmp[i][j] = win_tmp[i][j+1];
         }
       }
     

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win_tmp[i][SWIN_X-1] = lineBuff[i][col];
         } else {
           temp_lb = lineBuff[i][col];
           win_tmp[i][SWIN_X-1] = temp_lb;
           lineBuff[i-1][col] = temp_lb;
         }
       }
       if (SWIN_Y > 1) {
         lineBuff[SWIN_Y-2][col] = in_pixel;
       }
       win_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel;
     

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win[i][j] = win_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win[i][j] = win[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j  ,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j2 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+2,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+3,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1[i][j] = win[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1);
           win2[i][j] = win[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1);
           win3[i][j] = win[i][j3.win]((j3.pos)*WIDTH,(j3.pos+1)*WIDTH-1);
           win4[i][j] = win[i][j4.win]((j4.pos)*WIDTH,(j4.pos+1)*WIDTH-1);
         }
       }
     }
      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
      {
        OUT outp[VECT];
        outp[0] = filter(win1);
        outp[1] = filter(win2);
        outp[2] = filter(win3);
        outp[3] = filter(win4);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*O_WIDTH,(i+1)*O_WIDTH-1) = outp[i];
        out_s << out_pixel; 
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processVECT8(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert(VECT==8);
 
  IN lineBuff[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  INT win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  INT win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  INT win3[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win3 dim=0 complete
  INT win4[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win4 dim=0 complete
  INT win5[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win5 dim=0 complete
  INT win6[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win6 dim=0 complete
  INT win7[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win7 dim=0 complete
  INT win8[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win8 dim=0 complete
 
  IN in_pixel, temp_lb;
  OUT out_pixel;
  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width+GROUP_DELAY_X & row < height + GROUP_DELAY_Y)
      {
        for(i = 0; i < SWIN_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < SWIN_X-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      } 

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
     if (col < width & row < height + GROUP_DELAY_Y){
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win_tmp[i][SWIN_X-1] = lineBuff[i][col];
         } else {
           temp_lb = lineBuff[i][col];
           win_tmp[i][SWIN_X-1] = temp_lb;
           lineBuff[i-1][col] = temp_lb;
         }
       }
       if (SWIN_Y > 1) {
         lineBuff[SWIN_Y-2][col] = in_pixel;
       }
       win_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel;
      } 

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
      if (col < width+GROUP_DELAY_X & row < height + GROUP_DELAY_Y)
      {
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win[i][j] = win_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win[i][j] = win[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+2,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+3,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j5 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+4,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j6 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+5,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j7 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+6,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j8 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+7,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1[i][j] = win[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1);
           win2[i][j] = win[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1);
           win3[i][j] = win[i][j3.win]((j3.pos)*WIDTH,(j3.pos+1)*WIDTH-1);
           win4[i][j] = win[i][j4.win]((j4.pos)*WIDTH,(j4.pos+1)*WIDTH-1);
           win5[i][j] = win[i][j5.win]((j5.pos)*WIDTH,(j5.pos+1)*WIDTH-1);
           win6[i][j] = win[i][j6.win]((j6.pos)*WIDTH,(j6.pos+1)*WIDTH-1);
           win7[i][j] = win[i][j7.win]((j7.pos)*WIDTH,(j7.pos+1)*WIDTH-1);
           win8[i][j] = win[i][j8.win]((j8.pos)*WIDTH,(j8.pos+1)*WIDTH-1);
         }
       }
     }
      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
      {
        OUT outp[VECT];
        outp[0] = filter(win1);
        outp[1] = filter(win2);
        outp[2] = filter(win3);
        outp[3] = filter(win4);
        outp[4] = filter(win5);
        outp[5] = filter(win6);
        outp[6] = filter(win7);
        outp[7] = filter(win8);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*O_WIDTH,(i+1)*O_WIDTH-1) = outp[i];
        out_s << out_pixel; 
      }
    }
  }
}
  
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processVECT16(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert(VECT==16);
 
  IN lineBuff[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  INT win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  INT win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  INT win3[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win3 dim=0 complete
  INT win4[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win4 dim=0 complete
  INT win5[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win5 dim=0 complete
  INT win6[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win6 dim=0 complete
  INT win7[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win7 dim=0 complete
  INT win8[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win8 dim=0 complete
  INT win9[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win9 dim=0 complete
  INT win10[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win10 dim=0 complete
  INT win11[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win11 dim=0 complete
  INT win12[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win12 dim=0 complete
  INT win13[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win13 dim=0 complete
  INT win14[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win14 dim=0 complete
  INT win15[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win15 dim=0 complete
  INT win16[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win16 dim=0 complete
  
  IN in_pixel, out_pixel, temp_lb;

  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width+GROUP_DELAY_X & row < height + GROUP_DELAY_Y)
      {
        for(i = 0; i < SWIN_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < SWIN_X-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      } 

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
     if (col < width & row < height + GROUP_DELAY_Y){
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win_tmp[i][SWIN_X-1] = lineBuff[i][col];
         } else {
           temp_lb = lineBuff[i][col];
           win_tmp[i][SWIN_X-1] = temp_lb;
           lineBuff[i-1][col] = temp_lb;
         }
       }
       if (SWIN_Y > 1) {
         lineBuff[SWIN_Y-2][col] = in_pixel;
       }
       win_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel;
      } 

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
      if (col < width+GROUP_DELAY_X & row < height + GROUP_DELAY_Y)
      {
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win[i][j] = win_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win[i][j] = win[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+2,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+3,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j5 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+4,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j6 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+5,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j7 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+6,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j8 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+7,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j9 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+8,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j10 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+9,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j11 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+10,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j12 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+11,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j13 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+12,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j14 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+13,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j15 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+14,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j16 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+15,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1[i][j] = win[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1);
           win2[i][j] = win[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1);
           win3[i][j] = win[i][j3.win]((j3.pos)*WIDTH,(j3.pos+1)*WIDTH-1);
           win4[i][j] = win[i][j4.win]((j4.pos)*WIDTH,(j4.pos+1)*WIDTH-1);
           win5[i][j] = win[i][j5.win]((j5.pos)*WIDTH,(j5.pos+1)*WIDTH-1);
           win6[i][j] = win[i][j6.win]((j6.pos)*WIDTH,(j6.pos+1)*WIDTH-1);
           win7[i][j] = win[i][j7.win]((j7.pos)*WIDTH,(j7.pos+1)*WIDTH-1);
           win8[i][j] = win[i][j8.win]((j8.pos)*WIDTH,(j8.pos+1)*WIDTH-1);
           win9[i][j] = win[i][j9.win]((j9.pos)*WIDTH,(j9.pos+1)*WIDTH-1);
           win10[i][j] = win[i][j10.win]((j10.pos)*WIDTH,(j10.pos+1)*WIDTH-1);
           win11[i][j] = win[i][j11.win]((j11.pos)*WIDTH,(j11.pos+1)*WIDTH-1);
           win12[i][j] = win[i][j12.win]((j12.pos)*WIDTH,(j12.pos+1)*WIDTH-1);
           win13[i][j] = win[i][j13.win]((j13.pos)*WIDTH,(j13.pos+1)*WIDTH-1);
           win14[i][j] = win[i][j14.win]((j14.pos)*WIDTH,(j14.pos+1)*WIDTH-1);
           win15[i][j] = win[i][j15.win]((j15.pos)*WIDTH,(j15.pos+1)*WIDTH-1);
           win16[i][j] = win[i][j16.win]((j16.pos)*WIDTH,(j16.pos+1)*WIDTH-1);
         }
       }
     }
      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
      {
        INT outp[VECT];
        outp[0] = filter(win1);
        outp[1] = filter(win2);
        outp[2] = filter(win3);
        outp[3] = filter(win4);
        outp[4] = filter(win5);
        outp[5] = filter(win6);
        outp[6] = filter(win7);
        outp[7] = filter(win8);
        outp[8] = filter(win9);
        outp[9] = filter(win10);
        outp[10] = filter(win11);
        outp[11] = filter(win12);
        outp[12] = filter(win13);
        outp[13] = filter(win14);
        outp[14] = filter(win15);
        outp[15] = filter(win16);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*WIDTH,(i+1)*WIDTH-1) = outp[i];
        out_s << out_pixel; 
      }
    }
  }
}


template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processVECT32(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert(VECT==32);
 
  IN lineBuff[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  INT win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  INT win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  INT win3[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win3 dim=0 complete
  INT win4[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win4 dim=0 complete
  INT win5[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win5 dim=0 complete
  INT win6[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win6 dim=0 complete
  INT win7[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win7 dim=0 complete
  INT win8[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win8 dim=0 complete
  INT win9[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win9 dim=0 complete
  INT win10[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win10 dim=0 complete
  INT win11[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win11 dim=0 complete
  INT win12[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win12 dim=0 complete
  INT win13[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win13 dim=0 complete
  INT win14[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win14 dim=0 complete
  INT win15[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win15 dim=0 complete
  INT win16[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win16 dim=0 complete
  INT win17[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win17 dim=0 complete
  INT win18[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win18 dim=0 complete
  INT win19[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win19 dim=0 complete
  INT win20[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win20 dim=0 complete
  INT win21[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win21 dim=0 complete
  INT win22[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win22 dim=0 complete
  INT win23[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win23 dim=0 complete
  INT win24[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win24 dim=0 complete
  INT win25[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win25 dim=0 complete
  INT win26[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win26 dim=0 complete
  INT win27[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win27 dim=0 complete
  INT win28[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win28 dim=0 complete
  INT win29[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win29 dim=0 complete
  INT win30[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win30 dim=0 complete
  INT win31[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win31 dim=0 complete
  INT win32[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win32 dim=0 complete
  
  IN in_pixel, out_pixel, temp_lb;

  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width+GROUP_DELAY_X & row < height + GROUP_DELAY_Y)
      {
        for(i = 0; i < SWIN_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < SWIN_X-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      } 

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
     if (col < width & row < height + GROUP_DELAY_Y){
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win_tmp[i][SWIN_X-1] = lineBuff[i][col];
         } else {
           temp_lb = lineBuff[i][col];
           win_tmp[i][SWIN_X-1] = temp_lb;
           lineBuff[i-1][col] = temp_lb;
         }
       }
       if (SWIN_Y > 1) {
         lineBuff[SWIN_Y-2][col] = in_pixel;
       }
       win_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel;
      } 

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
      if (col < width+GROUP_DELAY_X & row < height + GROUP_DELAY_Y)
      {
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win[i][j] = win_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win[i][j] = win[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+2,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+3,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j5 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+4,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j6 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+5,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j7 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+6,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j8 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+7,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j9 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+8,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j10 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+9,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j11 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+10,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j12 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+11,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j13 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+12,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j14 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+13,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j15 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+14,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j16 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+15,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j17 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+16,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j18 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+17,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j19 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+18,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j20 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+19,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j21 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+20,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j22 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+21,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j23 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+22,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j24 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+23,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j25 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+24,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j26 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+25,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j27 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+26,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j28 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+27,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j29 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+28,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j30 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+29,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j31 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+10,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j32 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+11,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1[i][j] = win[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1);
           win2[i][j] = win[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1);
           win3[i][j] = win[i][j3.win]((j3.pos)*WIDTH,(j3.pos+1)*WIDTH-1);
           win4[i][j] = win[i][j4.win]((j4.pos)*WIDTH,(j4.pos+1)*WIDTH-1);
           win5[i][j] = win[i][j5.win]((j5.pos)*WIDTH,(j5.pos+1)*WIDTH-1);
           win6[i][j] = win[i][j6.win]((j6.pos)*WIDTH,(j6.pos+1)*WIDTH-1);
           win7[i][j] = win[i][j7.win]((j7.pos)*WIDTH,(j7.pos+1)*WIDTH-1);
           win8[i][j] = win[i][j8.win]((j8.pos)*WIDTH,(j8.pos+1)*WIDTH-1);
           win9[i][j] = win[i][j9.win]((j9.pos)*WIDTH,(j9.pos+1)*WIDTH-1);
           win10[i][j] = win[i][j10.win]((j10.pos)*WIDTH,(j10.pos+1)*WIDTH-1);
           win11[i][j] = win[i][j11.win]((j11.pos)*WIDTH,(j11.pos+1)*WIDTH-1);
           win12[i][j] = win[i][j12.win]((j12.pos)*WIDTH,(j12.pos+1)*WIDTH-1);
           win13[i][j] = win[i][j13.win]((j13.pos)*WIDTH,(j13.pos+1)*WIDTH-1);
           win14[i][j] = win[i][j14.win]((j14.pos)*WIDTH,(j14.pos+1)*WIDTH-1);
           win15[i][j] = win[i][j15.win]((j15.pos)*WIDTH,(j15.pos+1)*WIDTH-1);
           win16[i][j] = win[i][j16.win]((j16.pos)*WIDTH,(j16.pos+1)*WIDTH-1);
           win17[i][j] = win[i][j17.win]((j17.pos)*WIDTH,(j17.pos+1)*WIDTH-1);
           win18[i][j] = win[i][j18.win]((j18.pos)*WIDTH,(j18.pos+1)*WIDTH-1);
           win19[i][j] = win[i][j19.win]((j19.pos)*WIDTH,(j19.pos+1)*WIDTH-1);
           win20[i][j] = win[i][j20.win]((j20.pos)*WIDTH,(j20.pos+1)*WIDTH-1);
           win21[i][j] = win[i][j21.win]((j21.pos)*WIDTH,(j21.pos+1)*WIDTH-1);
           win22[i][j] = win[i][j22.win]((j22.pos)*WIDTH,(j22.pos+1)*WIDTH-1);
           win23[i][j] = win[i][j23.win]((j23.pos)*WIDTH,(j23.pos+1)*WIDTH-1);
           win24[i][j] = win[i][j24.win]((j24.pos)*WIDTH,(j24.pos+1)*WIDTH-1);
           win25[i][j] = win[i][j25.win]((j25.pos)*WIDTH,(j25.pos+1)*WIDTH-1);
           win26[i][j] = win[i][j26.win]((j26.pos)*WIDTH,(j26.pos+1)*WIDTH-1);
           win27[i][j] = win[i][j27.win]((j27.pos)*WIDTH,(j27.pos+1)*WIDTH-1);
           win28[i][j] = win[i][j28.win]((j28.pos)*WIDTH,(j28.pos+1)*WIDTH-1);
           win29[i][j] = win[i][j29.win]((j29.pos)*WIDTH,(j29.pos+1)*WIDTH-1);
           win30[i][j] = win[i][j30.win]((j30.pos)*WIDTH,(j30.pos+1)*WIDTH-1);
           win31[i][j] = win[i][j31.win]((j31.pos)*WIDTH,(j31.pos+1)*WIDTH-1);
           win32[i][j] = win[i][j32.win]((j32.pos)*WIDTH,(j32.pos+1)*WIDTH-1);
         }
       }
     }
      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
      {
        INT outp[VECT];
        outp[0] = filter(win1);
        outp[1] = filter(win2);
        outp[2] = filter(win3);
        outp[3] = filter(win4);
        outp[4] = filter(win5);
        outp[5] = filter(win6);
        outp[6] = filter(win7);
        outp[7] = filter(win8);
        outp[8] = filter(win9);
        outp[9] = filter(win10);
        outp[10] = filter(win11);
        outp[11] = filter(win12);
        outp[12] = filter(win13);
        outp[13] = filter(win14);
        outp[14] = filter(win15);
        outp[15] = filter(win16);
        outp[16] = filter(win17);
        outp[17] = filter(win18);
        outp[18] = filter(win19);
        outp[19] = filter(win20);
        outp[20] = filter(win21);
        outp[21] = filter(win22);
        outp[22] = filter(win23);
        outp[23] = filter(win24);
        outp[24] = filter(win25);
        outp[25] = filter(win26);
        outp[26] = filter(win27);
        outp[27] = filter(win28);
        outp[28] = filter(win29);
        outp[29] = filter(win30);
        outp[30] = filter(win31);
        outp[31] = filter(win32);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*WIDTH,(i+1)*WIDTH-1) = outp[i];
        out_s << out_pixel; 
      }
    }
  }
}


template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT, class Filter>
void processVECT64(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert(VECT==64);
 
  IN lineBuff[SWIN_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[SWIN_Y][SWIN_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  INT win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  INT win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  INT win3[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win3 dim=0 complete
  INT win4[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win4 dim=0 complete
  INT win5[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win5 dim=0 complete
  INT win6[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win6 dim=0 complete
  INT win7[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win7 dim=0 complete
  INT win8[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win8 dim=0 complete
  INT win9[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win9 dim=0 complete
  INT win10[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win10 dim=0 complete
  INT win11[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win11 dim=0 complete
  INT win12[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win12 dim=0 complete
  INT win13[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win13 dim=0 complete
  INT win14[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win14 dim=0 complete
  INT win15[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win15 dim=0 complete
  INT win16[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win16 dim=0 complete
  INT win17[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win17 dim=0 complete
  INT win18[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win18 dim=0 complete
  INT win19[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win19 dim=0 complete
  INT win20[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win20 dim=0 complete
  INT win21[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win21 dim=0 complete
  INT win22[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win22 dim=0 complete
  INT win23[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win23 dim=0 complete
  INT win24[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win24 dim=0 complete
  INT win25[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win25 dim=0 complete
  INT win26[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win26 dim=0 complete
  INT win27[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win27 dim=0 complete
  INT win28[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win28 dim=0 complete
  INT win29[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win29 dim=0 complete
  INT win30[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win30 dim=0 complete
  INT win31[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win31 dim=0 complete
  INT win32[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win32 dim=0 complete
  INT win33[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win33 dim=0 complete
  INT win34[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win34 dim=0 complete
  INT win35[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win35 dim=0 complete
  INT win36[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win36 dim=0 complete
  INT win37[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win37 dim=0 complete
  INT win38[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win38 dim=0 complete
  INT win39[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win39 dim=0 complete
  INT win40[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win40 dim=0 complete
  INT win41[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win41 dim=0 complete
  INT win42[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win42 dim=0 complete
  INT win43[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win43 dim=0 complete
  INT win44[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win44 dim=0 complete
  INT win45[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win45 dim=0 complete
  INT win46[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win46 dim=0 complete
  INT win47[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win47 dim=0 complete
  INT win48[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win48 dim=0 complete
  INT win49[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win49 dim=0 complete
  INT win50[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win50 dim=0 complete
  INT win51[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win51 dim=0 complete
  INT win52[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win52 dim=0 complete
  INT win53[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win53 dim=0 complete
  INT win54[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win54 dim=0 complete
  INT win55[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win55 dim=0 complete
  INT win56[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win56 dim=0 complete
  INT win57[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win57 dim=0 complete
  INT win58[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win58 dim=0 complete
  INT win59[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win59 dim=0 complete
  INT win60[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win60 dim=0 complete
  INT win61[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win61 dim=0 complete
  INT win62[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win62 dim=0 complete
  INT win63[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win63 dim=0 complete
  INT win64[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win64 dim=0 complete
  
  IN in_pixel, out_pixel, temp_lb;

  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width+GROUP_DELAY_X & row < height + GROUP_DELAY_Y)
      {
        for(i = 0; i < SWIN_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < SWIN_X-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      } 

     //**********************************************************
     // UPDATE THE LINE BUFFER
     // The line buffer behaves normally
     //**********************************************************
     if (col < width & row < height + GROUP_DELAY_Y){
       LINE_BUFF_1:
       for(i = 0; i < SWIN_Y-1; i++){
       #pragma HLS unroll
         if (i == 0) {
           win_tmp[i][SWIN_X-1] = lineBuff[i][col];
         } else {
           temp_lb = lineBuff[i][col];
           win_tmp[i][SWIN_X-1] = temp_lb;
           lineBuff[i-1][col] = temp_lb;
         }
       }
       if (SWIN_Y > 1) {
         lineBuff[SWIN_Y-2][col] = in_pixel;
       }
       win_tmp[SWIN_Y-1][SWIN_X-1] = in_pixel;
      } 

     //**********************************************************
     // UPDATE THE LP WINDOW
     // Window update includes mirroring border treatment
     //**********************************************************
      if (col < width+GROUP_DELAY_X & row < height + GROUP_DELAY_Y)
      {
       // X-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int jx = getNewCoords(j,SWIN_X,GROUP_DELAY_X,col,width,borderPadding);
           win[i][j] = win_tmp[i][jx];
         }
       }
       // Y-DIRECTION
       for(i = 0; i < SWIN_Y; i++){
         for(j = 0; j < SWIN_X; j++){
           int iy = getNewCoords(i,SWIN_Y,GROUP_DELAY_Y,row,height,borderPadding);
            win[i][j] = win[iy][j];
         }
       }
     
     //**********************************************************
     // ASSIGN WINDOWS 
     //**********************************************************
     // left
       for(int j = 0; j < KERNEL_SIZE; j++){
         winPos j1 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j,  KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+1,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+2,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+3,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j5 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+4,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j6 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+5,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j7 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+6,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j8 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+7,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j9 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+8,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j10 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+9,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j11 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+10,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j12 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+11,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j13 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+12,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j14 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+13,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j15 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+14,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j16 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+15,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j17 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+16,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j18 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+17,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j19 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+18,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j20 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+19,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j21 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+20,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j22 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+21,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j23 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+22,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j24 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+23,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j25 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+24,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j26 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+25,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j27 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+26,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j28 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+27,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j29 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+28,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j30 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+29,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j31 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+30,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j32 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+31,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j33 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+32,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j34 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+33,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j35 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+34,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j36 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+35,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j37 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+36,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j38 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+37,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j39 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+38,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j40 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+39,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j41 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+40,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j42 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+41,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j43 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+42,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j44 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+43,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j45 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+44,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j46 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+45,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j47 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+46,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j48 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+47,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j49 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+48,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j50 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+49,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j51 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+50,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j52 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+51,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j53 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+52,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j54 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+53,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j55 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+54,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j56 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+55,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j57 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+56,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j58 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+57,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j59 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+58,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j60 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+59,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j61 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+60,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j62 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+61,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j63 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+62,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j64 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+63,KERNEL_SIZE, SWIN_X,VECT,GROUP_DELAY,col,width,borderPadding);
         for(int i = 0; i < KERNEL_SIZE; i++){
           win1[i][j] = win[i][j1.win]((j1.pos)*WIDTH,(j1.pos+1)*WIDTH-1);
           win2[i][j] = win[i][j2.win]((j2.pos)*WIDTH,(j2.pos+1)*WIDTH-1);
           win3[i][j] = win[i][j3.win]((j3.pos)*WIDTH,(j3.pos+1)*WIDTH-1);
           win4[i][j] = win[i][j4.win]((j4.pos)*WIDTH,(j4.pos+1)*WIDTH-1);
           win5[i][j] = win[i][j5.win]((j5.pos)*WIDTH,(j5.pos+1)*WIDTH-1);
           win6[i][j] = win[i][j6.win]((j6.pos)*WIDTH,(j6.pos+1)*WIDTH-1);
           win7[i][j] = win[i][j7.win]((j7.pos)*WIDTH,(j7.pos+1)*WIDTH-1);
           win8[i][j] = win[i][j8.win]((j8.pos)*WIDTH,(j8.pos+1)*WIDTH-1);
           win9[i][j] = win[i][j9.win]((j9.pos)*WIDTH,(j9.pos+1)*WIDTH-1);
           win10[i][j] = win[i][j10.win]((j10.pos)*WIDTH,(j10.pos+1)*WIDTH-1);
           win11[i][j] = win[i][j11.win]((j11.pos)*WIDTH,(j11.pos+1)*WIDTH-1);
           win12[i][j] = win[i][j12.win]((j12.pos)*WIDTH,(j12.pos+1)*WIDTH-1);
           win13[i][j] = win[i][j13.win]((j13.pos)*WIDTH,(j13.pos+1)*WIDTH-1);
           win14[i][j] = win[i][j14.win]((j14.pos)*WIDTH,(j14.pos+1)*WIDTH-1);
           win15[i][j] = win[i][j15.win]((j15.pos)*WIDTH,(j15.pos+1)*WIDTH-1);
           win16[i][j] = win[i][j16.win]((j16.pos)*WIDTH,(j16.pos+1)*WIDTH-1);
           win17[i][j] = win[i][j17.win]((j17.pos)*WIDTH,(j17.pos+1)*WIDTH-1);
           win18[i][j] = win[i][j18.win]((j18.pos)*WIDTH,(j18.pos+1)*WIDTH-1);
           win19[i][j] = win[i][j19.win]((j19.pos)*WIDTH,(j19.pos+1)*WIDTH-1);
           win20[i][j] = win[i][j20.win]((j20.pos)*WIDTH,(j20.pos+1)*WIDTH-1);
           win21[i][j] = win[i][j21.win]((j21.pos)*WIDTH,(j21.pos+1)*WIDTH-1);
           win22[i][j] = win[i][j22.win]((j22.pos)*WIDTH,(j22.pos+1)*WIDTH-1);
           win23[i][j] = win[i][j23.win]((j23.pos)*WIDTH,(j23.pos+1)*WIDTH-1);
           win24[i][j] = win[i][j24.win]((j24.pos)*WIDTH,(j24.pos+1)*WIDTH-1);
           win25[i][j] = win[i][j25.win]((j25.pos)*WIDTH,(j25.pos+1)*WIDTH-1);
           win26[i][j] = win[i][j26.win]((j26.pos)*WIDTH,(j26.pos+1)*WIDTH-1);
           win27[i][j] = win[i][j27.win]((j27.pos)*WIDTH,(j27.pos+1)*WIDTH-1);
           win28[i][j] = win[i][j28.win]((j28.pos)*WIDTH,(j28.pos+1)*WIDTH-1);
           win29[i][j] = win[i][j29.win]((j29.pos)*WIDTH,(j29.pos+1)*WIDTH-1);
           win30[i][j] = win[i][j30.win]((j30.pos)*WIDTH,(j30.pos+1)*WIDTH-1);
           win31[i][j] = win[i][j31.win]((j31.pos)*WIDTH,(j31.pos+1)*WIDTH-1);
           win32[i][j] = win[i][j32.win]((j32.pos)*WIDTH,(j32.pos+1)*WIDTH-1);
           win33[i][j] = win[i][j33.win]((j33.pos)*WIDTH,(j33.pos+1)*WIDTH-1);
           win34[i][j] = win[i][j34.win]((j34.pos)*WIDTH,(j34.pos+1)*WIDTH-1);
           win35[i][j] = win[i][j35.win]((j35.pos)*WIDTH,(j35.pos+1)*WIDTH-1);
           win36[i][j] = win[i][j36.win]((j36.pos)*WIDTH,(j36.pos+1)*WIDTH-1);
           win37[i][j] = win[i][j37.win]((j37.pos)*WIDTH,(j37.pos+1)*WIDTH-1);
           win38[i][j] = win[i][j38.win]((j38.pos)*WIDTH,(j38.pos+1)*WIDTH-1);
           win39[i][j] = win[i][j39.win]((j39.pos)*WIDTH,(j39.pos+1)*WIDTH-1);
           win40[i][j] = win[i][j40.win]((j40.pos)*WIDTH,(j40.pos+1)*WIDTH-1);
           win41[i][j] = win[i][j41.win]((j41.pos)*WIDTH,(j41.pos+1)*WIDTH-1);
           win42[i][j] = win[i][j42.win]((j42.pos)*WIDTH,(j42.pos+1)*WIDTH-1);
           win43[i][j] = win[i][j43.win]((j43.pos)*WIDTH,(j43.pos+1)*WIDTH-1);
           win44[i][j] = win[i][j44.win]((j44.pos)*WIDTH,(j44.pos+1)*WIDTH-1);
           win45[i][j] = win[i][j45.win]((j45.pos)*WIDTH,(j45.pos+1)*WIDTH-1);
           win46[i][j] = win[i][j46.win]((j46.pos)*WIDTH,(j46.pos+1)*WIDTH-1);
           win47[i][j] = win[i][j47.win]((j47.pos)*WIDTH,(j47.pos+1)*WIDTH-1);
           win48[i][j] = win[i][j48.win]((j48.pos)*WIDTH,(j48.pos+1)*WIDTH-1);
           win49[i][j] = win[i][j49.win]((j49.pos)*WIDTH,(j49.pos+1)*WIDTH-1);
           win50[i][j] = win[i][j50.win]((j50.pos)*WIDTH,(j50.pos+1)*WIDTH-1);
           win51[i][j] = win[i][j51.win]((j51.pos)*WIDTH,(j51.pos+1)*WIDTH-1);
           win52[i][j] = win[i][j52.win]((j52.pos)*WIDTH,(j52.pos+1)*WIDTH-1);
           win53[i][j] = win[i][j53.win]((j53.pos)*WIDTH,(j53.pos+1)*WIDTH-1);
           win54[i][j] = win[i][j54.win]((j54.pos)*WIDTH,(j54.pos+1)*WIDTH-1);
           win55[i][j] = win[i][j55.win]((j55.pos)*WIDTH,(j55.pos+1)*WIDTH-1);
           win56[i][j] = win[i][j56.win]((j56.pos)*WIDTH,(j56.pos+1)*WIDTH-1);
           win57[i][j] = win[i][j57.win]((j57.pos)*WIDTH,(j57.pos+1)*WIDTH-1);
           win58[i][j] = win[i][j58.win]((j58.pos)*WIDTH,(j58.pos+1)*WIDTH-1);
           win59[i][j] = win[i][j59.win]((j59.pos)*WIDTH,(j59.pos+1)*WIDTH-1);
           win60[i][j] = win[i][j60.win]((j60.pos)*WIDTH,(j60.pos+1)*WIDTH-1);
           win61[i][j] = win[i][j61.win]((j61.pos)*WIDTH,(j61.pos+1)*WIDTH-1);
           win62[i][j] = win[i][j62.win]((j62.pos)*WIDTH,(j62.pos+1)*WIDTH-1);
           win63[i][j] = win[i][j63.win]((j63.pos)*WIDTH,(j63.pos+1)*WIDTH-1);
           win64[i][j] = win[i][j64.win]((j64.pos)*WIDTH,(j64.pos+1)*WIDTH-1);
         }
       }
     }
      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GROUP_DELAY_Y && col >= GROUP_DELAY_X)
      {
        INT outp[VECT];
        outp[0] = filter(win1);
        outp[1] = filter(win2);
        outp[2] = filter(win3);
        outp[3] = filter(win4);
        outp[4] = filter(win5);
        outp[5] = filter(win6);
        outp[6] = filter(win7);
        outp[7] = filter(win8);
        outp[8] = filter(win9);
        outp[9] = filter(win10);
        outp[10] = filter(win11);
        outp[11] = filter(win12);
        outp[12] = filter(win13);
        outp[13] = filter(win14);
        outp[14] = filter(win15);
        outp[15] = filter(win16);
        outp[16] = filter(win17);
        outp[17] = filter(win18);
        outp[18] = filter(win19);
        outp[19] = filter(win20);
        outp[20] = filter(win21);
        outp[21] = filter(win22);
        outp[22] = filter(win23);
        outp[23] = filter(win24);
        outp[24] = filter(win25);
        outp[25] = filter(win26);
        outp[26] = filter(win27);
        outp[27] = filter(win28);
        outp[28] = filter(win29);
        outp[29] = filter(win30);
        outp[30] = filter(win31);
        outp[31] = filter(win32);
        outp[32] = filter(win33);
        outp[33] = filter(win34);
        outp[34] = filter(win35);
        outp[35] = filter(win36);
        outp[36] = filter(win37);
        outp[37] = filter(win38);
        outp[38] = filter(win39);
        outp[39] = filter(win40);
        outp[40] = filter(win41);
        outp[41] = filter(win42);
        outp[42] = filter(win43);
        outp[43] = filter(win44);
        outp[44] = filter(win45);
        outp[45] = filter(win46);
        outp[46] = filter(win47);
        outp[47] = filter(win48);
        outp[48] = filter(win49);
        outp[49] = filter(win50);
        outp[50] = filter(win51);
        outp[51] = filter(win52);
        outp[52] = filter(win53);
        outp[53] = filter(win54);
        outp[54] = filter(win55);
        outp[55] = filter(win56);
        outp[56] = filter(win57);
        outp[57] = filter(win58);
        outp[58] = filter(win59);
        outp[59] = filter(win60);
        outp[60] = filter(win61);
        outp[61] = filter(win62);
        outp[62] = filter(win63);
        outp[63] = filter(win64);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*WIDTH,(i+1)*WIDTH-1) = outp[i];
        out_s << out_pixel; 
      }
    }
  }
}


//*********************************************************************************************************************
// POINT VECTOR OPERATORS
// currently only supports factor 2
//*********************************************************************************************************************

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT_I, typename INT_O, typename IN, typename OUT, class Filter>
void processPixelsVECT(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GROUP_DELAY; ++x) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const IN val = in_s.read();
      INT_I temp_i[VECT];
      INT_O temp_o[VECT]; 
      OUT out_val;
      for(int i = 0; i < VECT; i++){
        #pragma HLS unroll
        temp_i[i] = val(i*INT_I_WIDTH,(i+1)*INT_I_WIDTH-1);
        temp_o[i] = filter(temp_i[i]);
        out_val(i*INT_O_WIDTH,(i+1)*INT_O_WIDTH-1) = temp_o[i];
      }
      out_s << out_val;
    }
}
// 2:1
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT_I, typename INT_O, typename IN, typename OUT, class Filter>
void processPixels2VECT(
    hls::stream<IN> &in1_s,
    hls::stream<IN> &in2_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GROUP_DELAY; ++x) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const IN val1 = in1_s.read();
      const IN val2 = in2_s.read();
      INT_I temp1_i[VECT];
      INT_I temp2_i[VECT];
      INT_O temp_o[VECT]; 
      OUT out_val;
      for(int i = 0; i < VECT; i++){
        #pragma HLS unroll
        temp1_i[i] = val1(i*INT_I_WIDTH,(i+1)*INT_I_WIDTH-1);
        temp2_i[i] = val2(i*INT_I_WIDTH,(i+1)*INT_I_WIDTH-1);
        temp_o[i] = filter(temp1_i[i], temp2_i[i]);
        out_val(i*INT_O_WIDTH,(i+1)*INT_O_WIDTH-1) = temp_o[i];
      }
      out_s << out_val;
    }
}
// 3:1
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT_I, typename INT_O, typename IN, typename OUT, class Filter>
void processPixels3VECT(
    hls::stream<IN> &in1_s,
    hls::stream<IN> &in2_s,
    hls::stream<IN> &in3_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GROUP_DELAY; ++x) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const IN val1 = in1_s.read();
      const IN val2 = in2_s.read();
      const IN val3 = in3_s.read();
      INT_I temp1_i[VECT];
      INT_I temp2_i[VECT];
      INT_I temp3_i[VECT];
      INT_O temp_o[VECT]; 
      OUT out_val;
      for(int i = 0; i < VECT; i++){
        #pragma HLS unroll
        temp1_i[i] = val1(i*INT_I_WIDTH,(i+1)*INT_I_WIDTH-1);
        temp2_i[i] = val2(i*INT_I_WIDTH,(i+1)*INT_I_WIDTH-1);
        temp3_i[i] = val3(i*INT_I_WIDTH,(i+1)*INT_I_WIDTH-1);
        temp_o[i] = filter(temp1_i[i], temp2_i[i], temp3_i[i]);
        out_val(i*INT_O_WIDTH,(i+1)*INT_O_WIDTH-1) = temp_o[i];
      }
      out_s << out_val;
    }
}



//*********************************************************************************************************************
//PYRAMID VECTOR OPERATORS 
// currently only supports factor 2
//*********************************************************************************************************************
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT>
void downsampleVECT(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height)
{
  assert( width <= MAX_WIDTH); assert( height <= MAX_HEIGHT);
  
  IN in_pixel, out_pixel; 

  int row, col, i;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
        if(col%2 == 0 && row%2==0){
          for(i = 0; i < VECT/2; i++)
            out_pixel(i*WIDTH,(i+1)*WIDTH-1) = in_pixel((i*2)*WIDTH,((i*2)+1)*WIDTH-1);
        }else{
          for(i = 0; i < VECT/2; i++)
            out_pixel((i+VECT/2)*WIDTH,(i+VECT/2+1)*WIDTH-1) = in_pixel((i*2)*WIDTH,((i*2)+1)*WIDTH-1);
          out_s << out_pixel;
        }
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, int VECT, typename INT, typename IN, typename OUT>
void upsampleVECT(
    hls::stream<IN> &in_s, 
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height)
{
  assert( width <= MAX_WIDTH); assert( height <= MAX_HEIGHT);
  
  IN in_pixel, out_pixel; 

  int row, col, i;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GROUP_DELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GROUP_DELAY_X; ++col){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        out_pixel = 0;
        if(col%2 == 0 && row%2==0){
          in_s >> in_pixel;  
          for(i = 0; i < VECT/2; i++)
             out_pixel((i*2)*WIDTH,((i*2)+1)*WIDTH-1) = in_pixel(i*WIDTH,(i+1)*WIDTH-1);
        }else{
          for(i = 0; i < VECT/2; i++)
             out_pixel((i*2)*WIDTH,((i*2)+1)*WIDTH-1) = in_pixel((i+VECT/2)*WIDTH,(i+VECT/2+1)*WIDTH-1);
        }
        out_s << in_pixel;
      }
    }
  }
}

//*********************************************************************************************************************
// LOCAL OPERATORS
//*********************************************************************************************************************
// normal processing, one input, one output stream
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT, class Filter>
void process(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert( (KERNEL_SIZE_X % 2) == 1 );
    assert( (KERNEL_SIZE_Y % 2) == 1 );
  #endif

  IN lineBuff[KERNEL_SIZE_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  OUT out_pixel;
  IN temp_lb, in_pixel;
  int i, j ,row, col;

  process_main_loop:
  for (int row = 0; row < MAX_HEIGHT + GDELAY_Y; row++) {
    for (int col = 0; col < MAX_WIDTH + GDELAY_X; col++) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      //**********************************************************
      if (col < width + GDELAY_X & row < height + GDELAY_Y){
        for(i = 0; i < KERNEL_SIZE_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE_X-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      }

      //**********************************************************
      // UPDATE THE LINE BUFFER
      //**********************************************************
      if (col < width & row < height+GDELAY_Y){
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE_Y-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE_X-1] = lineBuff[i][col];
          } else {
            temp_lb = lineBuff[i][col];
            win_tmp[i][KERNEL_SIZE_X-1] = temp_lb;
            lineBuff[i-1][col] = temp_lb;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        if (KERNEL_SIZE_Y > 1) {
          lineBuff[KERNEL_SIZE_Y-2][col] = in_pixel;
        }
        win_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X-1] = in_pixel;
      }

      //**********************************************************
      // HANDLE BORDERS 
      //**********************************************************
      // X-DIRECTION
      for(i = 0; i < KERNEL_SIZE_Y; i++){
        for(j = 0; j < KERNEL_SIZE_X; j++){
          int jx = getNewCoords(j,KERNEL_SIZE_X,GDELAY_X,col,width,borderPadding);
          win[i][j] = win_tmp[i][jx];
        }
      }
      // Y-DIRECTION
      for(i = 0; i < KERNEL_SIZE_Y; i++){
        for(j = 0; j < KERNEL_SIZE_X; j++){
          int ix = getNewCoords(i,KERNEL_SIZE_Y,GDELAY_Y,row,height,borderPadding);
          win[i][j] = win[ix][j];
        }
      }

      //**********************************************************
      // FILTER COMPUTATION AND OUTPUT ASSIGNMENT 
      //**********************************************************
      // Do the filtering
      if (row >= GDELAY_Y && col >= GDELAY_X){
        out_pixel = filter(win);
        out_s.write(out_pixel);
      }
    }
  }
}

// process one input stream, put result into two output streams
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT, class Filter>
void processSIMO(
                 hls::stream<IN> &in_s,
                 hls::stream<OUT> &out1_s,
                 hls::stream<OUT> &out2_s,
                 const int &width,
                 const int &height,
                 Filter &filter,
                 const enum BorderPadding::values borderPadding)
{
#ifdef ASSERTION_CHECK
  assert( width <= MAX_WIDTH ); assert(height <= MAX_HEIGHT);
  assert( (KERNEL_SIZE_X % 2) == 1 );
  assert( (KERNEL_SIZE_Y % 2) == 1 );
#endif
  
  IN lineBuff[KERNEL_SIZE_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete
  
  OUT out_pixel;
  IN temp_lb, in_pixel;
  int i, j ,row, col;
  
  ROW_LOOP:
  for (int row = 0; row < MAX_HEIGHT + GDELAY_Y; row++) {
    COL_LOOP:
    for (int col = 0; col < MAX_WIDTH + GDELAY_X; col++) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
    
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }
      
      //**********************************************************
      // UPDATE THE WINDOW
      //**********************************************************
      if (col < width + GDELAY_X & row < height + GDELAY_Y){
        for(i = 0; i < KERNEL_SIZE_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE_X-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      }
      
      //**********************************************************
      // UPDATE THE LINE BUFFER
      //**********************************************************
      if (col < width & row < height+GDELAY_Y){
      LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE_Y-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE_X-1] = lineBuff[i][col];
          } else {
            temp_lb = lineBuff[i][col];
            win_tmp[i][KERNEL_SIZE_X-1] = temp_lb;
            lineBuff[i-1][col] = temp_lb;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        if (KERNEL_SIZE_Y > 1) {
          lineBuff[KERNEL_SIZE_Y-2][col] = in_pixel;
        }
        win_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X-1] = in_pixel;
      }
      
      //**********************************************************
      // HANDLE BORDERS
      //**********************************************************
      // X-DIRECTION
      for(i = 0; i < KERNEL_SIZE_Y; i++){
        for(j = 0; j < KERNEL_SIZE_X; j++){
          int jx = getNewCoords(j,KERNEL_SIZE_X,GDELAY_X,col,width,borderPadding);
          win[i][j] = win_tmp[i][jx];
        }
      }
      // Y-DIRECTION
      for(i = 0; i < KERNEL_SIZE_Y; i++){
        for(j = 0; j < KERNEL_SIZE_X; j++){
          int ix = getNewCoords(i,KERNEL_SIZE_Y,GDELAY_Y,row,height,borderPadding);
          win[i][j] = win[ix][j];
        }
      }
      
      //**********************************************************
      // FILTER COMPUTATION AND OUTPUT ASSIGNMENT
      //**********************************************************
      // Do the filtering
      if (row >= GDELAY_Y && col >= GDELAY_X){
        out_pixel = filter(win);
        out1_s.write(out_pixel);
        out2_s.write(out_pixel);
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT, class Filter>
void processMISO(
    hls::stream<IN> &in1_s,
    hls::stream<IN> &in2_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert( (KERNEL_SIZE_X % 2) == 1 );
    assert( (KERNEL_SIZE_Y % 2) == 1 );
  #endif

  IN lineBuff1[KERNEL_SIZE_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff1 dim=1 complete
  IN win1[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  IN win1_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win1_tmp dim=0 complete

  IN lineBuff2[KERNEL_SIZE_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff2 dim=1 complete
  IN win2[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  IN win2_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win2_tmp dim=0 complete

  OUT out_pixel;
  IN temp_lb2, in_pixel2;
  IN temp_lb1, in_pixel1;

  int i, j ,row, col;

  process_main_loop:
  for (int row = 0; row < MAX_HEIGHT + GDELAY_Y; row++) {
    for (int col = 0; col < MAX_WIDTH + GDELAY_X; col++) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in1_s >> in_pixel1;
        in2_s >> in_pixel2;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      //**********************************************************
      if (col < width + GDELAY_X & row < height + GDELAY_Y){
        for(i = 0; i < KERNEL_SIZE_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE_X-1; j++){
            win1_tmp[i][j] = win1_tmp[i][j+1];
            win2_tmp[i][j] = win2_tmp[i][j+1];
          }
        }
      }

      //**********************************************************
      // UPDATE THE LINE BUFFER
      //**********************************************************
      if (col < width & row < height+GDELAY_Y){
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE_Y-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win1_tmp[i][KERNEL_SIZE_X-1] = lineBuff1[i][col];
            win2_tmp[i][KERNEL_SIZE_X-1] = lineBuff2[i][col];
          } else {
            temp_lb1 = lineBuff1[i][col];
            temp_lb2 = lineBuff2[i][col];
            win1_tmp[i][KERNEL_SIZE_X-1] = temp_lb1;
            win2_tmp[i][KERNEL_SIZE_X-1] = temp_lb2;
            lineBuff1[i-1][col] = temp_lb1;
            lineBuff2[i-1][col] = temp_lb2;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        if (KERNEL_SIZE_Y > 1) {
          lineBuff1[KERNEL_SIZE_Y-2][col] = in_pixel1;
          lineBuff2[KERNEL_SIZE_Y-2][col] = in_pixel2;
        }
        win1_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X-1] = in_pixel1;
        win2_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X-1] = in_pixel2;
      }

      //**********************************************************
      // HANDLE BORDERS
      //**********************************************************
      // X-DIRECTION
      for(i = 0; i < KERNEL_SIZE_Y; i++){
        for(j = 0; j < KERNEL_SIZE_X; j++){
          int jx = getNewCoords(j,KERNEL_SIZE_X,GDELAY_X,col,width,borderPadding);
          win1[i][j] = win1_tmp[i][jx];
          win2[i][j] = win2_tmp[i][jx];
        }
      }
      // Y-DIRECTION
      for(i = 0; i < KERNEL_SIZE_Y; i++){
        for(j = 0; j < KERNEL_SIZE_X; j++){
          int ix = getNewCoords(i,KERNEL_SIZE_Y,GDELAY_Y,row,height,borderPadding);
          win1[i][j] = win1[ix][j];
          win2[i][j] = win2[ix][j];
        }
      }

      //**********************************************************
      // FILTER COMPUTATION AND OUTPUT ASSIGNMENT
      //**********************************************************
      // Do the filtering
      if (row >= GDELAY_Y && col >= GDELAY_X){
        out_pixel = filter(win1, win2);
        out_s.write(out_pixel);
      }
    }
  }
}

//*********************************************************************************************************************
// PYRAMID OPERATORS
//*********************************************************************************************************************
// downsampling
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void downsample(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    const int &factor,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif

  IN lineBuff[KERNEL_SIZE-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  OUT out_pixel;
  IN temp_lb, in_pixel;
  int i, j ,row, col;

  int mod = GROUP_DELAY%2;

  process_main_loop:
  for (int row = 0; row < MAX_HEIGHT + GROUP_DELAY; row++) {
    for (int col = 0; col < MAX_WIDTH + GROUP_DELAY; col++) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      //**********************************************************
      if (col < width + GROUP_DELAY & row < height + GROUP_DELAY){
        for(i = 0; i < KERNEL_SIZE; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      }

      //**********************************************************
      // UPDATE THE LINE BUFFER
      //**********************************************************
      if (col < width & row < height+GROUP_DELAY){
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE-1] = lineBuff[i][col];
          } else {
            temp_lb = lineBuff[i][col];
            win_tmp[i][KERNEL_SIZE-1] = temp_lb;
            lineBuff[i-1][col] = temp_lb;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        if (KERNEL_SIZE > 1) {
          lineBuff[KERNEL_SIZE-2][col] = in_pixel;
        }
        win_tmp[KERNEL_SIZE-1][KERNEL_SIZE-1] = in_pixel;
      }

      //**********************************************************
      // HANDLE BORDERS 
      //**********************************************************
      // X-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int jx = getNewCoords(j,KERNEL_SIZE,GROUP_DELAY,col,width,borderPadding);
          win[i][j] = win_tmp[i][jx];
        }
      }
      // Y-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int ix = getNewCoords(i,KERNEL_SIZE,GROUP_DELAY,row,height,borderPadding);
          win[i][j] = win[ix][j];
        }
      }

      //**********************************************************
      // FILTER COMPUTATION AND OUTPUT ASSIGNMENT 
      //**********************************************************
      // Do the filtering
      if (row >= GROUP_DELAY && col >= GROUP_DELAY){
        out_pixel = filter(win);
        //only retain every factor-th pixel
        // oh, and start with the first, so that we can hide some delay on the way out :)
        if(row%factor == mod && col%factor == mod) 
          out_s.write(out_pixel);
      }
    }
  }
}

// upsampling
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void upsample(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    const int factor,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  #ifdef ASSERTION_CHECK
    assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif

  IN lineBuff[KERNEL_SIZE-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  IN win[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  IN win_tmp[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  OUT out_pixel;
  IN temp_lb, in_pixel;
  int i, j ,row, col;

  int mod = factor - 1;

  process_main_loop:
  for (int row = 0; row < MAX_HEIGHT + GROUP_DELAY; row++) {
    for (int col = 0; col < MAX_WIDTH + GROUP_DELAY; col++) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region

      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width && row < height){
        if(col%factor == mod && row%factor == mod)
          in_s >> in_pixel;
        else
          in_pixel = 0;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      //**********************************************************
      if (col < width + GROUP_DELAY & row < height + GROUP_DELAY){
        for(i = 0; i < KERNEL_SIZE; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }
      }

      //**********************************************************
      // UPDATE THE LINE BUFFER
      //**********************************************************
      if (col < width & row < height+GROUP_DELAY){
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE-1] = lineBuff[i][col];
          } else {
            temp_lb = lineBuff[i][col];
            win_tmp[i][KERNEL_SIZE-1] = temp_lb;
            lineBuff[i-1][col] = temp_lb;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        if (KERNEL_SIZE > 1) {
          lineBuff[KERNEL_SIZE-2][col] = in_pixel;
        }
        win_tmp[KERNEL_SIZE-1][KERNEL_SIZE-1] = in_pixel;
      }

      //**********************************************************
      // HANDLE BORDERS 
      //**********************************************************
      // X-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int jx = getNewCoords(j,KERNEL_SIZE,GROUP_DELAY,col,width,borderPadding);
          win[i][j] = win_tmp[i][jx];
        }
      }
      // Y-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int ix = getNewCoords(i,KERNEL_SIZE,GROUP_DELAY,row,height,borderPadding);
          win[i][j] = win[ix][j];
        }
      }

      //**********************************************************
      // FILTER COMPUTATION AND OUTPUT ASSIGNMENT 
      //**********************************************************
      // Do the filtering
      if (row >= GROUP_DELAY && col >= GROUP_DELAY){
        out_pixel = filter(win, col, row); 
        out_s.write(out_pixel);
      }
    }
  }
}


//*********************************************************************************************************************
// POINT OPERATORS
//*********************************************************************************************************************
// 1:1
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT, class Filter>
void processPixels(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X; ++x) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const IN val = in_s.read();
      out_s << filter(val);
    }
}
// 2:1
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT, class Filter>
void processPixels2(
    hls::stream<IN> &in1_s,
    hls::stream<IN> &in2_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X; ++x) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const IN val1 = in1_s.read();
      const IN val2 = in2_s.read();
      out_s.write(filter(val1, val2));
    }
}
// 3:1
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT, class Filter>
void processPixels3(
    hls::stream<IN> &in1_s,
    hls::stream<IN> &in2_s,
    hls::stream<IN> &in3_s,
    hls::stream<OUT> &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X; ++x) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const IN val1 = in1_s.read();
      const IN val2 = in2_s.read();
      const IN val3 = in3_s.read();
      out_s.write(filter(val1, val2, val3));
    }
}

//1:2
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT1, typename OUT2>
void splitStream(
    hls::stream<IN> &in_s,
    hls::stream<OUT1> &out1_s,
    hls::stream<OUT2> &out2_s,
    const int &width,
    const int &height)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X; ++x) {
PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;

      const IN val = in_s.read();

      out1_s << val;
      out2_s << val;
    }
}

// 1:3
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT>
void splitStream3(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out1_s,
    hls::stream<OUT> &out2_s,
    hls::stream<OUT> &out3_s,
    const int &width,
    const int &height)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X; ++x) {
PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;

      const IN val = in_s.read();

      out1_s << val;
      out2_s << val;
      out3_s << val;
    }
}

//1:4
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT>
void splitStream4(
    hls::stream<IN> &in_s,
    hls::stream<OUT> &out1_s,
    hls::stream<OUT> &out2_s,
    hls::stream<OUT> &out3_s,
    hls::stream<OUT> &out4_s,
    const int &width,
    const int &height)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X; ++x) {
		PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;

      const IN val = in_s.read();

      out1_s << val;
      out2_s << val;
      out3_s << val;
      out4_s << val;
    }
}

//*********************************************************************************************************************
// LEGACY (QUADRATIC KERNEL SIZE)
//*********************************************************************************************************************
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void process(hls::stream<IN> &in_s, hls::stream<OUT> &out_s, const int &width, const int &height, Filter &filter, const enum BorderPadding::values borderPadding) {
  process<II_TARGET,MAX_WIDTH,MAX_HEIGHT,KERNEL_SIZE,KERNEL_SIZE>(in_s, out_s, width, height, filter);
}
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processSIMO(hls::stream<IN> &in_s, hls::stream<OUT> &out1_s, hls::stream<OUT> &out2_s, const int &width, const int &height, Filter &filter, const enum BorderPadding::values borderPadding) {
  processSIMO<II_TARGET,MAX_WIDTH,MAX_HEIGHT,KERNEL_SIZE,KERNEL_SIZE>(in_s, out1_s, out2_s, width, height, filter);
}
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processMISO(hls::stream<IN> &in1_s, hls::stream<IN> &in2_s, hls::stream<OUT> &out_s, const int &width, const int &height, Filter &filter, const enum BorderPadding::values borderPadding) {
  processMISO<II_TARGET,MAX_WIDTH,MAX_HEIGHT,KERNEL_SIZE,KERNEL_SIZE>(in1_s, in2_s, out_s, width, height, filter);
}
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processPixels(hls::stream<IN> &in_s, hls::stream<OUT> &out_s, const int &width, const int &height, Filter &filter) {
  processPixels<II_TARGET,MAX_WIDTH,MAX_HEIGHT,KERNEL_SIZE,KERNEL_SIZE>(in_s, out_s, width, height, filter);
}
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processPixels2(hls::stream<IN> &in1_s, hls::stream<IN> &in2_s, hls::stream<OUT> &out_s, const int &width, const int &height, Filter &filter) {
  processPixels2<II_TARGET,MAX_WIDTH,MAX_HEIGHT,KERNEL_SIZE,KERNEL_SIZE>(in1_s, in2_s, out_s, width, height, filter);
}
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processPixels3(hls::stream<IN> &in1_s, hls::stream<IN> &in2_s, hls::stream<IN> &in3_s, hls::stream<OUT> &out_s, const int &width, const int &height, Filter &filter) {
  processPixels3<II_TARGET,MAX_WIDTH,MAX_HEIGHT,KERNEL_SIZE,KERNEL_SIZE>(in1_s, in2_s, in3_s, out_s, width, height, filter);
}
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT1, typename OUT2>
void splitStream(hls::stream<IN> &in_s, hls::stream<OUT1> &out1_s, hls::stream<OUT2> &out2_s, const int &width, const int &height) {
  splitStream<II_TARGET,MAX_WIDTH,MAX_HEIGHT,KERNEL_SIZE,KERNEL_SIZE>(in_s, out1_s, out2_s, width, height);
}
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT>
void splitStream3(hls::stream<IN> &in_s, hls::stream<OUT> &out1_s, hls::stream<OUT> &out2_s, hls::stream<OUT> &out3_s, const int &width, const int &height) {
  splitStream3<II_TARGET,MAX_WIDTH,MAX_HEIGHT,KERNEL_SIZE,KERNEL_SIZE>(in_s, out1_s, out2_s, out3_s, width, height);
}
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT>
void splitStream4(hls::stream<IN> &in_s, hls::stream<OUT> &out1_s, hls::stream<OUT> &out2_s, hls::stream<OUT> &out3_s, hls::stream<OUT> &out4_s, const int &width, const int &height) {
  splitStream4<II_TARGET,MAX_WIDTH,MAX_HEIGHT,KERNEL_SIZE,KERNEL_SIZE>(in_s, out1_s, out2_s, out3_s, out4_s, width, height);
}

////////////////////////////////////////////////////////////////////////////////
// Oliver's VECT alternatives:
//   - processVECT
//   - processVECTF
//   - processSIMOVECT
//   - processSIMOVECTF
//   - processMISOVECT
//   - processMISOVECTF
//   - processPixelsVECT
//   - processPixelsVECTF
//   - processPixels2VECT
//   - processPixels2VECTF
//   - processPixels3VECT
//   - processPixels3VECTF
//   - splitStreamVECT
//   - splitStream3VECT
//   - splitStream4VECT
////////////////////////////////////////////////////////////////////////////////
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processVECT(
    hls::stream<ap_uint<BW_IN> > &in_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );

  ap_uint<BW_IN> lineBuff[KERNEL_SIZE_Y-1][MAX_WIDTH/VECT];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  ap_uint<BW_IN> win[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  ap_uint<BW_IN> win_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  INT win_vect[KERNEL_SIZE_Y][KERNEL_SIZE_X+VECT-1];
  #pragma HLS ARRAY_PARTITION variable=win_vect dim=0 complete

  ap_uint<BW_IN> in_pixel, temp_lb;
  ap_uint<BW_OUT> out_pixel;
  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GDELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GDELAY_X; col+=VECT){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width & row < height + GDELAY_Y)
      {
        for(i = 0; i < KERNEL_SIZE_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE_X_V-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }

        //**********************************************************
        // UPDATE THE LINE BUFFER
        // The line buffer behaves normally
        //**********************************************************
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE_Y-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE_X_V-1] = lineBuff[i][col/VECT];
          } else {
            temp_lb = lineBuff[i][col/VECT];
            win_tmp[i][KERNEL_SIZE_X_V-1] = temp_lb;
            lineBuff[i-1][col/VECT] = temp_lb;
          }
        }
        if (KERNEL_SIZE_Y > 1) {
          lineBuff[KERNEL_SIZE_Y-2][col/VECT] = in_pixel;
        }
        win_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X_V-1] = in_pixel;

        //**********************************************************
        // UPDATE THE LP WINDOW
        // Window update includes mirroring border treatment
        //**********************************************************
        // X-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int jx = getNewCoords(j,KERNEL_SIZE_X_V,GDELAY_X,col,width,borderPadding);
            win[i][j] = win_tmp[i][jx];
          }
        }
        // Y-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int iy = getNewCoords(i,KERNEL_SIZE_Y,GDELAY_Y,row,height,borderPadding);
             win[i][j] = win[iy][j];
          }
        }

        //**********************************************************
        // ASSIGN WINDOWS
        //**********************************************************
        // fill original window
        for(int j = 0; j < KERNEL_SIZE_X; j++){
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+j,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win_vect[i][j] = win[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1);
          }
        }

        // fill window extension caused by vectorization
        for (int v = 0; v < VECT-1; v++) {
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+KERNEL_SIZE_X+v,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win_vect[i][KERNEL_SIZE_X+v] = win[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1);
          }
        }
      }

      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GDELAY_Y && col >= GDELAY_X)
      {
        for (int v = 0; v < VECT; v++) {
          INT win_small[KERNEL_SIZE_Y][KERNEL_SIZE_X];
          for (int i = 0; i < KERNEL_SIZE_Y; i++) {
            for (int j = 0; j < KERNEL_SIZE_X; j++) {
              win_small[i][j] = win_vect[i][j+v];
            }
          }
          out_pixel(v*O_WIDTH_V,(v+1)*O_WIDTH_V-1) = filter(win_small);
        }
        out_s << out_pixel;
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processVECTF(
    hls::stream<ap_uint<BW_IN> > &in_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );

  ap_uint<BW_IN> lineBuff[KERNEL_SIZE_Y-1][MAX_WIDTH/VECT];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  ap_uint<BW_IN> win[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  ap_uint<BW_IN> win_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  INT win_vect[KERNEL_SIZE_Y][KERNEL_SIZE_X+VECT-1];
  #pragma HLS ARRAY_PARTITION variable=win_vect dim=0 complete

  ap_uint<BW_IN> in_pixel, temp_lb;
  ap_uint<BW_OUT> out_pixel;
  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GDELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GDELAY_X; col+=VECT){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width & row < height + GDELAY_Y)
      {
        for(i = 0; i < KERNEL_SIZE_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE_X_V-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }

        //**********************************************************
        // UPDATE THE LINE BUFFER
        // The line buffer behaves normally
        //**********************************************************
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE_Y-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE_X_V-1] = lineBuff[i][col/VECT];
          } else {
            temp_lb = lineBuff[i][col/VECT];
            win_tmp[i][KERNEL_SIZE_X_V-1] = temp_lb;
            lineBuff[i-1][col/VECT] = temp_lb;
          }
        }
        if (KERNEL_SIZE_Y > 1) {
          lineBuff[KERNEL_SIZE_Y-2][col/VECT] = in_pixel;
        }
        win_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X_V-1] = in_pixel;

        //**********************************************************
        // UPDATE THE LP WINDOW
        // Window update includes mirroring border treatment
        //**********************************************************
        // X-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int jx = getNewCoords(j,KERNEL_SIZE_X_V,GDELAY_X,col,width,borderPadding);
            win[i][j] = win_tmp[i][jx];
          }
        }
        // Y-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int iy = getNewCoords(i,KERNEL_SIZE_Y,GDELAY_Y,row,height,borderPadding);
             win[i][j] = win[iy][j];
          }
        }

        //**********************************************************
        // ASSIGN WINDOWS
        //**********************************************************
        // fill original window
        for(int j = 0; j < KERNEL_SIZE_X; j++){
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+j,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win_vect[i][j] = i2f(win[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1));
          }
        }

        // fill window extension caused by vectorization
        for (int v = 0; v < VECT-1; v++) {
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+KERNEL_SIZE_X+v,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win_vect[i][KERNEL_SIZE_X+v] = i2f(win[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1));
          }
        }
      }

      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GDELAY_Y && col >= GDELAY_X)
      {
        for (int v = 0; v < VECT; v++) {
          INT win_small[KERNEL_SIZE_Y][KERNEL_SIZE_X];
          for (int i = 0; i < KERNEL_SIZE_Y; i++) {
            for (int j = 0; j < KERNEL_SIZE_X; j++) {
              win_small[i][j] = win_vect[i][j+v];
            }
          }
          out_pixel(v*O_WIDTH_V,(v+1)*O_WIDTH_V-1) = f2i(filter(win_small));
        }
        out_s << out_pixel;
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processSIMOVECT(
    hls::stream<ap_uint<BW_IN> > &in_s,
    hls::stream<ap_uint<BW_OUT> > &out1_s,
    hls::stream<ap_uint<BW_OUT> > &out2_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );

  ap_uint<BW_IN> lineBuff[KERNEL_SIZE_Y-1][MAX_WIDTH/VECT];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  ap_uint<BW_IN> win[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  ap_uint<BW_IN> win_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  INT win_vect[KERNEL_SIZE_Y][KERNEL_SIZE_X+VECT-1];
  #pragma HLS ARRAY_PARTITION variable=win_vect dim=0 complete

  ap_uint<BW_IN> in_pixel, temp_lb;
  ap_uint<BW_OUT> out_pixel;
  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GDELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GDELAY_X; col+=VECT){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width & row < height + GDELAY_Y)
      {
        for(i = 0; i < KERNEL_SIZE_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE_X_V-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }

        //**********************************************************
        // UPDATE THE LINE BUFFER
        // The line buffer behaves normally
        //**********************************************************
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE_Y-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE_X_V-1] = lineBuff[i][col/VECT];
          } else {
            temp_lb = lineBuff[i][col/VECT];
            win_tmp[i][KERNEL_SIZE_X_V-1] = temp_lb;
            lineBuff[i-1][col/VECT] = temp_lb;
          }
        }
        if (KERNEL_SIZE_Y > 1) {
          lineBuff[KERNEL_SIZE_Y-2][col/VECT] = in_pixel;
        }
        win_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X_V-1] = in_pixel;

        //**********************************************************
        // UPDATE THE LP WINDOW
        // Window update includes mirroring border treatment
        //**********************************************************
        // X-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int jx = getNewCoords(j,KERNEL_SIZE_X_V,GDELAY_X,col,width,borderPadding);
            win[i][j] = win_tmp[i][jx];
          }
        }
        // Y-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int iy = getNewCoords(i,KERNEL_SIZE_Y,GDELAY_Y,row,height,borderPadding);
             win[i][j] = win[iy][j];
          }
        }

        //**********************************************************
        // ASSIGN WINDOWS
        //**********************************************************
        // fill original window
        for(int j = 0; j < KERNEL_SIZE_X; j++){
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+j,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win_vect[i][j] = win[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1);
          }
        }

        // fill window extension caused by vectorization
        for (int v = 0; v < VECT-1; v++) {
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+KERNEL_SIZE_X+v,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win_vect[i][KERNEL_SIZE_X+v] = win[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1);
          }
        }
      }

      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GDELAY_Y && col >= GDELAY_X)
      {
        for (int v = 0; v < VECT; v++) {
          INT win_small[KERNEL_SIZE_Y][KERNEL_SIZE_X];
          for (int i = 0; i < KERNEL_SIZE_Y; i++) {
            for (int j = 0; j < KERNEL_SIZE_X; j++) {
              win_small[i][j] = win_vect[i][j+v];
            }
          }
          out_pixel(v*O_WIDTH_V,(v+1)*O_WIDTH_V-1) = filter(win_small);
        }
        out1_s << out_pixel;
        out2_s << out_pixel;
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processSIMOVECTF(
    hls::stream<ap_uint<BW_IN> > &in_s,
    hls::stream<ap_uint<BW_OUT> > &out1_s,
    hls::stream<ap_uint<BW_OUT> > &out2_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );

  ap_uint<BW_IN> lineBuff[KERNEL_SIZE_Y-1][MAX_WIDTH/VECT];
  #pragma HLS ARRAY_PARTITION variable=lineBuff dim=1 complete
  ap_uint<BW_IN> win[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win dim=0 complete
  ap_uint<BW_IN> win_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win_tmp dim=0 complete

  INT win_vect[KERNEL_SIZE_Y][KERNEL_SIZE_X+VECT-1];
  #pragma HLS ARRAY_PARTITION variable=win_vect dim=0 complete

  ap_uint<BW_IN> in_pixel, temp_lb;
  ap_uint<BW_OUT> out_pixel;
  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GDELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GDELAY_X; col+=VECT){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in_s >> in_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width & row < height + GDELAY_Y)
      {
        for(i = 0; i < KERNEL_SIZE_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE_X_V-1; j++){
            win_tmp[i][j] = win_tmp[i][j+1];
          }
        }

        //**********************************************************
        // UPDATE THE LINE BUFFER
        // The line buffer behaves normally
        //**********************************************************
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE_Y-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win_tmp[i][KERNEL_SIZE_X_V-1] = lineBuff[i][col/VECT];
          } else {
            temp_lb = lineBuff[i][col/VECT];
            win_tmp[i][KERNEL_SIZE_X_V-1] = temp_lb;
            lineBuff[i-1][col/VECT] = temp_lb;
          }
        }
        if (KERNEL_SIZE_Y > 1) {
          lineBuff[KERNEL_SIZE_Y-2][col/VECT] = in_pixel;
        }
        win_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X_V-1] = in_pixel;

        //**********************************************************
        // UPDATE THE LP WINDOW
        // Window update includes mirroring border treatment
        //**********************************************************
        // X-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int jx = getNewCoords(j,KERNEL_SIZE_X_V,GDELAY_X,col,width,borderPadding);
            win[i][j] = win_tmp[i][jx];
          }
        }
        // Y-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int iy = getNewCoords(i,KERNEL_SIZE_Y,GDELAY_Y,row,height,borderPadding);
             win[i][j] = win[iy][j];
          }
        }

        //**********************************************************
        // ASSIGN WINDOWS
        //**********************************************************
        // fill original window
        for(int j = 0; j < KERNEL_SIZE_X; j++){
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+j,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win_vect[i][j] = i2f(win[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1));
          }
        }

        // fill window extension caused by vectorization
        for (int v = 0; v < VECT-1; v++) {
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+KERNEL_SIZE_X+v,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win_vect[i][KERNEL_SIZE_X+v] = i2f(win[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1));
          }
        }
      }

      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GDELAY_Y && col >= GDELAY_X)
      {
        for (int v = 0; v < VECT; v++) {
          INT win_small[KERNEL_SIZE_Y][KERNEL_SIZE_X];
          for (int i = 0; i < KERNEL_SIZE_Y; i++) {
            for (int j = 0; j < KERNEL_SIZE_X; j++) {
              win_small[i][j] = win_vect[i][j+v];
            }
          }
          out_pixel(v*O_WIDTH_V,(v+1)*O_WIDTH_V-1) = f2i(filter(win_small));
        }
        out1_s << out_pixel;
        out2_s << out_pixel;
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processMISOVECT(
    hls::stream<ap_uint<BW_IN> > &in1_s,
    hls::stream<ap_uint<BW_IN> > &in2_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );

  ap_uint<BW_IN> lineBuff1[KERNEL_SIZE_Y-1][MAX_WIDTH/VECT];
  #pragma HLS ARRAY_PARTITION variable=lineBuff1 dim=1 complete
  ap_uint<BW_IN> lineBuff2[KERNEL_SIZE_Y-1][MAX_WIDTH/VECT];
  #pragma HLS ARRAY_PARTITION variable=lineBuff2 dim=1 complete
  ap_uint<BW_IN> win1[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  ap_uint<BW_IN> win2[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  ap_uint<BW_IN> win1_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win1_tmp dim=0 complete
  ap_uint<BW_IN> win2_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win2_tmp dim=0 complete

  INT win1_vect[KERNEL_SIZE_Y][KERNEL_SIZE_X+VECT-1];
  #pragma HLS ARRAY_PARTITION variable=win1_vect dim=0 complete
  INT win2_vect[KERNEL_SIZE_Y][KERNEL_SIZE_X+VECT-1];
  #pragma HLS ARRAY_PARTITION variable=win2_vect dim=0 complete

  ap_uint<BW_IN> in1_pixel, temp1_lb;
  ap_uint<BW_IN> in2_pixel, temp2_lb;
  ap_uint<BW_OUT> out_pixel;
  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GDELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GDELAY_X; col+=VECT){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in1_s >> in1_pixel;
        in2_s >> in2_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width & row < height + GDELAY_Y)
      {
        for(i = 0; i < KERNEL_SIZE_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE_X_V-1; j++){
            win1_tmp[i][j] = win1_tmp[i][j+1];
            win2_tmp[i][j] = win2_tmp[i][j+1];
          }
        }

        //**********************************************************
        // UPDATE THE LINE BUFFER
        // The line buffer behaves normally
        //**********************************************************
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE_Y-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win1_tmp[i][KERNEL_SIZE_X_V-1] = lineBuff1[i][col/VECT];
            win2_tmp[i][KERNEL_SIZE_X_V-1] = lineBuff2[i][col/VECT];
          } else {
            temp1_lb = lineBuff1[i][col/VECT];
            temp2_lb = lineBuff2[i][col/VECT];
            win1_tmp[i][KERNEL_SIZE_X_V-1] = temp1_lb;
            win2_tmp[i][KERNEL_SIZE_X_V-1] = temp2_lb;
            lineBuff1[i-1][col/VECT] = temp1_lb;
            lineBuff2[i-1][col/VECT] = temp2_lb;
          }
        }
        if (KERNEL_SIZE_Y > 1) {
          lineBuff1[KERNEL_SIZE_Y-2][col/VECT] = in1_pixel;
          lineBuff2[KERNEL_SIZE_Y-2][col/VECT] = in2_pixel;
        }
        win1_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X_V-1] = in1_pixel;
        win2_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X_V-1] = in2_pixel;

        //**********************************************************
        // UPDATE THE LP WINDOW
        // Window update includes mirroring border treatment
        //**********************************************************
        // X-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int jx = getNewCoords(j,KERNEL_SIZE_X_V,GDELAY_X,col,width,borderPadding);
            win1[i][j] = win1_tmp[i][jx];
            win2[i][j] = win2_tmp[i][jx];
          }
        }
        // Y-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int iy = getNewCoords(i,KERNEL_SIZE_Y,GDELAY_Y,row,height,borderPadding);
             win1[i][j] = win1[iy][j];
             win2[i][j] = win2[iy][j];
          }
        }

        //**********************************************************
        // ASSIGN WINDOWS
        //**********************************************************
        // fill original window
        for(int j = 0; j < KERNEL_SIZE_X; j++){
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+j,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win1_vect[i][j] = win1[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1);
            win2_vect[i][j] = win2[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1);
          }
        }

        // fill window extension caused by vectorization
        for (int v = 0; v < VECT-1; v++) {
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+KERNEL_SIZE_X+v,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win1_vect[i][KERNEL_SIZE_X+v] = win1[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1);
            win2_vect[i][KERNEL_SIZE_X+v] = win2[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1);
          }
        }
      }

      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GDELAY_Y && col >= GDELAY_X)
      {
        for (int v = 0; v < VECT; v++) {
          INT win1_small[KERNEL_SIZE_Y][KERNEL_SIZE_X];
          INT win2_small[KERNEL_SIZE_Y][KERNEL_SIZE_X];
          for (int i = 0; i < KERNEL_SIZE_Y; i++) {
            for (int j = 0; j < KERNEL_SIZE_X; j++) {
              win1_small[i][j] = win1_vect[i][j+v];
              win2_small[i][j] = win2_vect[i][j+v];
            }
          }
          out_pixel(v*O_WIDTH_V,(v+1)*O_WIDTH_V-1) = filter(win1_small, win2_small);
        }
        out_s << out_pixel;
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processMISOVECTF(
    hls::stream<ap_uint<BW_IN> > &in1_s,
    hls::stream<ap_uint<BW_IN> > &in2_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter,
    const enum BorderPadding::values borderPadding)
{
  // TODO fix this
  assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );

  ap_uint<BW_IN> lineBuff1[KERNEL_SIZE_Y-1][MAX_WIDTH/VECT];
  #pragma HLS ARRAY_PARTITION variable=lineBuff1 dim=1 complete
  ap_uint<BW_IN> lineBuff2[KERNEL_SIZE_Y-1][MAX_WIDTH/VECT];
  #pragma HLS ARRAY_PARTITION variable=lineBuff2 dim=1 complete
  ap_uint<BW_IN> win1[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  ap_uint<BW_IN> win2[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  ap_uint<BW_IN> win1_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win1_tmp dim=0 complete
  ap_uint<BW_IN> win2_tmp[KERNEL_SIZE_Y][KERNEL_SIZE_X_V];
  #pragma HLS ARRAY_PARTITION variable=win2_tmp dim=0 complete

  INT win1_vect[KERNEL_SIZE_Y][KERNEL_SIZE_X+VECT-1];
  #pragma HLS ARRAY_PARTITION variable=win1_vect dim=0 complete
  INT win2_vect[KERNEL_SIZE_Y][KERNEL_SIZE_X+VECT-1];
  #pragma HLS ARRAY_PARTITION variable=win2_vect dim=0 complete

  ap_uint<BW_IN> in1_pixel, temp1_lb;
  ap_uint<BW_IN> in2_pixel, temp2_lb;
  ap_uint<BW_OUT> out_pixel;
  int row, col, i, j;

  IMG_ROWS:
  for(row = 0; row < MAX_HEIGHT+GDELAY_Y; ++row){
    //std::cout << "ROW: " << row << std::endl;
    IMG_COLS:
    for(col = 0; col < MAX_WIDTH+GDELAY_X; col+=VECT){
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      #pragma HLS INLINE region
      //**********************************************************
      // GET NEW INPUT
      //**********************************************************
      if(col < width & row < height){
        in1_s >> in1_pixel;
        in2_s >> in2_pixel;
      }

      //**********************************************************
      // UPDATE THE WINDOW
      // The line buffer behaves normally
      //**********************************************************
      if (col < width & row < height + GDELAY_Y)
      {
        for(i = 0; i < KERNEL_SIZE_Y; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE_X_V-1; j++){
            win1_tmp[i][j] = win1_tmp[i][j+1];
            win2_tmp[i][j] = win2_tmp[i][j+1];
          }
        }

        //**********************************************************
        // UPDATE THE LINE BUFFER
        // The line buffer behaves normally
        //**********************************************************
        LINE_BUFF_1:
        for(i = 0; i < KERNEL_SIZE_Y-1; i++){
        #pragma HLS unroll
          if (i == 0) {
            win1_tmp[i][KERNEL_SIZE_X_V-1] = lineBuff1[i][col/VECT];
            win2_tmp[i][KERNEL_SIZE_X_V-1] = lineBuff2[i][col/VECT];
          } else {
            temp1_lb = lineBuff1[i][col/VECT];
            temp2_lb = lineBuff2[i][col/VECT];
            win1_tmp[i][KERNEL_SIZE_X_V-1] = temp1_lb;
            win2_tmp[i][KERNEL_SIZE_X_V-1] = temp2_lb;
            lineBuff1[i-1][col/VECT] = temp1_lb;
            lineBuff2[i-1][col/VECT] = temp2_lb;
          }
        }
        if (KERNEL_SIZE_Y > 1) {
          lineBuff1[KERNEL_SIZE_Y-2][col/VECT] = in1_pixel;
          lineBuff2[KERNEL_SIZE_Y-2][col/VECT] = in2_pixel;
        }
        win1_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X_V-1] = in1_pixel;
        win2_tmp[KERNEL_SIZE_Y-1][KERNEL_SIZE_X_V-1] = in2_pixel;

        //**********************************************************
        // UPDATE THE LP WINDOW
        // Window update includes mirroring border treatment
        //**********************************************************
        // X-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int jx = getNewCoords(j,KERNEL_SIZE_X_V,GDELAY_X,col,width,borderPadding);
            win1[i][j] = win1_tmp[i][jx];
            win2[i][j] = win2_tmp[i][jx];
          }
        }
        // Y-DIRECTION
        for(i = 0; i < KERNEL_SIZE_Y; i++){
          for(j = 0; j < KERNEL_SIZE_X_V; j++){
            int iy = getNewCoords(i,KERNEL_SIZE_Y,GDELAY_Y,row,height,borderPadding);
             win1[i][j] = win1[iy][j];
             win2[i][j] = win2[iy][j];
          }
        }

        //**********************************************************
        // ASSIGN WINDOWS
        //**********************************************************
        // fill original window
        for(int j = 0; j < KERNEL_SIZE_X; j++){
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+j,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win1_vect[i][j] = i2f(win1[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1));
            win2_vect[i][j] = i2f(win2[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1));
          }
        }

        // fill window extension caused by vectorization
        for (int v = 0; v < VECT-1; v++) {
          winPos jv = getWinCoords((GDELAY_X_V*VECT)-GDELAY_X+KERNEL_SIZE_X+v,KERNEL_SIZE_X, KERNEL_SIZE_X_V,VECT,GDELAY_X,col,width,borderPadding);
          for(int i = 0; i < KERNEL_SIZE_Y; i++){
            win1_vect[i][KERNEL_SIZE_X+v] = i2f(win1[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1));
            win2_vect[i][KERNEL_SIZE_X+v] = i2f(win2[i][jv.win]((jv.pos)*I_WIDTH_V,(jv.pos+1)*I_WIDTH_V-1));
          }
        }
      }

      //**********************************************************
      // DO CALCULATIONS
      //**********************************************************
      if(row >= GDELAY_Y && col >= GDELAY_X)
      {
        for (int v = 0; v < VECT; v++) {
          INT win1_small[KERNEL_SIZE_Y][KERNEL_SIZE_X];
          INT win2_small[KERNEL_SIZE_Y][KERNEL_SIZE_X];
          for (int i = 0; i < KERNEL_SIZE_Y; i++) {
            for (int j = 0; j < KERNEL_SIZE_X; j++) {
              win1_small[i][j] = win1_vect[i][j+v];
              win2_small[i][j] = win2_vect[i][j+v];
            }
          }
          out_pixel(v*O_WIDTH_V,(v+1)*O_WIDTH_V-1) = f2i(filter(win1_small, win2_small));
        }
        out_s << out_pixel;
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processPixelsVECT(
    hls::stream<ap_uint<BW_IN> > &in_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X_V; x+=VECT) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const ap_uint<BW_IN> val = in_s.read();
      ap_uint<BW_OUT> out_val;
      INT temp[VECT];
      for(int i = 0; i < VECT; i++){
        #pragma HLS unroll
        temp[i] = val(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1);
        out_val(i*O_WIDTH_V,(i+1)*O_WIDTH_V-1) = filter(temp[i]);
      }
      out_s << out_val;
    }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processPixelsVECTF(
    hls::stream<ap_uint<BW_IN> > &in_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X_V; x+=VECT) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const ap_uint<BW_IN> val = in_s.read();
      ap_uint<BW_OUT> out_val;
      INT temp[VECT];
      for(int i = 0; i < VECT; i++){
        #pragma HLS unroll
        temp[i] = i2f(val(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1));
        out_val(i*O_WIDTH_V,(i+1)*O_WIDTH_V-1) = f2i(filter(temp[i]));
      }
      out_s << out_val;
    }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processPixels2VECT(
    hls::stream<ap_uint<BW_IN> > &in1_s,
    hls::stream<ap_uint<BW_IN> > &in2_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X_V; x+=VECT) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const ap_uint<BW_IN> val1 = in1_s.read();
      const ap_uint<BW_IN> val2 = in2_s.read();
      ap_uint<BW_OUT> out_val;
      INT temp1[VECT];
      INT temp2[VECT];
      for(int i = 0; i < VECT; i++){
        #pragma HLS unroll
        temp1[i] = val1(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1);
        temp2[i] = val2(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1);
        out_val(i*O_WIDTH_V,(i+1)*O_WIDTH_V-1) = filter(temp1[i], temp2[i]);
      }
      out_s << out_val;
    }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processPixels2VECTF(
    hls::stream<ap_uint<BW_IN> > &in1_s,
    hls::stream<ap_uint<BW_IN> > &in2_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X_V; x+=VECT) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const ap_uint<BW_IN> val1 = in1_s.read();
      const ap_uint<BW_IN> val2 = in2_s.read();
      ap_uint<BW_OUT> out_val;
      INT temp1[VECT];
      INT temp2[VECT];
      for(int i = 0; i < VECT; i++){
        #pragma HLS unroll
        temp1[i] = i2f(val1(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1));
        temp2[i] = i2f(val2(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1));
        out_val(i*O_WIDTH_V,(i+1)*O_WIDTH_V-1) = f2i(filter(temp1[i], temp2[i]));
      }
      out_s << out_val;
    }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processPixels3VECT(
    hls::stream<ap_uint<BW_IN> > &in1_s,
    hls::stream<ap_uint<BW_IN> > &in2_s,
    hls::stream<ap_uint<BW_IN> > &in3_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X_V; x+=VECT) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const ap_uint<BW_IN> val1 = in1_s.read();
      const ap_uint<BW_IN> val2 = in2_s.read();
      const ap_uint<BW_IN> val3 = in3_s.read();
      ap_uint<BW_OUT> out_val;
      INT temp1[VECT];
      INT temp2[VECT];
      INT temp3[VECT];
      for(int i = 0; i < VECT; i++){
        #pragma HLS unroll
        temp1[i] = val1(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1);
        temp2[i] = val2(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1);
        temp3[i] = val3(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1);
        out_val(i*O_WIDTH_V,(i+1)*O_WIDTH_V-1) = filter(temp1[i], temp2[i], temp3[i]);
      }
      out_s << out_val;
    }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename INT, int BW_IN, int BW_OUT, class Filter>
void processPixels3VECTF(
    hls::stream<ap_uint<BW_IN> > &in1_s,
    hls::stream<ap_uint<BW_IN> > &in2_s,
    hls::stream<ap_uint<BW_IN> > &in3_s,
    hls::stream<ap_uint<BW_OUT> > &out_s,
    const int &width,
    const int &height,
    Filter &filter)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X_V; x+=VECT) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const ap_uint<BW_IN> val1 = in1_s.read();
      const ap_uint<BW_IN> val2 = in2_s.read();
      const ap_uint<BW_IN> val3 = in3_s.read();
      ap_uint<BW_OUT> out_val;
      INT temp1[VECT];
      INT temp2[VECT];
      INT temp3[VECT];
      for(int i = 0; i < VECT; i++){
        #pragma HLS unroll
        temp1[i] = i2f(val1(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1));
        temp2[i] = i2f(val2(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1));
        temp3[i] = i2f(val3(i*I_WIDTH_V,(i+1)*I_WIDTH_V-1));
        out_val(i*O_WIDTH_V,(i+1)*O_WIDTH_V-1) = f2i(filter(temp1[i], temp2[i], temp3[i]));
      }
      out_s << out_val;
    }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename IN, typename OUT1, typename OUT2>
void splitStreamVECT(
    hls::stream<IN> &in_s,
    hls::stream<OUT1> &out1_s,
    hls::stream<OUT2> &out2_s,
    const int &width,
    const int &height)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X_V; x+=VECT) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;

      const IN val = in_s.read();

      out1_s << val;
      out2_s << val;
    }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename IN, typename OUT1, typename OUT2, typename OUT3>
void splitStream3VECT(
    hls::stream<IN> &in_s,
    hls::stream<OUT1> &out1_s,
    hls::stream<OUT2> &out2_s,
    hls::stream<OUT3> &out3_s,
    const int &width,
    const int &height)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X_V; x+=VECT) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;

      const IN val = in_s.read();

      out1_s << val;
      out2_s << val;
      out3_s << val;
    }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, int VECT, typename IN, typename OUT1, typename OUT2, typename OUT3, typename OUT4>
void splitStream4VECT(
    hls::stream<IN> &in_s,
    hls::stream<OUT1> &out1_s,
    hls::stream<OUT2> &out2_s,
    hls::stream<OUT3> &out3_s,
    hls::stream<OUT4> &out4_s,
    const int &width,
    const int &height)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GDELAY_X_V; x+=VECT) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;

      const IN val = in_s.read();

      out1_s << val;
      out2_s << val;
      out3_s << val;
      out4_s << val;
    }
}

#ifndef _OPSTUFF_
#define _OPSTUFF_
template<typename T>
struct OpOffset
{
  const T off;
  OpOffset(const T _off) : off(_off) {}
  T operator()(const T val) const { return val + off; }
};

template<typename T>
struct OpScale
{
  const T fact;
  OpScale(const T _fact) : fact(_fact) {}
  T operator()(const T val) const { return fact * val; }
};
#endif // _OPSTUFF_
