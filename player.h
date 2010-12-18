
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
#include "compacttree.h"
#include "log.h"

class Player {
public:
	class ExpPair {
		uword s, n;
		ExpPair(uword S, uword N) : s(S), n(N) { }
	public:
		ExpPair() : s(0), n(0) { }
		float avg() const { return 0.5f*s/n; }
		uword num() const { return n; }
		uword sum() const { return s/2; }

		void clear() { s = 0; n = 0; }

		void addvloss(){ INCR(n); }
		void addvtie() { INCR(s); }
		void addvwin() { PLUS(s, 2); }
		void addv(const ExpPair & a){
			if(a.s) PLUS(s, a.s);
			if(a.n) PLUS(n, a.n);
		}

		void addloss(){ n++; }
		void addtie() { s++; }
		void addwin() { s += 2; }
		void add(const ExpPair & a){
			s += a.s;
			n += a.n;
		}

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
		CompactTree<Node>::Children children;
//		int padding;
		//seems to need padding to multiples of 8 bytes or it segfaults?
		//don't forget to update the copy constructor/operator

		Node()                            : know(0), outcome(-3)          { }
		Node(const Move & m, char o = -3) : know(0), outcome(o), move(m)  { }
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
				//children = n.children; ignore the children, they need to be swap_tree'd in
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

			if(children.num())
				for(Node * i = children.begin(); i != children.end(); i++)
					num += i->size();

			return num;
		}

		~Node(){
			assert(children.empty());
		}

		unsigned int alloc(unsigned int num, CompactTree<Node> & ct){
			return children.alloc(num, ct);
		}
		unsigned int dealloc(CompactTree<Node> & ct){
			unsigned int num = 0;

			if(children.num())
				for(Node * i = children.begin(); i != children.end(); i++)
					num += i->dealloc(ct);
			num += children.dealloc(ct);

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

	struct MoveList {
		struct RaveMove : public Move {
			char player;

			RaveMove() : Move(M_UNKNOWN), player(0) { }
			RaveMove(const Move & m, char p = 0) : Move(m), player(p) { }
		};

		ExpPair  exp[2];       //aggregated outcomes overall
		ExpPair  rave[2][361]; //aggregated outcomes per move
		RaveMove moves[361];   //moves made in order
		int      tree;         //number of moves in the tree
		int      rollout;      //number of moves in the rollout
		Board *  board;        //reference to rootboard for xy()

		MoveList() : tree(0), rollout(0), board(NULL) { }

		void addtree(const Move & move, char player){
			moves[tree++] = RaveMove(move, player);
		}
		void addrollout(const Move & move, char player){
			moves[rollout++] = RaveMove(move, player);
		}
		void reset(Board * b){
			tree = 0;
			rollout = 0;
			board = b;
			exp[0].clear();
			exp[1].clear();
			for(int i = 0; i < b->vecsize(); i++){
				rave[0][i].clear();
				rave[1][i].clear();
			}
		}
		void finishrollout(int won){
			exp[0].addloss();
			exp[1].addloss();
			if(won == 0){
				exp[0].addtie();
				exp[1].addtie();
			}else{
				exp[won-1].addwin();

				for(RaveMove * i = begin(), * e = end(); i != e; i++){
					ExpPair & r = rave[i->player-1][board->xy(*i)];
					r.addloss();
					if(i->player == won)
						r.addwin();
				}
			}
			rollout = 0;
		}
		RaveMove * begin() {
			return moves;
		}
		RaveMove * end() {
			return moves + tree + rollout;
		}
		void subvlosses(int n){
			exp[0].addlosses(-n);
			exp[1].addlosses(-n);
		}
		const ExpPair & getrave(int player, const Move & move) const {
			return rave[player-1][board->xy(move)];
		}
		const ExpPair & getexp(int player) const {
			return exp[player-1];
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
		MoveList movelist;

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
		void walk_tree(Board & board, Node * node, int depth);
		bool create_children(Board & board, Node * node, int toplay);
		void add_knowledge(Board & board, Node * node, Node * child);
		Node * choose_move(const Node * node, int toplay, int remain) const;
		bool do_backup(Node * node, Node * backup, int toplay);
		void update_rave(const Node * node, int toplay);
		bool test_bridge_probe(const Board & board, const Move & move, const Move & test) const;

		int rollout(Board & board, Move move, int depth);
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
	int   rollouts;   //number of rollouts to run after the tree traversal
//tree building
	bool  shortrave;  //only update rave values on short rollouts
	bool  keeptree;   //reuse the tree from the previous move
	int   minimax;    //solve the minimax tree within the uct tree
	bool  detectdraw; //look for draws early, slow
	uint  visitexpand;//number of visits before expanding a node
	bool  prunesymmetry; //prune symmetric children from the move list, useful for proving but likely not for playing
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
	uword nodes;
	int   gclimit; //the minimum experience needed to not be garbage collected

	CompactTree<Node> ctmem;

	string solved_logname;
	FILE * solved_logfile;

	enum ThreadState {
		Thread_Cancelled,  //threads should exit
		Thread_Wait_Start, //threads are waiting to start
		Thread_Running,    //threads are running
		Thread_GC,         //one thread is running garbage collection, the rest are waiting
		Thread_GC_End,     //once done garbage collecting, go to wait_end instead of back to running
		Thread_Wait_End,   //threads are waiting to end
	};
	volatile ThreadState threadstate;
	vector<PlayerThread *> threads;
	Barrier runbarrier, gcbarrier;

	double time_used;

	Player() {
		nodes = 0;
		gclimit = 5;
		time_used = 0;

		solved_logfile = NULL;

		ponder      = false;
//#ifdef SINGLE_THREAD ... make sure only 1 thread
		numthreads  = 1;
		maxmem      = 1000*1024*1024;

		msrave      = -2;
		msexplore   = 0;

		explore     = 0;
		ravefactor  = 500;
		decrrave    = 200;
		knowledge   = true;
		userave     = 1;
		useexplore  = 1;
		fpurgency   = 1;
		rollouts    = 1;

		shortrave   = false;
		keeptree    = true;
		minimax     = 2;
		detectdraw  = false;
		visitexpand = 1;
		prunesymmetry = false;

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
		threadstate = Thread_Wait_Start;
	}
	~Player(){
		stop_threads();

		numthreads = 0;
		reset_threads(); //shut down the theads properly

		if(solved_logfile){
			Board copy = rootboard;
			garbage_collect(copy, & root, 0);
			fclose(solved_logfile);
		}

		root.dealloc(ctmem);
		ctmem.compact();
	}
	void timedout() {
		CAS(threadstate, Thread_Running, Thread_Wait_End);
		CAS(threadstate, Thread_GC, Thread_GC_End);
	}

	void set_maxmem(u64 m){
		maxmem = m;
	}

	void stop_threads(){
		if(threadstate != Thread_Wait_Start){
			timedout();
			runbarrier.wait();
			CAS(threadstate, Thread_Wait_End, Thread_Wait_Start);
		}
	}

	void start_threads(){
		assert(threadstate == Thread_Wait_Start);
		runbarrier.wait();
	}

	void reset_threads(){ //start and end with threadstate = Thread_Wait_Start
		assert(threadstate == Thread_Wait_Start);

	//kill all the threads
		for(unsigned int i = 0; i < threads.size(); i++)
			threads[i]->cancel();

		threadstate = Thread_Cancelled;
		runbarrier.wait();

	//make sure they exited cleanly
		for(unsigned int i = 0; i < threads.size(); i++)
			threads[i]->join();

		threads.clear();

		threadstate = Thread_Wait_Start;

		runbarrier.reset(numthreads + 1);
		gcbarrier.reset(numthreads);

	//start new threads
		for(int i = 0; i < numthreads; i++)
			threads.push_back(new PlayerUCT(this));
	}

	void set_ponder(bool p){
		if(ponder != p){
			ponder = p;
			stop_threads();

			if(ponder)
				start_threads();
		}
	}

	void set_board(const Board & board){
		stop_threads();

		if(solved_logfile){
			Board copy = rootboard;
			garbage_collect(copy, & root, 0);
		}

		rootboard = board;
		nodes -= root.dealloc(ctmem);
		root = Node();

		reset_threads();

		if(ponder)
			start_threads();
	}
	void move(const Move & m){
		stop_threads();

		if(solved_logfile){
			Board copy = rootboard;
			garbage_collect(copy, & root, 0);
		}

		rootboard.move(m, true, true);

		uword nodesbefore = nodes;

		if(keeptree && root.children.num() > 0){
			Node child;

			for(Node * i = root.children.begin(); i != root.children.end(); i++){
				if(i->move == m){
					child = *i;          //copy the child experience to temp
					child.swap_tree(*i); //move the child tree to temp
					break;
				}
			}

			nodes -= root.dealloc(ctmem);
			root = child;
			root.swap_tree(child);

			if(nodesbefore > 0)
				logerr("Nodes before: " + to_str(nodesbefore) + ", after: " + to_str(nodes) + ", saved " +  to_str(100.0*nodes/nodesbefore, 1) + "% of the tree\n");
		}else{
			nodes -= root.dealloc(ctmem);
			root = Node();
			root.move = m;
		}
		assert(nodes == root.size());

		root.exp.addwins(visitexpand+1); //+1 to compensate for the virtual loss
		if(rootboard.won() < 0)
			root.outcome = -3;

		if(ponder)
			start_threads();
	}

	double gamelen(){
		DepthStats len;
		for(unsigned int i = 0; i < threads.size(); i++)
			len += threads[i]->gamelen;
		return len.avg();
	}

	bool setlogfile(string name){
		if(solved_logfile)
			fclose(solved_logfile);

		solved_logfile = fopen(name.c_str(), "a");

		if(solved_logfile)
			solved_logname = name;
		else
			solved_logname = "";

		return solved_logfile;
	}

	void flushlog(){
		if(solved_logfile)
			fflush(solved_logfile);
	}

	void u64buf(char * buf, uint64_t val){
		static const char hexlookup[] = "0123456789abcdef";
		for(int i = 15; i >= 0; i--){
			buf[i] = hexlookup[val & 15];
			val >>= 4;
		}
		buf[16] = '\0';
	}

	void logsolved(hash_t hash, const Node * node){
		char hashbuf[17];
		u64buf(hashbuf, hash);
		string s = string("0x") + hashbuf + "," + to_str(node->exp.num()) + "," + to_str(node->outcome) + "\n";
		fprintf(solved_logfile, "%s", s.c_str());
	}

	Node * genmove(double time, int maxruns);
	vector<Move> get_pv();
	void garbage_collect(Board & board, Node * node, unsigned int limit); //destroys the board, so pass in a copy

protected:
	Node * return_move(Node * node, int toplay) const;
};

#endif

