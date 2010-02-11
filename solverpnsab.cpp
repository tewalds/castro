
#include "solver.h"

void Solver::solve_pnsab(const Board & board, double time, uint64_t memlimit){
	nodesremain = memlimit*1024*1024/sizeof(PNSNode);

	pnsab_depth = 1;

	if(board.won() >= 0){
		outcome = board.won();
		return;
	}

	Timer timer = Timer(time, bind(&Solver::timedout, this));
	int starttime = time_msec();

	int turn = board.toplay();

	PNSNode root(-1, -1, false);

	bool mem = true;
	while(mem && !timeout && root.phi != 0 && root.delta != 0)
		mem = pnsab(board, & root, 0);

	maxdepth += pnsab_depth+1;

	if(!mem)
		fprintf(stderr, "Ran out of memory\n");

	if(root.delta == 0)    outcome = (turn == 1 ? 2 : 1);
	else if(root.phi == 0) outcome = turn;

	fprintf(stderr, "Finished in %d msec\n", time_msec() - starttime);
}

bool Solver::pnsab(const Board & board, PNSNode * node, int depth){
	if(depth > maxdepth)
		maxdepth = depth;

	if(node->numchildren == 0){
		if(nodesremain <= 0)
			return false;
	
		nodesremain -= node->alloc(board.movesremain());
		nodes += board.movesremain();

		unsigned int min = INF16;
		unsigned int sum = 0;
		int i = 0;
		for(int y = 0; y < board.get_size_d(); y++){
			for(int x = 0; x < board.get_size_d(); x++){
				if(board.valid_move(x, y)){
					Board next = board;
					next.move(x, y);
					
					uint64_t prevnodes = nodes;
					
					int abval = negamax(next, pnsab_depth, -2, 2);

					if(abval == 2)
						node->children[i] = PNSNode(x, y, 0, INF16);
					else if(abval == -2)
						node->children[i] = PNSNode(x, y, INF16, 0);
					else
						node->children[i] = PNSNode(x, y, 1 + int(nodes - prevnodes));

					sum += node->children[i].phi;
					if(node->children[i].delta < min)
						min = node->children[i].delta;

					i++;
				}
			}
		}
		if(sum > INF16)
			sum = INF16 - 1;

		node->phi = min;
		node->delta = sum;

		if(node->phi == 0 || node->delta == 0)
			nodesremain += node->dealloc();

		return true;
	}

	unsigned int min, sum;
	bool mem;
	do{
		int i = 0;
		for(; i < node->numchildren; i++)
			if(node->children[i].delta == node->phi)
				break;
		PNSNode * child = &(node->children[i]);

		Board next = board;
		next.move(child->x, child->y);
		mem = pnsab(next, child, depth + 1);

		min = INF16;
		sum = 0;
		for(int i = 0; i < node->numchildren; i++){
			sum += node->children[i].phi;
			if(node->children[i].delta < min)
				min = node->children[i].delta;
		}
		if(sum > INF16)
			sum = INF16-1;
	}while(!timeout && mem && min == node->phi && sum == node->delta);

	node->phi = min;
	node->delta = sum;

	if(node->phi == 0 || node->delta == 0)
		nodesremain += node->dealloc();

	return mem;
}

