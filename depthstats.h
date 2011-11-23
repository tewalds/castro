
#pragma once

#include <stdint.h>
#include <cmath>
#include <string>
#include "string.h"
using namespace std;

struct DepthStats {
	uint32_t mindepth, maxdepth, num;
	uint64_t sumdepth, sumdepthsq;

	DepthStats(){
		reset();
	}
	void reset(){
		num = 0;
		mindepth = 1000000000;
		maxdepth = 0;
		sumdepth = 0;
		sumdepthsq = 0;
	}

	DepthStats & operator += (const DepthStats & o){
		num += o.num;
		sumdepth += o.sumdepth;
		sumdepthsq += o.sumdepthsq;
		if(mindepth > o.mindepth) mindepth = o.mindepth;
		if(maxdepth < o.maxdepth) maxdepth = o.maxdepth;
		return *this;
	}
	DepthStats operator + (const DepthStats & o) const {
		DepthStats n = *this;
		n += o;
		return n;
	}

	void add(unsigned int depth){
		num++;
		if(mindepth > depth) mindepth = depth;
		if(maxdepth < depth) maxdepth = depth;
		sumdepth += depth;
		sumdepthsq += depth*depth;
	}

	double avg() const {
		if(num == 0) return 0.0;
		return (double)sumdepth/num;
	}
	double std_dev() const {
		if(num == 0) return 0.0;
		return sqrt((double)sumdepthsq/num - ((double)sumdepth/num)*((double)sumdepth/num));
	}
	string to_s() const {
		if(num == 0) return "num=0";
		return to_str(avg(), 4) +", dev=" + to_str(std_dev(), 4) + ", min=" + to_str(mindepth) + ", max=" + to_str(maxdepth) + ", num=" + to_str(num);
	}
};

