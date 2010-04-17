
#ifndef _MOVE_H_
#define _MOVE_H_

#include <stdint.h>
#include <cstdlib>

enum MoveSpecial {
	M_SWAP    = -1, //-1 so that adding 1 makes it into a valid move
	M_RESIGN  = -2,
	M_NONE    = -3,
	M_UNKNOWN = -4,
};

struct Move {
	int8_t y, x;

	Move() : y(0), x(0) { }
	Move(MoveSpecial a) : y(a), x(120) { } //big x so it will always wrap to y=0 with swap
	Move(int X, int Y) : y(Y), x(X) { }

	bool operator< (const Move & b) const { return (y == b.y ? x <  b.x : y <  b.y); }
	bool operator<=(const Move & b) const { return (y == b.y ? x <= b.x : y <= b.y); }
	bool operator> (const Move & b) const { return (y == b.y ? x >  b.x : y >  b.y); }
	bool operator>=(const Move & b) const { return (y == b.y ? x >= b.x : y >= b.y); }
	bool operator==(const MoveSpecial & b) const { return (y == b); }
	bool operator==(const Move & b) const { return (y == b.y && x == b.x); }
	bool operator!=(const Move & b) const { return (y != b.y || x != b.x); }
	Move operator+ (const Move & b) const { return Move(x + b.x, y + b.y); }
	Move & operator+=(const Move & b)     { y += b.y; x += b.x; return *this; }
	Move operator- (const Move & b) const { return Move(x - b.x, y - b.y); }
	Move & operator-=(const Move & b)     { y -= b.y; x -= b.x; return *this; }

	int dist(const Move & b) const {
		return (abs(x - b.x) + abs(y - b.y) + abs((x - y) - (b.x - b.y)))/2;
	}
};

struct MoveScore : public Move {
	int8_t score;

	MoveScore() : score(0) { }
	MoveScore(MoveSpecial a) : Move(a), score(0) { }
	MoveScore(int X, int Y, int s) : Move(X, Y), score(s) { }
};

#endif

