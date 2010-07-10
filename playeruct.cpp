
#include "player.h"
#include "board.h"
#include <cmath>
#include <string>
#include "string.h"

#include "weightedrandtree.h"

Move Player::mcts(double time, int maxruns, uint64_t memlimit){
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


	double ptime = time * prooftime * rootboard.num_moves() / rootboard.numcells(); //ie scale up from 0 to prooftime
	if(ptime > 0.01){ //minimum of 10ms worth of solve time
		Timer timer2 = Timer(ptime, bind(&Solver::timedout, &solver));
		int ret = solver.run_pnsab(rootboard, (rootboard.toplay() == 1 ? 2 : 1), memlimit/2);

		//if it found a win, just play it
		if(ret == 1){
			int runtime = time_msec() - starttime;
			fprintf(stderr, "Solved in %i msec\n", runtime);
			time_used = (double)runtime/1000;
			return solver.bestmove;
		}

	//not sure how to transfer the knowledge in the proof tree to the UCT tree
	//the difficulty is taking into account keeping the tree between moves and adding knowledge heuristics to unproven nodes
//		nodes = root.construct(solver.root, proofscore);
		solver.reset();
	}


	root.outcome = -1;
	runs = 0;
	RaveMoveList movelist(rootboard.movesremain());
	root.exp.addwins(visitexpand);
	do{
		root.exp += 0;
		runs++;
		Board copy = rootboard;
		movelist.clear();
		walk_tree(copy, & root, movelist, 0);
	}while(!timeout && root.outcome == -1 && (maxruns == 0 || runs < maxruns));

//return the best one
	Node * ret = return_move(& root, rootboard.toplay());

	int runtime = time_msec() - starttime;
	time_used = (double)runtime/1000;

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


//return the winner of the simulation
int Player::walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth){
	int toplay = board.toplay();

	if(!node->children.empty() && node->outcome == -1){
	//choose a child and recurse
		Node * child = choose_move(node, toplay);

		if(child->outcome == -1){
			movelist.add(child->move, toplay);
			assert(board.move(child->move, locality));

			int won = walk_tree(board, child, movelist, depth+1);

			child->exp += (won == 0 ? 0.5 : won == toplay);

			if(child->outcome == toplay){ //backup a win right away. Losses and ties can wait
				node->outcome = child->outcome;
				node->bestmove = child->move;
				if(!minimaxtree && node != &root)
					nodes -= node->dealloc();
			}else if(ravefactor > min_rave && node->children.num() > 1){ //update the rave scores
				update_rave(node, movelist, won, toplay);
			}

			return won;
		}else{ //backup the win/loss/tie
			node->outcome = child->outcome;
			node->bestmove = child->move;
			if(!minimaxtree && node != &root)
				nodes -= node->dealloc();
		}
	}

	int won = (minimax ? node->outcome : board.won());
	if(won >= 0 || node->exp.num() < visitexpand || nodes >= maxnodes){
	//do random game on this node, unless it's already the end
		if(won == -1)
			won = rollout(board, movelist, node->move, depth);

		treelen.add(depth);

		if(ravefactor > min_rave){
			if(won == 0 || (shortrave && movelist.size() > gamelen.avg()))
				movelist.clear();
			else
				movelist.clean(ravescale);
		}

		return won;
	}

//create children
	nodes += node->alloc(board.movesremain());

	int losses = 0;

	Node * child = node->children.begin(),
	     * end   = node->children.end(),
	     * loss  = NULL;
	Board::MoveIterator move = board.moveit();
	for(; !move.done() && child != end; ++move, ++child){
		*child = Node(*move);

		if(minimax){
			child->outcome = board.test_win(*move);

			if(minimax >= 2 && board.test_win(*move, 3 - board.toplay()) > 0){
				losses++;
				loss = child;
			}

			if(child->outcome == toplay){ //proven win from here, don't need children
				node->outcome = child->outcome;
				node->bestmove = *move;
				losses = -1000000; //set to something very negative to skip the part below
				if(node != &root){
					nodes -= node->dealloc();
					break;
				}
			}
		}

		add_knowledge(board, node, child);
	}
	//both end conditions should happen in parallel, so either both happen or neither do
	assert(move.done() == (child == end));

	//Make a macro move, add experience to the move so the current simulation continues past this move
	if(losses == 1){
		Node temp = *loss;
		nodes -= node->dealloc();
		nodes += node->alloc(1);
		temp.exp.addwins(visitexpand);
		*(node->children.begin()) = temp;
	}else if(losses >= 2){ //proven loss, but at least try to block one of them
		node->outcome = 3 - toplay;
		node->bestmove = loss->move;
		if(node != &root)
			nodes -= node->dealloc();
	}

	return walk_tree(board, node, movelist, depth);
}

Player::Node * Player::choose_move(const Node * node, int toplay) const {
	float val, maxval = -1000000000;
	float logvisits = log(node->exp.num());

	float raveval = ravefactor*(skiprave == 0 || rand() % skiprave > 0); // = 0 or ravefactor

	Node * ret = NULL,
		 * child = node->children.begin(),
		 * end = node->children.end();

	for(; child != end; child++){
		if(child->outcome >= 0){
			if(child->outcome == toplay) //return a win immediately
				return child;

			val = (child->outcome == 0 ? -1 : -2); //-1 for tie so any unknown is better, -2 for loss so it's even worse
		}else{
			val = child->value(raveval, knowfactor, fpurgency) + explore*sqrt(logvisits/(child->exp.num() + 1));
		}

		if(maxval < val){
			maxval = val;
			ret = child;
		}
	}

	return ret;
}

void Player::update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay){
	//update the rave score of all children that were played
	RaveMoveList::iterator rave = movelist.begin(), raveend = movelist.end();
	Node * child = node->children.begin(),
	     * childend = node->children.end();

	while(rave != raveend && child != childend){
		if(*rave == child->move){
			if(rave->player == toplay || opmoves)
				child->rave += (rave->player == won ? rave->score : 0);
			rave++;
			child++;
		}else if(*rave > child->move){
			child++;
		}else{ //(*rave < child->move)
			rave++;
		}
	}
}

void Player::add_knowledge(Board & board, Node * node, Node * child){
	if(localreply){ //boost for moves near the previous move
		int dist = node->move.dist(child->move);
		if(dist < 4)
			child->know += 4 - dist;
	}

	if(locality) //boost for moves near previous stones
		child->know += board.local(child->move);

	if(connect) //boost for moves that connect to edges/corners
		child->know += board.test_connectivity(child->move);

	if(bridge && test_bridge_probe(board, node->move, child->move)) //boost for maintaining a virtual connection
		child->know += 5;
}

//test whether this move is a forced reply to the opponent probing your virtual connections
bool Player::test_bridge_probe(const Board & board, const Move & move, const Move & test){
	if(move.dist(test) != 1)
		return false;

	bool equals = false;

	int state = 0;
	int piece = 3 - board.get(move);
	for(int i = 0; i < 8; i++){
		Move cur = move + neighbours[i % 6];

		bool on = board.onboard(cur);
		int v;
		if(on)
			v = board.get(cur);

	//state machine that progresses when it see the pattern, but counting borders as part of the pattern
		if(state == 0){
			if(!on || v == piece)
				state = 1;
			//else state = 0;
		}else if(state == 1){
			if(on){
				if(v == 0){
					state = 2;
					equals = (test == cur);
				}else if(v != piece)
					state = 0;
				//else (v==piece) => state = 1;
			}
			//else state = 1;
		}else{ // state == 2
			if(!on || v == piece){
				if(equals)
					return true;
				state = 1;
			}else{
				state = 0;
			}
		}
	}
	return false;
}

///////////////////////////////////////////


//play a random game starting from a board state, and return the results of who won
int Player::rollout(Board & board, RaveMoveList & movelist, Move move, int depth){
	int won;

	int num = board.movesremain();
	Move moves[num];

	int i = 0;
	for(Board::MoveIterator m = board.moveit(false); !m.done(); ++m)
		moves[i++] = *m;

	bool wrand = (weightedrandom && ravefactor > min_rave && root.children.num() > 1);

	WeightedRandTree wtree;
	int order[num];
	int * rmove = order;

	if(wrand){
		wtree.resize(num);

		Move * m = moves,
		     * mend = moves + num;
		Node * child = root.children.begin(),
		     * childend = root.children.end();

		while(m != mend && child != childend){
			if(*m == child->move){
				wtree.set_weight_fast(m - moves, max(0.1f, child->rave.avg()));
				m++;
				child++;
			}else if(*m > child->move){
				child++;
			}else{ //(m < child->move)
				wtree.set_weight_fast(m - moves, 0.1f);
				m++;
			}
		}

		while(m != mend){
			wtree.set_weight_fast(m - moves, 0.1f);
			m++;
		}

		wtree.rebuild_tree();
	}else{
		for(int i = 0; i < num; i++)
			order[i] = i;
		random_shuffle(order, order + num);
	}

	while((won = board.won()) < 0){
		//do a complex choice
		move = rollout_choose_move(board, move);

		//or the simple random choice if complex found nothing
		if(move == M_UNKNOWN){
			do{
				if(wrand){
					int j = wtree.choose();
					wtree.set_weight(j, 0);
					move = moves[j];
				}else{
					move = moves[*rmove];
					rmove++;
				}
			}while(!board.valid_move_fast(move));
		}

		movelist.add(move, board.toplay());
		board.move(move);
		depth++;
	}

	gamelen.add(depth);

	//update the last good reply table
	if(lastgoodreply && won > 0){
		RaveMoveList::iterator rave = movelist.begin(), raveend = movelist.end();

		int m = -1;
		while(rave != raveend){
			if(m >= 0){
				if(rave->player == won && *rave != M_SWAP)
					goodreply[rave->player - 1][m] = *rave;
				else
					goodreply[rave->player - 1][m] = M_UNKNOWN;
			}
			m = board.xy(*rave);
			++rave;
		}
	}

	return won;
}

Move Player::rollout_choose_move(Board & board, const Move & prev){
	//look for instant wins
	if(instantwin == 1){
		for(Board::MoveIterator m = board.moveit(); !m.done(); ++m)
			if(board.test_win(*m) > 0)
				return *m;
	}

	//look for instant wins and forced replies
	if(instantwin == 2){
		Move loss = M_UNKNOWN;
		for(Board::MoveIterator m = board.moveit(); !m.done(); ++m){
			if(board.test_local(*m)){
				if(board.test_win(*m, board.toplay()) > 0) //win
					return *m;
				if(board.test_win(*m, 3 - board.toplay()) > 0) //lose
					loss = *m;
			}
		}
		if(loss != M_UNKNOWN)
			return loss;
	}

	//force a bridge reply
	if(rolloutpattern){
		Move move = rollout_pattern(board, prev);
		if(move != M_UNKNOWN)
			return move;
	}

	//reuse the last good reply
	if(lastgoodreply && prev != M_SWAP){
		Move move = goodreply[board.toplay()-1][board.xy(prev)];
		if(move != M_UNKNOWN && board.valid_move_fast(move))
			return move;
	}

	return M_UNKNOWN;
}

//look for good forced moves. In this case I only look for keeping a virtual connection active
//so looking from the last played position's perspective, which is a move by the opponent
//if you see a pattern of mine, empty, mine in the circle around the last move, their move
//would break the virtual connection, so should be played
//a virtual connection to a wall is also important
Move Player::rollout_pattern(const Board & board, const Move & move){
	Move ret;
	int state = 0;
	int a = rand() % 6;
	int piece = 3 - board.get(move);
	for(int i = 0; i < 8; i++){
		Move cur = move + neighbours[(i+a)%6];

		bool on = board.onboard(cur);
		int v;
		if(on)
			v = board.get(cur);

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
				return ret;
			}else{
				state = 0;
			}
		}
	}
	return M_UNKNOWN;
}



//////////////////////////////////////////////////////


void Player::solve(double time, uint64_t memlimit){
	visitexpand = 10;
	mcts(time/10, 0, memlimit/10);
	timeout = false;

	Timer timer = Timer(time*0.9, bind(&Player::timedout, this));
	int starttime = time_msec();

	for(double solvetime = 1; !timeout && root.outcome == -1; solvetime *= 2){
		fprintf(stderr, "Attempting to solve at %.2f s per node\n", solvetime);
		for(double rate = 0.11; !timeout && root.outcome == -1 && rate <= 0.5; rate += 0.05){
			fprintf(stderr, "Solving with rate: %.2f\n", rate);
			solve_recurse(rootboard, &root, rate, solvetime, 9*memlimit/10, 0);
		}
	}

	fprintf(stderr, "Finished solve_player in %d msec\n", time_msec() - starttime);
}

void Player::solve_recurse(Board & board, Node * node, double rate, double solvetime, uint64_t memlimit, int depth){
	if(!node->children.empty() && node->exp.num() > 200){
		Node * child = node->children.begin(),
			 * childend = node->children.end();

		for( ; node->outcome == -1 && child != childend; ++child){
			if(child->outcome == -1){
				Board next = board;
				next.move(child->move);

				solve_recurse(next, child, rate, solvetime, memlimit, depth+1);

				//back up the outcome....
				if(child->outcome >= 0){
					Node * choice = choose_move(node, board.toplay());
					if(choice->outcome >= 0){
						node->outcome = choice->outcome;
						node->bestmove = choice->move;
					}
				}
			}
		}
	}else{ //only solve nodes that are or are near leaves
		//make sure the child has at least 10 simulations, one extra per pass
		do{
			Board copy = board;
			RaveMoveList movelist(rootboard.movesremain());
			int won = rollout(copy, movelist, node->move, depth);
			node->exp += (won == 0 ? 0.5 : won == board.toplay());
		}while(node->exp.num() < 50);

		double avg = node->exp.avg();
		if(node->outcome == -1 && ((avg < rate && rate < avg + 0.06) || (avg > 1 - rate && 1 - rate > avg - 0.06))){
			solver.solve_dfpnsab(board, solvetime, memlimit);

			if(solver.outcome >= 0){
				node->outcome = solver.outcome;
				node->bestmove = solver.bestmove;
			}
		}
	}
}

