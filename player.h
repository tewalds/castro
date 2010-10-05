
#ifndef __PLAYER_H_
#define __PLAYER_H_

#include <cmath>
#include <cassert>

#include "types.h"
#include "move.h"
#include "board.h"
#include "time.h"
#include "depthstats.h"
#include "thread.h"
#include "mtrand.h"
#include "weightedrandtree.h"
#include "lbdist.h"
#include "children.h"

class Player {
public:
	class ExpPair {
		uint32_t s, n;
		ExpPair(uint32_t S, uint32_t N) : s(S), n(N) { }
	public:
		ExpPair() : s(0), n(0) { }
		float avg() const { return 0.5f*s/n; }
		uint32_t num() const { return n; }
		uint32_t sum() const { return s/2; }

		void addvloss(){ INCR(n); }
		void addvtie() { INCR(s); }
		void addvwin() { PLUS(s, 2); }

		void addwins(int num)  { n += num; s += 2*num; }
		void addlosses(int num){ n += num; }
		ExpPair & operator+=(const ExpPair & a){
			s += a.s;
			n += a.n;
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

	struct Node {
	public:
		ExpPair rave;
		ExpPair exp;
		int16_t know;
		int16_t outcome;
		Move    move;
		Move    bestmove; //if outcome is set, then bestmove is the way to get there
		Children<Node> children;
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
		float value(float ravefactor, bool knowledge, float fpurgency){
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

			if(knowledge && know > 0)
				val += 0.01f * know / sqrt((float)(exp.num() + 1));

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

			RaveMove(const Move & m, char p = 0) : Move(m), player(p) { }
		};
		typedef vector<RaveMove>::const_iterator iterator;

		vector<RaveMove> list;

		RaveMoveList(int s = 0){
			list.reserve(s);
		}

		void add(const Move & move, char player){
			list.push_back(RaveMove(move, player));
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
		//sort in y,x order
		void clean(){
			sort(list.begin(), list.end()); //sort in y,x order
		}
	};

	class PlayerThread {
	protected:
	public:
		mutable MTRand_int32 rand32;
		mutable MTRand unitrand;
		Thread thread;
		Player * player;
		bool cancelled;
	public:
		int runs, maxruns;
		DepthStats treelen, gamelen;
		DepthStats wintypes[2][4]; //player,wintype

		PlayerThread() : cancelled(false), runs(0), maxruns(0) {}
		virtual ~PlayerThread() { }
		virtual void reset() { }
		void cancel(){ cancelled = true; }
		int join(){ return thread.join(); }
		void run(); //thread runner, calls iterate on each iteration
		virtual void iterate() { } //handles each iteration
	};

	class PlayerUCT : public PlayerThread {
		Move goodreply[2][361]; //361 is big enough for size 10 (ie 19x19), but no bigger...
		bool use_rave;    //whether to use rave for this simulation
		bool use_explore; //whether to use exploration for this simulation
		int  rollout_pattern_offset; //where to start the rollout pattern
		WeightedRandTree wtree; //struct to hold the valued for weighted random values
		LBDists dists;    //holds the distances to the various non-ring wins as a heuristic for the minimum moves needed to win

	public:
		PlayerUCT(Player * p) {
			PlayerThread();
			player = p;
			reset();
			thread(bind(&PlayerUCT::run, this));
		}

		void reset(){
			runs = 0;
			treelen.reset();
			gamelen.reset();

			for(int p = 0; p < 2; p++)
				for(int i = 0; i < 361; i++)
					goodreply[p][i] = M_UNKNOWN;

			use_rave = false;
			use_explore = false;
			rollout_pattern_offset = 0;

			for(int a = 0; a < 2; a++)
				for(int b = 0; b < 4; b++)
					wintypes[a][b].reset();
		}

	private:
		void iterate();
		int walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth);
		int create_children(Board & board, Node * node, int toplay);
		void add_knowledge(Board & board, Node * node, Node * child);
		Node * choose_move(const Node * node, int toplay, int remain) const;
		bool do_backup(Node * node, Node * backup, int toplay);
		void update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay);
		bool test_bridge_probe(const Board & board, const Move & move, const Move & test) const;

		int rollout(Board & board, RaveMoveList & movelist, Move move, int depth);
		Move rollout_choose_move(Board & board, const 	Move & prev, int & doinstwin);
		Move rollout_pattern(const Board & board, const Move & move);
	};


public:

	static const float min_rave = 0.1;

	bool  ponder;     //think during opponents time?
	int   numthreads; //number of player threads to run
	u64   maxmem;     //maximum memory for the tree in bytes
//final move selection
	float msrave;     //rave factor in final move selection, -1 means use number instead of value
	float msexplore;  //the UCT constant in final move selection
//tree traversal
	float explore;    //greater than one favours exploration, smaller than one favours exploitation
	float ravefactor; //big numbers favour rave scores, small ignore it
	float decrrave;   //decrease rave over time, add this value for each empty position on the board
	bool  knowledge;  //whether to include knowledge
	float userave;    //what probability to use rave
	float useexplore; //what probability to use UCT exploration
	float fpurgency;  //what value to return for a move that hasn't been played yet
//tree building
	bool  shortrave;  //only update rave values on short rollouts
	bool  keeptree;   //reuse the tree from the previous move
	int   minimax;    //solve the minimax tree within the uct tree
	uint  visitexpand;//number of visits before expanding a node
//knowledge
	int   localreply; //boost for a local reply, ie a move near the previous move
	int   locality;   //boost for playing near previous stones
	int   connect;    //boost for having connections to edges and corners
	int   size;       //boost for large groups
	int   bridge;     //boost replying to a probe at a bridge
	int   dists;      //boost based on minimum number of stones needed to finish a non-ring win
//rollout
	bool  weightedrandom; //use a weighted shuffle for move ordering, based on the rave results
	bool  weightedknow;   //use knowledge in the weighted random values
	float checkrings;     //how often to allow rings as a win condition in a rollout
	float checkringdepth; //how deep to allow rings as a win condition in a rollout
	bool  rolloutpattern; //play the response to a virtual connection threat in rollouts
	int   lastgoodreply;  //use the last-good-reply rollout heuristic
	int   instantwin;     //look for instant wins in rollouts
	int   instwindepth;   //how deep to look for instant wins

	Board rootboard;
	Node  root;
	uword nodes, maxnodes;

	vector<PlayerThread *> threads;
	CondVar runners, done;
	RWLock  lock;
	volatile bool running; //whether the write lock is held by the master thread

	double time_used;

	Player() {
		nodes = 0;
		time_used = 0;

		ponder      = false;
//#ifdef SINGLE_THREAD ... make sure only 1 thread
		numthreads  = 1;
		maxmem      = 1000;

		msrave      = -2;
		msexplore   = 0;

		explore     = 0;
		ravefactor  = 500;
		decrrave    = 200;
		knowledge   = true;
		userave     = 1;
		useexplore  = 1;
		fpurgency   = 1;

		shortrave   = false;
		keeptree    = true;
		minimax     = 2;
		visitexpand = 1;

		localreply  = 0;
		locality    = 0;
		connect     = 20;
		size        = 0;
		bridge      = 25;
		dists       = 0;

		weightedrandom = false;
		weightedknow   = false;
		checkrings     = 1.0;
		checkringdepth = 1000;
		rolloutpattern = false;
		lastgoodreply  = false;
		instantwin     = 0;
		instwindepth   = 1000;


		set_maxmem(maxmem);

		//no threads started until a board is set
		lock.wrlock();
		running = false;
	}
	~Player(){
		if(running){
			lock.wrlock();
			running = false;
		}
		numthreads = 0;
		reset_threads(); //shut down the theads properly

		root.dealloc();
	}
	void timedout() { done.broadcast(); }

	void set_maxmem(u64 m){
		maxmem = m;
		maxnodes = maxmem*1024*1024/sizeof(Node);
	}

	void reset_threads(){ //better have the write lock before calling this
		assert(running == false);

	//kill all the threads
		for(unsigned int i = 0; i < threads.size(); i++)
			threads[i]->cancel();

		lock.unlock(); //let the runners run and exit since rdlock is not a cancellation point
		runners.broadcast();
		running = true;

	//make sure they exited cleanly
		for(unsigned int i = 0; i < threads.size(); i++)
			threads[i]->join();

		threads.clear();

		lock.wrlock(); //lock so the new threads don't run
		running = false;

	//start new threads
		for(int i = 0; i < numthreads; i++)
			threads.push_back(new PlayerUCT(this));
	}

	void set_ponder(bool p){
		if(ponder != p){
			ponder = p;
			if(ponder){
				if(!running){
					lock.unlock();
					runners.broadcast();
					running = true;
				}
			}else{
				if(running){
					lock.wrlock();
					running = false;
				}
			}
		}
	}

	void set_board(const Board & board){
		if(running){
			lock.wrlock();
			running = false;
		}

		rootboard = board;
		nodes -= root.dealloc();
		root = Node();

		reset_threads();

		if(ponder){
			lock.unlock();
			runners.broadcast();
			running = true;
		}
	}
	void move(const Move & m){
		if(running){
			lock.wrlock();
			running = false;
		}

		rootboard.move(m, true, true);

		uword nodesbefore = nodes;

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
				fprintf(stderr, "Nodes before: %u, after: %u, saved %.1f%% of the tree\n", nodesbefore, nodes, 100.0*nodes/nodesbefore);
		}else{
			nodes -= root.dealloc();
			root = Node();
			root.move = m;
		}
		assert(nodes == root.size());

		root.exp.addwins(visitexpand+1); //+1 to compensate for the virtual loss
		if(rootboard.won() == -1)
			root.outcome = -1;

		if(ponder){
			lock.unlock();
			runners.broadcast();
			running = true;
		}
	}

	double gamelen(){
		DepthStats len;
		for(unsigned int i = 0; i < threads.size(); i++)
			len += threads[i]->gamelen;
		return len.avg();
	}

	Move genmove(double time, int maxruns);
	vector<Move> get_pv();
	void garbage_collect(Node * node, unsigned int limit);

protected:
	Node * return_move(Node * node, int toplay) const;
};

#endif

