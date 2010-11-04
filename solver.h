
#ifndef _SOLVER_H_
#define _SOLVER_H_

#include <stdint.h>
#include "board.h"

class Solver {
public:
	int outcome; // 0 = tie, 1 = white, 2 = black, -1 = white or tie, -2 = black or tie, anything else unknown
	int maxdepth;
	uint64_t nodes_seen;
	Move bestmove;

	Solver() : outcome(-3), maxdepth(0), nodes_seen(0) { }
	virtual ~Solver() { }

	virtual void solve(double time) { }
	virtual void set_board(const Board & board) { }
	virtual void move(const Move & m) { }
	virtual void set_memlimit(uint64_t lim) { }

protected:
	volatile bool timeout;
	void timedout(){ timeout = true; }
	Board rootboard;

};

#endif

