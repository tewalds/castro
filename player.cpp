
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
		while(!cancelled && player->nodes < player->maxnodes && player->root.outcome == -1 && (maxruns == 0 || runs < maxruns) && player->lock.tryrdlock() == 0){ //not solved yet, has the lock, lock is last so it won't be holding the lock for gc
			runs++;
			iterate();
			player->lock.unlock();
		}

		if(player->root.outcome != -1 || !(maxruns == 0 || runs < maxruns)){ //let the main thread know in case it was solved early
			player->done.broadcast();
		}else if(player->nodes >= player->maxnodes){ //garbage collect
			player->lock.wrlock();
			if(player->nodes >= player->maxnodes){
				player->gclimit *= 0.8; //start with 80% of last time
				while(player->nodes >= player->maxnodes/2){
					fprintf(stderr, "Starting player GC with limit %i ... ", player->gclimit);
					player->garbage_collect(& player->root, player->gclimit);
					fprintf(stderr, "%.1f %% of tree remains\n", 100.0*player->nodes/player->maxnodes);
					player->gclimit *= 2;
				}
				player->gclimit /= 2; //undo the last doubling above
			}
			player->lock.unlock();
			player->runners.broadcast();
			continue; //skip the wait below, can't broadcast to self
		}
		player->runners.lock();
		player->runners.wait(); //wait for the broadcast to start
		player->runners.unlock();
	}
}


Move Player::genmove(double time, int maxruns){
	time_used = 0;
	int toplay = rootboard.toplay();

	if(rootboard.won() >= 0 || (time <= 0 && maxruns == 0))
		return Move(M_RESIGN);


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


//return the best one
	Node * ret = return_move(& root, toplay);

	time_used = Time() - starttime;

	DepthStats gamelen, treelen;
	runs = 0;
	DepthStats wintypes[2][4];
	for(unsigned int i = 0; i < threads.size(); i++){
		gamelen += threads[i]->gamelen;
		treelen += threads[i]->treelen;
		runs += threads[i]->runs;

		for(int a = 0; a < 2; a++)
			for(int b = 0; b < 4; b++)
				wintypes[a][b] += threads[i]->wintypes[a][b];

		threads[i]->reset();
	}

	string stats = "Finished " + to_str(runs) + " runs in " + to_str((int)(time_used*1000)) + " msec: " + to_str((int)(runs/time_used)) + " Games/s\n";
	if(runs > 0){
		stats += "Game length: " + gamelen.to_s() + "\n";
		stats += "Tree depth:  " + treelen.to_s() + "\n";
		stats += "Win Types:   ";
		stats += "P1: f " + to_str(wintypes[0][1].num) + ", b " + to_str(wintypes[0][2].num) + ", r " + to_str(wintypes[0][3].num) + "; ";
		stats += "P2: f " + to_str(wintypes[1][1].num) + ", b " + to_str(wintypes[1][2].num) + ", r " + to_str(wintypes[1][3].num) + "\n";

// show in verbose mode? stats are reset before getting back to the gtp command...
//		stats += "P1 fork:     " + wintypes[0][1].to_s() + "\n";
//		stats += "P1 bridge:   " + wintypes[0][2].to_s() + "\n";
//		stats += "P1 ring:     " + wintypes[0][3].to_s() + "\n";
//		stats += "P2 fork:     " + wintypes[1][1].to_s() + "\n";
//		stats += "P2 bridge:   " + wintypes[1][2].to_s() + "\n";
//		stats += "P2 ring:     " + wintypes[1][3].to_s() + "\n";
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
		SolverAB solver;
		solver.set_board(rootboard);
		solver.solve(0.1);
		node->bestmove = solver.bestmove;
	}

	assert(node->bestmove != M_UNKNOWN);

	return ret;
}

void Player::garbage_collect(Node * node, unsigned int limit){
	Node * child = node->children.begin(),
		 * end = node->children.end();

	for( ; child != end; child++){
		if(child->exp.num() < limit)
			nodes -= child->dealloc();
		else
			garbage_collect(child, limit);
	}
}

