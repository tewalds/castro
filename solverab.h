
#ifndef _SOLVERAB_H_
#define _SOLVERAB_H_

#include <stdint.h>

#include "solver.h"

class SolverAB : public Solver {
public:
	bool scout;
	int startdepth;

	SolverAB(bool Scout = false) {
		scout = Scout;
		startdepth = 2;
		reset();
	}
	~SolverAB() { }

	void set_board(const Board & board, bool clear = true){
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
	int negamax(const Board & board, const int depth, int alpha, int beta);
	int negamax_outcome(const Board & board, const int depth);
};

#endif

