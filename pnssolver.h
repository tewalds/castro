
#ifndef _PNSSOLVER_H_
#define _PNSSOLVER_H_

#include <stdint.h>
#include "solver.h"

class PNSSolver : public Solver {
	struct Node {
		uint8_t x, y; //move
		uint16_t phi, delta;
		uint16_t numchildren;
		Node * children;
		
		Node() { }
		Node(int X, int Y, bool terminal) : x(X), y(Y), numchildren(0), children(NULL) {
			if(terminal){
				phi = INF;
				delta = 0;
			}else{
				phi = 1;
				delta = 1;
			}
		}
		~Node(){
			if(children)
				delete[] children;
		}
		int alloc(int num){
			numchildren = num;
			children = new Node[num];
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

	int nodesremain;
	static const int INF = (1<<15)-1;

public:
	PNSSolver(const Board & board, double time, uint64_t memlimit = 2*1024){
		outcome = -1;
		maxdepth = 0;
		nodes = 1;
		x = y = -1;
		
		nodesremain = memlimit*1024*1024/sizeof(Node);

		if(board.won() >= 0){
			outcome = board.won();
			return;
		}

		int starttime = time_msec();

		int turn = board.toplay();

		Node root(-1, -1, false);

		bool mem = true;
		while(mem && root.phi != 0 && root.delta != 0 && time_msec() - starttime < time*1000)
			mem = pns(board, & root, 0);

		if(!mem)
			fprintf(stderr, "Ran out of memory\n");

		if(root.delta == 0)    outcome = (turn == 1 ? 2 : 1);
		else if(root.phi == 0) outcome = turn;

		fprintf(stderr, "Finished in %d msec\n", time_msec() - starttime);

	}
	
protected:

	bool pns(const Board & board, Node * node, int depth){
		if(depth > maxdepth)
			maxdepth = depth;

		if(node->numchildren == 0){
			if(nodesremain <= 0)
				return false;
		
			nodesremain -= node->alloc(board.movesremain());
			nodes += board.movesremain();

			int min = INF;
			int sum = 0;
			int i = 0;
			for(int y = 0; y < board.get_size_d(); y++){
				for(int x = 0; x < board.get_size_d(); x++){
					if(board.valid_move(x, y)){
						Board next = board;
						next.move(x, y);
						node->children[i] = Node(x, y, next.won() >= 0);

						sum += node->children[i].phi;
						if(node->children[i].delta < min)
							min = node->children[i].delta;

						i++;
					}
				}
			}
			if(sum > INF)
				sum = INF;

			node->phi = min;
			node->delta = sum;

			if(node->phi == 0 || node->delta == 0)
				nodesremain += node->dealloc();

			return true;
		}
		
		int min, sum;
		bool mem;
		do{
			int i = 0;
			for(; i < node->numchildren; i++)
				if(node->children[i].delta == node->phi)
					break;
			Node * child = &(node->children[i]);

			Board next = board;
			next.move(child->x, child->y);
			mem = pns(next, child, depth+1);

			min = INF;
			sum = 0;
			for(int i = 0; i < node->numchildren; i++){
				sum += node->children[i].phi;
				if(node->children[i].delta < min)
					min = node->children[i].delta;
			}
			if(sum > INF)
				sum = INF;
		}while(mem && min == node->phi && sum == node->delta);

		node->phi = min;
		node->delta = sum;

		if(node->phi == 0 || node->delta == 0)
			nodesremain += node->dealloc();

		return mem;
	}
};

#endif

