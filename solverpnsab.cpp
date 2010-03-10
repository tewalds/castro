
#include "solver.h"

void Solver::solve_pnsab(const Board & board, double time, int memlimit){
	reset();

	if(board.won() >= 0){
		outcome = board.won();
		return;
	}

	Timer timer = Timer(time, bind(&Solver::timedout, this));
	int starttime = time_msec();

	int turn = board.toplay();
	int otherturn = (turn == 1 ? 2 : 1);

	int ret1 = run_pnsab(board, otherturn, memlimit);

	if(ret1 == 1){ //win
		outcome = turn;
	}else{
		int ret2 = run_pnsab(board, turn, memlimit);

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

int Solver::run_pnsab(const Board & board, int ties, int memlimit){ //1 = win, 0 = unknown, -1 = loss
	assignties = ties;

	if(root) delete root;
	root = new PNSNode(0, 0, 1);
	nodesremain = memlimit*1024*1024/sizeof(PNSNode);

	bool mem = true;
	while(mem && !timeout && root->phi != 0 && root->delta != 0)
		mem = pnsab(board, root, 0);

	if(!mem)
		fprintf(stderr, "Ran out of memory\n");

	if(root->phi == 0){
		for(int i = 0; i < root->numchildren; i++)
			if(root->children[i].delta == 0)
				bestmove = root->children[i].move;
		return 1;
	}
	if(root->delta == 0)
		return -1;
	return 0;
}

bool Solver::pnsab(const Board & board, PNSNode * node, int depth){
	if(depth > maxdepth)
		maxdepth = depth;

	if(node->numchildren == 0){
		if(nodesremain <= 0)
			return false;
	
		nodesremain -= node->alloc(board.movesremain());
		nodes += board.movesremain();

		int i = 0;
		for(int y = 0; y < board.get_size_d(); y++){
			for(int x = 0; x < board.get_size_d(); x++){
				if(board.valid_move(x, y)){
					Board next = board;
					next.move(x, y);
					
					uint64_t prevnodes = nodes;

					int abval = -negamax(next, 1, -2, 2); //higher depth goes farther but takes longer, depth 1 seems to be best

					node->children[i] = PNSNode(x, y).abval(abval, (board.toplay() == assignties), 1 + int(nodes - prevnodes));

					i++;
				}
			}
		}

		updatePDnum(node);

		return true;
	}

	bool mem;
	do{
		int i = 0;
		for(; i < node->numchildren; i++)
			if(node->children[i].delta == node->phi)
				break;
		PNSNode * child = &(node->children[i]);

		Board next = board;
		next.move(child->move);
		mem = pnsab(next, child, depth + 1);

		if(child->phi == 0 || child->delta == 0)
			nodesremain += child->dealloc();

	}while(!timeout && mem && !updatePDnum(node));

	return mem;
}

