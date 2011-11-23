
#pragma once

#include <stdint.h>
#include "board.h"

class Solver {
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

protected:
	volatile bool timeout;
	void timedout(){ timeout = true; }
	Board rootboard;

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

};

