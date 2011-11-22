
#ifndef _SOLVER_H_
#define _SOLVER_H_

#include <stdint.h>
#include "board.h"
//#include "log.h"

class Solver {

protected:
	volatile bool timeout;
	void timedout(){ timeout = true; }
	Board rootboard;

public:
	int outcome; // 0 = tie, 1 = white, 2 = black, -1 = white or tie, -2 = black or tie, anything else unknown
	int maxdepth;
	uint64_t nodes_seen;
	double time_used;
	Move bestmove;

	Solver() : outcome(-3), maxdepth(0), nodes_seen(0), time_used(0) { }
	virtual ~Solver() { }

	virtual void solve(double time) { }
	virtual void set_board(const Board & board, bool clear = true) { }
	virtual void move(const Move & m) { }
	virtual void set_memlimit(uint64_t lim) { } // in bytes
	virtual void clear_mem() { }


	//returns outcome, -3, 0, 1, 2
	static int solve1ply(const Board & board, int & nodes) {
		int outcome = -3;
		int turn = board.toplay();
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			++nodes;
			int won = board.test_win(*move, turn);

			if(won == turn)
				return won;
			if(won == 0)
				outcome = 0;
		}
		return outcome;
	}

	//returns outcome, -3, 0, 1, 2
	static int solve2ply(const Board & board, int & nodes) {
		int losses = 0;
		int outcome = -3;
		int turn = board.toplay(), opponent = 3 - turn;
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			++nodes;
			int won = board.test_win(*move, turn);

			if(won == turn)
				return won;
			if(won == 0)
				outcome = 0;

			if(board.test_win(*move, opponent) > 0)
				losses++;
		}
		if(losses >= 2)
			return opponent;
		return outcome;
	}

	static PairMove solvebylast(const Board & board, const Move & prev, bool checkrings = true){

//		logerr(prev.to_s() + ":\n");
//		logerr(board.to_s(true));

		const Board::Cell * c = board.cell(board.find_group(prev));
		if(c->numcorners() == 0 && c->numedges() == 0 && c->size < 5){
//			logerr("c " + to_str(c->numcorners()) + " e " + to_str(c->numedges()) + " s " + to_str((int)c->size) + "\n");
			return M_UNKNOWN;
		}

		Move start, cur, loss = M_UNKNOWN;
		int turn = 3 - board.toplay();

		//find the first empty cell
		int dir = -1;
		for(int i = 0; i <= 5; i++){
			start = prev + neighbours[i];

			if(!board.onboard(start) || board.get(start) != turn){
				dir = (i + 5) % 6;
				break;
			}
		}

		if(dir == -1){ //possible if it's in the middle of a ring, which is possible if rings are being ignored
//			logerr("dir == -1\n");
			return M_UNKNOWN;
		}


		//advance to the next cell, do this early so the sum of directions works out
		int startdir = 0;
		for(int i = 5; i <= 9; i++){
			int nd = (dir + i) % 6;
			Move next = start + neighbours[nd];

			if(!board.onboard(next) || board.get(next) != turn){
				start = next;
				startdir = nd;
				break;
			}
		}

		cur = start;
		dir = startdir;

		//follow contour of the current group looking for wins
		do{
//			if(board.onboard(cur)) logerr(cur.to_s() + " ");
//			else                   logerr(to_str((int)cur.y) + "," + to_str((int)cur.x) + " ");

			//check the current cell
			if(board.onboard(cur) && board.get(cur) == 0 && board.test_win(cur, turn, checkrings) > 0){
//				logerr(cur.to_s() + " loss ");
				if(loss == M_UNKNOWN)
					loss = cur;
				else if(loss != cur){ //don't count finding the same loss twice
//					logerr("2 losses\n");
					return PairMove(loss, cur); //game over, two wins found for opponent
				}
			}

			//advance to the next cell
			for(int i = 5; i <= 9; i++){
				int nd = (dir + i) % 6;
				Move next = cur + neighbours[nd];

				if(!board.onboard(next) || board.get(next) != turn){
					cur = next;
					dir = nd;
					break;
				}
			}
		}while(cur != start || dir != startdir);

//		logerr("found " + loss.to_s() + "\n");

		return loss;
	}
};

#endif

