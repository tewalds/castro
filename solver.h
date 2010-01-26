
#ifndef _SOLVER_H_
#define _SOLVER_H_

class Solver {
public:
	int outcome;
	int maxdepth;
	int nodes;
	int x, y;
	
	Solver(const Board & board, double time){
		outcome = 0;
		maxdepth = 0;
		nodes = 1;
		x = y = 0;

		if(board.won() >= 0){
			outcome = board.won();
			return;
		}

		int turn = board.toplay();

		int alpha = -1;
		for(maxdepth = 1; true; maxdepth++){
			fprintf(stderr, "Starting depth %d\n", maxdepth);
			for(y = 0; y < board.get_size_d(); y++){
				for(x = 0; x < board.get_size_d(); x++){
					if(!board.valid_move(x, y))
						continue;

					Board next = board;
					next.move(x, y);

					int ret = -negamax(next, 1, -1, -alpha);

					if(ret > alpha)
						alpha = ret;
					
					if(alpha >= 1){
						outcome = turn;
						return;
					}
				}
			}
			if(alpha == -1){
				outcome = (turn == 1 ? 2 : 1);
				return;
			}
		}
	}
	
protected:
	int negamax(const Board & board, const int depth, int alpha, int beta){
		if(board.won() >= 0){
//			fprintf(stderr, "won: %d -> %d\n", board.won(), (board.won() ? -1 : 0));
			return (board.won() ? -1 : 0);
		}

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

//	fprintf(stderr, "%s\n", next.to_s().c_str());
//	fprintf(stderr, "%d: %d,%d -> %d\n", depth, x, y, value);

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

