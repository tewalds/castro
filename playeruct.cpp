
#include "player.h"
#include "board.h"
#include <cmath>
#include <string>
#include "string.h"

void Player::play_uct(const Board & board, double time, int memlimit){
	maxnodes = memlimit*1024*1024/sizeof(Node);
	nodes = 0;
	time_used = 0;
	runs = 0;

	treelen.reset();
	gamelen.reset();

	bestmove = Move(-2,-2);
	timeout = false;

	if(board.won() >= 0)
		return;

	timeout = false;
	Timer timer = Timer(time, bind(&Player::timedout, this));
	int starttime = time_msec();
	runs = 0;


	Solver solver;
	Timer timer2 = Timer(0.2*time, bind(&Solver::timedout, &solver));
	int ret = solver.run_pnsab(board, (board.toplay() == 1 ? 2 : 1), memlimit/2);

	//if it found a win, just play it
	if(ret == 1){
		bestmove = Move(solver.X, solver.Y);

		int runtime = time_msec() - starttime;
		fprintf(stderr, "Solved in %i msec\n", runtime);
		time_used = (double)runtime/1000;
		return;
	}

	Node root;
	root.construct(solver.root);
	solver.reset();

	vector<Move> movelist;
	movelist.reserve(board.movesremain());
	while(!timeout){
		runs++;
		Board copy = board;
		movelist.clear();
		walk_tree(copy, & root, movelist, 0);
	}


//return the best one
	int maxi = 0;
	for(int i = 1; i < root.numchildren; i++)
//		if(root.children[maxi].winrate() < root.children[i].winrate())
		if(root.children[maxi].visits    < root.children[i].visits)
			maxi = i;

	bestmove = root.children[maxi].move;

	int runtime = time_msec() - starttime;

	string stats = "Finished " + to_str(runs) + " runs in " + to_str(runtime) + " msec\n";
	stats += "Game length: " + gamelen.to_s() + "\n";
	stats += "Tree depth:  " + treelen.to_s() + "\n";
	stats += "Move Score:  " + to_str(root.children[maxi].winrate()/2) + "\n";
	stats += "Games/s:     " + to_str((int)((double)runs*1000/runtime)) + "\n";
	fprintf(stderr, "%s", stats.c_str());

	time_used = (double)runtime/1000;
}

int Player::walk_tree(Board & board, Node * node, vector<Move> & movelist, int depth){
	int result, won = -1;

	node->visits++;

	if(node->children){
	//choose a child and recurse
		int maxi = 0;
		if(node->visits < node->numchildren/5){
			maxi = rand() % node->numchildren;
		}else{
			float val, maxval = -1000000000;
			float logvisits = log(node->visits)/5;
			Node * child;

			for(unsigned int i = 0; i < node->numchildren; i++){
				child = & node->children[i];

				if(child->visits < minvisitspriority) // give priority to nodes that have few or no visits
					val = 10000 - child->visits*1000 + rand()%100;
				else
					val = ravefactor*child->ravescore(node->ravevisits) + child->winrate() + explore*sqrt(logvisits/child->visits);

				if(maxval < val){
					maxval = val;
					maxi = i;
				}
			}
		}
		
		//recurse on the chosen child
		Node * child = & node->children[maxi];
		board.move(child->move);
		movelist.push_back(child->move);
		result = - walk_tree(board, child, movelist, depth+1);

		//update the rave scores
		if(ravefactor > 0 && result > 0){ //if a win
			//incr the rave score of all children that were played
			unsigned int m = 0, c = 0;
			while(m < movelist.size() && c < node->numchildren){
				if(movelist[m] == node->children[c].move){
					node->children[c].rave += 1;
					m++;
				}
				c++;
			}
			node->ravevisits++;
		}
	}else if((won = board.won()) >= 0){
		//already done
		treelen.add(depth);
	}else if(node->visits <= minvisitschildren || nodes >= maxnodes){
	//do random game on this node
		treelen.add(depth);
		won = rand_game(board, movelist, node->move, depth);

		if(ravefactor > 0){
			//remove the moves that were played by the loser
			//sort in y,x order

			if(won == 0){
				movelist.clear();
			}else{
				unsigned int i = 0, j = 1;

				if((won == board.toplay()) == (depth % 2 == 0)){
					i++;
					j++;
				}

				while(j < movelist.size()){
					movelist[i] = movelist[j];
					i++;
					j += 2;
				}

				movelist.resize(i);
				sort(movelist.begin(), movelist.end());
			}
		}
	}else{
	//create children
		nodes += node->alloc(board.movesremain());

		int i = 0;
		for(int y = 0; y < board.get_size_d(); y++)
			for(int x = 0; x < board.get_size_d(); x++)
				if(board.valid_move(x, y))
					node->children[i++] = Node(x, y);

		return walk_tree(board, node, movelist, depth);
	}
	if(won >= 0)
		result = (won == 0 ? 0 : won == board.toplay() ? -1 : 1);

	node->score += result + 1;
	return result;
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
	int piece = 3 - board.get(move.x, move.y);
	for(int i = 0; i < 8; i++){
		int x = move.x + neighbours[(i+a)%6][0];
		int y = move.y + neighbours[(i+a)%6][1];

		bool on = board.onboard2(x,y);
		int v;
		if(on)
			v = board.get(x,y);

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
					ret = Move(x,y);
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
					ret = Move(x,y);
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
int Player::rand_game(Board & board, vector<Move> & movelist, Move move, int depth){
	int won;

	while((won = board.won()) < 0){
//		if(!check_pattern(board, move)){
			do{
				move.x = rand() % board.get_size_d();
				move.y = rand() % board.get_size_d();
			}while(!board.valid_move(move));
//		}

		board.move(move);
		movelist.push_back(move);
		depth++;
	}

	gamelen.add(depth);

	return won;
}


