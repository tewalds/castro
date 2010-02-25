
#ifndef __PLAYER_H_
#define __PLAYER_H_

#include "board.h"
#include "time.h"
#include "timer.h"
#include <stdint.h>

class Player {
	struct Node {
		uint32_t visits, score;
		uint8_t x, y;
		uint16_t numchildren;
		Node * children;

		Node(int X = 0, int Y = 0) : visits(0), score(0), x(X), y(Y), numchildren(0), children(NULL) { }
	
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
	};

	float explore;  //greater than one favours exploration, smaller than one favours exploitation
	
	int nodesremain;
	
	int minvisitschildren; //number of visits to a node before expanding its children nodes
	int minvisitspriority; //give priority to this node if it has less than this many visits

public:
	int runs;
	int conflicts;
	uint64_t nodes;
	int X, Y;
	bool timeout;

	double time_used;

	Player() {
		runs = 0;
		conflicts = 0;
		nodes = 0;
		X = Y = -1;
		timeout = false;
		
		explore = 1;
		minvisitschildren = 1;
		minvisitspriority = 1;
	}
	void timedout(){ timeout = true; }

	void play_uct(const Board & board, double time, uint64_t memlimit);

protected:
	int walk_tree(Board & board, Node * node);
	int rand_game(Board & board);


};

#endif

