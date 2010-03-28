
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
			move = n->move;

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
		float value(float ravefactor, float fpurgency){
			if(ravefactor <= min_rave)
				return (visits == 0 ? fpurgency : score/visits);

			if(ravevisits == 0 && visits == 0)
				return fpurgency;

			float alpha = ravefactor/(ravefactor + visits);
//			float alpha = sqrt(ravefactor/(ravefactor + 3*visits));
//			float alpha = (float)ravevisits/((float)visits + (float)ravevisits + 4.0*visits*ravevisits*ravefactor);

			float val = 0;
			if(ravevisits) val += alpha*rave/ravevisits;
			if(visits)     val += (1-alpha)*score/visits;

			return val;
		}
/*/
		//my understanding of how fuego does it
		float value(float ravefactor, float fpurgency){
			float val = 0;
			float weight = 0;
			if(visits) {
				val += score;
				weight += visits;
			}
			if(ravevisits){
				float bias = 1.0/(1.1 + ravevisits/20000.0);
				val += rave*bias;
				weight += ravevisits*bias;
			}
			if(weight > 0)
				return val / weight;
			else
				return fpurgency;
		}

		//based directly on fuego
		float value(float ravefactor, float fpurgency){
			float val = 0.f;
			float weightSum = 0.f;
			bool hasValue = false;
			if(visits){
				val += score;
				weightSum += visits;
				hasValue = true;
			}
			if(ravevisits){
				float weight = ravevisits / (  1.1 + ravevisits/20000.);
				val += weight * rave / ravevisits;
				weightSum += weight;
				hasValue = true;
			}
			if(hasValue)
				return val / weightSum;
			else
				return fpurgency;
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
		unsigned int size() const {
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

	static const float min_rave = 0.1;

	float explore;    //greater than one favours exploration, smaller than one favours exploitation
	float ravefactor; //big numbers favour rave scores, small ignore it
	bool  ravescale;  //scale rave numbers from 2 down to 0 in decreasing order of move recency instead of always 1
	bool  opmoves;    //take the opponents rave updates too, a good move for my opponent is a good move for me.
	int   skiprave;   //how often to skip rave, skip once in this many checks
	float fpurgency;  //what value to return for a move that hasn't been played yet
	float prooftime;  //fraction of time spent in proof number search, looking for a provable win and losses to avoid
	int   proofscore; //how many virtual rollouts to assign based on the proof number search values
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

		explore = 0.85;
		ravefactor = 50;
		ravescale = false;
		opmoves = false;
		skiprave = 0;
		fpurgency = 1;
		prooftime = 0;
		proofscore = 0;
		rolloutpattern = false;
	}
	void timedout(){ timeout = true; }

	void play_uct(const Board & board, double time, int maxruns, int memlimit);

protected:
	int walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth);
	Node * choose_move(const Node * node) const;
	void update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay);
	int rand_game(Board & board, RaveMoveList & movelist, Move move, int depth);
	bool check_pattern(const Board & board, Move & move);
};

#endif

