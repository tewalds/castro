
#include "solver.h"

/* three possible outcomes from run_pns: Win, Loss, Unknown (ran out of time/memory)
 * Ties are grouped with loss first, win second, forcing two runs:
 *   W L  U
 * W W T  WT
 * L W L  L
 * U W LT U
 */
void Solver::solve_pns(const Board & board, double time, uint64_t memlimit){
	nodesremain = memlimit*1024*1024/sizeof(PNSNode);

	if(board.won() >= 0){
		outcome = board.won();
		return;
	}

	Timer timer = Timer(time, bind(&Solver::timedout, this));
	int starttime = time_msec();

	int turn = board.toplay();
	int otherturn = (turn == 1 ? 2 : 1);

	int ret1 = run_pns(board, otherturn);

	if(ret1 == 1){ //win
		outcome = turn;
	}else{
		int ret2 = run_pns(board, turn);

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

int Solver::run_pns(const Board & board, int ties){ //1 = win, 0 = unknown, -1 = loss
	assignties = ties;

	PNSNode root(-1, -1, false);

	bool mem = true;
	while(mem && !timeout && root.phi != 0 && root.delta != 0)
		mem = pns(board, & root, 0);

	if(!mem)
		fprintf(stderr, "Ran out of memory\n");

	if(root.phi == 0)   return 1;
	if(root.delta == 0) return -1;
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

		unsigned int min = INF16;
		unsigned int sum = 0;
		int i = 0;
		for(int y = 0; y < board.get_size_d(); y++){
			for(int x = 0; x < board.get_size_d(); x++){
				if(board.valid_move(x, y)){
					Board next = board;
					next.move(x, y);

					node->children[i] = PNSNode(x, y).abval((next.won() > 0) + (next.won() >= 0), (board.toplay() == assignties));

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
		mem = pns(next, child, depth + 1);

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

