
#include "player.h"
#include <cmath>

void Player::play_uct(const Board & board, double time, uint64_t memlimit){
	nodesremain = memlimit*1024*1024/sizeof(Node);

	if(board.won() >= 0)
		return;

	timeout = false;
	Timer timer = Timer(time, bind(&Player::timedout, this));
	int starttime = time_msec();
	runs = 0;

	Node root;
	while(!timeout){
		runs++;
		Board copy = board;
		walk_tree(copy, & root);
	}


//return the best one
	int maxi = 0;
	for(int i = 1; i < root.numchildren; i++)
//		if(root.children[maxi].winrate() < root.children[i].winrate())
		if(root.children[maxi].visits < root.children[i].visits)
			maxi = i;

	X = root.children[maxi].x;
	Y = root.children[maxi].y;

	fprintf(stderr, "Finished %i runs in %d msec\n", runs, time_msec() - starttime);

	time_used = (double)(time_msec() - starttime)/1000;
}

int Player::walk_tree(Board & board, Node * node){
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
					val = child->winrate() + explore*sqrt(logvisits/child->visits);

				if(maxval < val){
					maxval = val;
					maxi = i;
				}
			}
		}
		
		Node * child = & node->children[maxi];
		board.move(child->x, child->y);
		result = - walk_tree(board, child);
	}else if((won = board.won()) >= 0){
		//already done
	}else if(node->visits <= minvisitschildren || nodesremain <= 0){
	//do random game on this node
		won = rand_game(board);
	}else{
	//create children
		nodesremain -= node->alloc(board.movesremain());

		int i = 0;
		for(int y = 0; y < board.get_size_d(); y++)
			for(int x = 0; x < board.get_size_d(); x++)
				if(board.valid_move(x, y))
					node->children[i++] = Node(x, y);

	//recurse on a random child
		Node * child = & node->children[rand() % node->numchildren];
		board.move(child->x, child->y);
		result = - walk_tree(board, child);
	}
	if(won >= 0)
		result = (won == 0 ? 0 : won == board.toplay() ? -1 : 1);

	node->score += result + 1;
	return result;
}

//play a random game starting from a board state, and return the results of who won	
int Player::rand_game(Board & board){
	int won;

	while((won = board.won()) < 0){
		int x, y;
		do{
			x = rand() % board.get_size_d();
			y = rand() % board.get_size_d();
		}while(!board.valid_move(x, y));
		
		board.move(x, y);
	}
	return won;
}


