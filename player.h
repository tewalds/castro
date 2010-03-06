
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
		uint32_t childravevisits, visits;
		Move move;
		uint16_t numchildren;
		Node * children;

		Node(const Move & m,       float s = 0, int v = 0) : rave(0), childravevisits(0), score(s), visits(v), move(m),         numchildren(0), children(NULL) { }
		Node(int x = 0, int y = 0, float s = 0, int v = 0) : rave(0), childravevisits(0), score(s), visits(v), move(Move(x,y)), numchildren(0), children(NULL) { }

		void construct(const Solver::PNSNode * n, int pnsscore){
			move.x = n->x;
			move.y = n->y;

			rave = 0;
			childravevisits = 0;

			if(n->delta == 0){ //a win!
				score  = 2000;
				visits = 1000;
			}else if(n->phi == 0){ //a loss or tie
				//set high but not insurmountable visits just in case it is a tie
				score = 0;
				visits = 100;
			}else{
				score = pnsscore*n->phi/n->delta;
				if(score > 2*pnsscore) score = 2*pnsscore;
				visits = pnsscore;
			}

			numchildren = n->numchildren;
			children = NULL;

			if(numchildren){
				children = new Node[numchildren];
				for(int i = 0; i < numchildren; i++)
					children[i].construct(& n->children[i], pnsscore);
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

		float ravescore(int parentravevisits){
			return rave/(visits + parentravevisits);
		}
	};

	struct RaveMoveList {
		vector<MoveScore> list;

		RaveMoveList(int s = 0){
			list.reserve(s);
		}

		void add(const Move & move){
			list.push_back(move);
		}
		void clear(){
			list.clear();
		}
		int size() const {
			return list.size();
		}
		const MoveScore & operator[](int i) const {
			return list[i];
		}
		//remove the moves that were played by the loser
		//sort in y,x order
		void clean(bool keepfirst, bool scale){
			unsigned int i = 0, j = 1;
			float base, factor;

			if(scale){
				base = 2;
				factor = 1.9/list.size(); //1.9 instead of 2.0 so it never goes below 0; 2 instead of 1 so the average stays at 1
			}else{
				base = 1;
				factor = 0;
			}

			if(keepfirst){
				list[0].score = base;
				i++;
				j++;
			}

			while(j < list.size()){
				list[i] = list[j];
				list[i].score = base - i*scale;
				i++;
				j += 2;
			}

			list.resize(i);
			sort(list.begin(), list.end());
		}
	};

public:

	float explore;    //greater than one favours exploration, smaller than one favours exploitation
	float ravefactor; //big numbers favour rave scores, small ignore it
	bool  ravescale;  //scale rave numbers from 2 down to 0 in decreasing order of move recency instead of always 1
	float prooftime;  //fraction of time spent in proof number search, looking for a provable win and losses to avoid
	int   proofscore;   //how many virtual rollouts to assign based on the proof number search values
	bool  rolloutpattern; //play the response to a virtual connection threat in rollouts

	unsigned int minvisitschildren; //number of visits to a node before expanding its children nodes
	unsigned int minvisitspriority; //give priority to this node if it has less than this many visits

	int runs;
	DepthStats treelen, gamelen;
	uint64_t nodes, maxnodes;
	Move bestmove;
	vector<Move> principle_variation;
	bool timeout;

	double time_used;

	Player() {
		time_used = 0;

		explore = 1;
		ravefactor = 10;
		ravescale = true;
		prooftime = 0.2;
		proofscore = 2;
		rolloutpattern = true;

		minvisitschildren = 1;
		minvisitspriority = 1;
	}
	void timedout(){ timeout = true; }

	void play_uct(const Board & board, double time, int memlimit);

protected:
	int walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth);
	int rand_game(Board & board, RaveMoveList & movelist, Move move, int depth);
	bool check_pattern(const Board & board, Move & move);
};

#endif

