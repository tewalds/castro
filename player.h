
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

typedef unsigned int uint;

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
		void addwins(int num){ add(num, num); }
		void addlosses(int num){ add(0, num); }
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
		ExpPair operator + (const ExpPair & a){
			return ExpPair(s + a.s, n + a.n);
		}
		ExpPair & operator*=(int m){
			s *= m;
			n *= m;
			return *this;
		}
	};
/*/
	class ExpPair {
		static const float k = 10000;
		float v;
		uint32_t n;
	public:
		ExpPair() : v(0), n(0) { }
		ExpPair(float S, uint32_t N) : v(S/N), n(N) { }
		float avg() const { return v; }
		float sum() const { return v*n; }
		uint32_t num() const { return n; }
		void addwins(int num){ add(num, num); }
		void addlosses(int num){ add(0, num); }
		void add(float val, int num){
			*this += ExpPair(val, num);
		}
		ExpPair & operator+=(const ExpPair & a){
			//there's got to be a better way...
			for(int i = 0; i < a.n; i++)
				*this += a.v;
			return *this;
		}
		ExpPair & operator+=(float nv){
			n++;
//			v += (nv - v)/n;
			v += (nv - v)/(n < k ? n : k);
			return *this;
		}
		ExpPair operator + (const ExpPair & a){
			ExpPair ret = *this;
			ret += a;
			return ret;
		}
		ExpPair & operator*=(int m){
			n *= m;
			return *this;
		}
	};
//*/

	struct Node {
		class Children {
			uint32_t _num; //can be smaller, but will be padded anyway.
			Node *   _children;
		public:
			typedef Node * iterator;
			Children()      : _num(0), _children(NULL) { }
			Children(int n) : _num(0), _children(NULL) { alloc(n); }
			~Children() { assert_empty(); }

			void assert_consistent() const { assert((_num == 0) == (_children == NULL)); }
			void assert_empty()      const { assert((_num == 0) && (_children == NULL)); }

			unsigned int alloc(unsigned int n){
				assert(_num == 0);
				assert(_children == NULL);
				assert(n > 0);

				_num = n;
				_children = new Node[_num];

				return _num;
			}
			void neuter(){
				_children = 0;
				_num = 0;
			}
			unsigned int dealloc(){
				assert_consistent();

				int n = _num;
				if(_children){
					Node * temp = _children; //CAS!
					neuter();
					delete[] temp;
				}
				return n;
			}
			void swap(Children & other){
				Children temp = other;
				other = *this;
				*this = temp;
				temp.neuter(); //to avoid problems with the destructor of temp
			}
			unsigned int num() const {
				assert_consistent();
				return _num;
			}
			bool empty() const {
				return num() == 0;
			}
			Node & operator[](unsigned int offset){
				assert(_children);
				assert(offset >= 0 && offset < _num);
				return _children[offset];
			}
			Node * begin() const {
				assert_consistent();
				return _children;
			}
			Node * end() const {
				assert_consistent();
				return _children + _num;
			}
		};

	public:
		ExpPair rave;
		ExpPair exp;
		int16_t know;
		int16_t outcome;
		Move    move;
		Move    bestmove; //if outcome is set, then bestmove is the way to get there
		Children children;
		//seems to need padding to multiples of 8 bytes or it segfaults?
		//don't forget to update the copy constructor/operator

		Node()                            : know(0), outcome(-1)          { }
		Node(const Move & m, char o = -1) : know(0), outcome(o), move(m)  { }
		Node(const Node & n) { *this = n; }
		Node & operator = (const Node & n){
			if(this != & n){ //don't copy to self
				//don't copy to a node that already has children
				assert(children.empty());

				rave = n.rave;
				exp  = n.exp;
				know = n.know;
				move = n.move;
				bestmove = n.bestmove;
				outcome = n.outcome;
			}
			return *this;
		}

		void swap_tree(Node & n){
			children.swap(n.children);
		}

		void print() const {
			printf("%s\n", to_s().c_str());
		}
		string to_s() const {
			return "Node: exp " + to_str(exp.avg(), 2) + "/" + to_str(exp.num()) +
					", rave " + to_str(rave.avg(), 2) + "/" + to_str(rave.num()) +
					", move " + to_str(move.x) + "," + to_str(move.y) + ", " + to_str(children.num());
		}

		unsigned int size() const {
			unsigned int num = children.num();

			for(Node * i = children.begin(); i != children.end(); i++)
				num += i->size();

			return num;
		}

/*
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
*/
		~Node(){
			assert(children.empty());
		}

		unsigned int alloc(unsigned int num){
			return children.alloc(num);
		}
		unsigned int dealloc(){
			unsigned int num = 0;

			for(Node * i = children.begin(); i != children.end(); i++)
				num += i->dealloc();
			num += children.dealloc();

			return num;
		}
//*
		//new way, more standard way of changing over from rave scores to real scores
		float value(float ravefactor, float knowfactor, float fpurgency){
			float val = fpurgency;

			if(ravefactor <= min_rave){
				if(exp.num() > 0)
					val = exp.avg();
			}else if(rave.num() || exp.num()){
				float alpha = ravefactor/(ravefactor + exp.num());
//				float alpha = sqrt(ravefactor/(ravefactor + 3*exp.num()));
//				float alpha = (float)rave.num()/((float)exp.num() + (float)rave.num() + 4.0*exp.num()*rave.num()*ravefactor);

				val = 0;
				if(rave.num()) val += alpha*rave.avg();
				if(exp.num())  val += (1-alpha)*exp.avg();
			}

			if(know > 0)
				val += knowfactor * know / sqrt((float)(exp.num() + 1));

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
		struct RaveMove : public Move {
			char player;
			float score;

			RaveMove(const Move & m, char p = 0, float s = 1) : Move(m), player(p), score(s) { }
		};
		typedef vector<RaveMove>::const_iterator iterator;

		vector<RaveMove> list;

		RaveMoveList(int s = 0){
			list.reserve(s);
		}

		void add(const Move & move, char player){
			list.push_back(RaveMove(move, player, 1));
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
		iterator begin() const {
			return list.begin();
		}
		iterator end() const {
			return list.end();
		}

		//remove the moves that were played by the loser
		//sort in y,x order
		void clean(bool scale){
			if(scale){
				float base = 2; //2 instead of 1 so the average of wins stays at 1
				float factor = 2*base/(list.size()+1); //+1 to keep it from going negative, 4 = base*2 since half the values are skipped

				for(unsigned int i = 0; i < list.size(); i++)
					list[i].score = base - i/2*factor;
			}

			sort(list.begin(), list.end()); //sort in y,x order
		}
	};

public:

	static const float min_rave = 0.1;

	bool  defaults;   //use the default settings on board reset
	float prooftime;  //fraction of time spent in proof number search, looking for a provable win and losses to avoid
//tree traversal
	float explore;    //greater than one favours exploration, smaller than one favours exploitation
	float ravefactor; //big numbers favour rave scores, small ignore it
	float knowfactor; //weight to give to knowledge values
	bool  ravescale;  //scale rave numbers from 2 down to 0 in decreasing order of move recency instead of always 1
	bool  opmoves;    //take the opponents rave updates too, a good move for my opponent is a good move for me.
	int   skiprave;   //how often to skip rave, skip once in this many checks
	bool  shortrave;  //only update rave values on short rollouts
	bool  keeptree;   //reuse the tree from the previous move
	bool  minimaxtree;//keep the solved part of the tree
	int   minimax;    //solve the minimax tree within the uct tree
	float fpurgency;  //what value to return for a move that hasn't been played yet
	uint  visitexpand;//number of visits before expanding a node
//knowledge
	int   proofscore; //how many virtual rollouts to assign based on the proof number search values
	bool  localreply; //boost for a local reply, ie a move near the previous move
	bool  locality;   //boost for playing near previous stones
	bool  connect;    //boost for having connections to edges and corners
	bool  bridge;     //boost replying to a probe at a bridge
//rollout
	bool  weightedrandom; //use a weighted shuffle for move ordering, based on the rave results
	bool  rolloutpattern; //play the response to a virtual connection threat in rollouts
	int   instantwin;     //look for instant wins in rollouts
	bool  lastgoodreply;  //use the last-good-reply rollout heuristic

	Solver solver;
	Node root;
	Board rootboard;

	Move goodreply[2][361]; //361 is big enough for size 10 (ie 19x19), but no bigger...

	int runs;
	DepthStats treelen, gamelen;
	uint64_t nodes, maxnodes;
	bool timeout;

	double time_used;

	Player() {
		nodes = 0;
		time_used = 0;

		set_default_params();
	}
	void set_default_params(){
		int s = rootboard.get_size();
		defaults    = true;
		explore     = 0;
		ravefactor  = 1000;
		knowfactor  = 0.02;
		ravescale   = false;
		opmoves     = false;
		skiprave    = 0;
		shortrave   = false;
		keeptree    = true;
		minimaxtree = true;
		minimax     = 2;
		fpurgency   = 1;
		visitexpand = 1;
		prooftime   = 0;
		proofscore  = 0;
		localreply  = false;
		locality    = false;
		connect     = false;
		bridge      = true;
		weightedrandom = false;
		rolloutpattern = true;
		lastgoodreply = false;
		instantwin  = 0;
	}
	~Player(){ root.dealloc(); }
	void timedout(){ timeout = true; }

	void set_board(const Board & board){
		rootboard = board;
		nodes -= root.dealloc();
		root = Node();

		if(defaults)
			set_default_params();

		for(int p = 0; p < 2; p++)
			for(int i = 0; i < 361; i++)
				goodreply[p][i] = M_UNKNOWN;
	}
	void move(const Move & m){
		rootboard.move(m, true);

		uint64_t nodesbefore = nodes;

		if(keeptree){
			Node child;

			for(Node * i = root.children.begin(); i != root.children.end(); i++){
				if(i->move == m){
					child = *i;          //copy the child experience to temp
					child.swap_tree(*i); //move the child tree to temp
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
			root.move = m;
		}
		assert(nodes == root.size());
	}

	Move mcts(double time, int maxruns, uint64_t memlimit);
	void solve(double time, uint64_t memlimit);
	vector<Move> get_pv();

protected:
	Node * return_move(const Node * node, int toplay) const;
	Node * return_move_outcome(const Node * node, int outcome) const;
	int walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth);
	void add_knowledge(Board & board, Node * node, Node * child);
	Node * choose_move(const Node * node, int toplay) const;
	void update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay);
	bool test_bridge_probe(const Board & board, const Move & move, const Move & test);

	int rollout(Board & board, RaveMoveList & movelist, Move move, int depth);
	Move rollout_choose_move(Board & board, const 	Move & prev);
	Move rollout_pattern(const Board & board, const Move & move);

	void solve_recurse(Board & board, Node * node, double rate, double solvetime, uint64_t memlimit, int depth);
};

#endif

