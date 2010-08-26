
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
	int toplay = rootboard.toplay();

	if(rootboard.won() >= 0 || (time <= 0 && maxruns == 0))
		return Move(M_RESIGN);


	Time starttime;

	Timer timer;
	if(time > 0)
		timer.set(time, bind(&Player::timedout, this));

	//solve to depth 1 just in case this is a terminal node
	if(root.outcome == -1 && root.children.empty() && rootboard.num_moves() > 0){
		Solver solver;
		solver.solve_ab(rootboard, 1, 2);
		if(solver.outcome >= 0){
			root.outcome = solver.outcome;
			root.bestmove = solver.bestmove;
		}
	}

	int runs = 0;
	for(unsigned int i = 0; i < threads.size(); i++){
		runs += threads[i]->runs;
		threads[i]->reset();
		threads[i]->maxruns = maxruns;
	}
	if(runs)
		fprintf(stderr, "Pondered %i runs\n", runs);

	root.exp.addwins(visitexpand+1); //+1 to compensate for the virtual loss

	//let them run!
	if(root.outcome == -1){
		if(!ponder)
			sync.unlock(); //remove the write lock
	
		sync.wait(); //wait for the timer or solved

		if(!ponder)
			sync.wrlock(); //stop the runners
	}

//return the best one
	Node * ret = return_move(& root, toplay);

	time_used = Time() - starttime;

	DepthStats gamelen, treelen;
	runs = 0;
	for(unsigned int i = 0; i < threads.size(); i++){
		gamelen += threads[i]->gamelen;
		treelen += threads[i]->treelen;
		runs += threads[i]->runs;
		threads[i]->reset();
	}

	string stats = "Finished " + to_str(runs) + " runs in " + to_str((int)(time_used*1000)) + " msec: " + to_str((int)(runs/time_used)) + " Games/s\n";
	if(runs > 0){
		stats += "Game length: " + gamelen.to_s() + "\n";
		stats += "Tree depth:  " + treelen.to_s() + "\n";
	}
	if(ret)
		stats += "Move Score:  " + to_str(ret->exp.avg()) + "\n";
	if(root.outcome >= 0){
		stats += "Solved as a ";
		if(root.outcome == 0)           stats += "draw";
		else if(root.outcome == toplay) stats += "win";
		else                            stats += "loss";
		stats += "\n";
	}
	fprintf(stderr, "%s", stats.c_str());

	return root.bestmove;
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
	double val, maxval = -10000000000.0; //10 billion

	Node * ret = NULL,
		 * child = node->children.begin(),
		 * end = node->children.end();

	for( ; child != end; child++){
		if(child->outcome != -1){
			if(child->outcome == toplay) val =  8000000000.0 - child->exp.num(); //shortest win
			else if(child->outcome == 0) val = -4000000000.0 + child->exp.num(); //longest tie
			else                         val = -8000000000.0 + child->exp.num(); //longest loss
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
		Solver solver;
		solver.solve_ab(rootboard, 1, 2);
		node->bestmove = solver.bestmove;
	}

	assert(node->bestmove != M_UNKNOWN);

	return ret;
}

