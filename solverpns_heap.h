
#ifndef _SOLVERPNS_HEAP_H_
#define _SOLVERPNS_HEAP_H_

#include <stdint.h>
#include "time.h"
#include "timer.h"

#include "board.h"
#include "move.h"

#include "solver.h"


class SolverPNSHeap : public Solver {
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
		void build_heap(){ //turn the array into a heap
			for(int i = numchildren/2; i >= 0; i--)
				heapify(i);
		}
		void heapify(int i = 0){ //move the root down to maintain a heap
			while(1){
				int l = i*2+1,
				    r = i*2+2,
				    min = i;

				if(l < numchildren && children[l].delta < children[min].delta) min = l;
				if(r < numchildren && children[r].delta < children[min].delta) min = r;

				if(min == i)
					break;
				
				PNSNode temp = children[i];
				children[i] = children[min];
				children[min] = temp;
				temp.neuter();

				i = min;
			}
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
		void neuter(){
			numchildren = 0;
			children = NULL;
		}
	};


//memory management for PNS which uses a tree to store the nodes
	uint64_t nodes, maxnodes;
	int assignties; //which player to assign a tie to

	int   ab; // how deep of an alpha-beta search to run at each leaf node
	bool  df; // go depth first?
	float epsilon; //if depth first, how wide should the threshold be?

	bool timeout;

	PNSNode * root;

	SolverPNSHeap(int AB = 0, bool DF = true, float eps = 0.25) {
		ab = AB;
		df = DF;
		epsilon = eps;

		root = NULL;
		reset();
	}
	~SolverPNSHeap(){
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

	void solve(Board board, double time, uint64_t memlimit);

//basic proof number search building a tree
	int run_pns(const Board & board, int ties, uint64_t memlimit); //1 = win, 0 = unknown, -1 = loss
	bool pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td);

//remove all the leaf nodes to free up some memory
	bool garbage_collect(PNSNode * node);
};

#endif

