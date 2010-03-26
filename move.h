
#ifndef _MOVE_H_
#define _MOVE_H_

#include <stdint.h>

#define M_SWAP    -1
#define M_RESIGN  -2
#define M_NONE    -3
#define M_UNKNOWN -4

struct Move {
	int8_t y, x;

	Move() : y(0), x(0) { }
	Move(int a) : y(a), x(120) { } //big x so it will always wrap to y=0 with swap
	Move(int X, int Y) : y(Y), x(X) { }

	bool operator< (const Move & b) const { return (y == b.y ? x <  b.x : y <  b.y); }
	bool operator<=(const Move & b) const { return (y == b.y ? x <= b.x : y <= b.y); }
	bool operator> (const Move & b) const { return (y == b.y ? x >  b.x : y >  b.y); }
	bool operator>=(const Move & b) const { return (y == b.y ? x >= b.x : y >= b.y); }
	bool operator==(const int  & b) const { return (y == b); }
	bool operator==(const Move & b) const { return (y == b.y && x == b.x); }
	bool operator!=(const Move & b) const { return (y != b.y || x != b.x); }
	Move operator+ (const Move & b) const { return Move(x + b.x, y + b.y); }
	Move & operator+=(const Move & b)     { y += b.y; x += b.x; return *this; }
};

#endif

