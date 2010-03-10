
#ifndef _MOVE_H_
#define _MOVE_H_

#include <stdint.h>

#define M_UNKNOWN -1
#define M_NONE    -2
#define M_SWAP    -3
#define M_RESIGN  -4

struct Move {
	int8_t x, y;

	Move(int X = 0, int Y = 0) : x(X), y(Y) { }
	Move(const Move & m){ x = m.x; y = m.y; }

	bool operator< (const Move & b) const { return (y == b.y ? x <  b.x : y <  b.y); }
	bool operator<=(const Move & b) const { return (y == b.y ? x <= b.x : y <= b.y); }
	bool operator> (const Move & b) const { return (y == b.y ? x >  b.x : y >  b.y); }
	bool operator>=(const Move & b) const { return (y == b.y ? x >= b.x : y >= b.y); }
	bool operator==(const Move & b) const { return (x == b.x && y == b.y); }
	bool operator!=(const Move & b) const { return (x != b.x || y != b.y); }
	Move operator+ (const Move & b) const { return Move(x + b.x, y + b.y); }
	Move & operator+=(const Move & b)     { x += b.x; y += b.y; return *this; }
};

#endif

