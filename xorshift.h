
#pragma once

//Generates random numbers using the XORShift algorithm.

#include <stdint.h>

//generates 32 bit values, has a 32bit period
class XORShift_uint32 {
	uint32_t r;
public:
	XORShift_uint32(uint32_t s = def_seed) { seed(s); }
	void seed(uint32_t s) { r = (s ? s : def_seed); }
	uint32_t operator()() { return rand(); }
protected:
	static const uint32_t def_seed = 2463534242UL;
	uint32_t rand(){
		r ^= (r << 13);
		r ^= (r >> 17);
		r ^= (r <<  5);
		return r;
	}
};

//generates 64 bit values, has a 64bit period
class XORShift_uint64 {
	uint64_t r;
public:
	XORShift_uint64(uint64_t s = def_seed) { seed(s); }
	void seed(uint64_t s) { r = (s ? s : def_seed); }
	uint64_t operator()() { return rand(); }
protected:
	static const uint64_t def_seed = 88172645463325252ULL;
	uint64_t rand(){
		r ^= (r >> 17);
		r ^= (r << 31);
		r ^= (r >>  8);
		return r;
	}
};

// generates floating point numbers in the half-open interval [0, 1)
class XORShift_float : XORShift_uint32 {
public:
	XORShift_float(uint32_t seed = def_seed) : XORShift_uint32(seed) {}
	float operator()() { return static_cast<float>(rand()) * (1.f / 4294967296.f ); } // divide by 2^32
};

// generates double floating point numbers in the half-open interval [0, 1)
class XORShift_double : XORShift_uint64 {
public:
	XORShift_double(uint64_t seed = def_seed) : XORShift_uint64(seed) {}
	double operator()() { return static_cast<double>(rand()) * (1. / 18446744073709551616.); } // divide by 2^64
};

