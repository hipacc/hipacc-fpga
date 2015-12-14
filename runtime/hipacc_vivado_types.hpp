//
// Copyright (c) 2014, University of Erlangen-Nuremberg
// Copyright (c) 2014, Siemens AG
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


#ifndef __HIPACC_VIVADO_TYPES_HPP__
#define __HIPACC_VIVADO_TYPES_HPP__


#define PRAGMA_SUB(x) _Pragma (#x)
#define PRAGMA_HLS(x) PRAGMA_SUB(x)
#define getWindowAt(wnd, x, y)      wnd[y][x]
#define setWindowAt(wnd, val, x, y) wnd[y][x]=val


// TODO reiche: most of this code could be included from hipacc_types.hpp
// HIPAcc replaces all image types
//   uchar4  -> ap_uint<32>
//   ushort4 -> ap_uint<64>
//   uint4   -> ap_uint<128>
//   ulong4  -> ap_uint<256>
//   float4  -> ap_uint<128>
//   double4 -> ap_uint<256>


#include "ap_int.h"


typedef unsigned char       uchar;
typedef unsigned short      ushort;
typedef unsigned int        uint;
typedef unsigned long       ulong;
#define ATTRIBUTES inline
#define MAKE_VEC_F(NEW_TYPE, BASIC_TYPE, RET_TYPE) \
    MAKE_TYPE(NEW_TYPE, BASIC_TYPE) \
    MAKE_VMOP(NEW_TYPE, BASIC_TYPE) \
    MAKE_MOP(NEW_TYPE, BASIC_TYPE) \
    MAKE_VOPS_A(NEW_TYPE, BASIC_TYPE, RET_TYPE)
#define MAKE_VEC_I(NEW_TYPE, BASIC_TYPE, RET_TYPE) \
    MAKE_VEC_F(NEW_TYPE, BASIC_TYPE, RET_TYPE) \
    MAKE_VOPS_I(NEW_TYPE, BASIC_TYPE, RET_TYPE)


// vector type definition
#define MAKE_TYPE(NEW_TYPE, BASIC_TYPE) \
struct NEW_TYPE { \
    BASIC_TYPE x, y, z, w; \
    void operator=(BASIC_TYPE b) { \
        x = b; y = b; z = b; w = b; \
    } \
}; \
typedef struct NEW_TYPE NEW_TYPE;


// make function
#define MAKE_VMOP(NEW_TYPE, BASIC_TYPE) \
static ATTRIBUTES NEW_TYPE make_##NEW_TYPE(BASIC_TYPE x, BASIC_TYPE y, BASIC_TYPE z, BASIC_TYPE w) { \
    NEW_TYPE t; t.x = x; t.y = y; t.z = z; t.w = w; return t; \
}

#define MAKE_MOP(NEW_TYPE, BASIC_TYPE) \
static ATTRIBUTES NEW_TYPE make_##NEW_TYPE(BASIC_TYPE s) \
{ \
    return make_##NEW_TYPE(s, s, s, s); \
}


// vector operators for all data types
#define MAKE_VOPS_A(NEW_TYPE, BASIC_TYPE, RET_TYPE) \
 \
 /* binary operator: add */ \
 \
ATTRIBUTES NEW_TYPE operator+(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); \
} \
ATTRIBUTES NEW_TYPE operator+(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x + b, a.y + b, a.z + b, a.w + b); \
} \
ATTRIBUTES NEW_TYPE operator+(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a + b.x, a + b.y, a + b.z, a + b.w); \
} \
ATTRIBUTES void operator+=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x += b.x; a.y += b.y; a.z += b.z; a.w += b.w; \
} \
ATTRIBUTES void operator+=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x += b, a.y += b, a.z += b, a.w += b; \
} \
 \
 /* binary operator: subtract */ \
 \
ATTRIBUTES NEW_TYPE operator-(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); \
} \
ATTRIBUTES NEW_TYPE operator-(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x - b, a.y - b, a.z - b, a.w - b); \
} \
ATTRIBUTES NEW_TYPE operator-(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a - b.x, a - b.y, a - b.z, a - b.w); \
} \
ATTRIBUTES void operator-=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x -= b.x; a.y -= b.y; a.z -= b.z; a.w -= b.w; \
} \
ATTRIBUTES void operator-=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x -= b, a.y -= b, a.z -= b, a.w -= b; \
} \
 \
 /* binary operator: multiply */ \
 \
ATTRIBUTES NEW_TYPE operator*(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); \
} \
ATTRIBUTES NEW_TYPE operator*(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x * b, a.y * b, a.z * b, a.w * b); \
} \
ATTRIBUTES NEW_TYPE operator*(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a * b.x, a * b.y, a * b.z, a * b.w); \
} \
ATTRIBUTES void operator*=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x *= b.x; a.y *= b.y; a.z *= b.z; a.w *= b.w; \
} \
ATTRIBUTES void operator*=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x *= b, a.y *= b, a.z *= b, a.w *= b; \
} \
 \
 /* binary operator: divide */ \
 \
ATTRIBUTES NEW_TYPE operator/(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); \
} \
ATTRIBUTES NEW_TYPE operator/(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x / b, a.y / b, a.z / b, a.w / b); \
} \
ATTRIBUTES NEW_TYPE operator/(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a / b.x, a / b.y, a / b.z, a / b.w); \
} \
ATTRIBUTES void operator/=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x /= b.x; a.y /= b.y; a.z /= b.z; a.w /= b.w; \
} \
ATTRIBUTES void operator/=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x /= b, a.y /= b, a.z /= b, a.w /= b; \
} \
 \
 /* unary operator: plus */ \
 \
ATTRIBUTES NEW_TYPE operator+(NEW_TYPE a) { \
    return make_##NEW_TYPE(+a.x, +a.y, +a.z, +a.w); \
} \
 \
 /* unary operator: minus */ \
 \
ATTRIBUTES NEW_TYPE operator-(NEW_TYPE a) { \
    return make_##NEW_TYPE(-a.x, -a.y, -a.z, -a.w); \
} \
 \
 /* relational operator: greater-than */ \
 \
ATTRIBUTES RET_TYPE operator>(NEW_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a.x > b.x, a.y > b.y, a.z > b.z, a.w > b.w); \
} \
ATTRIBUTES RET_TYPE operator>(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##RET_TYPE(a.x > b, a.y > b, a.z > b, a.w > b); \
} \
ATTRIBUTES RET_TYPE operator>(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a > b.x, a > b.y, a > b.z, a > b.w); \
} \
 \
 /* relational operator: less-than */ \
 \
ATTRIBUTES RET_TYPE operator<(NEW_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w); \
} \
ATTRIBUTES RET_TYPE operator<(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##RET_TYPE(a.x < b, a.y < b, a.z < b, a.w < b); \
} \
ATTRIBUTES RET_TYPE operator<(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a < b.x, a < b.y, a < b.z, a < b.w); \
} \
 \
 /* relational operator: greater-than or equal-to */ \
 \
ATTRIBUTES RET_TYPE operator>=(NEW_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w); \
} \
ATTRIBUTES RET_TYPE operator>=(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##RET_TYPE(a.x >= b, a.y >= b, a.z >= b, a.w >= b); \
} \
ATTRIBUTES RET_TYPE operator>=(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a >= b.x, a >= b.y, a >= b.z, a >= b.w); \
} \
 \
 /* relational operator: less-than or equal-to */ \
 \
ATTRIBUTES RET_TYPE operator<=(NEW_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a.x <= b.x, a.y <= b.y, a.z <= b.z, a.w <= b.w); \
} \
ATTRIBUTES RET_TYPE operator<=(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##RET_TYPE(a.x <= b, a.y <= b, a.z <= b, a.w <= b); \
} \
ATTRIBUTES RET_TYPE operator<=(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a <= b.x, a <= b.y, a <= b.z, a <= b.w); \
} \
 \
 /* equality operator: equal */ \
 \
ATTRIBUTES RET_TYPE operator==(NEW_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w); \
} \
ATTRIBUTES RET_TYPE operator==(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##RET_TYPE(a.x == b, a.y == b, a.z == b, a.w == b); \
} \
ATTRIBUTES RET_TYPE operator==(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a == b.x, a == b.y, a == b.z, a == b.w); \
} \
 \
 /* equality operator: not equal */ \
 \
ATTRIBUTES RET_TYPE operator!=(NEW_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a.x != b.x, a.y != b.y, a.z != b.z, a.w != b.w); \
} \
ATTRIBUTES RET_TYPE operator!=(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##RET_TYPE(a.x != b, a.y != b, a.z != b, a.w != b); \
} \
ATTRIBUTES RET_TYPE operator!=(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a != b.x, a != b.y, a != b.z, a != b.w); \
} \
 \
 /* logical operator: and */ \
 \
ATTRIBUTES RET_TYPE operator&&(NEW_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a.x && b.x, a.y && b.y, a.z && b.z, a.w && b.w); \
} \
ATTRIBUTES RET_TYPE operator&&(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##RET_TYPE(a.x && b, a.y && b, a.z && b, a.w && b); \
} \
ATTRIBUTES RET_TYPE operator&&(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a && b.x, a && b.y, a && b.z, a && b.w); \
} \
 \
 /* logical operator: or */ \
 \
ATTRIBUTES RET_TYPE operator||(NEW_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a.x || b.x, a.y || b.y, a.z || b.z, a.w || b.w); \
} \
ATTRIBUTES RET_TYPE operator||(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##RET_TYPE(a.x || b, a.y || b, a.z || b, a.w || b); \
} \
ATTRIBUTES RET_TYPE operator||(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##RET_TYPE(a || b.x, a || b.y, a || b.z, a || b.w); \
} \
 \
 /* logical unary operator: not */ \
 \
ATTRIBUTES RET_TYPE operator!(NEW_TYPE a) { \
    return make_##RET_TYPE(!a.x, !a.y, !a.z, !a.w); \
} \
 \
 /* operator: comma */ \
 \
ATTRIBUTES NEW_TYPE operator,(NEW_TYPE a, NEW_TYPE b) { \
    return b; \
} \
ATTRIBUTES BASIC_TYPE operator,(NEW_TYPE a, BASIC_TYPE b) { \
    return b; \
} \
ATTRIBUTES NEW_TYPE operator,(BASIC_TYPE a, NEW_TYPE b) { \
    return b; \
}


// vector operators for integer data types only
#define MAKE_VOPS_I(NEW_TYPE, BASIC_TYPE, RET_TYPE) \
 \
 /* binary operator: remainder */ \
 \
ATTRIBUTES NEW_TYPE operator%(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x % b.x, a.y % b.y, a.z % b.z, a.w % b.w); \
} \
ATTRIBUTES NEW_TYPE operator%(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x % b, a.y % b, a.z % b, a.w % b); \
} \
ATTRIBUTES NEW_TYPE operator%(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a % b.x, a % b.y, a % b.z, a % b.w); \
} \
ATTRIBUTES void operator%=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x %= b.x; a.y %= b.y; a.z %= b.z; a.w %= b.w; \
} \
ATTRIBUTES void operator%=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x %= b, a.y %= b, a.z %= b, a.w %= b; \
} \
 \
 /* unary operator: post- and pre-increment */ \
 \
ATTRIBUTES NEW_TYPE operator++(NEW_TYPE a) { \
    return make_##NEW_TYPE(++a.x, ++a.y, ++a.z, ++a.w); \
} \
ATTRIBUTES NEW_TYPE operator++(NEW_TYPE a, int) { \
    return make_##NEW_TYPE(a.x++, a.y++, a.z++, a.w++); \
} \
 \
 /* unary operator: post- and pre-decrement */ \
 \
ATTRIBUTES NEW_TYPE operator--(NEW_TYPE a) { \
    return make_##NEW_TYPE(--a.x, --a.y, --a.z, --a.w); \
} \
ATTRIBUTES NEW_TYPE operator--(NEW_TYPE a, int) { \
    return make_##NEW_TYPE(a.x--, a.y--, a.z--, a.w--); \
} \
 \
 /* bitwise operator: and */ \
 \
ATTRIBUTES NEW_TYPE operator&(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x & b.x, a.y & b.y, a.z & b.z, a.w & b.w); \
} \
ATTRIBUTES NEW_TYPE operator&(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x & b, a.y & b, a.z & b, a.w & b); \
} \
ATTRIBUTES NEW_TYPE operator&(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a & b.x, a & b.y, a & b.z, a & b.w); \
} \
ATTRIBUTES void operator&=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x &= b.x; a.y &= b.y; a.z &= b.z; a.w &= b.w; \
} \
ATTRIBUTES void operator&=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x &= b, a.y &= b, a.z &= b, a.w &= b; \
} \
 \
 /* bitwise operator: or */ \
 \
ATTRIBUTES NEW_TYPE operator|(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x | b.x, a.y | b.y, a.z | b.z, a.w | b.w); \
} \
ATTRIBUTES NEW_TYPE operator|(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x | b, a.y | b, a.z | b, a.w | b); \
} \
ATTRIBUTES NEW_TYPE operator|(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a | b.x, a | b.y, a | b.z, a | b.w); \
} \
ATTRIBUTES void operator|=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x |= b.x; a.y |= b.y; a.z |= b.z; a.w |= b.w; \
} \
ATTRIBUTES void operator|=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x |= b, a.y |= b, a.z |= b, a.w |= b; \
} \
 \
 /* bitwise operator: exclusive or */ \
 \
ATTRIBUTES NEW_TYPE operator^(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x ^ b.x, a.y ^ b.y, a.z ^ b.z, a.w ^ b.w); \
} \
ATTRIBUTES NEW_TYPE operator^(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x ^ b, a.y ^ b, a.z ^ b, a.w ^ b); \
} \
ATTRIBUTES NEW_TYPE operator^(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a ^ b.x, a ^ b.y, a ^ b.z, a ^ b.w); \
} \
ATTRIBUTES void operator^=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x ^= b.x; a.y ^= b.y; a.z ^= b.z; a.w ^= b.w; \
} \
ATTRIBUTES void operator^=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x ^= b, a.y ^= b, a.z ^= b, a.w ^= b; \
} \
 \
 /* bitwise operator: not */ \
 \
ATTRIBUTES NEW_TYPE operator~(NEW_TYPE a) { \
    return make_##NEW_TYPE(~a.x, ~a.y, ~a.z, ~a.w); \
} \
 \
 /* operator: right-shift */ \
 \
ATTRIBUTES NEW_TYPE operator>>(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x >> b.x, a.y >> b.y, a.z >> b.z, a.w >> b.w); \
} \
ATTRIBUTES NEW_TYPE operator>>(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x >> b, a.y >> b, a.z >> b, a.w >> b); \
} \
ATTRIBUTES NEW_TYPE operator>>(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a >> b.x, a >> b.y, a >> b.z, a >> b.w); \
} \
ATTRIBUTES void operator>>=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x >>= b.x; a.y >>= b.y; a.z >>= b.z; a.w >>= b.w; \
} \
ATTRIBUTES void operator>>=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x >>= b, a.y >>= b, a.z >>= b, a.w >>= b; \
} \
 \
 /* operator: left-shift */ \
 \
ATTRIBUTES NEW_TYPE operator<<(NEW_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a.x << b.x, a.y << b.y, a.z << b.z, a.w << b.w); \
} \
ATTRIBUTES NEW_TYPE operator<<(NEW_TYPE a, BASIC_TYPE b) { \
    return make_##NEW_TYPE(a.x << b, a.y << b, a.z << b, a.w << b); \
} \
ATTRIBUTES NEW_TYPE operator<<(BASIC_TYPE a, NEW_TYPE b) { \
    return make_##NEW_TYPE(a << b.x, a << b.y, a << b.z, a << b.w); \
} \
ATTRIBUTES void operator<<=(NEW_TYPE &a, NEW_TYPE b) { \
    a.x <<= b.x; a.y <<= b.y; a.z <<= b.z; a.w <<= b.w; \
} \
ATTRIBUTES void operator<<=(NEW_TYPE a, BASIC_TYPE b) { \
    a.x <<= b, a.y <<= b, a.z <<= b, a.w <<= b; \
}


MAKE_VEC_I(char4,     char,     char4)
MAKE_VEC_I(uchar4,    uchar,    char4)
MAKE_VEC_I(short4,    short,    short4)
MAKE_VEC_I(ushort4,   ushort,   short4)
MAKE_VEC_I(int4,      int,      int4)
MAKE_VEC_I(uint4,     uint,     int4)
MAKE_VEC_I(long4,     long,     long4)
MAKE_VEC_I(ulong4,    ulong,    long4)
MAKE_VEC_F(float4,    float,    int4)
MAKE_VEC_F(double4,   double,   long4)



// conversion function
#define MAKE_CONV_FUNC(BASIC_TYPE, RET_TYPE, VEC_TYPE) \
  ATTRIBUTES RET_TYPE convert_##RET_TYPE(VEC_TYPE vec) { \
  PRAGMA_HLS(HLS inline) \
    return make_##RET_TYPE(vec.x, vec.y, vec.z, vec.w); \
  }

// generate conversion functions for types
#define MAKE_CONV(VEC_TYPE) \
    MAKE_CONV_FUNC(char,   char4,   VEC_TYPE) \
    MAKE_CONV_FUNC(uchar,  uchar4,  VEC_TYPE) \
    MAKE_CONV_FUNC(short,  short4,  VEC_TYPE) \
    MAKE_CONV_FUNC(ushort, ushort4, VEC_TYPE) \
    MAKE_CONV_FUNC(int,    int4,    VEC_TYPE) \
    MAKE_CONV_FUNC(uint,   uint4,   VEC_TYPE) \
    MAKE_CONV_FUNC(long,   long4,   VEC_TYPE) \
    MAKE_CONV_FUNC(ulong,  ulong4,  VEC_TYPE) \
    MAKE_CONV_FUNC(float,  float4,  VEC_TYPE) \
    MAKE_CONV_FUNC(double, double4, VEC_TYPE)

#define VIVADO_IN_CONV(TYPE, WIDTH) \
	TYPE ## 4 convert_ ## TYPE ## 4(ap_uint<WIDTH> val) { \
	PRAGMA_HLS(HLS inline) \
		return make_ ## TYPE ## 4( \
			(TYPE)val(WIDTH/4*3, WIDTH-1), \
			(TYPE)val(WIDTH/2,   WIDTH/4*3-1), \
			(TYPE)val(WIDTH/4,   WIDTH/2-1), \
			(TYPE)val(0,         WIDTH/4-1)); \
	}

#define VIVADO_OUT_CONV(TYPE, WIDTH, OUT_TYPE) \
	ap_uint<WIDTH> convert_ ## OUT_TYPE ## 4(TYPE ## 4 val, bool output) { \
	PRAGMA_HLS(HLS inline) \
	  ap_uint<WIDTH> ret; \
	  ret(WIDTH/4*3, WIDTH-1)     = (TYPE)val.x; \
	  ret(WIDTH/2,   WIDTH/4*3-1) = (TYPE)val.y; \
	  ret(WIDTH/4,   WIDTH/2-1)   = (TYPE)val.z; \
	  ret(0,         WIDTH/4-1)   = (TYPE)val.w; \
	  return ret; \
	}

#define VIVADO_VEC_FUNC(TYPE, FUNC) \
	TYPE ## 4 FUNC(TYPE ## 4 a, TYPE b) { \
	PRAGMA_HLS(HLS inline) \
		return make_ ## TYPE ## 4( \
			FUNC(a.x, b), \
			FUNC(a.y, b), \
			FUNC(a.z, b), \
			FUNC(a.w, b) \
		); \
	} \
	TYPE ## 4 FUNC(TYPE a, TYPE ## 4 b) { \
	PRAGMA_HLS(HLS inline) \
		return make_ ## TYPE ## 4( \
			FUNC(a, b.x), \
			FUNC(a, b.y), \
			FUNC(a, b.z), \
			FUNC(a, b.w) \
		); \
	} \
	TYPE ## 4 FUNC(TYPE ## 4 a, TYPE ## 4 b) { \
	PRAGMA_HLS(HLS inline) \
		return make_ ## TYPE ## 4( \
			FUNC(a.x, b.x), \
			FUNC(a.y, b.y), \
			FUNC(a.z, b.z), \
			FUNC(a.w, b.w) \
		); \
	}

#define VIVADO_MIN_FUNC(TYPE) \
	TYPE min(TYPE a, TYPE b) { \
	PRAGMA_HLS(HLS inline) \
		return (a > b ? b : a); \
	} \
	VIVADO_VEC_FUNC(TYPE, min)

#define VIVADO_MAX_FUNC(TYPE) \
	TYPE max(TYPE a, TYPE b) { \
	PRAGMA_HLS(HLS inline) \
		return (a < b ? b : a); \
	} \
	VIVADO_VEC_FUNC(TYPE, max)

#define VIVADO_MATH_FUNC(TYPE) \
	VIVADO_MAX_FUNC(TYPE) \
	VIVADO_MIN_FUNC(TYPE)

#define VIVADO_CONV(TYPE) \
	VIVADO_IN_CONV(TYPE, 32) \
	VIVADO_OUT_CONV(TYPE, 32,  char) \
	VIVADO_OUT_CONV(TYPE, 32,  uchar) \
	VIVADO_OUT_CONV(TYPE, 64,  short) \
	VIVADO_OUT_CONV(TYPE, 64,  ushort) \
	VIVADO_OUT_CONV(TYPE, 128, int) \
	VIVADO_OUT_CONV(TYPE, 128, uint) \
	VIVADO_OUT_CONV(TYPE, 256, long) \
	VIVADO_OUT_CONV(TYPE, 256, ulong) \
	VIVADO_OUT_CONV(TYPE, 128, float) \
	VIVADO_OUT_CONV(TYPE, 256, double) \
	VIVADO_MATH_FUNC(TYPE)


MAKE_CONV(char4)
MAKE_CONV(uchar4)
MAKE_CONV(short4)
MAKE_CONV(ushort4)
MAKE_CONV(int4)
MAKE_CONV(uint4)
MAKE_CONV(long4)
MAKE_CONV(ulong4)
MAKE_CONV(float4)
MAKE_CONV(double4)

VIVADO_CONV(char)
VIVADO_CONV(uchar)
VIVADO_CONV(short)
VIVADO_CONV(ushort)
VIVADO_CONV(int)
VIVADO_CONV(uint)
VIVADO_CONV(long)
VIVADO_CONV(ulong)
VIVADO_CONV(float)
VIVADO_CONV(double)


#endif  // __HIPACC_VIVADO_TYPES_HPP__

