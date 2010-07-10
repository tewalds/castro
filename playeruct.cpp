
#include "player.h"
#include "board.h"
#include <cmath>
#include <string>
#include "string.h"

#include "weightedrandtree.h"

void Player::PlayerUCT::run(){
	RaveMoveList movelist;
	while(1){
		player->lock.rdlock(); //wait for the write lock to come off
		do{
			if(player->root.outcome != -1){ //solved, return early
				player->lock.unlock();
				player->cond.broadcast(); //let the main thread know
				break;
			}
			runs++;
			Board copy = player->rootboard;
			movelist.clear();
			walk_tree(copy, & player->root, movelist, 0);
			player->lock.unlock();
		}while(player->lock.tryrdlock() == 0); //fails when the write lock comes back
	}
}

//return the winner of the simulation
int Player::PlayerUCT::walk_tree(Board & board, Node * node, RaveMoveList & movelist, int depth){
	int toplay = board.toplay();

	if(!node->children.empty() && node->outcome == -1){
	//choose a child and recurse
		Node * child = choose_move(node, toplay);

		if(child->outcome == -1){
			movelist.add(child->move, toplay);
			assert(board.move(child->move, player->locality));

			int won = walk_tree(board, child, movelist, depth+1);

			child->exp += (won == 0 ? 0.5 : won == toplay);

			if(child->outcome == toplay){ //backup a win right away. Losses and ties can wait
				node->outcome = child->outcome;
				node->bestmove = child->move;
			}else if(player->ravefactor > min_rave && node->children.num() > 1){ //update the rave scores
				update_rave(node, movelist, won, toplay);
			}

			return won;
		}else{ //backup the win/loss/tie
			node->outcome = child->outcome;
			node->bestmove = child->move;
		}
	}

	int won = (player->minimax ? node->outcome : board.won());

	//create children if valid
	if(won == -1 && node->exp.num() >= player->visitexpand && player->nodes < player->maxnodes)
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
			movelist.clean(player->ravescale);
	}

	return won;
}

int Player::PlayerUCT::create_children(Board & board, Node * node, int toplay){
	if(!node->children.lock())
		return false;

	Node::Children temp;

	temp.alloc(board.movesremain());

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
				if(node != & player->root){
					node->children.unlock();
					temp.dealloc();
					return true;
				}
				losses = -1000000; //set to something very negative to skip the part below
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
		if(node != &player->root){
			node->children.unlock();
			temp.dealloc();
			return true;
		}
	}

	player->nodes += temp.num();
	node->children.atomic_set(temp);
	temp.neuter();

	return true;
}

Player::Node * Player::PlayerUCT::choose_move(const Node * node, int toplay) const {
	float val, maxval = -1000000000;
	float logvisits = log(node->exp.num());

	float raveval = player->ravefactor*(player->skiprave == 0 || rand() % player->skiprave > 0); // = 0 or ravefactor

	Node * ret = NULL,
		 * child = node->children.begin(),
		 * end = node->children.end();

	for(; child != end; child++){
		if(child->outcome >= 0){
			if(child->outcome == toplay) //return a win immediately
				return child;

			val = (child->outcome == 0 ? -1 : -2); //-1 for tie so any unknown is better, -2 for loss so it's even worse
		}else{
			val = child->value(raveval, player->knowfactor, player->fpurgency) + player->explore*sqrt(logvisits/(child->exp.num() + 1));
		}

		if(maxval < val){
			maxval = val;
			ret = child;
		}
	}

	return ret;
}

void Player::PlayerUCT::update_rave(const Node * node, const RaveMoveList & movelist, int won, int toplay){
	//update the rave score of all children that were played
	RaveMoveList::iterator rave = movelist.begin(), raveend = movelist.end();
	Node * child = node->children.begin(),
	     * childend = node->children.end();

	while(rave != raveend && child != childend){
		if(*rave == child->move){
			if(rave->player == toplay || player->opmoves)
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

void Player::PlayerUCT::add_knowledge(Board & board, Node * node, Node * child){
	if(player->localreply){ //boost for moves near the previous move
		int dist = node->move.dist(child->move);
		if(dist < 4)
			child->know += 4 - dist;
	}

	if(player->locality) //boost for moves near previous stones
		child->know += board.local(child->move);

	if(player->connect) //boost for moves that connect to edges/corners
		child->know += board.test_connectivity(child->move);

	if(player->bridge && test_bridge_probe(board, node->move, child->move)) //boost for maintaining a virtual connection
		child->know += 5;
}

//test whether this move is a forced reply to the opponent probing your virtual connections
bool Player::PlayerUCT::test_bridge_probe(const Board & board, const Move & move, const Move & test){
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
	for(Board::MoveIterator m = board.moveit(false); !m.done(); ++m)
		moves[i++] = *m;

	bool wrand = (player->weightedrandom && player->ravefactor > min_rave && player->root.children.num() > 1);

	WeightedRandTree wtree;
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

Move Player::PlayerUCT::rollout_choose_move(Board & board, const Move & prev){
	//look for instant wins
	if(player->instantwin == 1){
		for(Board::MoveIterator m = board.moveit(); !m.done(); ++m)
			if(board.test_win(*m) > 0)
				return *m;
	}

	//look for instant wins and forced replies
	if(player->instantwin == 2){
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


