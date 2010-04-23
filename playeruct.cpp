
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
	//not sure how to handle the solver in combination with keeping the tree around between moves, nor adding knowledge in tree

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
		root.exp += 0;
		runs++;
		Board copy = rootboard;
		movelist.clear();
		walk_tree(copy, & root, movelist, 0);
	}while(!timeout && root.outcome == -1 && (maxruns == 0 || runs < maxruns));

//return the best one
	Node ret;
	if(root.outcome >= 0){
		ret.move = root.bestmove;
		ret.outcome = root.outcome;
		ret.exp += (root.outcome == 0 ? 0.5 : (root.outcome == rootboard.toplay()));
	}else{
		ret.exp += 0;
		for(int i = 0; i < root.numchildren; i++)
//			if(ret.exp.avg() < root.children[i].exp.avg())
			if(ret.exp.num() < root.children[i].exp.num())
				ret = root.children[i];
	}

	int runtime = time_msec() - starttime;
	time_used = (double)runtime/1000;

	string stats = "Finished " + to_str(runs) + " runs in " + to_str(runtime) + " msec\n";
	stats += "Game length: " + gamelen.to_s() + "\n";
	stats += "Tree depth:  " + treelen.to_s() + "\n";
	stats += "Move Score:  " + to_str(ret.exp.avg()) + "\n";
	stats += "Games/s:     " + to_str((int)((double)runs*1000/runtime)) + "\n";
	if(ret.outcome >= 0){
		stats += "Solved as a ";
		if(ret.outcome == 0)                       stats += "draw";
		else if(ret.outcome == rootboard.toplay()) stats += "win";
		else                                       stats += "loss";
		stats += "\n";
	}
	fprintf(stderr, "%s", stats.c_str());

	return ret.move;
}

vector<Move> Player::get_pv(){
	vector<Move> pv;

	Node * n = & root;
	while(n->numchildren){
		int maxi = 0;
		for(int i = 1; i < n->numchildren; i++)
//			if(n->children[maxi].exp.avg() < n->children[i].exp.avg())
			if(n->children[maxi].exp.num() < n->children[i].exp.num())
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
		Node * child = choose_move(node, toplay);

		if(child->outcome == -1){
			movelist.add(child->move, toplay);
			board.move(child->move, locality);

			int won = walk_tree(board, child, movelist, depth+1);

			child->exp += (won == 0 ? 0.5 : won == toplay);

			if(child->outcome == toplay){ //backup a win right away. Losses and ties can wait
				node->outcome = child->outcome;
				node->bestmove = child->move;
				if(!minimaxtree)
					nodes -= node->dealloc();
			}else if(ravefactor > min_rave){ //update the rave scores
				update_rave(node, movelist, won, toplay);
			}

			return won;
		}else{ //backup the win/loss/tie
			node->outcome = child->outcome;
			node->bestmove = child->move;
			if(!minimaxtree)
				nodes -= node->dealloc();
		}
	}

	int won = (minimax ? node->outcome : board.won());
	if(won >= 0 || node->exp.num() == 0 || nodes >= maxnodes){
	//do random game on this node, unless it's already the end
		if(won == -1)
			won = rand_game(board, movelist, node->move, depth);

		treelen.add(depth);

		if(ravefactor > min_rave){
			if(won == 0)
				movelist.clear();
			else
				movelist.clean(ravescale);
		}

		return won;
	}

//create children
	nodes += node->alloc(board.movesremain());

	Node * child = node->children;
	for(Board::MoveIterator move = board.moveit(); !move.done(); ++move){
		*child = Node(*move);

		if(minimax){
			child->outcome = board.test_win(*move);

			if(child->outcome == toplay){ //proven win from here, don't need children
				node->outcome = child->outcome;
				node->bestmove = *move;
				nodes -= node->dealloc();
				break;
			}
		}

		add_knowledge(board, node, child);

		child++;
	}
	return walk_tree(board, node, movelist, depth);
}

Player::Node * Player::choose_move(const Node * node, int toplay) const {
	int maxi = 0;
	float val, maxval = -1000000000;
	float logvisits = log(node->exp.num());

	float raveval = ravefactor*(skiprave == 0 || rand() % skiprave > 0); // = 0 or ravefactor

	for(unsigned int i = 0; i < node->numchildren; i++){
		Node * child = & node->children[i];

		if(child->outcome >= 0){
			if(child->outcome == toplay) //return a win immediately
				return child;

			val = (child->outcome == 0 ? -1 : -2); //-1 for tie so any unknown is better, -2 for loss so it's even worse
		}else{
			val = child->value(raveval, fpurgency) + explore*sqrt(logvisits/(child->exp.num() + 1));
		}

		if(maxval < val){
			maxval = val;
			maxi = i;
		}
	}

	return & node->children[maxi];
}

void Player::update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay){
	//update the rave score of all children that were played
	RaveMoveList::iterator rave = movelist.begin(), raveend = movelist.end();
	Node * child = node->children, * childend = node->children + node->numchildren;

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
	if(localreply){ //give exp boost for moves near the previous move
		int dist = node->move.dist(child->move);
		if(dist <= 2)
			child->know.addwins(3 - dist);
	}

	if(locality) //give exp boost for moves near previous stones
		child->know.addlosses(3 - board.local(child->move));

	if(connect) //boost for moves that connect to edges/corners
		child->know.addwins(board.test_connectivity(child->move));

	if(bridge && test_bridge_probe(board, node->move, child->move))
		child->know.addwins(5);
}


//return a list of positions where the opponent is probing your virtual connections
bool Player::test_bridge_probe(const Board & board, const Move & move, const Move & test){
	if(move.dist(test) != 1)
		return false;

	bool equals = false;

	int state = 0;
	int piece = 3 - board.get(move);
	for(int i = 0; i < 8; i++){
		Move cur = move + neighbours[i % 6];

		bool on = board.onboard2(cur);
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
		if(instantwin){
			for(Board::MoveIterator m = board.moveit(); !m.done(); ++m){
				if(board.test_win(*m) > 0){
					move = *m;
					goto makemove; //yes, evil...
				}
			}
		}
		if(!rolloutpattern || !check_pattern(board, move)){
			do{
				move = order[i++];
			}while(!board.valid_move(move));
		}

makemove:
		movelist.add(move, board.toplay());
		board.move(move);
		depth++;
	}

	gamelen.add(depth);

	return won;
}


