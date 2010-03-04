
#ifndef __PLAYER_H_
#define __PLAYER_H_

#include <stdint.h>
#include <cmath>

#include "move.h"
#include "board.h"
#include "time.h"
#include "timer.h"
#include "depthstats.h"
#include "solver.h"

class Player {
	struct Node {
		float rave, score;
		uint32_t ravevisits, visits;
		Move move;
		uint16_t numchildren;
		Node * children;

		Node(const Move & m,       float s = 0, int v = 0) : rave(0), ravevisits(0), score(s), visits(v), move(m),         numchildren(0), children(NULL) { }
		Node(int x = 0, int y = 0, float s = 0, int v = 0) : rave(0), ravevisits(0), score(s), visits(v), move(Move(x,y)), numchildren(0), children(NULL) { }

		void construct(const Solver::PNSNode * n){
			move.x = n->x;
			move.y = n->y;

			rave = 0;
			ravevisits = 0;

			if(n->delta == 0){ //a win!
				score  = 2000;
				visits = 1000;
			}else if(n->phi == 0){ //a loss or tie
				//set high but not insurmountable visits just in case it is a tie
				score = 0;
				visits = 100;
			}else{
				score = 2*n->phi/n->delta;
				if(score > 4) score = 4;
				visits = 2;
			}

			numchildren = n->numchildren;
			children = NULL;

			if(numchildren){
				children = new Node[numchildren];
				for(int i = 0; i < numchildren; i++)
					children[i].construct(& n->children[i]);
			}
		}

		~Node(){
			if(children)
				delete[] children;
			numchildren = 0;
			children = NULL;
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

		float winrate(){
			return score/visits;
		}

		float ravescore(int parentvisits){
			return rave/(visits + parentvisits);
		}
	};

public:

	float explore;    //greater than one favours exploration, smaller than one favours exploitation
	float ravefactor; //big numbers favour rave scores, small ignore it

	unsigned int minvisitschildren; //number of visits to a node before expanding its children nodes
	unsigned int minvisitspriority; //give priority to this node if it has less than this many visits

	int runs;
	DepthStats treelen, gamelen;
	uint64_t nodes, maxnodes;
	Move bestmove;
	bool timeout;

	double time_used;

	Player() {
		time_used = 0;

		explore = 1;
		ravefactor = 10;

		minvisitschildren = 1;
		minvisitspriority = 1;
	}
	void timedout(){ timeout = true; }

	void play_uct(const Board & board, double time, int memlimit);

protected:
	int walk_tree(Board & board, Node * node, vector<Move> & movelist, int depth);
	int rand_game(Board & board, vector<Move> & movelist, Move move, int depth);
	bool check_pattern(const Board & board, Move & move);
};

#endif

