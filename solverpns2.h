
#ifndef _SOLVERPNS2_H_
#define _SOLVERPNS2_H_

#include <stdint.h>

#include "solver.h"
#include "compacttree.h"
#include "lbdist.h"
#include "log.h"


class SolverPNS2 : public Solver {
	static const uint32_t LOSS  = (1<<30)-1;
	static const uint32_t DRAW  = (1<<30)-2;
	static const uint32_t INF32 = (1<<30)-3;
public:

	struct PNSNode {
		static const uint16_t reflock = 1<<15;
		uint32_t phi, delta;
		uint64_t work;
		uint16_t refcount; //how many threads are down this node
		Move move;
		CompactTree<PNSNode>::Children children;

		PNSNode() { }
		PNSNode(int x, int y,   int v = 1)     : phi(v), delta(v), work(0), refcount(0), move(Move(x,y)) { }
		PNSNode(const Move & m, int v = 1)     : phi(v), delta(v), work(0), refcount(0), move(m)         { }
		PNSNode(int x, int y,   int p, int d)  : phi(p), delta(d), work(0), refcount(0), move(Move(x,y)) { }
		PNSNode(const Move & m, int p, int d)  : phi(p), delta(d), work(0), refcount(0), move(m)         { }

		PNSNode(const PNSNode & n) { *this = n; }
		PNSNode & operator = (const PNSNode & n){
			if(this != & n){ //don't copy to self
				//don't copy to a node that already has children
				assert(children.empty());

				phi = n.phi;
				delta = n.delta;
				work = n.work;
				move = n.move;
				//don't copy the children
			}
			return *this;
		}

		~PNSNode(){
			assert(children.empty());
		}

		PNSNode & abval(int outcome, int toplay, int assign, int value = 1){
			if(assign && (outcome == 1 || outcome == -1))
				outcome = (toplay == assign ? 2 : -2);

			if(     outcome ==  0)   { phi = value; delta = value; }
			else if(outcome ==  2)   { phi = LOSS;  delta = 0;     }
			else if(outcome == -2)   { phi = 0;     delta = LOSS; }
			else /*(outcome 1||-1)*/ { phi = 0;     delta = DRAW; }
			return *this;
		}

		PNSNode & outcome(int outcome, int toplay, int assign, int value = 1){
			if(assign && outcome == 0)
				outcome = assign;

			if(     outcome == -3)       { phi = value; delta = value; }
			else if(outcome ==   toplay) { phi = LOSS;  delta = 0;     }
			else if(outcome == 3-toplay) { phi = 0;     delta = LOSS; }
			else /*(outcome == 0)*/      { phi = 0;     delta = DRAW; }
			return *this;
		}


		bool terminal(){ return (phi == 0 || delta == 0); }

		uint32_t refdelta() const {
			return delta + refcount;
		}

		void ref()  { PLUS(refcount, 1); }
		void deref(){ PLUS(refcount, -1); }

		unsigned int size() const {
			unsigned int num = children.num();

			for(PNSNode * i = children.begin(); i != children.end(); i++)
				num += i->size();

			return num;
		}

		void swap_tree(PNSNode & n){
			children.swap(n.children);
		}

		unsigned int alloc(unsigned int num, CompactTree<PNSNode> & ct){
			return children.alloc(num, ct);
		}
		unsigned int dealloc(CompactTree<PNSNode> & ct){
			unsigned int num = 0;

			for(PNSNode * i = children.begin(); i != children.end(); i++)
				num += i->dealloc(ct);
			num += children.dealloc(ct);

			return num;
		}
	};

	class SolverThread {
	protected:
	public:
		Thread thread;
		SolverPNS2 * solver;
	public:
		uint64_t iters;
		LBDists dists;    //holds the distances to the various non-ring wins as a heuristic for the minimum moves needed to win

		SolverThread(SolverPNS2 * s) : solver(s), iters(0) {
			thread(bind(&SolverThread::run, this));
		}
		virtual ~SolverThread() { }
		void reset(){
			iters = 0;
		}
		int join(){ return thread.join(); }
		void run(); //thread runner

	//basic proof number search building a tree
		bool pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td);

	//update the phi and delta for the node
		bool updatePDnum(PNSNode * node);
	};


//memory management for PNS which uses a tree to store the nodes
	uint64_t nodes, memlimit;
	int gclimit;
	CompactTree<PNSNode> ctmem;

	enum ThreadState {
		Thread_Cancelled,  //threads should exit
		Thread_Wait_Start, //threads are waiting to start
		Thread_Wait_Start_Cancelled, //once done waiting, go to cancelled instead of running
		Thread_Running,    //threads are running
		Thread_GC,         //one thread is running garbage collection, the rest are waiting
		Thread_GC_End,     //once done garbage collecting, go to wait_end instead of back to running
		Thread_Wait_End,   //threads are waiting to end
	};
	volatile ThreadState threadstate;
	vector<SolverThread *> threads;
	Barrier runbarrier, gcbarrier;


	int   ab; // how deep of an alpha-beta search to run at each leaf node
	bool  df; // go depth first?
	float epsilon; //if depth first, how wide should the threshold be?
	int   ties;    //which player to assign ties to: 0 handle ties, 1 assign p1, 2 assign p2
	bool  lbdist;
	int   numthreads;

	PNSNode root;
	LBDists dists;

	SolverPNS2() {
		ab = 2;
		df = true;
		epsilon = 0.25;
		ties = 0;
		lbdist = false;
		numthreads = 1;
		gclimit = 5;

		reset();

		set_memlimit(100*1024*1024);

		//no threads started until a board is set
		threadstate = Thread_Wait_Start;
	}

	~SolverPNS2(){
		stop_threads();

		numthreads = 0;
		reset_threads(); //shut down the theads properly

		root.dealloc(ctmem);
		ctmem.compact();
	}

	void reset(){
		outcome = -3;
		maxdepth = 0;
		nodes_seen = 0;
		bestmove = Move(M_UNKNOWN);

		timeout = false;
	}

	string statestring();
	void stop_threads();
	void start_threads();
	void reset_threads();
	void timedout();

	void set_board(const Board & board, bool clear = true){
		rootboard = board;
		rootboard.setswap(false);
		reset();
		if(clear)
			clear_mem();

		reset_threads(); //needed since the threads aren't started before a board it set
	}
	void move(const Move & m){
		stop_threads();

		rootboard.move(m, true, true);
		reset();


		uint64_t nodesbefore = nodes;

		PNSNode child;

		for(PNSNode * i = root.children.begin(); i != root.children.end(); i++){
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
			logerr(string("PNS Nodes before: ") + to_str(nodesbefore) + ", after: " + to_str(nodes) + ", saved " + to_str(100.0*nodes/nodesbefore, 1) + "% of the tree\n");

		assert(nodes == root.size());

		if(nodes == 0)
			clear_mem();
	}

	void set_memlimit(uint64_t lim){
		memlimit = lim;
	}

	void clear_mem(){
		reset();
		root.dealloc(ctmem);
		ctmem.compact();
		root = PNSNode(0, 0, 1);
		nodes = 0;
	}

	void solve(double time);

//remove all the leaf nodes to free up some memory
	void garbage_collect(PNSNode * node, unsigned int limit);
};

#endif

