
#ifndef _SOLVERAB_H_
#define _SOLVERAB_H_

#include <stdint.h>
#include "time.h"
#include "timer.h"

#include "board.h"
#include "move.h"

#include "solver.h"


class SolverAB : public Solver {
public:
	bool scout;

	SolverAB(bool Scout = false) {
		scout = Scout;
		reset();
	}
	~SolverAB() { }

	void set_board(const Board & board){
		rootboard = board;
		reset();
	}
	void move(const Move & m){
		 rootboard.move(m);
		 reset();
	}
	void set_memlimit(uint64_t lim) { }

	void reset(){
		outcome = -3;
		maxdepth = 0;
		nodes_seen = 0;
		bestmove = Move(M_UNKNOWN);

		timeout = false;
	}

	void solve(double time);

//return -2 for loss, -1,1 for tie, 0 for unknown, 2 for win, all from toplay's perspective
	int negamax(Board & board, const int depth, int alpha, int beta);
};

#endif

