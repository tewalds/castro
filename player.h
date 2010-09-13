
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
		class Children {
			static const int LOCK = 1; //needs to be cast to (Node *) at usage point

			uint32_t _num; //can be smaller, but will be padded anyway.
			Node *   _children;
		public:
			typedef Node * iterator;
			Children()      : _num(0), _children(NULL) { }
			Children(int n) : _num(0), _children(NULL) { alloc(n); }
			~Children() { assert_empty(); }

			void assert_consistent() const { assert((_num == 0) || (_children > (Node *) LOCK)); }
			void assert_empty()      const { assert((_num == 0) && (_children == NULL)); }

			bool lock()   { return CAS(_children, (Node *) NULL, (Node *) LOCK); }
			bool unlock() { return CAS(_children, (Node *) LOCK, (Node *) NULL); }

			void atomic_set(Children & o){
				assert(CAS(_children, (Node *) LOCK, o._children)); //undoes the lock
				assert(CAS(_num, 0, o._num)); //keeps consistency
			}

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
				assert(_children > (Node *) LOCK);
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
		Thread thread;
		Player * player;
		bool cancelled;
	public:
		int runs, maxruns;
		DepthStats treelen, gamelen;

		PlayerThread() : cancelled(false), runs(0), maxruns(0) {}
		virtual void reset() { }
		void cancel(){ cancelled = true; }
		int join(){ return thread.join(); }
	};

	class PlayerUCT : public PlayerThread {
		Move goodreply[2][361]; //361 is big enough for size 10 (ie 19x19), but no bigger...

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
		}

	private:
		void run();
		int walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth);
		int create_children(Board & board, Node * node, int toplay);
		void add_knowledge(Board & board, Node * node, Node * child);
		Node * choose_move(const Node * node, int toplay, int remain) const;
		bool do_backup(Node * node, Node * backup, int toplay);
		void update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay);
		bool test_bridge_probe(const Board & board, const Move & move, const Move & test);

		int rollout(Board & board, RaveMoveList & movelist, Move move, int depth);
		Move rollout_choose_move(Board & board, const 	Move & prev);
		Move rollout_pattern(const Board & board, const Move & move);
	};

	class Sync {
		RWLock  lock; // stop the thread runners from doing work
		CondVar cond; // tell the master thread that the threads are done
		volatile bool stop; // write lock asked
		volatile bool writelock; //is the write lock

	public:
		Sync() : stop(false), writelock(false) { }

		int wrlock(){ //aquire the write lock, set stop, blocks
			assert(writelock == false);

//			fprintf(stderr, "ask write lock\n");

			CAS(stop, false, true);
			int r = lock.wrlock();
			writelock = true;
			CAS(stop, true, false);
//			fprintf(stderr, "got write lock\n");
			return r;
		}
		int rdlock(){ //aquire the read lock, blocks
//			fprintf(stderr, "Ask for read lock\n");
			if(stop){ //spin on the read lock so that the write lock isn't starved
//				fprintf(stderr, "Spinning on read lock\n");
				while(stop)
					continue;
//				fprintf(stderr, "Done spinning on read lock\n");
			}
			int ret = lock.rdlock();
//			fprintf(stderr, "Got a read lock\n");
			return ret;
		}
		int relock(){ //succeeds if stop isn't requested
			return (stop == false);
		}
		int unlock(){ //unlocks the lock
			if(writelock){
//				fprintf(stderr, "unlock read lock\n");
			}else{
//				fprintf(stderr, "unlock write lock\n");
			}
			writelock = false;
			return lock.unlock();
		}
		int done(){   //signals that the timeout happened or solved
			return cond.broadcast();
		}
		int wait(){   //waits for the signal
			cond.lock(); //lock the signal that defines the end condition
			int ret = cond.wait(); //wait a signal to end (could be from the timer)
			cond.unlock();
			return ret;
		}
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
	int   skiprave;   //how often to skip rave, skip once in this many checks
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
	int   bridge;     //boost replying to a probe at a bridge
//rollout
	bool  weightedrandom; //use a weighted shuffle for move ordering, based on the rave results
	bool  rolloutpattern; //play the response to a virtual connection threat in rollouts
	int   instantwin;     //look for instant wins in rollouts
	int   lastgoodreply;  //use the last-good-reply rollout heuristic

	Board rootboard;
	Node  root;
	uword nodes, maxnodes;

	vector<PlayerThread *> threads;
	Sync sync;

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
		skiprave    = 0;
		fpurgency   = 1;

		shortrave   = false;
		keeptree    = true;
		minimax     = 2;
		visitexpand = 1;

		localreply  = 0;
		locality    = 0;
		connect     = 20;
		bridge      = 25;

		weightedrandom = false;
		rolloutpattern = false;
		lastgoodreply  = false;
		instantwin     = 0;


		set_maxmem(maxmem);

		//no threads started until a board is set

		if(!ponder)
			sync.wrlock();
	}
	~Player(){
		if(ponder)
			sync.wrlock();
		root.dealloc();
	}
	void timedout() { sync.done(); }

	void set_maxmem(u64 m){
		maxmem = m;
		maxnodes = maxmem*1024*1024/sizeof(Node);
	}

	void reset_threads(){ //better have the write lock before calling this
	//kill all the threads
		for(unsigned int i = 0; i < threads.size(); i++)
			threads[i]->cancel();

		sync.unlock(); //let the runners run and exit since rdlock is not a cancellation point

	//make sure they exited cleanly
		for(unsigned int i = 0; i < threads.size(); i++)
			threads[i]->join();

		threads.clear();

		sync.wrlock(); //lock so the new threads don't run

	//start new threads
		for(int i = 0; i < numthreads; i++)
			threads.push_back(new PlayerUCT(this));
	}

	void set_ponder(bool p){
		if(ponder != p){
			if(p) sync.unlock();
			else  sync.wrlock();
			ponder = p;
		}
	}

	void set_board(const Board & board){
		if(ponder)
			sync.wrlock();

		rootboard = board;
		nodes -= root.dealloc();
		root = Node();

		reset_threads();

		if(ponder)
			sync.unlock();
	}
	void move(const Move & m){
		if(ponder && root.outcome == -1)
			sync.wrlock();

		rootboard.move(m, true);

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

		if(ponder && root.outcome == -1)
			sync.unlock();
	}

	double gamelen(){
		DepthStats len;
		for(unsigned int i = 0; i < threads.size(); i++)
			len += threads[i]->gamelen;
		return len.avg();
	}

	Move genmove(double time, int maxruns);
	vector<Move> get_pv();

protected:
	Node * return_move(Node * node, int toplay) const;
};

#endif

