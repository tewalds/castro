
#ifndef _SOLVER_H_
#define _SOLVER_H_

#include <stdint.h>
#include "time.h"

#include "board.h"


class Solver {
	static const unsigned int INF16 = (1<<15)-1;
	static const unsigned int INF32 = (1<<30)-1;

//memory management for PNS which uses a tree to store the nodes
	int nodesremain;

//depth to run the alpha-beta search at each pns node
	int pnsab_depth;

	struct PNSNode {
		uint8_t x, y; //move
		uint16_t phi, delta;
		uint16_t numchildren;
		PNSNode * children;

		PNSNode() { }
		PNSNode(int X, int Y, int v)         : x(X), y(Y), phi(v), delta(v), numchildren(0), children(NULL) { }
		PNSNode(int X, int Y, int p, int d)  : x(X), y(Y), phi(p), delta(d), numchildren(0), children(NULL) { }
		PNSNode(int X, int Y, bool terminal) : x(X), y(Y), numchildren(0), children(NULL) {
			if(terminal){
				phi = INF16;
				delta = 0;
			}else{
				phi = 1;
				delta = 1;
			}
		}

		~PNSNode(){
			if(children)
				delete[] children;
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

	void solve_ab(const Board & board, double time, int mdepth = 1000);
	void solve_pns(const Board & board, double time, uint64_t memlimit);
	void solve_pnsab(const Board & board, double time, uint64_t memlimit);

protected:
//used for alpha-beta solvers
	int negamax(const Board & board, const int depth, int alpha, int beta);  //plain negamax
	int negamaxh(const Board & board, const int depth, int alpha, int beta); //negamax with move ordering heuristic

//basic proof number search building a tree
	bool pns(const Board & board, PNSNode * node, int depth);
	bool pnsab(const Board & board, PNSNode * node, int depth);

};

#endif

