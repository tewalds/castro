
#ifndef _SOLVERPNS_H_
#define _SOLVERPNS_H_

#include <stdint.h>
#include "time.h"
#include "timer.h"

#include "board.h"
#include "move.h"

#include "solver.h"


class SolverPNS : public Solver {
	static const unsigned int INF32 = (1<<30)-1;
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
		PNSNode & abval(int outcome, bool ties, int value = 1){
			if(outcome == 1 || outcome == -1)
				outcome = (ties ? 2 : -2);

			if(     outcome ==  0)   { phi = value; delta = value; }
			else if(outcome ==  2)   { phi = INF32; delta = 0;     }
			else /*(outcome == -2)*/ { phi = 0;     delta = INF32; }
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
	uint64_t nodes, maxnodes;
	int assignties; //which player to assign a tie to
	static const float epsilon = 0.25;

	bool timeout;

	PNSNode * root;

	SolverPNS() {
		root = NULL;
		reset();
	}
	~SolverPNS(){
		if(root)
			delete root;
	}
	void reset(){
		outcome = -3;
		maxdepth = 0;
		nodes_seen = 0;
		bestmove = Move(M_UNKNOWN);

		timeout = false;
		nodes = 0;
		maxnodes = 0;

		if(root){
			delete root;
			root = NULL;
		}
	}
	void timedout(){ timeout = true; }

	void solve_pns    (Board board, double time, uint64_t memlimit);
	void solve_pnsab  (Board board, double time, uint64_t memlimit);
	void solve_dfpnsab(Board board, double time, uint64_t memlimit);


//basic proof number search building a tree
	int run_pns    (const Board & board, int ties, uint64_t memlimit); //1 = win, 0 = unknown, -1 = loss
	int run_pnsab  (const Board & board, int ties, uint64_t memlimit); //1 = win, 0 = unknown, -1 = loss
	int run_dfpnsab(const Board & board, int ties, uint64_t memlimit); //1 = win, 0 = unknown, -1 = loss

	bool pns    (const Board & board, PNSNode * node, int depth);   //basic proof number search
	bool pnsab  (const Board & board, PNSNode * node, int depth); //use a tiny negamax search as a pn,dn heuristic
	bool dfpnsab(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td);

//update the phi and delta for the node
	bool updatePDnum(PNSNode * node);

//remove all the leaf nodes to free up some memory
	bool garbage_collect(PNSNode * node);
};

#endif

