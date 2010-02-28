
#ifndef __PLAYER_H_
#define __PLAYER_H_

#include "move.h"
#include "board.h"
#include "time.h"
#include "timer.h"
#include "depthstats.h"
#include <stdint.h>
#include <cmath>

class Player {
	struct Node {
		uint32_t visits, score, rave;
		Move move;
		uint16_t numchildren;
		Node * children;

		Node(const Move & m)       : visits(0), score(0), rave(0), move(m),         numchildren(0), children(NULL) { }
		Node(int x = 0, int y = 0) : visits(0), score(0), rave(0), move(Move(x,y)), numchildren(0), children(NULL) { }
	
		~Node(){
			if(numchildren)
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

		float winrate(){
			return (float)score/visits;
		}

		float ravescore(int parentvisits){
			return (float)rave/(visits*visits + parentvisits);
		}
	};

public:

	float explore;    //greater than one favours exploration, smaller than one favours exploitation
	float ravefactor; //big numbers favour rave scores, small ignore it

	int minvisitschildren; //number of visits to a node before expanding its children nodes
	int minvisitspriority; //give priority to this node if it has less than this many visits

	int runs;
	DepthStats treelen, gamelen;
	uint64_t nodes, maxnodes;
	Move bestmove;
	bool timeout;

	double time_used;

	Player() {
		time_used = 0;
		
		explore = 3;
		ravefactor = 1;

		minvisitschildren = 1;
		minvisitspriority = 1;
	}
	void timedout(){ timeout = true; }

	void play_uct(const Board & board, double time, uint64_t memlimit);

protected:
	int walk_tree(Board & board, Node * node, vector<Move> & movelist, int depth);
	int rand_game(Board & board, vector<Move> & movelist, int depth);
};

#endif

