
#ifndef _DEPTHSTATS_H_
#define _DEPTHSTATS_H_

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
	void add(unsigned int depth){
		num++;
		if(mindepth > depth) mindepth = depth;
		if(maxdepth < depth) maxdepth = depth;
		sumdepth += depth;
		sumdepthsq += depth*depth;
	}

	unsigned int avg(){
		if(num == 0) return 0;
		return sumdepth/num;
	}
	double std_dev(){
		if(num == 0) return 0;
		return sqrt((double)sumdepthsq/num - ((double)sumdepth/num)*((double)sumdepth/num));
	}
	string to_s(){
		if(num == 0) return "num=0";
		return to_str(avg()) +", dev=" + to_str(std_dev()) + ", min=" + to_str(mindepth) + ", max=" + to_str(maxdepth) + ", num=" + to_str(num);
	}
};

#endif

