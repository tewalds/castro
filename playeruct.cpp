
#include "player.h"
#include "board.h"
#include <cmath>
#include <string>
#include "string.h"

void Player::play_uct(const Board & board, double time, int maxruns, int memlimit){
	maxnodes = memlimit*1024*1024/sizeof(Node);
	nodes = 0;
	time_used = 0;
	runs = 0;

	principle_variation.clear();
	treelen.reset();
	gamelen.reset();

	bestmove = Move(M_RESIGN);
	timeout = false;

	if(board.won() >= 0 || (time <= 0 && maxruns == 0))
		return;

	Timer timer;
	if(time > 0)
		timer.set(time, bind(&Player::timedout, this));

	int starttime = time_msec();
	runs = 0;

	Node root;

	double ptime = time * prooftime * board.num_moves() / board.numcells(); //ie scale up from 0 to prooftime
	if(ptime > 0.01){ //minimum of 10ms worth of solve time
		Solver solver;
		Timer timer2 = Timer(ptime, bind(&Solver::timedout, &solver));
		int ret = solver.run_pnsab(board, (board.toplay() == 1 ? 2 : 1), memlimit/2);

		//if it found a win, just play it
		if(ret == 1){
			bestmove = solver.bestmove;

			int runtime = time_msec() - starttime;
			fprintf(stderr, "Solved in %i msec\n", runtime);
			time_used = (double)runtime/1000;
			return;
		}

		nodes = root.construct(solver.root, proofscore);
		solver.reset();
	}

	cur_player = board.toplay();
	RaveMoveList movelist(board.movesremain());
	do{
		root.visits++;
		runs++;
		Board copy = board;
		movelist.clear();
		walk_tree(copy, & root, movelist, 0);
	}while(!timeout && (maxruns == 0 || runs < maxruns));


//return the best one
	int maxi = 0;
	for(int i = 1; i < root.numchildren; i++)
//		if(root.children[maxi].winrate() < root.children[i].winrate())
		if(root.children[maxi].visits    < root.children[i].visits)
			maxi = i;

	bestmove = root.children[maxi].move;

	int runtime = time_msec() - starttime;

	time_used = (double)runtime/1000;

	string stats = "Finished " + to_str(runs) + " runs in " + to_str(runtime) + " msec\n";
	stats += "Game length: " + gamelen.to_s() + "\n";
	stats += "Tree depth:  " + treelen.to_s() + "\n";
	stats += "Move Score:  " + to_str(root.children[maxi].winrate()) + "\n";
	stats += "Games/s:     " + to_str((int)((double)runs*1000/runtime)) + "\n";
	fprintf(stderr, "%s", stats.c_str());

//return the principle variation
	Node * n = & root;
	while(n->numchildren){
		int maxi = 0;
		for(int i = 1; i < n->numchildren; i++)
//			if(n->children[maxi].winrate() < n->children[i].winrate())
			if(n->children[maxi].visits    < n->children[i].visits)
				maxi = i;

		Node * child = & n->children[maxi];
		principle_variation.push_back(child->move);
		n = child;
	}
}

//return the winner of the simulation
int Player::walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth){
	int toplay = board.toplay();

	if(node->children){
	//choose a child and recurse
		int maxi = 0;
		float val, maxval = -1000000000;
		float logvisits = log(node->visits);
		Node * child;

		float raveval = ravefactor*(skiprave == 0 || rand() % skiprave > 0); // = 0 or ravefactor

		for(unsigned int i = 0; i < node->numchildren; i++){
			child = & node->children[i];

			val = child->value(raveval, fpurgency) + explore*sqrt(logvisits/(child->visits+1));

			if(maxval < val){
				maxval = val;
				maxi = i;
			}
		}
		
		//recurse on the chosen child
		child = & node->children[maxi];
		board.move(child->move);
		movelist.add(child->move);

		int won = walk_tree(board, child, movelist, depth+1);

		child->visits++;
		child->score += (won == 0 ? 0.5 : won == toplay);

		//update the rave scores
		if(ravefactor > min_rave)
			update_rave(node, movelist, won, toplay);

		return won;
	}

	int won = board.won();
	if(won >= 0 || node->visits == 0 || nodes >= maxnodes){
	//do random game on this node, unless it's already the end
		if(won == -1)
			won = rand_game(board, movelist, node->move, depth);

		treelen.add(depth);

		if(ravefactor > min_rave){
			if(won == 0)
				movelist.clear();
			else
				movelist.clean(cur_player, ravescale);
		}

		return won;
	}

//create children
	nodes += node->alloc(board.movesremain());

	int i = 0;
	for(Board::MoveIterator move = board.moveit(); !move.done(); ++move)
		node->children[i++] = Node(*move);

	return walk_tree(board, node, movelist, depth);
}

void Player::update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay){
	//update the rave score of all children that were played
	unsigned int m = 0, c = 0;
	while(m < movelist.size() && c < node->numchildren){
		Node * child = & node->children[c];

		if(movelist[m].move == child->move){
			if(movelist[m].player == toplay || opmoves){
				if(movelist[m].player == won)
					child->rave += movelist[m].score;
				child->ravevisits++;
			}
			m++;
			c++;
		}else if(movelist[m].move > child->move){
			c++;
		}else{//(movelist[m].move < child->move){
			m++;
		}
	}
}

//look for good forced moves. In this case I only look for keeping a virtual connection active
//so looking from the last played position's perspective, which is a move by the opponent
//if you see a pattern of mine, empty, mine in the circle around the last move, their move
//would break the virtual connection, so should be played
//a virtual connection to a wall is also important
bool Player::check_pattern(const Board & board, Move & move){
	Move ret;
	int state = 0;
	int a = rand() % 6;
	int piece = 3 - board.get(move);
	for(int i = 0; i < 8; i++){
		Move cur = move + neighbours[(i+a)%6];

		bool on = board.onboard2(cur);
		int v;
		if(on)
			v = board.get(cur);

/*
	//state machine that progresses when it see the pattern, but only taking pieces into account
		if(state == 0){
			if(on && v == piece)
				state = 1;
			//else state = 0;
		}else if(state == 1){
			if(on){
				if(v == 0){
					state = 2;
					ret = cur;
				}else if(v != piece)
					state = 0;
				//else (v==piece) => state = 1;
			}else
				state = 0;
		}else{ // state == 2
			if(on && v == piece){
				move = ret;
				return true;
			}else{
				state = 0;
			}
		}

/*/
	//state machine that progresses when it see the pattern, but counting borders as part of the pattern
		if(state == 0){
			if(!on || v == piece)
				state = 1;
			//else state = 0;
		}else if(state == 1){
			if(on){
				if(v == 0){
					state = 2;
					ret = cur;
				}else if(v != piece)
					state = 0;
				//else (v==piece) => state = 1;
			}
			//else state = 1;
		}else{ // state == 2
			if(!on || v == piece){
				move = ret;
				return true;
			}else{
				state = 0;
			}
		}
//*/
	}
	return false;
}

//play a random game starting from a board state, and return the results of who won	
int Player::rand_game(Board & board, RaveMoveList & movelist, Move move, int depth){
	int won;

	Move order[board.movesremain()];

	int i = 0;
	for(Board::MoveIterator m = board.moveit(); !m.done(); ++m)
		order[i++] = *m;

	random_shuffle(order, order + i);

	i = 0;
	while((won = board.won()) < 0){
		if(!rolloutpattern || !check_pattern(board, move)){
			do{
				move = order[i++];
			}while(!board.valid_move(move));
		}

		board.move(move);
		movelist.add(move);
		depth++;
	}

	gamelen.add(depth);

	return won;
}


