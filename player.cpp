
#include "player.h"
#include "board.h"
#include <cmath>
#include <string>
#include "string.h"
#include "solverab.h"
#include "timer.h"

void Player::PlayerThread::run(){
//	player->runners.lock();
//	player->runners.wait(); //wait for the broadcast to start
//	player->runners.unlock();
	while(!cancelled){
		while(!cancelled && player->ctmem.memused() < player->maxmem && player->root.outcome == -1 && (maxruns == 0 || runs < maxruns) && player->lock.tryrdlock() == 0){ //not solved yet, has the lock, lock is last so it won't be holding the lock for gc
			runs++;
			iterate();
			player->lock.unlock();
		}

		if(player->root.outcome != -1 || !(maxruns == 0 || runs < maxruns)){ //let the main thread know in case it was solved early
			player->done.broadcast();
		}else if(player->ctmem.memused() >= player->maxmem){ //garbage collect
			if(player->gcbarrier.wait()){
				fprintf(stderr, "Starting player GC with limit %i ... ", player->gclimit);
				uint64_t nodesbefore = player->nodes;
				Board copy = player->rootboard;
				player->garbage_collect(copy, & player->root, player->gclimit);
				player->ctmem.compact();
				fprintf(stderr, "%.1f %% of tree remains\n", 100.0*player->nodes/nodesbefore);

				if(player->ctmem.memused() >= player->maxmem/2)
					player->gclimit *= 1.3;
				else if(player->gclimit > 5)
					player->gclimit *= 0.9; //slowly decay to a minimum of 5
			}
			player->gcbarrier.wait();
		}else{
			player->runners.lock();
			player->runners.wait(); //wait for the broadcast to start
			player->runners.unlock();
		}
	}
}


Player::Node * Player::genmove(double time, int maxruns){
	time_used = 0;
	int toplay = rootboard.toplay();

	if(rootboard.won() >= 0 || (time <= 0 && maxruns == 0))
		return NULL;


	Time starttime;

	Timer timer;
	if(time > 0)
		timer.set(time, bind(&Player::timedout, this));

	int runs = 0;
	for(unsigned int i = 0; i < threads.size(); i++){
		runs += threads[i]->runs;
		threads[i]->reset();
		threads[i]->maxruns = maxruns;
	}
	if(runs)
		fprintf(stderr, "Pondered %i runs\n", runs);

	//let them run!
	if(!running){
		lock.unlock(); //remove the write lock
		runners.broadcast();
		running = true;
	}

	done.lock();
	done.wait(); //wait for the timer or solved
	done.unlock();

	if(!ponder || root.outcome != -1){
		lock.wrlock(); //stop the runners
		running = false;
	}

	time_used = Time() - starttime;

//return the best one
	return return_move(& root, toplay);
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

