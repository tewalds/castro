
#ifndef _SOLVER_H_
#define _SOLVER_H_

#include <time.h>
#include <sys/time.h>

int time_msec(){
	struct timeval time;
	gettimeofday(&time, NULL);
	return (time.tv_sec*1000 + time.tv_usec/1000);
}

class Solver {
public:
	int outcome;
	int maxdepth;
	int nodes;
	int x, y;
	
	Solver(const Board & board, double time){
		outcome = -1;
		maxdepth = 0;
		nodes = 1;
		x = y = -1;

		if(board.won() >= 0){
			outcome = board.won();
			return;
		}

		int starttime = time_msec();

		int turn = board.toplay();

		for(maxdepth = 1; time_msec() - starttime < time*1000; maxdepth++){
			fprintf(stderr, "Starting depth %d\n", maxdepth);

			int ret = negamax(board, 0, -2, 2);

			if(ret){
				if(     ret == -2) outcome = (turn == 1 ? 2 : 1);
				else if(ret == 2)  outcome = turn;
				else /*-1 || 1*/   outcome = 0;

				fprintf(stderr, "Finished in %d msec\n", time_msec() - starttime);
				return;
			}
		}
		fprintf(stderr, "Timed out after %d msec\n", time_msec() - starttime);
	}
	
protected:
	int negamax(const Board & board, const int depth, int alpha, int beta){
		if(board.won() >= 0)
			return (board.won() ? -2 : -1);

		if(depth > maxdepth)
			return 0;

		int turn = board.toplay();

		for(int y = 0; y < board.get_size_d(); y++){
			for(int x = 0; x < board.get_size_d(); x++){
				if(!board.valid_move(x, y))
					continue;

				nodes++;
//				depths[depth]++;

				Board next = board;
				next.move(x, y);

				int value = -negamax(next, depth + 1, -beta, -alpha);

				if(value > alpha)
					alpha = value;

				if(alpha >= beta)
					return beta;
			}
		}
		return alpha;
	}
};

#endif

