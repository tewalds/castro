
#ifndef _SOLVER_H_
#define _SOLVER_H_

#include <stdint.h>
#include "time.h"
#include "timer.h"

#include "board.h"


class Solver {
	static const unsigned int INF16 = (1<<15)-1;
	static const unsigned int INF32 = (1<<30)-1;

//memory management for PNS which uses a tree to store the nodes
	int nodesremain;
	int assignties; //which player to assign a tie to

	struct PNSNode {
		uint8_t x, y; //move
		uint16_t phi, delta;
		uint16_t numchildren;
		PNSNode * children;

		PNSNode() { }
		PNSNode(int X, int Y, int v = 0)     : x(X), y(Y), phi(v), delta(v), numchildren(0), children(NULL) { }
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
		PNSNode & abval(int outcome, bool ties, int value = 1){
			if(outcome == 1 || outcome == -1)
				outcome == (ties ? 2 : -2);

			if(     outcome ==  0)   { phi = value; delta = value; }
			else if(outcome ==  2)   { phi = INF16; delta = 0;     }
			else /*(outcome == -2)*/ { phi = 0;     delta = INF16; }
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


public:
	int outcome; // 0 = tie, 1 = white, 2 = black, -1 = white or tie, -2 = black or tie, anything else unknown
	int maxdepth;
	uint64_t nodes;
	int X, Y;
	bool timeout;

	Solver() {
		outcome = -3;
		maxdepth = 0;
		nodes = 0;
		X = Y = -1;
		timeout = false;
	}
	void timedout(){ timeout = true; }

	void solve_ab(const Board & board, double time, int mdepth = 1000);
	void solve_pns(const Board & board, double time, uint64_t memlimit);
	void solve_pnsab(const Board & board, double time, uint64_t memlimit);

protected:

//used for alpha-beta solvers
//return -2 for loss, -1,1 for tie, 0 for unknown, 2 for win, all from toplay's perspective
	int run_negamax(const Board & board, const int depth, int alpha, int beta);  //plain negamax
	int negamax(const Board & board, const int depth, int alpha, int beta);  //plain negamax
	int negamaxh(const Board & board, const int depth, int alpha, int beta); //negamax with move ordering heuristic

//basic proof number search building a tree
	int run_pns(const Board & board, int ties); //1 = win, 0 = unknown, -1 = loss
	int run_pnsab(const Board & board, int ties); //1 = win, 0 = unknown, -1 = loss

	bool pns(const Board & board, PNSNode * node, int depth);   //basic proof number search
	bool pnsab(const Board & board, PNSNode * node, int depth); //use a tiny negamax search as a pn,dn heuristic

};

#endif

