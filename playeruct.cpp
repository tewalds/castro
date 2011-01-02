
#include "player.h"
#include <cmath>
#include <string>
#include "string.h"

void Player::PlayerUCT::iterate(){
	movelist.reset(&(player->rootboard));
	player->root.exp.addvloss();
	Board copy = player->rootboard;
	use_rave    = (unitrand() < player->userave);
	use_explore = (unitrand() < player->useexplore);
	walk_tree(copy, & player->root, 0);
	player->root.exp.addv(movelist.getexp(3-player->rootboard.toplay()));
}

void Player::PlayerUCT::walk_tree(Board & board, Node * node, int depth){
	int toplay = board.toplay();

	if(!node->children.empty() && node->outcome < 0){
	//choose a child and recurse
		Node * child;
		do{
			int remain = board.movesremain();
			child = choose_move(node, toplay, remain);

			if(child->outcome < 0){
				movelist.addtree(child->move, toplay);

				if(!board.move(child->move, (player->minimax == 0), player->locality)){
					logerr("move failed: " + child->move.to_s() + "\n" + board.to_s());
					assert(false && "move failed");
				}

				child->exp.addvloss(); //balanced out after rollouts

				walk_tree(board, child, depth+1);

				child->exp.addv(movelist.getexp(toplay));

				if(!do_backup(node, child, toplay) && //not solved
					player->ravefactor > min_rave &&  //using rave
					node->children.num() > 1 &&       //not a macro move
					50*remain*(player->ravefactor + player->decrrave*remain) > node->exp.num()) //rave is still significant
					update_rave(node, toplay);

				return;
			}
		}while(!do_backup(node, child, toplay));

		return;
	}

	int won = (player->minimax ? node->outcome : board.won());

	//if it's not already decided
	if(won < 0){
		//create children if valid
		if(node->exp.num() >= player->visitexpand+1 && create_children(board, node, toplay)){
			walk_tree(board, node, depth);
			return;
		}

		//do random game on this node
		for(int i = 0; i < player->rollouts; i++){
			Board copy = board;
			rollout(copy, node->move, depth);
		}
	}else{
		movelist.finishrollout(won); //got to a terminal state, it's worth recording
	}

	treelen.add(depth);

	movelist.subvlosses(1);

	return;
}

bool sort_node_know(const Player::Node & a, const Player::Node & b){
	return (a.know > b.know);
}

bool Player::PlayerUCT::create_children(Board & board, Node * node, int toplay){
	if(!node->children.lock())
		return false;

	if(player->dists || player->detectdraw){
		dists.run(&board);

		if(player->detectdraw){
//			assert(node->outcome == -3);
			node->outcome = dists.isdraw(); //could be winnable by only one side

			if(node->outcome == 0){ //proven draw, neither side can influence the outcome
				node->bestmove = *(board.moveit()); //just choose the first move since all are equal at this point
				node->children.unlock();
				return true;
			}
		}
	}

	CompactTree<Node>::Children temp;
	temp.alloc(board.movesremain(), player->ctmem);

	int losses = 0;

	Node * child = temp.begin(),
	     * end   = temp.end(),
	     * loss  = NULL;
	Board::MoveIterator move = board.moveit(player->prunesymmetry);
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
				temp.dealloc(player->ctmem);
				return true;
			}
		}

		add_knowledge(board, node, child);
	}

	if(player->prunesymmetry)
		for(; child != end; ++child) //add losses to the end so they aren't considered
			*child = Node(M_NONE, 3 - toplay);

	//both end conditions should happen in parallel, so either both happen or neither do
	assert(move.done() == (child == end));

	//Make a macro move, add experience to the move so the current simulation continues past this move
	if(losses == 1){
		Node macro = *loss;
		temp.dealloc(player->ctmem);
		temp.alloc(1, player->ctmem);
		macro.exp.addwins(player->visitexpand);
		*(temp.begin()) = macro;
	}else if(losses >= 2){ //proven loss, but at least try to block one of them
		node->outcome = 3 - toplay;
		node->bestmove = loss->move;
		node->children.unlock();
		temp.dealloc(player->ctmem);
		return true;
	}

	if(player->dynwiden > 0) //sort in decreasing order by knowledge
		sort(temp.begin(), temp.end(), sort_node_know);

	PLUS(player->nodes, temp.num());
	node->children.swap(temp);
	assert(temp.unlock());

	return true;
}

Player::Node * Player::PlayerUCT::choose_move(const Node * node, int toplay, int remain) const {
	float val, maxval = -1000000000;
	float logvisits = log(node->exp.num());
	int dynwidenlim = (player->dynwiden > 0 ? (int)(logvisits/player->logdynwiden) : 361);

	float raveval = use_rave * (player->ravefactor + player->decrrave*remain);
	float explore = use_explore * player->explore;

	Node * ret = NULL,
		 * child = node->children.begin(),
		 * end = min(node->children.end(), child + dynwidenlim);

	for(; child != end; child++){
		if(child->outcome >= 0){
			if(child->outcome == toplay) //return a win immediately
				return child;

			val = (child->outcome == 0 ? -1 : -2); //-1 for tie so any unknown is better, -2 for loss so it's even worse
		}else{
			val = child->value(raveval, player->knowledge, player->fpurgency);
			if(explore > 0)
				val += explore*sqrt(logvisits/(child->exp.num() + 1));
		}

		if(maxval < val){
			maxval = val;
			ret = child;
		}
	}

	return ret;
}

/*
backup in this order:

6 win
5 win/draw
4 draw if draw/loss
3 win/draw/loss
2 draw
1 draw/loss
0 lose
return true if fully solved, false if it's unknown or partially unknown
*/
bool Player::PlayerUCT::do_backup(Node * node, Node * backup, int toplay){
	int nodeoutcome = node->outcome;
	if(nodeoutcome >= 0) //already proven, probably by a different thread
		return true;

	if(backup->outcome == -3) //not yet proven by this child, so no chance
		return false;

	if(backup->outcome != toplay){
		uint64_t sims = 0, bestsims = 0, outcome = 0, bestoutcome = 0;
		backup = NULL;

		Node * child = node->children.begin(),
			 * end = node->children.end();

		for( ; child != end; child++){
			//these should be sorted in likelyness of matching, most likely first
			if(child->outcome == -3){ // win/draw/loss
				outcome = 3;
			}else if(child->outcome == toplay){ //win
				backup = child;
				outcome = 6;
				break;
			}else if(child->outcome == 3-toplay){ //loss
				outcome = 0;
			}else if(child->outcome == 0){ //draw
				if(node->outcome == toplay-3) //draw/loss
					outcome = 4;
				else
					outcome = 2;
			}else if(child->outcome == -toplay){ //win/draw
				outcome = 5;
			}else if(child->outcome == toplay-3){ //draw/loss
				outcome = 1;
			}else{
				assert(false && "How'd I get here? All outcomes should be tested above");
			}

			sims = child->exp.num();
			if(bestoutcome < outcome){ //better outcome is always preferable
				bestoutcome = outcome;
				bestsims = sims;
				backup = child;
			}else if(bestoutcome == outcome && ((outcome == 0 && bestsims < sims) || bestsims > sims)){
				//find long losses or easy wins/draws
				bestsims = sims;
				backup = child;
			}
		}

		if(bestoutcome == 3) //no win, but found an unknown
			return false;
	}

	if(CAS(node->outcome, nodeoutcome, backup->outcome))
		node->bestmove = backup->move;
	else //if it was in a race, try again, might promote a partial solve to full solve
		return do_backup(node, backup, toplay);

	return (node->outcome >= 0);
}

//update the rave score of all children that were played
void Player::PlayerUCT::update_rave(const Node * node, int toplay){
	Node * child = node->children.begin(),
	     * childend = node->children.end();

	for( ; child != childend; ++child)
		child->rave.addv(movelist.getrave(toplay, child->move));
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

	if(player->size) //boost for size of the group
		child->know += player->size*board.test_size(child->move);

	if(player->bridge && test_bridge_probe(board, node->move, child->move)) //boost for maintaining a virtual connection
		child->know += player->bridge;

	if(player->dists)
		child->know += player->dists * max(0, board.get_size_d() - dists.get(child->move));
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
		int v = 0;
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
int Player::PlayerUCT::rollout(Board & board, Move move, int depth){
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

	bool checkrings = (unitrand() < player->checkrings);
	int  checkdepth = (int)player->checkringdepth;
	if(player->checkringdepth < 0)
		checkdepth = (int)(board.movesremain() * player->checkringdepth * -1);

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

		movelist.addrollout(move, board.toplay());
		board.move(move, true, false, (checkrings && depth < checkdepth));
		depth++;
	}

	gamelen.add(depth);

	if(won > 0)
		wintypes[won-1][(int)board.getwintype()].add(depth);

	//update the last good reply table
	if(player->lastgoodreply && won > 0){
		MoveList::RaveMove * rave = movelist.begin(), *raveend = movelist.end();

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

	movelist.finishrollout(won);
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
		int v = 0;
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


