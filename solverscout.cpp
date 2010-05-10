
#include "solver.h"

void Solver::solve_scout(Board board, double time, int mdepth){
	reset();
	if(board.won() >= 0){
		outcome = board.won();
		return;
	}
	board.setswap(false);

	Timer timer = Timer(time, bind(&Solver::timedout, this));
	int starttime = time_msec();

	int turn = board.toplay();

	for(maxdepth = 1; !timeout && maxdepth < mdepth; maxdepth++){
		fprintf(stderr, "Starting depth %d\n", maxdepth);

		int ret = run_negascout(board, maxdepth, -2, 2);

		if(ret){
			if(     ret == -2){ outcome = (turn == 1 ? 2 : 1); bestmove = Move(M_UNKNOWN); }
			else if(ret ==  2){ outcome = turn; }
			else /*-1 || 1*/  { outcome = 0; }

			fprintf(stderr, "Finished in %d msec\n", time_msec() - starttime);
			return;
		}
	}
	fprintf(stderr, "Timed out after %d msec\n", time_msec() - starttime);
}

int Solver::run_negascout(const Board & board, const int depth, int alpha, int beta){
	for(Board::MoveIterator move = board.moveit(); !move.done(); ++move){
		nodes_seen++;

		Board next = board;
		next.move(*move);

		int value = -negascout(next, maxdepth - 1, -beta, -alpha);

		if(value > alpha){
			alpha = value;
			bestmove = *move;
		}

		if(alpha >= beta)
			return beta;
	}
	return alpha;
}

int Solver::negascout(const Board & board, const int depth, int alpha, int beta){
	if(board.won() >= 0)
		return (board.won() ? -2 : -1);

	if(depth <= 0 || timeout)
		return 0;

	int b = beta;
	int first = true;
	for(Board::MoveIterator move = board.moveit(); !move.done(); ++move){
		nodes_seen++;

		Board next = board;
		next.move(*move);

		int value = -negascout(next, depth - 1, -b, -alpha);

		if(value > alpha && value < beta && !first) // re-search
			value = -negascout(next, depth - 1, -beta, -alpha);

		if(value > alpha)
			alpha = value;

		if(alpha >= beta)
			return beta;

		b = alpha + 1; // set up null window
		first = false;
	}
	return alpha;
}

