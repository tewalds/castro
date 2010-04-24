
#ifndef _BOARD_H_
#define _BOARD_H_

#include <cstdio>
#include <algorithm>
#include <vector>
#include <string>
using namespace std;

#include "move.h"

#define BITCOUNT6(a) ((a & 1) + ((a & (1<<1))>>1) + ((a & (1<<2))>>2) + ((a & (1<<3))>>3) + ((a & (1<<4))>>4) + ((a & (1<<5))>>5))
/*
 * the board is represented as a flattened 2d array of the form:
 *   1 2 3
 * A 0 1 2    0 1       0 1
 * B 3 4 5 => 3 4 5 => 3 4 5
 * C 6 7 8      7 8     7 8
 * This follows the H-Gui convention, not the 'standard' convention
 */

const MoveScore neighbours[18] = {
	MoveScore(-1,-1, 3), MoveScore(0,-1, 3), MoveScore(1, 0, 3), MoveScore(1, 1, 3), MoveScore( 0, 1, 3), MoveScore(-1, 0, 3), //direct neighbours, clockwise
	MoveScore(-2,-2, 1), MoveScore(0,-2, 1), MoveScore(2, 0, 1), MoveScore(2, 2, 1), MoveScore( 0, 2, 1), MoveScore(-2, 0, 1), //corners of ring 2, easy to block
	MoveScore(-1,-2, 2), MoveScore(1,-1, 2), MoveScore(2, 1, 2), MoveScore(1, 2, 2), MoveScore(-1, 1, 2), MoveScore(-2,-1, 2), //sides of ring 2, virtual connections
	};

class Board{
	struct Cell {
		unsigned piece  : 2; //who controls this cell, 0 for none, 1,2 for players
		unsigned parent : 9; //parent for this group of cells
		unsigned size   : 7; //size of this group of cells
		unsigned corner : 6; //which corners are this group connected to
		unsigned edge   : 6; //which edges are this group connected to
		unsigned local  : 2; //0 for far, 1 for distance 2, 2 for virtual connection, 3 for neighbour

		Cell() : piece(0), parent(0), size(0), corner(0), edge(0), local(0) { }
		Cell(unsigned int p, unsigned int a, unsigned int s, unsigned int c, unsigned int e) :
			piece(p), parent(a), size(s), corner(c), edge(e) { }

		int numcorners(){ return BITCOUNT6(corner); }
		int numedges(){   return BITCOUNT6(edge); }
	};

public:
	class MoveIterator { //only returns valid moves...
		const Board & board;
		Move move;
	public:
		MoveIterator(const Board & b) : board(b), move(Move(M_SWAP)) {
			if(!board.valid_move(move))
				++(*this);
		}

		const Move & operator * () const {
			return move;
		}
		const Move * operator -> () const {
			return & move;
		}
		bool done() const {
			return (move.y >= board.get_size_d());
		}
		bool operator == (const Board::MoveIterator & rhs) const {
			return (move == rhs.move);
		}
		bool operator != (const Board::MoveIterator & rhs) const {
			return (move != rhs.move);
		}
		MoveIterator & operator ++ (){ //prefix form
			do{
				move.x++;

				if(move.x >= board.get_size_d()){
					move.y++;
					if(move.y >= board.get_size_d())
						break;

					move.x = board.linestart(move.y);
				}
			}while(!board.valid_move(move));

			return *this;
		}
		MoveIterator operator ++ (int){ //postfix form, discouraged from being used
			MoveIterator newit(*this);
			++(*this);
			return newit;
		}
	};

private:
	short size; //the length of one side of the hexagon
	short size_d; //diameter of the board = size*2-1

	short nummoves;
	char toPlay;
	char outcome; //-1 = unknown, 0 = tie, 1,2 = player win

	vector<Cell> cells;

public:
	Board(){
		size = 0;
	}

	Board(int s){
		size = s;
		size_d = s*2-1;
		nummoves = 0;
		toPlay = 1;
		outcome = -1;

		cells.resize(vecsize());

		for(int y = 0; y < size_d; y++){
			for(int x = 0; x < size_d; x++){
				int i = xy(x, y);
				cells[i] = Cell(0, i, 1, (1 << iscorner(x, y)), (1 << isedge(x, y)));
			}
		}
	}

	int memsize() const { return sizeof(Board) + sizeof(Cell)*vecsize(); }

	int get_size_d() const { return size_d; }
	int get_size() const{ return size; }

	int vecsize() const { return size_d*size_d; }
	int numcells() const { return vecsize() - size*(size - 1); }

	int num_moves() const { return nummoves; }
	int movesremain() const { return numcells() - nummoves + canswap(); }

	int xy(int x, int y)   const { return   y*size_d +   x; }
	int xy(const Move & m) const { return m.y*size_d + m.x; }
	
	//assumes valid x,y
	int get(int i)          const { return cells[i].piece; }
	int get(int x, int y)   const { return get(xy(x,y)); }
	int get(const Move & m) const { return get(xy(m)); }

	int local(const Move & m) const { return cells[xy(m)].local; }

	//assumes x, y are in array bounds
	bool onboard(int x, int y)   const { return (  y -   x < size) && (  x -   y < size); }
	bool onboard(const Move & m) const { return (m.y - m.x < size) && (m.x - m.y < size); }
	//checks array bounds too
	bool onboard2(int x, int y)  const { return (  x >= 0 &&   y >= 0 &&   x < size_d &&   y < size_d && onboard(x, y) ); }
	bool onboard2(const Move & m)const { return (m.x >= 0 && m.y >= 0 && m.x < size_d && m.y < size_d && onboard(m) ); }

	bool canswap() const { return (nummoves == 1 && toPlay == 2); }

	bool valid_move(int x, int y)   const { return (outcome == -1 && onboard2(x, y) && !get(x,y)); } //ignores swap rule!
	bool valid_move(const Move & m) const { return (outcome == -1 && ((onboard2(m) && !get(m)) || (m == M_SWAP && canswap()))); }

	int iscorner(int x, int y) const {
		if(!onboard(x,y))
			return -1;

		int m = size-1, e = size_d-1;

		if(x == 0 && y == 0) return 0;
		if(x == m && y == 0) return 1;
		if(x == e && y == m) return 2;
		if(x == e && y == e) return 3;
		if(x == m && y == e) return 4;
		if(x == 0 && y == m) return 5;

		return -1;
	}

	int isedge(int x, int y) const {
		if(!onboard(x,y))
			return -1;

		int m = size-1, e = size_d-1;

		if(y   == 0 && x != 0 && x != m) return 0;
		if(x-y == m && x != m && x != e) return 1;
		if(x   == e && y != m && y != e) return 2;
		if(y   == e && x != e && x != m) return 3;
		if(y-x == m && x != m && x != 0) return 4;
		if(x   == 0 && y != m && y != 0) return 5;

		return -1;
	}


	int linestart(int y) const { return (y < size ? 0 : y - (size-1)); }
	int linelen(int y)   const { return size_d - abs((size-1) - y); }

	string to_s() const {
		string s;
		for(int y = 0; y < size_d; y++){
			s += string(abs(size-1 - y) + 2, ' ');
			for(int x = 0; x < size_d; x++){
				if(onboard(x, y)){
					int p = get(x, y);
					if(p == 0) s += '.';
					if(p == 1) s += 'W';
					if(p == 2) s += 'B';
					s += ' ';
				}
			}
			s += '\n';
		}
		return s;
	}
	
	void print() const {
		printf("%s", to_s().c_str());
	}
	
	string won_str() const {
		switch(outcome){
			case -1: return "none";
			case 0:  return "draw";
			case 1:  return "white";
			case 2:  return "black";
		}
		return "unknown";
	}

	char won() const {
		return outcome;
	}

	int win() const{
		if(outcome <= 0)
			return 0;
		return (outcome == toplay() ? 1 : -1);
	}

	char toplay() const {
		return toPlay;
	}

	MoveIterator moveit() const {
		return MoveIterator(*this);
	}

	void set(const Move & m, int v){
		cells[xy(m)].piece = v;
		nummoves++;
		toPlay = 3 - toPlay;
	}
	void doswap(){
		for(int y = 0; y < size_d; y++){
			for(int x = 0; x < size_d; x++){
				if(get(x,y) != 0){
					cells[xy(x,y)].piece = 2;
					toPlay = 1;
					return;
				}
			}
		}
	}

	int find_group(const Move & m) { return find_group(xy(m)); }
	int find_group(int x, int y)   { return find_group(xy(x, y)); }
	int find_group(int i){
		if(cells[i].parent != i)
			cells[i].parent = find_group(cells[i].parent);
		return cells[i].parent;
	}

	//join the groups of two positions, propagating group size, and edge/corner connections
	//returns true if they're already the same group, false if they are now joined
	bool join_groups(const Move & a, const Move & b) { return join_groups(xy(a), xy(b)); }
	bool join_groups(int x1, int y1, int x2, int y2) { return join_groups(xy(x1, y1), xy(x2, y2)); }
	bool join_groups(int i, int j){
		i = find_group(i);
		j = find_group(j);
		
		if(i == j)
			return true;
		
		if(cells[i].size < cells[j].size) //force i's subtree to be bigger
			swap(i, j);

		cells[j].parent = i;
		cells[i].size   += cells[j].size;
		cells[i].corner |= cells[j].corner;
		cells[i].edge   |= cells[j].edge;
		
		return false;
	}

	int test_connectivity(const Move & pos){
		char turn = toplay();

		Cell testcell;
		for(int i = 0; i < 6; i++){
			Move loc = pos + neighbours[i];

			if(onboard2(loc) && turn == get(loc)){
				Cell * g = & cells[find_group(loc)];
				testcell.corner |= g->corner;
				testcell.edge   |= g->edge;
				i++; //skip the next one
			}
		}
		return testcell.numcorners() + testcell.numedges();
	}

	// recursively follow a ring
	bool detectring(const Move & pos, char turn){
		for(int i = 0; i < 6; i++){
			Move loc = pos + neighbours[i];
			
			if(onboard2(loc) && turn == get(loc) && followring(pos, loc, i, turn))
				return true;
		}
		return false;
	}
	// only take the 3 directions that are valid in a ring
	// the backwards directions are either invalid or not part of the shortest loop
	bool followring(const Move & start, const Move & cur, const int & dir, const int & turn){
		for(int i = 5; i <= 7; i++){
			int nd = (dir + i) % 6;
			Move next = cur + neighbours[nd];

			if(start == next || (onboard2(next) && turn == get(next) && followring(start, next, nd, turn)))
				return true;
		}
		return false;
	}

	bool move(const Move & pos, bool local = false){
		if(!valid_move(pos))
			return false;

		if(pos == M_SWAP){
			doswap();
			return true;
		}

		char turn = toplay();

		set(pos, turn);

		if(local){
			for(int i = 0; i < 18; i++){
				MoveScore loc = neighbours[i] + pos;

				if(onboard2(loc))
					cells[xy(loc)].local |= loc.score;
			}
		}

		bool alreadyjoined = false; //useful for finding rings
		for(int i = 0; i < 6; i++){
			Move loc = pos + neighbours[i];
		
			if(onboard2(loc) && turn == get(loc)){
				alreadyjoined |= join_groups(pos, loc);
				i++; //skip the next one. If it is the same group,
				     //it is already connected and forms a corner, which we can ignore
			}
		}

		Cell * g = & cells[find_group(pos)];
		if(g->numcorners() >= 2 || g->numedges() >= 3 || (alreadyjoined && g->size >= 6 && detectring(pos, turn))){
			outcome = turn;
		}else if(nummoves == numcells()){
			outcome = 0;
		}
		return true;	
	}

	//test if making this move would win, but don't actually make the move
	int test_win(const Move & pos){
		char turn = toplay();

		Cell testcell;
		int numgroups = 0;
		for(int i = 0; i < 6; i++){
			Move loc = pos + neighbours[i];

			if(onboard2(loc) && turn == get(loc)){
				Cell * g = & cells[find_group(loc)];
				testcell.corner |= g->corner;
				testcell.edge   |= g->edge;
				i++; //skip the next one
				numgroups++;
			}
		}

		if(testcell.numcorners() >= 2 || testcell.numedges() >= 3 || (numgroups >= 2 && detectring(pos, turn)))
			return turn;

		if(nummoves+1 == numcells())
			return 0;

		return -1;
	}
};

#endif

