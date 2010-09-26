
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
	int otherturn = 3 - turn;

	if(ties == 3){ //do both
		assignties = otherturn;
		int ret1 = run_pns(board, memlimit);

		if(ret1 == turn){ //win
			outcome = turn;
		}else{
			int ret2 = -3;
			assignties = turn;
			if(!timeout)
				ret2 = run_pns(board, memlimit);

			if(ret2 == otherturn){
				outcome = otherturn; //loss
			}else{
				if(ret1 == otherturn && ret2 == turn) outcome = 0;          //tie
				if(ret1 == otherturn && ret2 == -3  ) outcome = -otherturn; //loss or tie
				if(ret1 == -3        && ret2 == turn) outcome = -turn;      //win or tie
				if(ret1 == -3        && ret2 == -3  ) outcome = -3;         //unknown
			}
		}
	}else{
		assignties = ties; //could be 0, 1 or 2
		outcome = run_pns(board, memlimit);
	}

	fprintf(stderr, "Finished in %d msec\n", time_msec() - starttime);
}

int SolverPNSTT::run_pns(const Board & board, uint64_t memlimit){
	maxnodes = memlimit*1024*1024/sizeof(PNSNode);

	if(TT) delete[] TT;
	TT = new PNSNode[maxnodes];

	fprintf(stderr, "max nodes: %lli, max memory: %lli Mb\n", maxnodes, maxnodes*sizeof(PNSNode)/1024/1024);

	hash_t basehash = board.gethash();

	PNSNode root = PNSNode(basehash, 1);
	while(!timeout && root.phi != 0 && root.delta != 0)
		pns(board, &root, 0, INF32/2, INF32/2);


//	printf("DRAW: %u, LOSS: %u\n", DRAW, LOSS);
//	printf("root : %u, %u\n", root->phi, root->delta);
//	for(PNSNode * i = root->children; i != root->children + root->numchildren; i++)
//		printf("%i,%i : %u, %u\n", i->move.x, i->move.y, i->phi, i->delta);

	if(root.phi == 0 && root.delta == LOSS){ //look for the winning move
		PNSNode * i = NULL;
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			i = tt(board, *move);
			if(i->delta == 0){
				bestmove = *move;
				break;
			}
		}
		return board.toplay();
	}
	if(root.phi == 0 && root.delta == DRAW){ //look for the move to tie
		PNSNode * i = NULL;
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			i = tt(board, *move);
			if(i->delta == DRAW){
				bestmove = *move;
				break;
			}
		}
		return 0;
	}

	if(root.delta == 0){ //loss
		bestmove = M_NONE;
		return 3 - board.toplay();
	}

	bestmove = M_UNKNOWN;
	return -3;
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
	uint32_t min = LOSS;
	uint64_t sum = 0;

	bool win = false;
	PNSNode * i = NULL;
	for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
		i = tt(board, *move);

		win |= (i->phi == LOSS);
		sum += i->phi;
		if( min > i->delta)
			min = i->delta;
	}

	if(win)
		sum = LOSS;
	else if(sum >= INF32)
		sum = INF32;

	hash_t hash = board.gethash();
	if(hash == node->hash && min == node->phi && sum == node->delta){
		return false;
	}else{
		node->hash = hash; //just in case it was overwritten by something else
		if(sum == 0 && min == DRAW){
			node->phi = 0;
			node->delta = DRAW;
		}else{
			node->phi = min;
			node->delta = sum;
		}
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

		*node = PNSNode(hash).abval(abval, board.toplay(), assignties, pd);
		nodes_seen++;
	}

	return node;
}

