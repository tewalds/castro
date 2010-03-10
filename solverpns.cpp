
#include "solver.h"

/* three possible outcomes from run_pns: Win, Loss, Unknown (ran out of time/memory)
 * Ties are grouped with loss first, win second, forcing two runs:
 *   W L  U   Run 1: W | TL
 * W W T  WT  Run 2: WT | L
 * L W L  L   from the perspective of toplay
 * U W LT U
 */
void Solver::solve_pns(const Board & board, double time, int memlimit){
	reset();

	if(board.won() >= 0){
		outcome = board.won();
		return;
	}

	Timer timer = Timer(time, bind(&Solver::timedout, this));
	int starttime = time_msec();

	int turn = board.toplay();
	int otherturn = (turn == 1 ? 2 : 1);

	int ret1 = run_pns(board, otherturn, memlimit);

	if(ret1 == 1){ //win
		outcome = turn;
	}else{
		int ret2 = run_pns(board, turn, memlimit);

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

int Solver::run_pns(const Board & board, int ties, int memlimit){ //1 = win, 0 = unknown, -1 = loss
	assignties = ties;

	if(root) delete root;
	root = new PNSNode(0, 0, 1);
	nodesremain = memlimit*1024*1024/sizeof(PNSNode);

	bool mem = true;
	while(mem && !timeout && root->phi != 0 && root->delta != 0)
		mem = pns(board, root, 0);

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

bool Solver::pns(const Board & board, PNSNode * node, int depth){
	if(depth > maxdepth)
		maxdepth = depth;

	if(node->numchildren == 0){
		if(nodesremain <= 0)
			return false;
	
		nodesremain -= node->alloc(board.movesremain());
		nodes += board.movesremain();

		int i = 0;
		for(Board::MoveIterator move = board.moveit(); !move.done(); ++move){
			Board next = board;
			next.move(*move);

			node->children[i] = PNSNode(*move).abval((next.won() > 0) + (next.won() >= 0), (board.toplay() == assignties));

			i++;
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
		mem = pns(next, child, depth + 1);

		if(child->phi == 0 || child->delta == 0)
			nodesremain += child->dealloc();

	}while(!timeout && mem && !updatePDnum(node));

	return mem;
}

bool Solver::updatePDnum(PNSNode * node){
	unsigned int min = INF16;
	unsigned int sum = 0;

	PNSNode * i = node->children;
	PNSNode * end = node->children + node->numchildren;

	for( ; i != end; i++){
		sum += i->phi;
		if( min > i->delta)
			min = i->delta;
	}

	if(sum >= INF16)
		sum = INF16-1;

	if(min == node->phi && sum == node->delta){
		return false;
	}else{
		node->phi = min;
		node->delta = sum;
		return true;
	}
}

