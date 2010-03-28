
#include "player.h"
#include "board.h"
#include <cmath>
#include <string>
#include "string.h"

Move Player::mcts(double time, int maxruns, int memlimit){
	maxnodes = memlimit*1024*1024/sizeof(Node);
	time_used = 0;
	runs = 0;

	treelen.reset();
	gamelen.reset();

	if(rootboard.won() >= 0 || (time <= 0 && maxruns == 0))
		return Move(M_RESIGN);

	timeout = false;
	Timer timer;
	if(time > 0)
		timer.set(time, bind(&Player::timedout, this));

	int starttime = time_msec();

/*
	//not sure how to handle the solver in combination with keeping the tree around between moves

	double ptime = time * prooftime * board.num_moves() / board.numcells(); //ie scale up from 0 to prooftime
	if(ptime > 0.01){ //minimum of 10ms worth of solve time
		Solver solver;
		Timer timer2 = Timer(ptime, bind(&Solver::timedout, &solver));
		int ret = solver.run_pnsab(board, (board.toplay() == 1 ? 2 : 1), memlimit/2);

		//if it found a win, just play it
		if(ret == 1){
			int runtime = time_msec() - starttime;
			fprintf(stderr, "Solved in %i msec\n", runtime);
			time_used = (double)runtime/1000;
			return solver.bestmove;
		}

		nodes = root.construct(solver.root, proofscore);
		solver.reset();
	}
*/

	runs = 0;
	RaveMoveList movelist(rootboard.movesremain());
	do{
		root.visits++;
		runs++;
		Board copy = rootboard;
		movelist.clear();
		walk_tree(copy, & root, movelist, 0);
	}while(!timeout && (maxruns == 0 || runs < maxruns));

//return the best one
	int maxi = 0;
	for(int i = 1; i < root.numchildren; i++)
//		if(root.children[maxi].winrate() < root.children[i].winrate())
		if(root.children[maxi].visits    < root.children[i].visits)
			maxi = i;

	int runtime = time_msec() - starttime;
	time_used = (double)runtime/1000;

	string stats = "Finished " + to_str(runs) + " runs in " + to_str(runtime) + " msec\n";
	stats += "Game length: " + gamelen.to_s() + "\n";
	stats += "Tree depth:  " + treelen.to_s() + "\n";
	stats += "Move Score:  " + to_str(root.children[maxi].winrate()) + "\n";
	stats += "Games/s:     " + to_str((int)((double)runs*1000/runtime)) + "\n";
	fprintf(stderr, "%s", stats.c_str());

	return root.children[maxi].move;
}

vector<Move> Player::get_pv(){
	vector<Move> pv;

	Node * n = & root;
	while(n->numchildren){
		int maxi = 0;
		for(int i = 1; i < n->numchildren; i++)
//			if(n->children[maxi].winrate() < n->children[i].winrate())
			if(n->children[maxi].visits    < n->children[i].visits)
				maxi = i;

		Node * child = & n->children[maxi];
		pv.push_back(child->move);
		n = child;
	}

	if(pv.size() == 0)
		pv.push_back(Move(M_RESIGN));

	return pv;
}

//return the winner of the simulation
int Player::walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth){
	int toplay = board.toplay();

	if(node->children){
	//choose a child and recurse
		Node * child = choose_move(node);

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
				movelist.clean(rootboard.toplay(), ravescale);
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

Player::Node * Player::choose_move(const Node * node) const {
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

	return & node->children[maxi];
}

void Player::update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay){
	//update the rave score of all children that were played
	unsigned int m = 0, c = 0, mls = movelist.size();
	while(m < mls && c < node->numchildren){
		Node * child = & node->children[c];
		const RaveMoveList::RaveMove & rave = movelist[m];

		if(rave.move == child->move){
			if(rave.player == toplay || opmoves){
				if(rave.player == won)
					child->rave += rave.score;
				child->ravevisits++;
			}
			m++;
			c++;
		}else if(rave.move > child->move){
			c++;
		}else{//(rave.move < child->move){
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


