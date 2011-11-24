
#pragma once

#include <stdint.h>
#include <cstdlib>
#include "string.h"

enum MoveSpecial {
	M_SWAP    = -1, //-1 so that adding 1 makes it into a valid move
	M_RESIGN  = -2,
	M_NONE    = -3,
	M_UNKNOWN = -4,
};

struct Move {
	int8_t y, x;

	Move(MoveSpecial a = M_UNKNOWN) : y(a), x(120) { } //big x so it will always wrap to y=0 with swap
	Move(int X, int Y) : y(Y), x(X) { }

	Move(const std::string & str, int size = 0){
		if(     str == "swap"   ){ y = M_SWAP;    x = 120; }
		else if(str == "resign" ){ y = M_RESIGN;  x = 120; }
		else if(str == "none"   ){ y = M_NONE;    x = 120; }
		else if(str == "unknown"){ y = M_UNKNOWN; x = 120; }
		else{
			y = tolower(str[0]) - 'a';
			x = atoi(str.c_str() + 1) - 1;

			if(size && y >= size)
				x += y + 1 - size;
		}
	}

	std::string to_s(int size = 0) const {
		if(y == M_UNKNOWN) return "unknown";
		if(y == M_NONE)    return "none";
		if(y == M_SWAP)    return "swap";
		if(y == M_RESIGN)  return "resign";

		if(size && y >= size)
			return std::string() + char(y + 'a') + to_str(x - y + size);
		else
			return std::string() + char(y + 'a') + to_str(x + 1);
	}

	bool operator< (const Move & b) const { return (y == b.y ? x <  b.x : y <  b.y); }
	bool operator<=(const Move & b) const { return (y == b.y ? x <= b.x : y <= b.y); }
	bool operator> (const Move & b) const { return (y == b.y ? x >  b.x : y >  b.y); }
	bool operator>=(const Move & b) const { return (y == b.y ? x >= b.x : y >= b.y); }
	bool operator==(const MoveSpecial & b) const { return (y == b); }
	bool operator==(const Move & b) const { return (y == b.y && x == b.x); }
	bool operator!=(const Move & b) const { return (y != b.y || x != b.x); }
	bool operator!=(const MoveSpecial & b) const { return (y != b); }
	Move operator+ (const Move & b) const { return Move(x + b.x, y + b.y); }
	Move & operator+=(const Move & b)     { y += b.y; x += b.x; return *this; }
	Move operator- (const Move & b) const { return Move(x - b.x, y - b.y); }
	Move & operator-=(const Move & b)     { y -= b.y; x -= b.x; return *this; }

	int z() const { return (x - y); }
	int dist(const Move & b) const {
		return (abs(x - b.x) + abs(y - b.y) + abs(z() - b.z()))/2;
	}
};

struct MoveScore : public Move {
	int16_t score;

	MoveScore() : score(0) { }
	MoveScore(MoveSpecial a) : Move(a), score(0) { }
	MoveScore(int X, int Y, int s) : Move(X, Y), score(s) { }
	MoveScore operator+ (const Move & b) const { return MoveScore(x + b.x, y + b.y, score); }
};

struct MoveValid : public Move {
	int16_t xy;

	MoveValid() : Move(), xy(-1) { }
	MoveValid(int x, int y, int XY) : Move(x,y), xy(XY) { }
	MoveValid(const Move & m, int XY) : Move(m), xy(XY) { }
	bool onboard() const { return xy != -1; }
};

struct PairMove {
	Move a, b;
	PairMove(Move A = M_UNKNOWN, Move B = M_UNKNOWN) : a(A), b(B) { }
	PairMove(MoveSpecial A) : a(Move(A)), b(M_UNKNOWN) { }
};

