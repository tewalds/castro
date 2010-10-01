
#include "solverab.h"

void SolverAB::solve(double time){
	reset();
	if(rootboard.won() >= 0){
		outcome = rootboard.won();
		return;
	}
	rootboard.setswap(false);

	Timer timer(time, bind(&SolverAB::timedout, this));
	int starttime = time_msec();

	int turn = rootboard.toplay();

	for(maxdepth = 2; !timeout; maxdepth++){
		fprintf(stderr, "Starting depth %d\n", maxdepth);

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

			fprintf(stderr, "Finished in %d msec\n", time_msec() - starttime);
			return;
		}
	}
	fprintf(stderr, "Timed out after %d msec\n", time_msec() - starttime);
}


int SolverAB::negamax(Board & board, const int depth, int alpha, int beta){
	if(board.won() >= 0)
		return (board.won() ? -2 : -1);

	if(depth <= 0 || timeout)
		return 0;

	int b = beta;
	int first = true;
	int value, losses = 0;
	static const int lookup[4] = {0, 1, 2, 2};
	for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
		nodes_seen++;

		if(depth <= 2){
			value = lookup[board.test_win(*move)+1];

			if(board.test_win(*move, 3 - board.toplay()) > 0)
				losses++;
		}else{
			Board next = board;
			next.move(*move, true, false);

			value = -negamax(next, depth - 1, -b, -alpha);

			if(scout && value > alpha && value < beta && !first) // re-search
				value = -negamax(next, depth - 1, -beta, -alpha);
		}

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

