
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
public:
	struct ExpPair {
		float sum;
		uint32_t num;
		ExpPair() : sum(0), num(0) { }
		ExpPair(float s, uint32_t n) : sum(s), num(n) { }
		float avg() const { return sum/num; }
		ExpPair & operator+=(float a){
			sum += a;
			num++;
			return *this;
		}
		ExpPair & operator*=(int a){
			sum *= a;
			num *= a;
			return *this;
		}
	};

	struct Node {
		ExpPair rave;
		ExpPair exp;
		Move move;
		uint16_t numchildren;
		Node * children;

		Node()               :          numchildren(0), children(NULL) { }
		Node(const Move & m) : move(m), numchildren(0), children(NULL) { }

		void neuter(){
			numchildren = 0;
			children = NULL;
		}

		void print() const {
			printf("Node: exp %.2f/%i, rave %.2f/%i, move %i,%i, %i children\n", exp.avg(), exp.num, rave.avg(), rave.num, move.x, move.y, numchildren);
		}

		int size() const {
			int s = 1;
			for(int i = 0; i < numchildren; i++)
				s += children[i].size();
			return s;
		}

		int construct(const Solver::PNSNode * n, int pnsscore){
			move = n->move;

			if(n->delta == 0){ //a win!
				exp = ExpPair(1000, 1000);
			}else if(n->phi == 0){ //a loss or tie
				//set high but not insurmountable visits just in case it is a tie
				exp = ExpPair(0, 100);
			}else if(pnsscore > 0){
				if(n->phi >= n->delta)
					exp = (ExpPair((1 - n->delta/(2*n->phi)), 1) *= pnsscore);
				else
					exp = (ExpPair((n->phi/(2*n->delta)), 1) *= pnsscore);
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
			neuter();
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
				delete[] children;
				neuter();
			}
			return s;
		}

		//need to return a pointer to a new object due to extra copies and destructor calls made during function return... need a move constructor...
		Node * make_move(Move m){
			for(int i = 0; i < numchildren; i++){
				if(children[i].move == m){
					Node * ret = new Node(children[i]); //move the child
					children[i].neuter();
					return ret;
				}
			}
			return new Node();
		}

//*
		//new way, more standard way of changing over from rave scores to real scores
		float value(float ravefactor, float fpurgency){
			if(ravefactor <= min_rave)
				return (exp.num == 0 ? fpurgency : exp.avg());

			if(rave.num == 0 && exp.num == 0)
				return fpurgency;

			float alpha = ravefactor/(ravefactor + exp.num);
//			float alpha = sqrt(ravefactor/(ravefactor + 3*exp.num));
//			float alpha = (float)rave.num/((float)exp.num + (float)rave.num + 4.0*exp.num*rave.num*ravefactor);

			float val = 0;
			if(rave.num) val += alpha*rave.avg();
			if(exp.num)  val += (1-alpha)*exp.avg();

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
	bool  keeptree;   //reuse the tree from the previous move
	float fpurgency;  //what value to return for a move that hasn't been played yet
	float prooftime;  //fraction of time spent in proof number search, looking for a provable win and losses to avoid
	int   proofscore; //how many virtual rollouts to assign based on the proof number search values
	bool  rolloutpattern; //play the response to a virtual connection threat in rollouts

	Node root;
	Board rootboard;

	int runs;
	DepthStats treelen, gamelen;
	uint64_t nodes, maxnodes;
	bool timeout;

	double time_used;

	Player() {
		nodes = 0;
		time_used = 0;

		explore = 0.85;
		ravefactor = 50;
		ravescale = false;
		opmoves = false;
		skiprave = 0;
		keeptree = true;
		fpurgency = 1;
		prooftime = 0;
		proofscore = 0;
		rolloutpattern = false;
	}
	void timedout(){ timeout = true; }

	void set_board(const Board & board){
		rootboard = board;
		nodes -= root.dealloc();
		root = Node();
	}
	void move(const Move & m){
		rootboard.move(m);
		if(keeptree){
			Node * child = root.make_move(m);
			nodes -= root.dealloc();
			root = *child;
			child->neuter();
			delete child;
		}else{
			nodes -= root.dealloc();
			root = Node();
		}
	}

	Move mcts(double time, int maxruns, int memlimit);
	vector<Move> get_pv();

protected:
	int walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth);
	Node * choose_move(const Node * node) const;
	void update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay);
	int rand_game(Board & board, RaveMoveList & movelist, Move move, int depth);
	bool check_pattern(const Board & board, Move & move);
};

#endif

