
#include "solverab.h"
#include "time.h"
#include "timer.h"
#include "log.h"

void SolverAB::solve(double time){
	reset();
	if(rootboard.won() >= 0){
		outcome = rootboard.won();
		return;
	}
	rootboard.setswap(false);

	if(TT == NULL && maxnodes)
		TT = new ABTTNode[maxnodes];

	Timer timer(time, bind(&SolverAB::timedout, this));
	Time start;

	int turn = rootboard.toplay();

	for(maxdepth = startdepth; !timeout; maxdepth++){
//		logerr("Starting depth " + to_str(maxdepth) + "\n");

		//the first depth of negamax
		int ret, alpha = -2, beta = 2;
		for(Board::MoveIterator move = rootboard.moveit(true); !move.done(); ++move){
			nodes_seen++;

			Board next = rootboard;
			next.move(*move, true, false);

			int value = -negamax(next, maxdepth - 1, -beta, -alpha);

			if(value > alpha){
				alpha = value;
				bestmove = *move;
			}

			if(alpha >= beta){
				ret = beta;
				break;
			}
		}
		ret = alpha;


		if(ret){
			if(     ret == -2){ outcome = (turn == 1 ? 2 : 1); bestmove = Move(M_NONE); }
			else if(ret ==  2){ outcome = turn; }
			else /*-1 || 1*/  { outcome = 0; }

			break;
		}
	}

	time_used = Time() - start;
}


int SolverAB::negamax(const Board & board, const int depth, int alpha, int beta){
	if(board.won() >= 0)
		return (board.won() ? -2 : -1);

	if(depth <= 0 || timeout)
		return 0;

	int b = beta;
	int first = true;
	int value, losses = 0;
	static const int lookup[6] = {0, 0, 0, 1, 2, 2};
	for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
		nodes_seen++;

		hash_t hash = board.test_hash(*move);
		if(int ttval = tt_get(hash)){
			value = ttval;
		}else if(depth <= 2){
			value = lookup[board.test_win(*move)+3];

			if(board.test_win(*move, 3 - board.toplay()) > 0)
				losses++;
		}else{
			Board next = board;
			next.move(*move, true, false);

			value = -negamax(next, depth - 1, -b, -alpha);

			if(scout && value > alpha && value < beta && !first) // re-search
				value = -negamax(next, depth - 1, -beta, -alpha);
		}
		tt_set(hash, value);

		if(value > alpha)
			alpha = value;

		if(alpha >= beta)
			return beta;

		if(scout){
			b = alpha + 1; // set up null window
			first = false;
		}
	}

	if(losses >= 2)
		return -2;

	return alpha;
}

int SolverAB::negamax_outcome(const Board & board, const int depth){
	int abval = negamax(board, depth, -2, 2);
	if(     abval == 0)  return -1; //unknown
	else if(abval == 2)  return board.toplay(); //win
	else if(abval == -2) return 3 - board.toplay(); //loss
	else                 return 0; //draw
}

int SolverAB::tt_get(const Board & board){
	return tt_get(board.gethash());
}
int SolverAB::tt_get(const hash_t & hash){
	if(!TT) return 0;
	ABTTNode * node = & TT[hash % maxnodes];
	return (node->hash == hash ? node->value : 0);
}
void SolverAB::tt_set(const Board & board, int value){
	tt_set(board.gethash(), value);
}
void SolverAB::tt_set(const hash_t & hash, int value){
	if(!TT || value == 0) return;
	ABTTNode * node = & TT[hash % maxnodes];
	node->hash = hash;
	node->value = value;
}

