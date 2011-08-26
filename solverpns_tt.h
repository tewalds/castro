
#ifndef _SOLVERPNS_TT_H_
#define _SOLVERPNS_TT_H_

#include <stdint.h>

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

			if(     outcome ==  0)   { phi = value; delta = value; } //unknown
			else if(outcome ==  2)   { phi = LOSS;  delta = 0;     } //win
			else if(outcome == -2)   { phi = 0;     delta = LOSS; }  //loss
			else /*(outcome 1||-1)*/ { phi = 0;     delta = DRAW; }  //draw
			return *this;
		}

		PNSNode & outcome(int outcome, int toplay, int assign, int value = 1){
			if(assign && outcome == 0)
				outcome = assign;

			if(     outcome == -3)       { phi = value; delta = value; }
			else if(outcome ==   toplay) { phi = LOSS;  delta = 0;     }
			else if(outcome == 3-toplay) { phi = 0;     delta = LOSS; }
			else /*(outcome == 0)*/      { phi = 0;     delta = DRAW; }
			return *this;
		}

		bool terminal(){ return (phi == 0 || delta == 0); }
	};

	PNSNode root;
	PNSNode * TT;
	uint64_t maxnodes, memlimit;

	int   ab; // how deep of an alpha-beta search to run at each leaf node
	bool  df; // go depth first?
	float epsilon; //if depth first, how wide should the threshold be?
	int   ties;    //which player to assign ties to: 0 handle ties, 1 assign p1, 2 assign p2
	int   copyproof; //how many siblings to try to copy a proof to


	SolverPNSTT() {
		ab = 2;
		df = true;
		epsilon = 0.25;
		ties = 0;
		copyproof = 0;

		TT = NULL;
		reset();

		set_memlimit(100*1024*1024);
	}

	~SolverPNSTT(){
		if(TT){
			delete[] TT;
			TT = NULL;
		}
	}

	void reset(){
		outcome = -3;
		maxdepth = 0;
		nodes_seen = 0;
		bestmove = Move(M_UNKNOWN);

		timeout = false;

		root = PNSNode(rootboard.gethash(), 1);
	}

	void set_board(const Board & board, bool clear = true){
		rootboard = board;
		rootboard.setswap(false);
		reset();
		if(clear)
			clear_mem();
	}
	void move(const Move & m){
		rootboard.move(m, true, true);
		reset();
	}
	void set_memlimit(uint64_t lim){
		memlimit = lim;
		maxnodes = memlimit/sizeof(PNSNode);
		clear_mem();
	}

	void clear_mem(){
		reset();
		if(TT){
			delete[] TT;
			TT = NULL;
		}
	}

	void solve(double time);

//basic proof number search building a tree
	void run_pns();
	void pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td);

	void copy_proof(const Board & source, const Board & dest, Move smove, Move dmove);

//update the phi and delta for the node
	bool updatePDnum(const Board & board, PNSNode * node = NULL);

	PNSNode * tt(const Board & board);
	PNSNode * tt(const Board & board, Move move);
};

#endif

