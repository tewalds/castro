
#include "solverpns_tt.h"
#include "solverab.h"
#include "time.h"
#include "timer.h"

void SolverPNSTT::solve(double time){
	if(rootboard.won() >= 0){
		outcome = rootboard.won();
		return;
	}

	timeout = false;
	Timer timer(time, bind(&SolverPNSTT::timedout, this));
	Time start;

	fprintf(stderr, "max nodes: %lli, max memory: %lli Mb\n", maxnodes, memlimit);

	run_pns();

	if(root.phi == 0 && root.delta == LOSS){ //look for the winning move
		PNSNode * i = NULL;
		for(Board::MoveIterator move = rootboard.moveit(true); !move.done(); ++move){
			i = tt(rootboard, *move);
			if(i->delta == 0){
				bestmove = *move;
				break;
			}
		}
		outcome = rootboard.toplay();
	}else if(root.phi == 0 && root.delta == DRAW){ //look for the move to tie
		PNSNode * i = NULL;
		for(Board::MoveIterator move = rootboard.moveit(true); !move.done(); ++move){
			i = tt(rootboard, *move);
			if(i->delta == DRAW){
				bestmove = *move;
				break;
			}
		}
		outcome = 0;
	}else if(root.delta == 0){ //loss
		bestmove = M_NONE;
		outcome = 3 - rootboard.toplay();
	}else{ //unknown
		bestmove = M_UNKNOWN;
		outcome = -3;
	}

	fprintf(stderr, "Finished in %.0f msec\n", (Time() - start)*1000);
}

void SolverPNSTT::run_pns(){
	if(TT == NULL)
		TT = new PNSNode[maxnodes];

	while(!timeout && root.phi != 0 && root.delta != 0)
		pns(rootboard, &root, 0, INF32/2, INF32/2);
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
			next.move(move1);//, false, false);
			pns(next, child, depth + 1, tpc, tdc);

			//just found a loss, try to copy proof to siblings
			if(child->delta == LOSS){
//				fprintf(stderr, "!%s ", move1.to_s().c_str());
				int count = copyproof;
				for(Board::MoveIterator move = board.moveit(true); count-- && !move.done(); ++move){
					if(!tt(board, *move)->terminal()){
//						fprintf(stderr, "?%s ", move->to_s().c_str());
						Board sibling = board;
						sibling.move(*move);
						copy_proof(next, sibling, move1, *move);
						updatePDnum(sibling);
					}
				}
			}
		}

		if(updatePDnum(board, node) && !df) //must pass node to updatePDnum since it may refer to the root which isn't in the TT
			break;

	}while(!timeout && node->phi && node->delta && (!df || (node->phi < tp && node->delta < td)));
}

bool SolverPNSTT::updatePDnum(const Board & board, PNSNode * node){
	hash_t hash = board.gethash();

	if(node == NULL)
		node = TT + (hash % maxnodes);

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

//source is a move that is a proven loss, and dest is an unproven sibling
//each has one move that the other doesn't, which are stored in smove and dmove
//if either move is used but only available in one board, the other is substituted
void SolverPNSTT::copy_proof(const Board & source, const Board & dest, Move smove, Move dmove){
	if(tt(source)->delta != LOSS || tt(dest)->terminal())
		return;

	//find winning move from the source tree
	Move bestmove = M_UNKNOWN;
	for(Board::MoveIterator move = source.moveit(true); !move.done(); ++move){
		if(tt(source, *move)->phi == LOSS){
			bestmove = *move;
			break;
		}
	}

	if(bestmove == M_UNKNOWN) //due to transposition table collision
		return;

	Board dest2 = dest;

	if(bestmove == dmove){
		assert(dest2.move(smove));
		smove = dmove = M_UNKNOWN;
	}else{
		assert(dest2.move(bestmove));
		if(bestmove == smove)
			smove = dmove = M_UNKNOWN;
	}

	if(tt(dest2)->terminal())
		return;

	Board source2 = source;
	assert(source2.move(bestmove));

	if(source2.won() >= 0)
		return;

	//test all responses
	for(Board::MoveIterator move = dest2.moveit(true); !move.done(); ++move){
		if(tt(dest2, *move)->terminal())
			continue;

		Move csmove = smove, cdmove = dmove;

		Board source3 = source2, dest3 = dest2;

		if(*move == csmove){
			assert(source3.move(cdmove));
			csmove = cdmove = M_UNKNOWN;
		}else{
			assert(source3.move(*move));
			if(*move == csmove)
				csmove = cdmove = M_UNKNOWN;
		}

		assert(dest3.move(*move));

		copy_proof(source3, dest3, csmove, cdmove);

		updatePDnum(dest3);
	}

	updatePDnum(dest2);
}

SolverPNSTT::PNSNode * SolverPNSTT::tt(const Board & board){
	hash_t hash = board.gethash();

	PNSNode * node = TT + (hash % maxnodes);

	if(node->hash != hash){
		int abval, pd = 1; // alpha-beta value, phi & delta value

		if(ab){
			SolverAB solveab(false);
			abval = -solveab.negamax(board, ab, -2, 2);
			pd = 1 + solveab.nodes_seen;
			nodes_seen += solveab.nodes_seen;
		}else{
			int won = board.won();
			abval = (won > 0) + (won >= 0);
			pd = 1; // phi & delta value
		}

		*node = PNSNode(hash).abval(abval, board.toplay(), ties, pd);
		nodes_seen++;
	}

	return node;
}

SolverPNSTT::PNSNode * SolverPNSTT::tt(const Board & board, Move move){
	hash_t hash = board.test_hash(move, board.toplay());

	PNSNode * node = TT + (hash % maxnodes);
	
	if(node->hash != hash){
		int abval, pd = 1; // alpha-beta value, phi & delta value

		if(ab){
			Board next = board;
			next.move(move);//, false, false);

			SolverAB solveab(false);
			abval = -solveab.negamax(next, ab, -2, 2);
			pd = 1 + solveab.nodes_seen;
			nodes_seen += solveab.nodes_seen;
		}else{
			int won = board.test_win(move);
			abval = (won > 0) + (won >= 0);
			pd = 1; // phi & delta value
		}

		*node = PNSNode(hash).abval(abval, board.toplay(), ties, pd);
		nodes_seen++;
	}

	return node;
}

