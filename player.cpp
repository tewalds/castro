
#include "player.h"
#include "board.h"
#include <cmath>
#include <string>
#include "string.h"
#include "solver.h"
#include "timer.h"

Move Player::genmove(double time, int maxruns, uint64_t memlimit){
	maxnodes = memlimit*1024*1024/sizeof(Node);
	time_used = 0;

	if(rootboard.won() >= 0 || (time <= 0 && maxruns == 0))
		return Move(M_RESIGN);


	int starttime = time_msec();

	Timer timer;
	if(time > 0)
		timer.set(time, bind(&Player::timedout, this));

	root.outcome = -1;
	root.exp.addwins(visitexpand);

	//let them run!

	if(!ponder)
		lock.unlock(); //remove the write lock

	cond.lock(); //lock the signal that defines the end condition
	cond.wait(); //wait a signal to end (could be from the timer)
	cond.unlock();

	if(!ponder)
		lock.wrlock(); //stop the runners
	//maybe	let them run again after making the move?



//return the best one
	Node * ret = return_move(& root, rootboard.toplay());

	int runtime = time_msec() - starttime;
	time_used = (double)runtime/1000;

	DepthStats gamelen, treelen;
	int runs = 0;
	for(unsigned int i = 0; i < threads.size(); i++){
		gamelen += threads[i]->gamelen;
		treelen += threads[i]->treelen;
		runs += threads[i]->runs;
		threads[i]->reset();
	}

	string stats = "Finished " + to_str(runs) + " runs in " + to_str(runtime) + " msec\n";
	stats += "Game length: " + gamelen.to_s() + "\n";
	stats += "Tree depth:  " + treelen.to_s() + "\n";
	stats += "Move Score:  " + to_str(ret->exp.avg()) + "\n";
	stats += "Games/s:     " + to_str((int)((double)runs*1000/runtime)) + "\n";
	if(ret->outcome >= 0){
		stats += "Solved as a ";
		if(ret->outcome == 0)                       stats += "draw";
		else if(ret->outcome == rootboard.toplay()) stats += "win";
		else                                        stats += "loss";
		stats += "\n";
	}
	fprintf(stderr, "%s", stats.c_str());

	return ret->move;
}

vector<Move> Player::get_pv(){
	vector<Move> pv;

	Node * n = & root;
	char turn = rootboard.toplay();
	while(!n->children.empty()){
		n = return_move(n, turn);
		pv.push_back(n->move);
		turn = 3 - turn;
	}

	if(pv.size() == 0)
		pv.push_back(Move(M_RESIGN));

	return pv;
}

Player::Node * Player::return_move(const Node * node, int toplay) const {
	Node * ret;
	ret = return_move_outcome(node, toplay);     if(ret) return ret; //win
	ret = return_move_outcome(node, -1);         if(ret) return ret; //unknown
	ret = return_move_outcome(node, 0);          if(ret) return ret; //tie
	ret = return_move_outcome(node, 3 - toplay); if(ret) return ret; //lose

	assert(ret);
	return NULL;
}

Player::Node * Player::return_move_outcome(const Node * node, int outcome) const {
	int val, maxval = -1000000000;

	Node * ret = NULL,
		 * child = node->children.begin(),
		 * end = node->children.end();

	for( ; child != end; child++){

		if(child->outcome != outcome)
			continue;

		val = child->exp.num();
//		val = child->exp.avg();

		if(maxval < val){
			maxval = val;
			ret = child;
		}
	}

	return ret;
}


