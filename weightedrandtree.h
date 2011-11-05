
#ifndef _WEIGHTED_RAND_TREE_H_
#define _WEIGHTED_RAND_TREE_H_

#include <cstdlib>
#include "xorshift.h"

//rounds to power of 2 sizes, completely arbitrary weights
// O(log n) updates, O(log n) choose
class WeightedRandTree {
	mutable XORShift_float unitrand;
	unsigned int size, allocsize;
	float * weights;
public:
	WeightedRandTree()      : size(0), allocsize(0), weights(NULL) { }
	WeightedRandTree(unsigned int s) : allocsize(0), weights(NULL) { resize(s); }

	~WeightedRandTree(){
		if(weights)
			delete[] weights;
		weights = NULL;
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

	//resize and clear the tree
	void resize(unsigned int s){
		if(s < 2) s = 2;
		size = roundup(s);

		if(size > allocsize){
			if(weights)
				delete[] weights;

			allocsize = size;
			weights = new float[size*2];
		}

		clear();
	}

	//reset all weights to 0, O(s)
	void clear(){
		for(unsigned int i = 0; i < size*2; i++)
			weights[i] = 0;
	}

	//get an individual weight, O(1)
	float get_weight(unsigned int i) const {
		return weights[i + size];
	}

	//get the sum of the weights in the tree, O(1)
	float sum_weight() const {
		return weights[1];
	}

	//rebuilds the tree based on the weights, O(s)
	void rebuild_tree(){
		for(unsigned int i = size-1; i >= 1; i--)
			weights[i] = weights[2*i] + weights[2*i + 1];
	}

	//sets the weight but doesn't update the tree, needs to be fixed with rebuild_tree(), O(1)
	void set_weight_fast(unsigned int i, float w){
		weights[i+size] = w;
	}

	//sets the weight and updates the tree, O(log s)
	void set_weight(unsigned int i, float w){
		i += size;

		if(weights[i] == w)
			return;

		weights[i] = w;

		while(i /= 2)
			weights[i] = weights[i*2] + weights[i*2 + 1];
	}

	//return a weighted random index, O(log s), infinite loop if sum = 0
	unsigned int choose() const {
		if(weights[1] <= 0.00001f)
			return -1;

		float r;
		unsigned int i;
		do{
			r = unitrand() * weights[1];
			i = 2;
			while(i < size){
				if(r > weights[i]){
					r -= weights[i];
					i++;
				}
				i *= 2;
			}

			if(r > weights[i])
				i++;
		}while(weights[i] <= 0.0001f);

		return i - size;
	}
};

#endif

