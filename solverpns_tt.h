
#ifndef _SOLVERPNS_TT_H_
#define _SOLVERPNS_TT_H_

#include <stdint.h>
#include "time.h"
#include "timer.h"

#include "board.h"
#include "move.h"

#include "solver.h"
#include "zobrist.h"


class SolverPNSTT : public Solver {
	static const uint32_t LOSS  = (1<<30)-1;
	static const uint32_t DRAW  = (1<<30)-2;
	static const uint32_t INF32 = (1<<30)-3;
public:

	struct PNSNode {
		hash_t hash;
		uint32_t phi, delta;

		PNSNode()                        : hash(0), phi(0), delta(0) { }
		PNSNode(hash_t h, int v = 1)     : hash(h), phi(v), delta(v) { }
		PNSNode(hash_t h, int p, int d)  : hash(h), phi(p), delta(d) { }

		PNSNode & abval(int outcome, int toplay, int assign, int value = 1){
			if(assign && (outcome == 1 || outcome == -1))
				outcome = (toplay == assign ? 2 : -2);

			if(     outcome ==  0)   { phi = value; delta = value; }
			else if(outcome ==  2)   { phi = LOSS;  delta = 0;     }
			else if(outcome == -2)   { phi = 0;     delta = LOSS; }
			else /*(outcome 1||-1)*/ { phi = 0;     delta = DRAW; }
			return *this;
		}
	};


	PNSNode * TT;
	uint64_t maxnodes;

	int assignties; //which player to assign a tie to

	int   ab; // how deep of an alpha-beta search to run at each leaf node
	bool  df; // go depth first?
	float epsilon; //if depth first, how wide should the threshold be?
	int   ties;    //0 handle ties, 1 assign p1, 2 assign p2, 3 do two searches, once assigned to each

	bool timeout;

	SolverPNSTT(int AB = 0, bool DF = true, float eps = 0.25) {
		ab = AB;
		df = DF;
		epsilon = eps;
		ties = 0;

		TT = NULL;
		reset();
	}
	~SolverPNSTT(){
		if(TT)
			delete[] TT;
		TT = NULL;
	}
	void reset(){
		outcome = -3;
		maxdepth = 0;
		nodes_seen = 0;
		bestmove = Move(M_UNKNOWN);

		timeout = false;

		maxnodes = 0;

		if(TT){
			delete[] TT;
			TT = NULL;
		}
	}
	void timedout(){ timeout = true; }

	void solve(Board board, double time, uint64_t memlimit);

//basic proof number search building a tree
	int run_pns(const Board & board, uint64_t memlimit); //-3 = unknown, 0 = tie, 1 = p1, 2 = p2
	void pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td);

//update the phi and delta for the node
	bool updatePDnum(const Board & board, PNSNode * node);

	PNSNode * tt(const Board & board, Move move);
};

#endif

