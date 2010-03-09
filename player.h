
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

		Node(const Move & m,       float s = 0, int v = 0) : rave(0), score(s), ravevisits(0), visits(v), move(m),         numchildren(0), children(NULL) { }
		Node(int x = 0, int y = 0, float s = 0, int v = 0) : rave(0), score(s), ravevisits(0), visits(v), move(Move(x,y)), numchildren(0), children(NULL) { }

		int construct(const Solver::PNSNode * n, int pnsscore){
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
			}else if(pnsscore > 0){
				if(n->phi >= n->delta)
					score = pnsscore*(1 - n->delta/(2*n->phi));
				else
					score = pnsscore*(n->phi/(2*n->delta));

				visits = pnsscore;
			}else{
				score = 0;
				visits = 0;
			}

			numchildren = n->numchildren;
			children = NULL;

			int num = 1;
			if(numchildren){
				children = new Node[numchildren];
				for(int i = 0; i < numchildren; i++)
					num += children[i].construct(& n->children[i], pnsscore);
			}
			return num;
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
//*
		//new way, more standard way of changing over from rave scores to real scores
		float value(int ravefactor){
			if(visits == 0 && ravevisits == 0)
				return 10000 + rand()%100;

			float alpha = (ravefactor == 0 ? 0 : (float)ravefactor/(ravefactor + visits));

			float val = 0;
			if(ravevisits) val += alpha*rave/ravevisits;
			if(visits)     val += (1-alpha)*score/visits;

			return val;
		}
/*/
		//my understanding of how fuego does it
		float value(int ravefactor){
			float val = 0;
			float weight = 0;
			if(visits) {
				val += score;
				weight += visits;
			}
			if(ravevisits){
				val += rave;
				weight += ravevisits/(1.1 + ravevisits/20000);
			}
			if(weight > 0)
				return val / weight;
			else
				return 10000 + rand()%100; //first play urgency...
		}
//*/
	};

	struct RaveMoveList {
		struct RaveMove {
			Move move;
			char player;
			float score;

			RaveMove(const Move & m) : move(m), player(0), score(0) { }

			bool operator< (const RaveMove & b) const { return (move <  b.move); }
			bool operator<=(const RaveMove & b) const { return (move <= b.move); }
			bool operator> (const RaveMove & b) const { return (move >  b.move); }
			bool operator>=(const RaveMove & b) const { return (move >= b.move); }
			bool operator==(const RaveMove & b) const { return (move == b.move); }
			bool operator!=(const RaveMove & b) const { return (move != b.move); }
		};

		vector<RaveMove> list;

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
		const RaveMove & operator[](int i) const {
			return list[i];
		}
		//remove the moves that were played by the loser
		//sort in y,x order
		void clean(int player, bool scale){
			float base, factor;

			if(scale){
				base = 2; //2 instead of 1 so the average of wins stays at 1
				factor = 2*2.0/(list.size()+1); //+1 to keep it from going negative, 4 = base*2 since half the values are skipped
			}else{
				base = 1;
				factor = 0;
			}

			//the wins get values, the losses stay at default=0
			for(unsigned int i = 0; i < list.size(); i++){
				list[i].player = player;
				list[i].score = base - i/2*factor;
				player = 3 - player;
			}

			sort(list.begin(), list.end()); //sort in y,x order
		}
	};

public:
	float explore;    //greater than one favours exploration, smaller than one favours exploitation
	int   ravefactor; //big numbers favour rave scores, small ignore it
	bool  ravescale;  //scale rave numbers from 2 down to 0 in decreasing order of move recency instead of always 1
	bool  raveall;    //add rave value for win, 0 for loss, 0.5 for not used
	bool  opmoves;    //take the opponents rave updates too, a good move for my opponent is a good move for me.
	int   skiprave;   //how often to skip rave
	float prooftime;  //fraction of time spent in proof number search, looking for a provable win and losses to avoid
	int   proofscore;   //how many virtual rollouts to assign based on the proof number search values
	bool  rolloutpattern; //play the response to a virtual connection threat in rollouts

	int cur_player;
	int runs;
	DepthStats treelen, gamelen;
	uint64_t nodes, maxnodes;
	Move bestmove;
	vector<Move> principle_variation;
	bool timeout;

	double time_used;

	Player() {
		time_used = 0;

		explore = 10;
		ravefactor = 10;
		ravescale = false;
		raveall = false;
		opmoves = false;
		skiprave = 20;
		prooftime = 0.8;
		proofscore = 2;
		rolloutpattern = true;
	}
	void timedout(){ timeout = true; }

	void play_uct(const Board & board, double time, int memlimit);

protected:
	int walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth);
	int rand_game(Board & board, RaveMoveList & movelist, Move move, int depth);
	bool check_pattern(const Board & board, Move & move);
};

#endif

