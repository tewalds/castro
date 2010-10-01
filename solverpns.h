
#ifndef _SOLVERPNS_H_
#define _SOLVERPNS_H_

#include <stdint.h>
#include "time.h"
#include "timer.h"

#include "board.h"
#include "move.h"

#include "solver.h"


class SolverPNS : public Solver {
	static const uint32_t LOSS  = (1<<30)-1;
	static const uint32_t DRAW  = (1<<30)-2;
	static const uint32_t INF32 = (1<<30)-3;
public:

	struct PNSNode {
		uint32_t phi, delta;
		Move move;
		uint16_t numchildren;
		PNSNode * children;

		PNSNode() { }
		PNSNode(int x, int y,   int v = 1)     : phi(v), delta(v), move(Move(x,y)), numchildren(0), children(NULL) { }
		PNSNode(const Move & m, int v = 1)     : phi(v), delta(v), move(m),         numchildren(0), children(NULL) { }
		PNSNode(int x, int y,   int p, int d)  : phi(p), delta(d), move(Move(x,y)), numchildren(0), children(NULL) { }
		PNSNode(const Move & m, int p, int d)  : phi(p), delta(d), move(m),         numchildren(0), children(NULL) { }

		~PNSNode(){
			if(children)
				delete[] children;
			children = NULL;
			numchildren = 0;
		}
		PNSNode & abval(int outcome, int toplay, int assign, int value = 1){
			if(assign && (outcome == 1 || outcome == -1))
				outcome = (toplay == assign ? 2 : -2);

			if(     outcome ==  0)   { phi = value; delta = value; }
			else if(outcome ==  2)   { phi = LOSS;  delta = 0;     }
			else if(outcome == -2)   { phi = 0;     delta = LOSS; }
			else /*(outcome 1||-1)*/ { phi = 0;     delta = DRAW; }
			return *this;
		}
		
		int alloc(int num){
			numchildren = num;
			children = new PNSNode[num];
			return num;
		}
		int dealloc(){
			int s = numchildren;
			if(numchildren){
				for(int i = 0; i < numchildren; i++)
					s += children[i].dealloc();
				numchildren = 0;
				delete[] children;
				children = NULL;
			}
			return s;
		}
	};


//memory management for PNS which uses a tree to store the nodes
	uint64_t nodes, maxnodes, memlimit;
	int assignties; //which player to assign a tie to

	int   ab; // how deep of an alpha-beta search to run at each leaf node
	bool  df; // go depth first?
	float epsilon; //if depth first, how wide should the threshold be?
	int   ties;    //0 handle ties, 1 assign p1, 2 assign p2, 3 do two searches, once assigned to each

	bool timeout;

	PNSNode * root;

	SolverPNS(int AB = 0, bool DF = true, float eps = 0.25) {
		ab = AB;
		df = DF;
		epsilon = eps;
		ties = 0;

		set_memlimit(1000);

		root = NULL;
		reset();
	}
	~SolverPNS(){
		if(root)
			delete root;
		root = NULL;
	}
	void reset(){
		outcome = -3;
		maxdepth = 0;
		nodes_seen = 0;
		bestmove = Move(M_UNKNOWN);

		nodes = 0;

		timeout = false;
		if(root){
			delete root;
			root = NULL;
		}
	}

	void set_board(const Board & board){
		rootboard = board;
		reset();
	}
	void move(const Move & m){
		rootboard.move(m, true, true);
		reset();
	}

	void set_memlimit(uint64_t lim){
		memlimit = lim;
		maxnodes = memlimit*1024*1024/sizeof(PNSNode);
	}

	void solve(double time);

//basic proof number search building a tree
	int run_pns(); //-3 = unknown, 0 = tie, 1 = p1, 2 = p2
	bool pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td);

//update the phi and delta for the node
	bool updatePDnum(PNSNode * node);

//remove all the leaf nodes to free up some memory
	bool garbage_collect(PNSNode * node);
};

#endif

