
#ifndef _RAND_H_
#define _RAND_H_

#include <cstdlib>
#include "types.h"

inline uint16_t rand16(){
	return rand() & 0xFFFF;
}

inline uint32_t rand32(){
	return (uint32_t)rand16() | ((uint32_t)rand16() << 16);
}

inline uint64_t rand64(){
	return (uint64_t)rand32() | ((uint64_t)rand32() << 32);
}

inline float unitrand(){
	return ((float)rand())/RAND_MAX;
}

#endif

