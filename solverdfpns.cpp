
#include "solverpns.h"
#include "solverab.h"

void SolverPNS::solve_dfpns(Board board, double time, uint64_t memlimit){
	reset();

	if(board.won() >= 0){
		outcome = board.won();
		return;
	}
	board.setswap(false);

	Timer timer(time, bind(&SolverPNS::timedout, this));
	int starttime = time_msec();

	int turn = board.toplay();
	int otherturn = (turn == 1 ? 2 : 1);

	int ret1 = run_dfpns(board, otherturn, memlimit);

	if(ret1 == 1){ //win
		outcome = turn;
	}else{
		int ret2 = 0;
		if(!timeout)
			ret2 = run_dfpns(board, turn, memlimit);

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

int SolverPNS::run_dfpns(const Board & board, int ties, uint64_t memlimit){ //1 = win, 0 = unknown, -1 = loss
	assignties = ties;

	if(root) delete root;
	root = new PNSNode(0, 0, 1);
	maxnodes = memlimit*1024*1024/sizeof(PNSNode);

	fprintf(stderr, "max nodes: %lli, max memory: %lli Mb\n", maxnodes, maxnodes*sizeof(PNSNode)/1024/1024);

	while(!timeout && root->phi != 0 && root->delta != 0){
		if(!dfpns(board, root, 0, INF32/2, INF32/2)){
			int64_t before = nodes;
			garbage_collect(root);
			fprintf(stderr, "Garbage collection cleaned up %lli nodes, %lli of %lli Mb still in use\n", before - nodes, nodes*sizeof(PNSNode)/1024/1024, maxnodes*sizeof(PNSNode)/1024/1024);
			if(maxnodes - nodes < maxnodes/100)
				break;
		}
	}

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

bool SolverPNS::dfpns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td){
	if(depth > maxdepth)
		maxdepth = depth;

	if(node->numchildren == 0){
		if(nodes >= maxnodes)
			return false;
	
		nodes += node->alloc(board.movesremain());
		nodes_seen += board.movesremain();

		int i = 0;
		for(Board::MoveIterator move = board.moveit(); !move.done(); ++move){
			Board next = board;
			next.move(*move);

			int abval = (next.won() > 0) + (next.won() >= 0);
			int pd = 1; // phi & delta value

			if(ab && abval == 0){
				uint64_t prevnodes = nodes_seen;

				SolverAB solveab(false);
				abval = -solveab.negamax(next, ab, -2, 2);
				pd = 1 + int(nodes_seen - prevnodes);
			}

			node->children[i] = PNSNode(*move).abval(abval, (board.toplay() == assignties), pd);

			i++;
		}

		updatePDnum(node);

		return true;
	}

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
		next.move(c1->move, false);
		
		uint32_t tpc = min(INF32/2, (td + c1->phi - node->delta));
		uint32_t tdc = min(tp, (uint32_t)(c2->delta*(1.0 + epsilon) + 1));

//		printf("depth: %i, tp: %i, td: %i, tpc: %i, tdc: %i\n", depth, tp, td, tpc, tdc);

		mem = dfpns(next, c1, depth + 1, tpc, tdc);

		if(c1->phi == 0 || c1->delta == 0)
			nodes -= c1->dealloc();

		updatePDnum(node);
	}while(!timeout && mem && node->phi < tp && node->delta < td);

	return mem;
}

