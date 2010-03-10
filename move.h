
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
};

struct MoveScore {
	int8_t x, y;
	float score;

	MoveScore(int X = 0, int Y = 0, float s = 0) : x(X), y(Y), score(s) { }
	MoveScore(const Move & m, float s = 0) : x(m.x), y(m.y), score(s) { }

	bool operator< (const Move      & b) const { return (y == b.y ? x <  b.x : y <  b.y); }
	bool operator< (const MoveScore & b) const { return (y == b.y ? x <  b.x : y <  b.y); }
	bool operator<=(const Move      & b) const { return (y == b.y ? x <= b.x : y <= b.y); }
	bool operator<=(const MoveScore & b) const { return (y == b.y ? x <= b.x : y <= b.y); }
	bool operator> (const Move      & b) const { return (y == b.y ? x >  b.x : y >  b.y); }
	bool operator> (const MoveScore & b) const { return (y == b.y ? x >  b.x : y >  b.y); }
	bool operator>=(const Move      & b) const { return (y == b.y ? x >= b.x : y >= b.y); }
	bool operator>=(const MoveScore & b) const { return (y == b.y ? x >= b.x : y >= b.y); }
	bool operator==(const Move      & b) const { return (x == b.x && y == b.y); }
	bool operator==(const MoveScore & b) const { return (x == b.x && y == b.y); }
	bool operator!=(const Move      & b) const { return (x != b.x || y != b.y); }
	bool operator!=(const MoveScore & b) const { return (x != b.x || y != b.y); }
};

#endif

