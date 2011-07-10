
#include "player.h"
#include "board.h"
#include <cmath>
#include <string>
#include "string.h"
#include "solverab.h"
#include "timer.h"
#include "time.h"

void Player::PlayerThread::run(){
	while(true){
		switch(player->threadstate){
		case Thread_Cancelled:  //threads should exit
			return;

		case Thread_Wait_Start: //threads are waiting to start
		case Thread_Wait_Start_Cancelled:
			player->runbarrier.wait();
			CAS(player->threadstate, Thread_Wait_Start, Thread_Running);
			CAS(player->threadstate, Thread_Wait_Start_Cancelled, Thread_Cancelled);
			break;

		case Thread_Wait_End:   //threads are waiting to end
			player->runbarrier.wait();
			CAS(player->threadstate, Thread_Wait_End, Thread_Wait_Start);
			break;

		case Thread_Running:    //threads are running
			if(player->root.outcome >= 0 || (player->maxruns > 0 && player->runs >= player->maxruns)){ //solved or finished runs
				if(CAS(player->threadstate, Thread_Running, Thread_Wait_End) && player->root.outcome >= 0)
					logerr("Solved as " + to_str(player->root.outcome >= 0) + "\n");
				break;
			}
			if(player->ctmem.memalloced() >= player->maxmem){ //out of memory, start garbage collection
				CAS(player->threadstate, Thread_Running, Thread_GC);
				break;
			}

			INCR(player->runs);
			iterate();
			break;

		case Thread_GC:         //one thread is running garbage collection, the rest are waiting
		case Thread_GC_End:     //once done garbage collecting, go to wait_end instead of back to running
			if(player->gcbarrier.wait()){
				Time starttime;
				logerr("Starting player GC with limit " + to_str(player->gclimit) + " ... ");
				uint64_t nodesbefore = player->nodes;
				Board copy = player->rootboard;
				player->garbage_collect(copy, & player->root, player->gclimit);
				player->flushlog();
				Time gctime;
				player->ctmem.compact(1.0, 0.75);
				Time compacttime;
				logerr(to_str(100.0*player->nodes/nodesbefore, 1) + " % of tree remains - " +
					to_str((gctime - starttime)*1000, 0)  + " msec gc, " + to_str((compacttime - gctime)*1000, 0) + " msec compact\n");

				if(player->ctmem.meminuse() >= player->maxmem/2)
					player->gclimit = (int)(player->gclimit*1.3);
				else if(player->gclimit > 5)
					player->gclimit = (int)(player->gclimit*0.9); //slowly decay to a minimum of 5

				CAS(player->threadstate, Thread_GC,     Thread_Running);
				CAS(player->threadstate, Thread_GC_End, Thread_Wait_End);
			}
			player->gcbarrier.wait();
			break;
		}
	}
}

Player::Node * Player::genmove(double time, int max_runs){
	time_used = 0;
	int toplay = rootboard.toplay();

	if(rootboard.won() >= 0 || (time <= 0 && max_runs == 0))
		return NULL;

	Time starttime;

	stop_threads();

	if(runs)
		logerr("Pondered " + to_str(runs) + " runs\n");

	runs = 0;
	maxruns = max_runs;
	for(unsigned int i = 0; i < threads.size(); i++)
		threads[i]->reset();


	//let them run!
	start_threads();

	Timer timer;
	if(time > 0)
		timer.set(time - (Time() - starttime), bind(&Player::timedout, this));

	//wait for the timer to stop them
	runbarrier.wait();
	CAS(threadstate, Thread_Wait_End, Thread_Wait_Start);
	assert(threadstate == Thread_Wait_Start);

	if(ponder && root.outcome < 0)
		start_threads();

	time_used = Time() - starttime;

//return the best one
	return return_move(& root, toplay);
}



Player::Player() {
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
	parentexplore = false;
	ravefactor  = 500;
	decrrave    = 200;
	knowledge   = true;
	userave     = 1;
	useexplore  = 1;
	fpurgency   = 1;
	rollouts    = 1;
	dynwiden    = 0;

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

	//no threads started until a board is set
	threadstate = Thread_Wait_Start;
}
Player::~Player(){
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
void Player::timedout() {
	CAS(threadstate, Thread_Running, Thread_Wait_End);
	CAS(threadstate, Thread_GC, Thread_GC_End);
}

string Player::statestring(){
	switch(threadstate){
	case Thread_Cancelled:  return "Thread_Wait_Cancelled";
	case Thread_Wait_Start: return "Thread_Wait_Start";
	case Thread_Wait_Start_Cancelled: return "Thread_Wait_Start_Cancelled";
	case Thread_Running:    return "Thread_Running";
	case Thread_GC:         return "Thread_GC";
	case Thread_GC_End:     return "Thread_GC_End";
	case Thread_Wait_End:   return "Thread_Wait_End";
	}
	return "Thread_State_Unknown!!!";
}

void Player::stop_threads(){
	if(threadstate != Thread_Wait_Start){
		timedout();
		runbarrier.wait();
		CAS(threadstate, Thread_Wait_End, Thread_Wait_Start);
		assert(threadstate == Thread_Wait_Start);
	}
}

void Player::start_threads(){
	assert(threadstate == Thread_Wait_Start);
	runbarrier.wait();
	CAS(threadstate, Thread_Wait_Start, Thread_Running);
}

void Player::reset_threads(){ //start and end with threadstate = Thread_Wait_Start
	assert(threadstate == Thread_Wait_Start);

//wait for them to all get to the barrier
	assert(CAS(threadstate, Thread_Wait_Start, Thread_Wait_Start_Cancelled));
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

void Player::set_ponder(bool p){
	if(ponder != p){
		ponder = p;
		stop_threads();

		if(ponder)
			start_threads();
	}
}

void Player::set_board(const Board & board){
	stop_threads();

	if(solved_logfile){
		Board copy = rootboard;
		garbage_collect(copy, & root, 0);
	}

	rootboard = board;
	nodes -= root.dealloc(ctmem);
	root = Node();

	reset_threads(); //needed since the threads aren't started before a board it set

	if(ponder)
		start_threads();
}
void Player::move(const Move & m){
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

double Player::gamelen(){
	DepthStats len;
	for(unsigned int i = 0; i < threads.size(); i++)
		len += threads[i]->gamelen;
	return len.avg();
}

bool Player::setlogfile(string name){
	if(solved_logfile)
		fclose(solved_logfile);

	solved_logfile = fopen(name.c_str(), "a");

	if(solved_logfile)
		solved_logname = name;
	else
		solved_logname = "";

	return solved_logfile;
}

void Player::flushlog(){
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

void Player::logsolved(hash_t hash, const Node * node){
	char hashbuf[17];
	u64buf(hashbuf, hash);
	string s = string("0x") + hashbuf + "," + to_str(node->exp.num()) + "," + to_str(node->outcome) + "\n";
	fprintf(solved_logfile, "%s", s.c_str());
}

vector<Move> Player::get_pv(){
	vector<Move> pv;

	Node * r, * n = & root;
	char turn = rootboard.toplay();
	while(!n->children.empty()){
		r = return_move(n, turn);
		if(!r) break;
		pv.push_back(r->move);
		turn = 3 - turn;
		n = r;
	}

	if(pv.size() == 0)
		pv.push_back(Move(M_RESIGN));

	return pv;
}

Player::Node * Player::return_move(Node * node, int toplay) const {
	double val, maxval = -1000000000000.0; //1 trillion

	Node * ret = NULL,
		 * child = node->children.begin(),
		 * end = node->children.end();

	for( ; child != end; child++){
		if(child->outcome >= 0){
			if(child->outcome == toplay) val =  800000000000.0 - child->exp.num(); //shortest win
			else if(child->outcome == 0) val = -400000000000.0 + child->exp.num(); //longest tie
			else                         val = -800000000000.0 + child->exp.num(); //longest loss
		}else{ //not proven
			if(msrave == -1) //num simulations
				val = child->exp.num();
			else if(msrave == -2) //num wins
				val = child->exp.sum();
			else
				val = child->value(msrave, 0, 0) - msexplore*sqrt(log(node->exp.num())/(child->exp.num() + 1));
		}

		if(maxval < val){
			maxval = val;
			ret = child;
		}
	}

//set bestmove, but don't touch outcome, if it's solved that will already be set, otherwise it shouldn't be set
	if(ret){
		node->bestmove = ret->move;
	}else if(node->bestmove == M_UNKNOWN){
		SolverAB solver;
		solver.set_board(rootboard);
		solver.solve(0.1);
		node->bestmove = solver.bestmove;
	}

	assert(node->bestmove != M_UNKNOWN);

	return ret;
}

void Player::garbage_collect(Board & board, Node * node, unsigned int limit){
	Node * child = node->children.begin(),
		 * end = node->children.end();

	for( ; child != end; child++){
		if(child->outcome >= 0){ //solved
			if(solved_logfile && child->exp.num() > 1000){ //log heavy solved nodes
				board.set(child->move);
				if(child->children.num() > 0)
					garbage_collect(board, child, limit);
				logsolved(board.gethash(), child);
				board.unset(child->move);
			}
			nodes -= child->dealloc(ctmem);
		}else if(child->exp.num() < limit){ //low exp, ignore solvedness since it's trivial to re-solve
			nodes -= child->dealloc(ctmem);
		}else if(child->children.num() > 0){
			board.set(child->move);
			garbage_collect(board, child, limit);
			board.unset(child->move);
		}
	}
}

