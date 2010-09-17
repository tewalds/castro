
#include "solverpns.h"
#include "solverab.h"

/* three possible outcomes from run_pns: Win, Loss, Unknown (ran out of time/memory)
 * Ties are grouped with loss first, win second, forcing two runs:
 *   W L  U   Run 1: W | TL
 * W W T  WT  Run 2: WT | L
 * L W L  L   from the perspective of toplay
 * U W LT U
 */
void SolverPNS::solve(Board board, double time, uint64_t memlimit){
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

	int ret1 = run_pns(board, otherturn, memlimit);

	if(ret1 == 1){ //win
		outcome = turn;
	}else{
		int ret2 = 0;
		if(!timeout)
			ret2 = run_pns(board, turn, memlimit);

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

int SolverPNS::run_pns(const Board & board, int ties, uint64_t memlimit){ //1 = win, 0 = unknown, -1 = loss
	assignties = ties;

	if(root) delete root;
	root = new PNSNode(0, 0, 1);
	maxnodes = memlimit*1024*1024/sizeof(PNSNode);
	nodes = 0;

	fprintf(stderr, "max nodes: %lli, max memory: %lli Mb\n", maxnodes, maxnodes*sizeof(PNSNode)/1024/1024);

	while(!timeout && root->phi != 0 && root->delta != 0){
		if(!pns(board, root, 0, INF32/2, INF32/2)){
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

bool SolverPNS::pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td){
	if(depth > maxdepth)
		maxdepth = depth;

	if(node->numchildren == 0){
		if(nodes >= maxnodes)
			return false;

		int numnodes = board.movesremain();
		nodes += node->alloc(numnodes);
		nodes_seen += numnodes;

		int i = 0;
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			int abval, pd = 1; // alpha-beta value, phi & delta value

			if(ab){
				Board next = board;
				next.move(*move, false, false);

				uint64_t prevnodes = nodes_seen;

				SolverAB solveab(false);
				abval = -solveab.negamax(next, ab, -2, 2);
				pd = 1 + int(nodes_seen - prevnodes);
			}else{
				int won = board.test_win(*move);
				abval = (won > 0) + (won >= 0);
				pd = 1; // phi & delta value
			}

			node->children[i] = PNSNode(*move).abval(abval, (board.toplay() == assignties), pd);

			i++;
		}
		for(; i < numnodes; i++) //add losses to the end so they aren't considered, only used when unique is true
			node->children[i] = PNSNode(Move(M_NONE), 0, INF32);

		updatePDnum(node);

		return true;
	}
	
	bool mem;
	do{
		PNSNode * child = node->children,
		        * child2 = node->children,
		        * childend = node->children + node->numchildren;

		uint32_t tpc, tdc;

		if(df){
			for(PNSNode * i = node->children; i != childend; i++){
				if(i->delta <= child->delta){
					child2 = child;
					child = i;
				}
			}

			tpc = min(INF32/2, (td + child->phi - node->delta));
			tdc = min(tp, (uint32_t)(child2->delta*(1.0 + epsilon) + 1));
		}else{
			tpc = tdc = 0;
			while(child->delta != node->phi)
				child++;
		}

		Board next = board;
		next.move(child->move, false, false);
		mem = pns(next, child, depth + 1, tpc, tdc);

		if(child->phi == 0 || child->delta == 0)
			nodes -= child->dealloc();

		if(updatePDnum(node) && !df)
			break;

	}while(!timeout && mem && (!df || (node->phi < tp && node->delta < td)));

	return mem;
}

bool SolverPNS::updatePDnum(PNSNode * node){
	uint32_t min = INF32;
	uint64_t sum = 0;

	PNSNode * i = node->children;
	PNSNode * end = node->children + node->numchildren;

	for( ; i != end; i++){
		sum += i->phi;
		if( min > i->delta)
			min = i->delta;
	}

	if(sum >= INF32)
		sum = INF32-1;

	if(min == node->phi && sum == node->delta){
		return false;
	}else{
		node->phi = min;
		node->delta = sum;
		return true;
	}
}

//removes the children of any node whos children are all unproven and have no children
bool SolverPNS::garbage_collect(PNSNode * node){
	if(node->numchildren == 0)
		return (node->phi != 0 && node->delta != 0);

	PNSNode * i = node->children;
	PNSNode * end = node->children + node->numchildren;

	bool collect = true;
	for( ; i != end; i++)
		collect &= garbage_collect(i);

	if(collect)
		nodes -= node->dealloc();

	return false;
}
