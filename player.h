
#ifndef __PLAYER_H_
#define __PLAYER_H_

#include <stdint.h>
#include <cmath>
#include <cassert>

#include "move.h"
#include "board.h"
#include "time.h"
#include "timer.h"
#include "depthstats.h"
#include "solver.h"

class Player {
public:
//*
	class ExpPair {
		float s;
		uint32_t n;
	public:
		ExpPair() : s(0), n(0) { }
		ExpPair(float S, uint32_t N) : s(S), n(N) { }
		float avg() const { return s/n; }
		float sum() const { return s; }
		uint32_t num() const { return n; }
		void add(float val, int num){
			s += val;
			n += num;
		}
		ExpPair & operator+=(const ExpPair & a){
			s += a.s;
			n += a.n;
			return *this;
		}
		ExpPair & operator+=(float nv){
			s += nv;
			n++;
			return *this;
		}
		ExpPair & operator*=(int m){
			s *= m;
			n *= m;
			return *this;
		}
	};
/*/
	class ExpPair {
		float v;
		uint32_t n;
	public:
		ExpPair() : v(0), n(0) { }
		ExpPair(float S, uint32_t N) : v(S/N), n(N) { }
		float avg() const { return v; }
		float sum() const { return v*n; }
		uint32_t num() const { return n; }
		ExpPair & operator+=(float nv){
			n++;
			v += (nv - v)/n;
//			v += (nv - v)/(n < 1000 ? n : 1000);
			return *this;
		}
		ExpPair & operator*=(int m){
			n *= m;
			return *this;
		}
	};
//*/

	struct Node {
		ExpPair rave;
		ExpPair exp;
		Move move;
		Move bestmove; //if outcome is set, then bestmove is the way to get there
		int16_t outcome;
		uint16_t numchildren;
		Node * children;
		//don't forget to update the copy constructor/operator

		Node()                            :          outcome(-1), numchildren(0), children(NULL) { }
		Node(const Move & m, char o = -1) : move(m), outcome(o),  numchildren(0), children(NULL) { }
		Node(const Node & n) { *this = n; }
		Node & operator = (const Node & n){
			if(this != & n){
				//don't copy to a node that already has children
				assert(numchildren == 0 && children == NULL);

				rave = n.rave;
				exp  = n.exp;
				move = n.move;
				outcome = n.outcome;
				bestmove = n.bestmove;
			}
			return *this;
		}

		void swap_tree(Node & n){
			Node * temp = children;
			uint16_t tempnum = numchildren;

			children = n.children;
			numchildren = n.numchildren;

			n.children = temp;
			n.numchildren = tempnum;
		}

		void neuter(){
			numchildren = 0;
			children = NULL;
		}

		void print() const {
			printf("%s\n", to_s().c_str());
		}
		string to_s() const {
			return "Node: exp " + to_str(exp.avg(), 2) + "/" + to_str(exp.num()) +
					", rave " + to_str(rave.avg(), 2) + "/" + to_str(rave.num()) +
					", move " + to_str(move.x) + "," + to_str(move.y) + ", " + to_str(numchildren);
		}

		int size() const {
			int s = numchildren;
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

			int num = numchildren;
			if(numchildren){
				children = new Node[numchildren];
				for(int i = 0; i < numchildren; i++)
					num += children[i].construct(& n->children[i], pnsscore);
			}
			return num;
		}

		~Node(){
			assert(children == NULL && numchildren == 0);
		}

		int alloc(int num){
			assert(numchildren == 0);
			assert(children == NULL);
			assert(num > 0);

			numchildren = num;
			children = new Node[num];

			assert(numchildren == num);

			return num;
		}
		int dealloc(){
			assert((children == NULL) == (numchildren == 0));

			int s = numchildren;
			if(numchildren){
				for(int i = 0; i < numchildren; i++)
					s += children[i].dealloc();
				delete[] children;
				neuter();
			}

			assert(children == NULL && numchildren == 0);

			return s;
		}

//*
		//new way, more standard way of changing over from rave scores to real scores
		float value(float ravefactor, float fpurgency){
			if(ravefactor <= min_rave)
				return (exp.num() == 0 ? fpurgency : exp.avg());

			if(rave.num() == 0 && exp.num() == 0)
				return fpurgency;

			float alpha = ravefactor/(ravefactor + exp.num());
//			float alpha = sqrt(ravefactor/(ravefactor + 3*exp.num()));
//			float alpha = (float)rave.num()/((float)exp.num() + (float)rave.num() + 4.0*exp.num()*rave.num()*ravefactor);

			float val = 0;
			if(rave.num()) val += alpha*rave.avg();
			if(exp.num())  val += (1-alpha)*exp.avg();

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

	float prooftime;  //fraction of time spent in proof number search, looking for a provable win and losses to avoid
//tree traversal
	float explore;    //greater than one favours exploration, smaller than one favours exploitation
	float ravefactor; //big numbers favour rave scores, small ignore it
	bool  ravescale;  //scale rave numbers from 2 down to 0 in decreasing order of move recency instead of always 1
	bool  opmoves;    //take the opponents rave updates too, a good move for my opponent is a good move for me.
	int   skiprave;   //how often to skip rave, skip once in this many checks
	bool  keeptree;   //reuse the tree from the previous move
	bool  minimax;    //solve the minimax tree within the uct tree
	float fpurgency;  //what value to return for a move that hasn't been played yet
//knowledge
	int   proofscore; //how many virtual rollouts to assign based on the proof number search values
//rollout
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
		minimax = true;
		fpurgency = 1;
		prooftime = 0;
		proofscore = 0;
		rolloutpattern = false;
	}
	~Player(){ root.dealloc(); }
	void timedout(){ timeout = true; }

	void set_board(const Board & board){
		rootboard = board;
		nodes -= root.dealloc();
		root = Node();
	}
	void move(const Move & m){
		rootboard.move(m);

		uint64_t nodesbefore = nodes;

		if(keeptree){
			Node child;

			for(int i = 0; i < root.numchildren; i++){
				if(root.children[i].move == m){
					child = root.children[i];          //copy the child experience to temp
					child.swap_tree(root.children[i]); //move the child tree to temp
					break;
				}
			}

			nodes -= root.dealloc();
			root = child;
			root.swap_tree(child);

			if(nodesbefore > 0)
				fprintf(stderr, "Nodes before: %llu, after: %llu, saved %.1f%% of the tree\n", nodesbefore, nodes, 100.0*nodes/nodesbefore);
		}else{
			nodes -= root.dealloc();
			root = Node();
		}
		assert(nodes == root.size());
	}

	Move mcts(double time, int maxruns, int memlimit);
	vector<Move> get_pv();

protected:
	int walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth);
	Node * choose_move(const Node * node, int toplay) const;
	void update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay);
	int rand_game(Board & board, RaveMoveList & movelist, Move move, int depth);
	bool check_pattern(const Board & board, Move & move);
};

#endif

