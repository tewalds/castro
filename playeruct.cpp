
#include "player.h"
#include <cmath>
#include <string>
#include "string.h"

void Player::play_uct(const Board & board, double time, uint64_t memlimit){
	nodesremain = memlimit*1024*1024/sizeof(Node);

	if(board.won() >= 0)
		return;

	timeout = false;
	Timer timer = Timer(time, bind(&Player::timedout, this));
	int starttime = time_msec();
	runs = 0;

	Node root;
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
		if(root.children[maxi].winrate() < root.children[i].winrate())
//		if(root.children[maxi].visits < root.children[i].visits)
			maxi = i;

	X = root.children[maxi].x;
	Y = root.children[maxi].y;

	string stats = "Finished " + to_str(runs) + " runs in " + to_str(time_msec() - starttime) + " msec\n";
	stats += "Rollout depths: min " + to_str(mindepth) + ", max: " + to_str(maxdepth) + ", avg: " + to_str(sumdepth/runs);
	stats += ", std dev: " + to_str(sqrt((double)sumdepthsq/runs - ((double)sumdepth/runs)*((double)sumdepth/runs))) + "\n";
	fprintf(stderr, "%s", stats.c_str());

	time_used = (double)(time_msec() - starttime)/1000;
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
					val = child->ravescore(ravefactor, ravepower) + child->winrate() + explore*sqrt(logvisits/child->visits);

				if(maxval < val){
					maxval = val;
					maxi = i;
				}
			}
		}
		
		Node * child = & node->children[maxi];
		board.move(child->x, child->y);
		result = - walk_tree(board, child, movelist, depth+1);

		if(ravefactor > 0 && result > 0){ //if a win
			//incr the rave score of all children that were played
			int m = 0, c = 0;
			while(m < movelist.size() && c < node->numchildren){
				if(movelist[m].x == node->children[c].x && movelist[m].y == node->children[c].y){
					node->children[c].rave++;
					m++;
				}
				c++;
			}
		}
	}else if((won = board.won()) >= 0){
		//already done
		update_depths(depth);
	}else if(node->visits <= minvisitschildren || nodesremain <= 0){
	//do random game on this node
		won = rand_game(board, movelist, depth);

		if(ravefactor > 0){
			//remove the moves that were played by the loser
			//sort in y,x order

			if(won == 0){
				movelist.clear();
			}else{
				int i = 0, j = 1;

				if(won != board.toplay()){
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
		nodesremain -= node->alloc(board.movesremain());

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

//play a random game starting from a board state, and return the results of who won	
int Player::rand_game(Board & board, vector<Move> & movelist, int depth){
	int won;

	while((won = board.won()) < 0){
		int x, y;
		do{
			x = rand() % board.get_size_d();
			y = rand() % board.get_size_d();
		}while(!board.valid_move(x, y));
		
		board.move(x, y);
		movelist.push_back(Move(x, y));
		depth++;
	}

	update_depths(depth);

	return won;
}


