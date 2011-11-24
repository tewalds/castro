
#pragma once

//A few fixed width or architecture specific types

#include <stdint.h>

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;

typedef  int8_t  i8;
typedef uint8_t  u8;
typedef  int16_t i16;
typedef uint16_t u16;
typedef  int32_t i32;
typedef uint32_t u32;
typedef  int64_t i64;
typedef uint64_t u64;

#if __WORDSIZE == 16
	typedef  int16_t iword;
	typedef uint16_t uword;
#elif  __WORDSIZE == 32
	typedef  int32_t iword;
	typedef uint32_t uword;
#elif __WORDSIZE == 64
	typedef  int64_t iword;
	typedef uint64_t uword;
#else
#error Unknown word size
#endif

