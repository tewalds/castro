
#include "player.h"
#include <cmath>
#include <string>
#include "string.h"

void Player::PlayerUCT::run(){
	RaveMoveList movelist;
	MTRand unitrand;
//	fprintf(stderr, "Runner start\n");
	while(!cancelled){
		player->sync.rdlock(); //wait for the write lock to come off

		while(!cancelled && player->sync.relock() && player->root.outcome == -1 && (maxruns == 0 || runs < maxruns)){ //has the lock and not solved yet
			runs++;
			player->root.exp.addvloss();
			Board copy = player->rootboard;
			movelist.clear();
			use_rave    = (unitrand() < player->userave);
			use_explore = (unitrand() < player->useexplore);
			walk_tree(copy, & player->root, movelist, 0);
		}
		player->sync.done(); //let the main thread know in case it was solved early
		player->sync.unlock();
	}
//	fprintf(stderr, "Runner exit\n");
}

//return the winner of the simulation
int Player::PlayerUCT::walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth){
	int toplay = board.toplay();

	if(!node->children.empty() && node->outcome == -1){
	//choose a child and recurse
		Node * child;
		do{
			child = choose_move(node, toplay, board.movesremain());

			if(child->outcome == -1){
				movelist.add(child->move, toplay);
				assert(board.move(child->move, (player->minimax == 0), player->locality));

				child->exp.addvloss();

				int won = walk_tree(board, child, movelist, depth+1);

				if(won == toplay) child->exp.addvwin();
				else if(won == 0) child->exp.addvtie();
				//else loss is already added

				if(!do_backup(node, child, toplay) && player->ravefactor > min_rave && node->children.num() > 1) //update the rave scores
					update_rave(node, movelist, won, toplay);

				return won;
			}
		}while(!do_backup(node, child, toplay));
	}

	int won = (player->minimax ? node->outcome : board.won());

	//create children if valid
	if(won == -1 && node->exp.num() >= player->visitexpand+1 && player->nodes < player->maxnodes)
		if(create_children(board, node, toplay))
			return walk_tree(board, node, movelist, depth);

	//do random game on this node if it's not already decided
	if(won == -1)
		won = rollout(board, movelist, node->move, depth);


	treelen.add(depth);

	if(player->ravefactor > min_rave){
		if(won == 0 || (player->shortrave && movelist.size() > gamelen.avg()))
			movelist.clear();
		else
			movelist.clean();
	}

	return won;
}

int Player::PlayerUCT::create_children(Board & board, Node * node, int toplay){
	if(!node->children.lock())
		return false;

	Node::Children temp;

	temp.alloc(board.movesremain());

	if(player->dists)
		dists.run(&board);

	int losses = 0;

	Node * child = temp.begin(),
	     * end   = temp.end(),
	     * loss  = NULL;
	Board::MoveIterator move = board.moveit();
	for(; !move.done() && child != end; ++move, ++child){
		*child = Node(*move);

		if(player->minimax){
			child->outcome = board.test_win(*move);

			if(player->minimax >= 2 && board.test_win(*move, 3 - board.toplay()) > 0){
				losses++;
				loss = child;
			}

			if(child->outcome == toplay){ //proven win from here, don't need children
				node->outcome = child->outcome;
				node->bestmove = *move;
				node->children.unlock();
				temp.dealloc();
				return true;
			}
		}

		add_knowledge(board, node, child);
	}
	//both end conditions should happen in parallel, so either both happen or neither do
	assert(move.done() == (child == end));

	//Make a macro move, add experience to the move so the current simulation continues past this move
	if(losses == 1){
		Node macro = *loss;
		temp.dealloc();
		temp.alloc(1);
		macro.exp.addwins(player->visitexpand);
		*(temp.begin()) = macro;
	}else if(losses >= 2){ //proven loss, but at least try to block one of them
		node->outcome = 3 - toplay;
		node->bestmove = loss->move;
		node->children.unlock();
		temp.dealloc();
		return true;
	}

	PLUS(player->nodes, temp.num());
	node->children.atomic_set(temp);
	temp.neuter();

	return true;
}

Player::Node * Player::PlayerUCT::choose_move(const Node * node, int toplay, int remain) const {
	float val, maxval = -1000000000;
	float logvisits = log(node->exp.num());

	float raveval = use_rave * (player->ravefactor + player->decrrave*remain);
	float explore = use_explore * player->explore;

	Node * ret = NULL,
		 * child = node->children.begin(),
		 * end = node->children.end();

	for(; child != end; child++){
		if(child->outcome >= 0){
			if(child->outcome == toplay) //return a win immediately
				return child;

			val = (child->outcome == 0 ? -1 : -2); //-1 for tie so any unknown is better, -2 for loss so it's even worse
		}else{
			val = child->value(raveval, player->knowledge, player->fpurgency) + explore*sqrt(logvisits/(child->exp.num() + 1));
		}

		if(maxval < val){
			maxval = val;
			ret = child;
		}
	}

	return ret;
}

bool Player::PlayerUCT::do_backup(Node * node, Node * backup, int toplay){
	if(node->outcome != -1) //already proven by a different thread
		return true;

	if(backup->outcome == -1) //not yet proven by this child, so no chance
		return false;

	if(backup->outcome != toplay){
		int val, maxval = -1000000000;
		backup = NULL;

		Node * child = node->children.begin(),
			 * end = node->children.end();

		for( ; child != end; child++){

			if(child->outcome == toplay){
				backup = child;
				val = 3;
				break;
			}
			else if(child->outcome == -1)
				val = 2;
			else if(child->outcome == 0)
				val = 1;
			else
				val = 0;

			if(maxval < val){
				maxval = val;
				backup = child;
			}
		}

		if(val == 2) //no win, but found an unknown
			return false;
	}

	if(CAS(node->outcome, -1, backup->outcome))
		node->bestmove = backup->bestmove;

	return true;
}

void Player::PlayerUCT::update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay){
	//update the rave score of all children that were played
	RaveMoveList::iterator rave = movelist.begin(), raveend = movelist.end();
	Node * child = node->children.begin(),
	     * childend = node->children.end();

	while(rave != raveend && child != childend){
		if(*rave == child->move){
			if(rave->player == toplay){
				child->rave.addvloss();
				if(rave->player == won)
					child->rave.addvwin();
			}
			rave++;
			child++;
		}else if(*rave > child->move){
			child++;
		}else{ //(*rave < child->move)
			rave++;
		}
	}
}

void Player::PlayerUCT::add_knowledge(Board & board, Node * node, Node * child){
	if(player->localreply){ //boost for moves near the previous move
		int dist = node->move.dist(child->move);
		if(dist < 4)
			child->know += player->localreply*(4 - dist);
	}

	if(player->locality) //boost for moves near previous stones
		child->know += player->locality*board.local(child->move);

	if(player->connect) //boost for moves that connect to edges/corners
		child->know += player->connect*board.test_connectivity(child->move);

	if(player->bridge && test_bridge_probe(board, node->move, child->move)) //boost for maintaining a virtual connection
		child->know += player->bridge;

	if(player->dists)
		child->know += player->dists * max(0, board.get_size() - dists.get(node->move));
}

//test whether this move is a forced reply to the opponent probing your virtual connections
bool Player::PlayerUCT::test_bridge_probe(const Board & board, const Move & move, const Move & test) const {
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
int Player::PlayerUCT::rollout(Board & board, RaveMoveList & movelist, Move move, int depth){
	int won;

	int num = board.movesremain();
	Move moves[num];

	int i = 0;
	for(Board::MoveIterator m = board.moveit(false, false); !m.done(); ++m)
		moves[i++] = *m;

	bool wrand = (player->weightedrandom && player->ravefactor > min_rave && player->root.children.num() > 1);

	int order[num];
	int * rmove = order;

	if(wrand){
		wtree.resize(num);

		Move * m = moves,
		     * mend = moves + num;
		Node * child = player->root.children.begin(),
		     * childend = player->root.children.end();

		while(m != mend && child != childend){
			if(*m == child->move){
				wtree.set_weight_fast(m - moves, max(0.1f, child->rave.avg() + (player->weightedknow * child->know / 100.f)));
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
		int i;
		for(i = 0; i < num; i++)
			order[i] = i;

		i = num;
		while(i > 1){
			int j = rand32() % i--;
			int tmp = order[j];
			order[j] = order[i];
			order[i] = tmp;
		}

//		random_shuffle(order, order + num);
	}

	int doinstwin = player->instwindepth;
	if(doinstwin < 0)
		doinstwin *= - board.get_size();

	while((won = board.won()) < 0){
		//do a complex choice
		move = rollout_choose_move(board, move, doinstwin);

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
		board.move(move, true, false);
		depth++;
	}

	gamelen.add(depth);

	//update the last good reply table
	if(player->lastgoodreply && won > 0){
		RaveMoveList::iterator rave = movelist.begin(), raveend = movelist.end();

		int m = -1;
		while(rave != raveend){
			if(m >= 0){
				if(rave->player == won && *rave != M_SWAP)
					goodreply[rave->player - 1][m] = *rave;
				else if(player->lastgoodreply == 2)
					goodreply[rave->player - 1][m] = M_UNKNOWN;
			}
			m = board.xy(*rave);
			++rave;
		}
	}

	return won;
}

Move Player::PlayerUCT::rollout_choose_move(Board & board, const Move & prev, int & doinstwin){
	//look for instant wins
	if(player->instantwin == 1 && --doinstwin >= 0){
		for(Board::MoveIterator m = board.moveit(); !m.done(); ++m)
			if(board.test_win(*m) > 0)
				return *m;
	}

	//look for instant wins and forced replies
	if(player->instantwin == 2 && --doinstwin >= 0){
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
	if(player->rolloutpattern){
		Move move = rollout_pattern(board, prev);
		if(move != M_UNKNOWN)
			return move;
	}

	//reuse the last good reply
	if(player->lastgoodreply && prev != M_SWAP){
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
Move Player::PlayerUCT::rollout_pattern(const Board & board, const Move & move){
	Move ret;
	int state = 0;
	int a = (++rollout_pattern_offset % 6);
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


