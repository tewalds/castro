
#ifndef _MOVE_H_
#define _MOVE_H_

#include <stdint.h>

struct Move {
	int8_t x, y;
//	Move() { }
	Move(int X = 0, int Y = 0) : x(Y), y(Y) { }

	bool operator<(const Move & b) const {
		return (y == b.y ? x < b.x : y < b.y);
	}
};

struct MoveScore {
	int x, y, score;
//	MoveScore() { }
	MoveScore(int X = 0, int Y = 0, int s = 0) : x(X), y(Y), score(s) { }
};

#endif

