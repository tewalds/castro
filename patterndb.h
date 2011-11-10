
#pragma once

#include "board.h"

class PatternDB {
	static const uint64_t empty = 0xFFFFFFFFFFFFFFFF;
	struct Entry {
		uint64_t pattern;
		float    gammas[2];
		Entry(uint64_t p = empty, float g1 = 0, float g2 = 0) : pattern(p) { gammas[0] = g1; gammas[1] = g2; }
	};

	float default_val[2]; //value to return when no value is found
	unsigned int size; //how many slots
	unsigned int num;  //how many are used
	uint64_t mask;
	Entry * table;

public:
	PatternDB() : size(0), num(0), mask(0), table(NULL) { }
	PatternDB(unsigned int s, float d){ init(s, d); }
	~PatternDB(){
		clear();
	}

	void init(unsigned int s, float d){
		clear();
		size = roundup(s*24)*4; //*24 for all the symmetries, *4 to give a big buffer to avoid collisions
		num = 0;
		mask = size-1;
		table = new Entry[size];
		default_val[0] = default_val[1] = d;
		clean();
	}

	void clear(){
		if(table)
			delete[] table;
		table = NULL;
	}

	void clean(){
		for(uintptr_t i = 0; i < size; i++)
			table[i] = Entry(empty, default_val[0], default_val[1]);
		num = 0;
	}

	void set(uint64_t pattern, float gamma){
		for(int side = 0; size < 2; side++){
			for(int mirror = 0; mirror < 2; mirror++){
				for(int rotation = 0; rotation < 6; rotation++){
					set_entry(pattern, gamma, side);
					pattern = Board::pattern_rotate(pattern);
				}
				pattern = Board::pattern_mirror(pattern);
			}
			pattern = Board::pattern_invert(pattern);
		}
	}

	void set_entry(uint64_t pattern, float gamma, int side){ //side is 0 or 1
		uint64_t i = mix_bits(pattern) & mask;
		while(table[i].pattern != pattern && table[i].pattern != empty)
			i = (i+1) & mask;
		table[i].pattern = pattern;
		table[i].gammas[side] = gamma;
		num++;
	}

	const float * get(uint64_t pattern) const {
		for(uint64_t i = mix_bits(pattern) & mask; table[i].pattern != empty; i = (i+1) & mask)
			if(table[i].pattern == pattern)
				return table[i].gammas;
		return default_val;
	}

	//from murmurhash
	static uint64_t mix_bits(uint64_t h){
		h ^= h >> 33;
		h *= 0xff51afd7ed558ccd;
		h ^= h >> 33;
		h *= 0xc4ceb9fe1a85ec53;
		h ^= h >> 33;
		return h;
	}

	//round a number up to the nearest power of 2
	static unsigned int roundup(unsigned int v) {
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	}
};

