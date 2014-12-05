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

#define GDELAY_X     (KERNEL_SIZE_X/2)
#define GDELAY_Y     (KERNEL_SIZE_Y/2)

#ifndef _BORDERPADDING_
#define _BORDERPADDING_
// Border Handling Enums
struct BorderPadding {
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
// VECTOR FLOAT 
//*********************************************************************************************************************
#if 0
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
       
       lineBuff[SWIN_Y-2][col] = in_pixel;
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
         winPos j1 = getWinCoords(j,KERNEL_SIZE,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 = getWinCoords(j+1,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
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
       lineBuff[SWIN_Y-2][col] = in_pixel;
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
         winPos j1 = getWinCoords(j,KERNEL_SIZE,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 = getWinCoords(j+1,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 = getWinCoords(j+2,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 = getWinCoords(j+3,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
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
       lineBuff[SWIN_Y-2][col] = in_pixel;
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
         winPos j1 = getWinCoords(j,KERNEL_SIZE,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 = getWinCoords(j+1,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
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
       lineBuff[SWIN_Y-2][col] = in_pixel;
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
         winPos j1 = getWinCoords(j,KERNEL_SIZE,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 = getWinCoords(j+1,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 = getWinCoords(j+2,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
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
       lineBuff[SWIN_Y-2][col] = in_pixel;
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
         winPos j1 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j  ,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j2 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+1,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+2,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+3,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
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
        INT outp[VECT];
        outp[0] = filter(win1);
        outp[1] = filter(win2);
        outp[2] = filter(win3);
        outp[3] = filter(win4);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*WIDTH,(i+1)*WIDTH-1) = outp[i];
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
       lineBuff[SWIN_Y-2][col] = in_pixel;
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
         winPos j1 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j,KERNEL_SIZE,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+1,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+2,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+3,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j5 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+4,KERNEL_SIZE,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j6 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+5,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j7 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+6,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j8 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+7,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
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
        INT outp[VECT];
        outp[0] = filter(win1);
        outp[1] = filter(win2);
        outp[2] = filter(win3);
        outp[3] = filter(win4);
        outp[4] = filter(win5);
        outp[5] = filter(win6);
        outp[6] = filter(win7);
        outp[7] = filter(win8);
        for(int i = 0; i < VECT; i++)
          out_pixel(i*WIDTH,(i+1)*WIDTH-1) = outp[i];
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
       lineBuff[SWIN_Y-2][col] = in_pixel;
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
         winPos j1 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j,KERNEL_SIZE,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j2 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+1,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j3 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+2,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j4 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+3,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j5 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+4,KERNEL_SIZE,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j6 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+5,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j7 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+6,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j8 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+7,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j9 =  getWinCoords((SRADIUS_X*VECT)-RADIUS+j+8,KERNEL_SIZE,VECT,GROUP_DELAY,col,width,borderPadding);
         winPos j10 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+9,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j11 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+10,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j12 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+11,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j13 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+12,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j14 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+13,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j15 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+14,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
         winPos j16 = getWinCoords((SRADIUS_X*VECT)-RADIUS+j+15,KERNEL_SIZE,VECT,GROUP_DELAY_X,col,width,borderPadding);
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
#endif

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
        lineBuff[KERNEL_SIZE_Y-2][col] = in_pixel;
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

// process two input streams, one output stream
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE_X, int KERNEL_SIZE_Y, typename IN, typename OUT, class Filter>
void process2(
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
  IN lineBuff2[KERNEL_SIZE_Y-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff2 dim=1 complete
  IN win1[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  IN win2[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  IN win_tmp1[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp1 dim=0 complete
  IN win_tmp2[KERNEL_SIZE_Y][KERNEL_SIZE_X];
  #pragma HLS ARRAY_PARTITION variable=win_tmp2 dim=0 complete

  OUT out_pixel;
  IN temp_lb1, in_pixel1;
  IN temp_lb2, in_pixel2;
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
            win_tmp1[i][j] = win_tmp1[i][j+1];
            win_tmp2[i][j] = win_tmp2[i][j+1];
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
            win_tmp1[i][KERNEL_SIZE_X-1] = lineBuff1[i][col];
            win_tmp2[i][KERNEL_SIZE_X-1] = lineBuff2[i][col];
          } else {
            temp_lb1 = lineBuff1[i][col];
            win_tmp1[i][KERNEL_SIZE_X-1] = temp_lb1;
            lineBuff1[i-1][col] = temp_lb1;
            temp_lb2 = lineBuff2[i][col];
            win_tmp2[i][KERNEL_SIZE_X-1] = temp_lb2;
            lineBuff2[i-1][col] = temp_lb2;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        lineBuff1[KERNEL_SIZE_Y-2][col] = in_pixel1;
        lineBuff2[KERNEL_SIZE_Y-2][col] = in_pixel2;
        win_tmp1[KERNEL_SIZE_Y-1][KERNEL_SIZE_X-1] = in_pixel1;
        win_tmp2[KERNEL_SIZE_Y-1][KERNEL_SIZE_X-1] = in_pixel2;
      }

      //**********************************************************
      // HANDLE BORDERS 
      //**********************************************************
      // X-DIRECTION
      for(i = 0; i < KERNEL_SIZE_Y; i++){
        for(j = 0; j < KERNEL_SIZE_X; j++){
          int jx = getNewCoords(j,KERNEL_SIZE_X,GDELAY_X,col,width,borderPadding);
          win1[i][j] = win_tmp1[i][jx];
          win2[i][j] = win_tmp2[i][jx];
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

// process one input stream, put result into two output streams
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
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
  
  ROW_LOOP:
  for (int row = 0; row < MAX_HEIGHT + GROUP_DELAY; row++) {
    COL_LOOP:
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
        lineBuff[KERNEL_SIZE-2][col] = in_pixel;
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
        out1_s.write(out_pixel);
        out2_s.write(out_pixel);
      }
    }
  }
}

template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
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
    assert( (KERNEL_SIZE % 2) == 1 );
  #endif

  IN lineBuff1[KERNEL_SIZE-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff1 dim=1 complete
  IN win1[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1 dim=0 complete
  IN win1_tmp[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win1_tmp dim=0 complete

  IN lineBuff2[KERNEL_SIZE-1][MAX_WIDTH];
  #pragma HLS ARRAY_PARTITION variable=lineBuff2 dim=1 complete
  IN win2[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2 dim=0 complete
  IN win2_tmp[KERNEL_SIZE][KERNEL_SIZE];
  #pragma HLS ARRAY_PARTITION variable=win2_tmp dim=0 complete

  OUT out_pixel;
  IN temp_lb2, in_pixel2;
  IN temp_lb1, in_pixel1;

  int i, j ,row, col;

  process_main_loop:
  for (int row = 0; row < MAX_HEIGHT + GROUP_DELAY; row++) {
    for (int col = 0; col < MAX_WIDTH + GROUP_DELAY; col++) {
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
      if (col < width + GROUP_DELAY & row < height + GROUP_DELAY){
        for(i = 0; i < KERNEL_SIZE; i++){
        #pragma HLS unroll
          for(j = 0; j < KERNEL_SIZE-1; j++){
            win1_tmp[i][j] = win1_tmp[i][j+1];
            win2_tmp[i][j] = win2_tmp[i][j+1];
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
            win1_tmp[i][KERNEL_SIZE-1] = lineBuff1[i][col];
            win2_tmp[i][KERNEL_SIZE-1] = lineBuff2[i][col];
          } else {
            temp_lb1 = lineBuff1[i][col];
            temp_lb2 = lineBuff2[i][col];
            win1_tmp[i][KERNEL_SIZE-1] = temp_lb1;
            win2_tmp[i][KERNEL_SIZE-1] = temp_lb2;
            lineBuff1[i-1][col] = temp_lb1;
            lineBuff2[i-1][col] = temp_lb2;
          }
        }
        //this is not necessary for the last lines, but it does not hurt and simplifies control
        lineBuff1[KERNEL_SIZE-2][col] = in_pixel1;
        lineBuff2[KERNEL_SIZE-2][col] = in_pixel2;
        win1_tmp[KERNEL_SIZE-1][KERNEL_SIZE-1] = in_pixel1;
        win2_tmp[KERNEL_SIZE-1][KERNEL_SIZE-1] = in_pixel2;
      }

      //**********************************************************
      // HANDLE BORDERS
      //**********************************************************
      // X-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int jx = getNewCoords(j,KERNEL_SIZE,GROUP_DELAY,col,width,borderPadding);
          win1[i][j] = win1_tmp[i][jx];
          win2[i][j] = win2_tmp[i][jx];
        }
      }
      // Y-DIRECTION
      for(i = 0; i < KERNEL_SIZE; i++){
        for(j = 0; j < KERNEL_SIZE; j++){
          int ix = getNewCoords(i,KERNEL_SIZE,GROUP_DELAY,row,height,borderPadding);
          win1[i][j] = win1[ix][j];
          win2[i][j] = win2[ix][j];
        }
      }

      //**********************************************************
      // FILTER COMPUTATION AND OUTPUT ASSIGNMENT
      //**********************************************************
      // Do the filtering
      if (row >= GROUP_DELAY && col >= GROUP_DELAY){
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
        lineBuff[KERNEL_SIZE-2][col] = in_pixel;
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
        lineBuff[KERNEL_SIZE-2][col] = in_pixel;
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
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processPixels(
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
      out_s << filter(val);
    }
}
// 2:1
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
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
    for (int x = 0; x < width + GROUP_DELAY; ++x) {
      PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;
      const IN val1 = in1_s.read();
      const IN val2 = in2_s.read();
      out_s.write(filter(val1, val2));
    }
}
// 3:1
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT, class Filter>
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
    for (int x = 0; x < width + GROUP_DELAY; ++x) {
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
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT1, typename OUT2>
void splitStream(
    hls::stream<IN> &in_s,
    hls::stream<OUT1> &out1_s,
    hls::stream<OUT2> &out2_s,
    const int &width,
    const int &height)
{
  assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width + GROUP_DELAY; ++x) {
PRAGMA_HLS(HLS pipeline ii=II_TARGET)
      if (x >= width)
        continue;

      const IN val = in_s.read();

      out1_s << val;
      out2_s << val;
    }
}

// 1:3
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT>
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
    for (int x = 0; x < width + GROUP_DELAY; ++x) {
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
template<int II_TARGET, int MAX_WIDTH, int MAX_HEIGHT, int KERNEL_SIZE, typename IN, typename OUT>
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
    for (int x = 0; x < width + GROUP_DELAY; ++x) {
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
