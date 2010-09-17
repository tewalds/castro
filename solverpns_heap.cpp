
#include "solverpns_heap.h"
#include "solverab.h"

/* three possible outcomes from run_pns: Win, Loss, Unknown (ran out of time/memory)
 * Ties are grouped with loss first, win second, forcing two runs:
 *   W L  U   Run 1: W | TL
 * W W T  WT  Run 2: WT | L
 * L W L  L   from the perspective of toplay
 * U W LT U
 */
void SolverPNSHeap::solve(Board board, double time, uint64_t memlimit){
	reset();

	if(board.won() >= 0){
		outcome = board.won();
		return;
	}
	board.setswap(false);

	Timer timer(time, bind(&SolverPNSHeap::timedout, this));
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

int SolverPNSHeap::run_pns(const Board & board, int ties, uint64_t memlimit){ //1 = win, 0 = unknown, -1 = loss
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

bool SolverPNSHeap::pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td){
	if(depth > maxdepth)
		maxdepth = depth;

	if(node->numchildren == 0){
		if(nodes >= maxnodes)
			return false;
	
		int numnodes = board.movesremain();
		nodes += node->alloc(numnodes);
		nodes_seen += numnodes;

		PNSNode * child = node->children;
		uint64_t sum = 0;
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			int abval, pd = 1; // alpha-beta value, phi & delta value

			if(ab){
				Board next = board;
				next.move(*move, false, false);

				SolverAB solveab(false);
				abval = -solveab.negamax(next, ab, -2, 2);
				pd = 1;
				nodes_seen += solveab.nodes_seen;
			}else{
				int won = board.test_win(*move);
				abval = (won > 0) + (won >= 0);
				pd = 1; // phi & delta value
			}

			*child = PNSNode(*move).abval(abval, (board.toplay() == assignties), pd);

			sum += child->phi;

			child++;
		}

		//add losses to the end so they aren't considered, only used when unique is true
		for(; child != node->children + numnodes; child++)
			*child = PNSNode(Move(M_NONE), 0, INF32);

		if(sum >= INF32)
			sum = INF32-1;

		node->build_heap();

		node->phi = node->children[0].delta;
		node->delta = sum;

		return true;
	}
	
	bool mem;
	do{
		PNSNode * child = node->children,
		        * child2 = node->children;

		uint32_t tpc = 0, 
		         tdc = 0;

		if(df){
			if(node->numchildren > 1){
				child2++;
				if(node->numchildren > 2 && child2->delta > (child2+1)->delta)
					child2++;
			}

			tpc = min(INF32/2, (td + child->phi - node->delta));
			tdc = min(tp, (uint32_t)(child2->delta*(1.0 + epsilon) + 1));
		}

		Board next = board;
		next.move(child->move, false, false);

		uint32_t prev_phi   = child->phi,
		         prev_delta = child->delta;

		mem = pns(next, child, depth + 1, tpc, tdc);

		if(child->phi == 0 || child->delta == 0)
			nodes -= child->dealloc();

		bool updated = false;
		if(prev_phi != child->phi){ //update the sum
			node->delta -= prev_phi;
			node->delta += child->phi;
			if(node->delta >= INF32)
				node->delta = INF32-1;

			updated = true;
		}

		if(prev_delta != child->delta){ //update the min
			child = NULL; //child pointer is invalid after heapify
			node->heapify();
			node->phi = node->children[0].delta;
			updated = true;
		}

		if(updated && !df)
			break;

	}while(!timeout && mem && (!df || (node->phi < tp && node->delta < td)));

	return mem;
}

//removes the children of any node whos children are all unproven and have no children
bool SolverPNSHeap::garbage_collect(PNSNode * node){
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
