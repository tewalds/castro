
#ifndef _SOLVER_H_
#define _SOLVER_H_

#include <time.h>
#include <sys/time.h>

int time_msec(){
	struct timeval time;
	gettimeofday(&time, NULL);
	return (time.tv_sec*1000 + time.tv_usec/1000);
}

class Solver {
public:
	int outcome;
	int maxdepth;
	uint64_t nodes;
	int x, y;
	
	Solver() {
		outcome = -1;
		maxdepth = 0;
		nodes = 0;
		x = y = -1;
	}
	
	Solver(const Board & board, double time, uint64_t mem){
		outcome = -1;
		maxdepth = 0;
		nodes = 0;
		x = y = -1;
	}
};

#endif

