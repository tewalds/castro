
#include "solverpns_tt.h"
#include "solverab.h"

/* three possible outcomes from run_pns: Win, Loss, Unknown (ran out of time/memory)
 * Ties are grouped with loss first, win second, forcing two runs:
 *   W L  U   Run 1: W | TL
 * W W T  WT  Run 2: WT | L
 * L W L  L   from the perspective of toplay
 * U W LT U
 */
void SolverPNSTT::solve(Board board, double time, uint64_t memlimit){
	reset();

	if(board.won() >= 0){
		outcome = board.won();
		return;
	}
	board.setswap(false);

	Timer timer(time, bind(&SolverPNSTT::timedout, this));
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

int SolverPNSTT::run_pns(const Board & board, int ties, uint64_t memlimit){ //1 = win, 0 = unknown, -1 = loss
	assignties = ties;

	maxnodes = memlimit*1024*1024/sizeof(PNSNode);

	if(TT) delete[] TT;
	TT = new PNSNode[maxnodes];

	fprintf(stderr, "max nodes: %lli, max memory: %lli Mb\n", maxnodes, maxnodes*sizeof(PNSNode)/1024/1024);

	hash_t basehash = board.gethash();

	PNSNode root = PNSNode(basehash, 1);
	while(!timeout && root.phi != 0 && root.delta != 0)
		pns(board, &root, 0, INF32/2, INF32/2);

	if(root.phi == 0){
		PNSNode * i = NULL;
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			i = tt(board, *move);
			if(i->delta == 0)
				bestmove = *move;
		}
		return 1;
	}
	if(root.delta == 0)
		return -1;
	return 0;
}

void SolverPNSTT::pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td){
	if(depth > maxdepth)
		maxdepth = depth;

	do{
		PNSNode * child = NULL,
		        * child2 = NULL;

		Move move1, move2;

		uint32_t tpc, tdc;

		PNSNode * i = NULL;
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			i = tt(board, *move);
			if(child == NULL){
				child = child2 = i;
				move1 = move2 = *move;
			}else if(i->delta <= child->delta){
				child2 = child;
				child = i;
				move2 = move1;
				move1 = *move;
			}
		}

		if(child->delta && child->phi){ //unsolved
			if(df){
				tpc = min(INF32/2, (td + child->phi - node->delta));
				tdc = min(tp, (uint32_t)(child2->delta*(1.0 + epsilon) + 1));
			}else{
				tpc = tdc = 0;
			}

			Board next = board;
			next.move(move1, false, false);
			pns(next, child, depth + 1, tpc, tdc);
		}

		if(updatePDnum(board, node) && !df)
			break;

	}while(!timeout && node->phi && node->delta && (!df || (node->phi < tp && node->delta < td)));
}

bool SolverPNSTT::updatePDnum(const Board & board, PNSNode * node){
	uint32_t min = INF32;
	uint64_t sum = 0;

	PNSNode * i = NULL;
	for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
		i = tt(board, *move);

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

SolverPNSTT::PNSNode * SolverPNSTT::tt(const Board & board, Move move){
	hash_t hash = board.test_hash(move, board.toplay());

	PNSNode * node = TT + (hash % maxnodes);
	
	if(node->hash != hash){
		int abval, pd = 1; // alpha-beta value, phi & delta value

		if(ab){
			Board next = board;
			next.move(move, false, false);

			SolverAB solveab(false);
			abval = -solveab.negamax(next, ab, -2, 2);
			pd = 1 + solveab.nodes_seen;
			nodes_seen += solveab.nodes_seen;
		}else{
			int won = board.test_win(move);
			abval = (won > 0) + (won >= 0);
			pd = 1; // phi & delta value
		}

		*node = PNSNode(hash).abval(abval, (board.toplay() == assignties), pd);
		nodes_seen++;
	}

	return node;
}

