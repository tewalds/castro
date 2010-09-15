
#ifndef _HASHSET_H_
#define _HASHSET_H_

#include "zobrist.h"

class HashSet {
	unsigned int size;
	hash_t * table;

public:
	HashSet() : size(0), table(NULL) { }
	HashSet(unsigned int s){ init(s); }
	~HashSet(){
		if(table)
			delete[] table;
		table = NULL;
	}

	void init(unsigned int s){
		size = s*4;
		table = new hash_t[size];
		for(unsigned int i = 0; i < size; i++)
			table[i] = 0;
	}

	void add(hash_t h){
		unsigned int i = h % size;
		while(table[i])
			i = (i+1)%size;
		table[i] = h;
	}
	
	bool exists(hash_t h){
		for(unsigned int i = h % size; table[i]; i = (i+1)%size)
			if(table[i] == h)
				return true;
		return false;
	}
};

#endif

