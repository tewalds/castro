
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

const int neighbours[6][2] = {{-1,-1}, {0,-1}, {1, 0}, {1, 1}, {0, 1}, {-1, 0}}; //x, y, clockwise
const int neighbourscores[18][3] = {
	{-1,-1, 3}, {0,-1, 3}, {1, 0, 3}, {1, 1, 3}, { 0, 1, 3}, {-1, 0, 3}, //direct neighbours
	{-2,-2, 1}, {0,-2, 1}, {2, 0, 1}, {2, 2, 1}, { 0, 2, 1}, {-2, 0, 1}, //corners of ring 2, easy to block
	{-1,-2, 2}, {1,-1, 2}, {2, 1, 2}, {1, 2, 2}, {-1, 1, 2}, {-2,-1, 2}, //sides of ring 2, virtual connections
	};

const int score_range = 8;
const int score_offset = 2;

class Board{
	struct Cell {
		unsigned piece  : 2; //who controls this cell, 0 for none, 1,2 for players
		unsigned parent : 9; //parent for this group of cells
		unsigned size   : 7; //size of this group of cells, technically could be >= 128, but in practice won't happen
		unsigned corner : 6; //which corners are this group connected to
		unsigned edge   : 6; //which edges are this group connected to
		signed   color  : 6; //how black/white a cell is, < 0 is white, > 0 is black
		unsigned near   : 4; //how many cells are taken within the neighbourhood

		Cell() : piece(0), parent(0), size(0), corner(0), edge(0), color(0), near(0) { }
		Cell(unsigned int p, unsigned int a, unsigned int s, unsigned int c, unsigned int e) :
			piece(p), parent(a), size(s), corner(c), edge(e), color(0), near(0) {
			if(corner) near += 3;
			if(edge)   near += 2;
		}

		int numcorners(){
			return BITCOUNT6(corner);
		}
		int numedges(){
			return BITCOUNT6(edge);
		}
	};

public:
	class MoveIterator { //only returns valid moves...
		const Board & board;
		Move move;
	public:
		MoveIterator(const Board & b) : board(b) {
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
					move.x = 0;
					move.y++;
					if(move.y >= board.get_size_d())
						break;
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

//	static const int neighbours[6][2]; //x, y, clockwise

public:
	Board(){ }

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

	int memsize() const {
		return sizeof(Board) + sizeof(Cell)*vecsize();
	}

	int get_size_d() const {
		return size_d;
	}

	int get_size() const{
		return size;
	}
	
	int vecsize() const {
		return (2*size-1)*(2*size-1);
	}
	
	int numcells() const {
		//derived from (2n-1)^2 - n(n-1)
		return 3*size*(size - 1) + 1;
	}
	
	int num_moves() const {
		return nummoves;
	}

	int movesremain() const {
		return numcells() - nummoves;
	}

	int xy(int x, int y) const {
		return y*size_d + x;
	}
	
	int get(int i) const {
		return cells[i].piece;
	}
	int get(int x, int y) const { //assumes valid x,y
		return cells[xy(x, y)].piece;
	}

	//assumes x, y are in array bounds
	bool onboard(int x, int y) const{
		return (y - x < size) && (x - y < size);
	}
	//checks array bounds too
	bool onboard2(int x, int y) const {
        return (x >= 0 && y >= 0 && x < size_d && y < size_d && onboard(x, y) );
	}

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


	int linestart(int y) const {
		return (y < size ? 0 : y - (size-1));
	}

	int linelen(int y) const {
		return size_d - abs((size-1) - y);
	}

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

	bool valid_move(const Move & m) const {
		return valid_move(m.x, m.y);
	}

	bool valid_move(int x, int y) const {
		return (outcome == -1 && onboard2(x, y) && !cells[xy(x, y)].piece);
	}

	void set(int x, int y, int v){
		cells[xy(x, y)].piece = v;
		nummoves++;
		toPlay = 3 - toPlay;
	}

	int find_group(int x, int y){
		return find_group(xy(x, y));
	}
	int find_group(int i){
		if(cells[i].parent != i)
			cells[i].parent = find_group(cells[i].parent);
		return cells[i].parent;
	}

	//join the groups of two positions, propagating group size, and edge/corner connections
	//returns true if they're already the same group, false if they are now joined
	bool join_groups(int x1, int y1, int x2, int y2){
		return join_groups(xy(x1, y1), xy(x2, y2));
	}
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

	// recursively follow a ring
	bool detectring(int x, int y){
		int group = find_group(xy(x, y));
		for(int i = 0; i < 6; i++){
			int nx = x + neighbours[i][0];
			int ny = y + neighbours[i][1];
			
			if(onboard2(nx, ny) && find_group(nx, ny) == group && followring(x, y, nx, ny, i, group))
				return true;
		}
		return false;
		
	}
	// only take the 3 directions that are valid in a ring
	// the backwards directions are either invalid or not part of the shortest loop
	bool followring(const int & sx, const int & sy, const int & cx, const int & cy, const int & dir, const int & group){
		if(sx == cx && sy == cy)
			return true;

		for(int i = 5; i <= 7; i++){
			int nd = (dir + i) % 6;
			int nx = cx + neighbours[nd][0];
			int ny = cy + neighbours[nd][1];
			
			if(onboard2(nx, ny) && find_group(nx, ny) == group && followring(sx, sy, nx, ny, nd, group))
				return true;
		}
		return false;
	}

	bool move(const Move & m, char turn = -1){
		return move(m.x, m.y, turn);
	}

	bool move(int x, int y, char turn = -1){
		if(!valid_move(x, y))
			return false;

		if(turn == -1)
			turn = toplay();

		set(x, y, turn);
//		update_scores(x, y, turn);

		bool alreadyjoined = false; //useful for finding rings
		for(int i = 0; i < 6; i++){
			int X = x + neighbours[i][0];
			int Y = y + neighbours[i][1];
		
			if(onboard2(X, Y) && turn == get(X, Y)){
				alreadyjoined |= join_groups(x, y, X, Y);
				i++; //skip the next one. If it is the same group,
				     //it is already connected and forms a corner, which we can ignore
			}
		}

		Cell * g = & cells[find_group(x, y)];
		if(g->numcorners() >= 2 || g->numedges() >= 3 || (alreadyjoined && g->size >= 6 && detectring(x, y))){
			outcome = turn;
		}else if(nummoves == numcells()){
			outcome = 0;
		}
		return true;	
	}

	void update_scores(int x, int y, char turn){
		turn = (turn == 1 ? -1 : 1);
		for(int i = 0; i < 18; i++){
			int X = x + neighbourscores[i][0];
			int Y = y + neighbourscores[i][1];
			if(onboard2(X, Y)){
				Cell * cell = & cells[xy(X, Y)];
				if(!cell->piece){
					if(cell->near < 15) //max value near can hold in 4 bits
						cell->near++;
					cell->color += turn * neighbourscores[i][2];
				}
			}
		}
	}
	
	int calc_score(int x, int y, char turn) const {
		const Cell * cell = & cells[xy(x, x)];
		return (score_range - abs(score_offset*turn - cell->color))*cell->near;
	}

	static bool cmpmoves(const MoveScore & a, const MoveScore & b) {
		return a.score > b.score;
	}

	int get_moves(MoveScore * moves, bool s = false, char turn = -1) const {
		MoveScore * mend = moves;
		turn = (turn == 1 ? 1 : -1);

		for(MoveIterator move = moveit(); !move.done(); ++move){
			*mend = MoveScore(*move, calc_score(move->x, move->y, turn));
			mend++;
		}

		if(s)
			sort(moves, mend, cmpmoves);
		return mend - moves;
	}
};

#endif

