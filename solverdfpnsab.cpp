
#include "solver.h"

void Solver::solve_dfpnsab(const Board & board, double time, uint64_t memlimit){
	nodesremain = memlimit*1024*1024/sizeof(PNSNode);

	if(board.won() >= 0){
		outcome = board.won();
		return;
	}

	Timer timer = Timer(time, bind(&Solver::timedout, this));
	int starttime = time_msec();

	int turn = board.toplay();
	int otherturn = (turn == 1 ? 2 : 1);

	int ret1 = run_dfpnsab(board, otherturn);

	if(ret1 == 1){ //win
		outcome = turn;
	}else{
		int ret2 = run_dfpnsab(board, turn);

		if(ret2 == -1){
			outcome = otherturn; //loss
		}else{
			if(ret1 == -1 && ret2 == 1) outcome = 0;          //tie
			if(ret1 == -1 && ret2 == 0) outcome = -otherturn; //loss or tie
			if(ret1 ==  0 && ret2 == 1) outcome = -turn;      //win or tie
			if(ret1 ==  0 && ret2 == 0) outcome = -3;         //unknown
		}
	}

	fprintf(stderr, "Finished in %d msec\n", time_msec() - starttime);
}

int Solver::run_dfpnsab(const Board & board, int ties){ //1 = win, 0 = unknown, -1 = loss
	assignties = ties;

	PNSNode root(-1, -1, false);

	bool mem = true;
	while(mem && !timeout && root.phi != 0 && root.delta != 0)
		mem = dfpnsab(board, & root, 0, INF16, INF16);

	if(!mem)
		fprintf(stderr, "Ran out of memory\n");

	if(root.phi == 0){
		for(int i = 0; i < root.numchildren; i++){
			if(root.children[i].delta == 0){
				X = root.children[i].x;
				Y = root.children[i].y;
			}
		}
		return 1;
	}
	if(root.delta == 0)
		return -1;
	return 0;
}

bool Solver::dfpnsab(const Board & board, PNSNode * node, int depth, int tp, int td){
	if(depth > maxdepth)
		maxdepth = depth;

	if(node->numchildren == 0){
		if(nodesremain <= 0)
			return false;
	
		nodesremain -= node->alloc(board.movesremain());
		nodes += board.movesremain();

		unsigned int minp = INF16;
		unsigned int sum = 0;
		int i = 0;
		for(int y = 0; y < board.get_size_d(); y++){
			for(int x = 0; x < board.get_size_d(); x++){
				if(board.valid_move(x, y)){
					Board next = board;
					next.move(x, y);
//*
					node->children[i] = PNSNode(x, y).abval((next.won() > 0) + (next.won() >= 0), (board.toplay() == assignties));
/*/					
					uint64_t prevnodes = nodes;

					int abval = -negamax(next, 1, -2, 2); //higher depth goes farther but takes longer, depth 1 seems to be best

					node->children[i] = PNSNode(x, y).abval(abval, (board.toplay() == assignties), 1 + int(nodes - prevnodes));
//*/
					sum += node->children[i].phi;
					if( minp > node->children[i].delta)
						minp = node->children[i].delta;

					i++;
				}
			}
		}
		if(sum > INF16)
			sum = INF16 - 1;

		node->phi = minp;
		node->delta = sum;

		return true;
	}

	unsigned int minp, sum;
	bool mem;
	do{
		PNSNode * c1 = node->children;
		PNSNode * c2 = node->children;
		for(PNSNode * i = node->children; i != node->children + node->numchildren; i++){
			if(i->delta <= c1->delta){
				c2 = c1;
				c1 = i;
			}
		}

		Board next = board;
		next.move(c1->x, c1->y);
		
		int tpc = (td + c1->phi - node->delta);
		int tdc = min(tp, c2->delta + 1);
		
//		printf("depth: %i, tp: %i, td: %i, tpc: %i, tdc: %i\n", depth, tp, td, tpc, tdc);
		
		mem = dfpnsab(next, c1, depth + 1, tpc, tdc);

		if(c1->phi == 0 || c1->delta == 0)
			nodesremain += c1->dealloc();

		minp = INF16;
		sum = 0;
		for(int i = 0; i < node->numchildren; i++){
			sum += node->children[i].phi;
			if( minp > node->children[i].delta)
				minp = node->children[i].delta;
		}
		if(sum > INF16)
			sum = INF16-1;

		node->phi = minp;
		node->delta = sum;
	}while(!timeout && mem && node->phi < tp && node->delta < td);

	return mem;
}

