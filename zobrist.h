
#pragma once

//Maintains 12 zobrist hashes, one for each permutation

#include <stdint.h>

typedef uint64_t hash_t;

class Zobrist {
private:
	static const hash_t strings[4096];

	hash_t values[12];

public:
	Zobrist(){
		for(int i = 0; i < 12; i++)
			values[i] = 0;
	}

	static hash_t string(int i) {
		return strings[i];
	}

	hash_t test(int permutation, int position) const {
		return values[permutation] ^ strings[position];
	}
	void update(int permutation, int position){
		values[permutation] ^= strings[position];
	}
	hash_t get(int permutation) const {
		return values[permutation];
	}

	hash_t get() const {
		hash_t m = values[0];
		for(int i = 1; i < 12; i++)
			if(m > values[i])
				m = values[i];
		return m;
	}
};

