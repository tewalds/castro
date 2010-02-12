
#include "solver.h"

void Solver::solve_scout(const Board & board, double time, int mdepth){
	if(board.won() >= 0){
		outcome = board.won();
		return;
	}

	Timer timer = Timer(time, bind(&Solver::timedout, this));
	int starttime = time_msec();

	int turn = board.toplay();

	for(maxdepth = 1; !timeout && maxdepth < mdepth; maxdepth++){
		fprintf(stderr, "Starting depth %d\n", maxdepth);

		int ret = run_negascout(board, maxdepth, -2, 2);

		if(ret){
			if(     ret == -2){ outcome = (turn == 1 ? 2 : 1); X = -1; Y = -1; }
			else if(ret ==  2){ outcome = turn; }
			else /*-1 || 1*/  { outcome = 0; }

			fprintf(stderr, "Finished in %d msec\n", time_msec() - starttime);
			return;
		}
	}
	fprintf(stderr, "Timed out after %d msec\n", time_msec() - starttime);
}

int Solver::run_negascout(const Board & board, const int depth, int alpha, int beta){
	for(int y = 0; y < board.get_size_d(); y++){
		for(int x = 0; x < board.get_size_d(); x++){
			if(!board.valid_move(x, y))
				continue;

			nodes++;

			Board next = board;
			next.move(x, y);

			int value = -negascout(next, maxdepth - 1, -beta, -alpha);

			if(value > alpha){
				alpha = value;
				X = x;
				Y = y;
			}

			if(alpha >= beta)
				return beta;
		}
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
	for(int y = 0; y < board.get_size_d(); y++){
		for(int x = 0; x < board.get_size_d(); x++){
			if(!board.valid_move(x, y))
				continue;

			nodes++;

			Board next = board;
			next.move(x, y);


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
	}
	return alpha;
}

