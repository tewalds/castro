
#pragma once

//An Alpha-beta solver, single threaded with an optional transposition table.

#include "solver.h"

class SolverAB : public Solver {
	struct ABTTNode {
		hash_t hash;
		char   value;
		ABTTNode(hash_t h = 0, char v = 0) : hash(h), value(v) { }
	};

public:
	bool scout;
	int startdepth;

	ABTTNode * TT;
	uint64_t maxnodes, memlimit;

	SolverAB(bool Scout = false) {
		scout = Scout;
		startdepth = 2;
		TT = NULL;
		set_memlimit(100*1024*1024);
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
	void set_memlimit(uint64_t lim){
		memlimit = lim;
		maxnodes = memlimit/sizeof(ABTTNode);
		clear_mem();
	}

	void clear_mem(){
		reset();
		if(TT){
			delete[] TT;
			TT = NULL;
		}
	}
	void reset(){
		outcome = -3;
		maxdepth = 0;
		nodes_seen = 0;
		time_used = 0;
		bestmove = Move(M_UNKNOWN);

		timeout = false;
	}

	void solve(double time);

//return -2 for loss, -1,1 for tie, 0 for unknown, 2 for win, all from toplay's perspective
	int negamax(const Board & board, const int depth, int alpha, int beta);
	int negamax_outcome(const Board & board, const int depth);

	int tt_get(const hash_t & hash);
	int tt_get(const Board & board);
	void tt_set(const hash_t & hash, int val);
	void tt_set(const Board & board, int val);
};

