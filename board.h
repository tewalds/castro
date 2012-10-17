
#pragma once

#include <cstdio>
#include <algorithm>
#include <vector>
#include <string>
#include <cassert>
using namespace std;

#include "move.h"
#include "string.h"
#include "zobrist.h"
#include "hashset.h"

static const int BitsSetTable64[] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6
};

/*
 * the board is represented as a flattened 2d array of the form:
 *   1 2 3
 * A 0 1 2    0 1       0 1
 * B 3 4 5 => 3 4 5 => 3 4 5
 * C 6 7 8      7 8     7 8
 * This follows the H-Gui convention, not the 'standard' convention
 */

/* neighbours are laid out in this pattern:
 *      6  12   7
 *   17   0   1  13
 * 11   5   X   2   8
 *   16   4   3  14
 *     10  15   9
 */
const MoveScore neighbours[18] = {
	MoveScore(-1,-1, 3), MoveScore(0,-1, 3), MoveScore(1, 0, 3), MoveScore(1, 1, 3), MoveScore( 0, 1, 3), MoveScore(-1, 0, 3), //direct neighbours, clockwise
	MoveScore(-2,-2, 1), MoveScore(0,-2, 1), MoveScore(2, 0, 1), MoveScore(2, 2, 1), MoveScore( 0, 2, 1), MoveScore(-2, 0, 1), //corners of ring 2, easy to block
	MoveScore(-1,-2, 2), MoveScore(1,-1, 2), MoveScore(2, 1, 2), MoveScore(1, 2, 2), MoveScore(-1, 1, 2), MoveScore(-2,-1, 2), //sides of ring 2, virtual connections
	};

static MoveValid * staticneighbourlist[11] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL}; //one per boardsize


class Board{
public:
	struct Cell {
/*
		unsigned piece  : 2; //who controls this cell, 0 for none, 1,2 for players
		mutable unsigned parent : 9; //parent for this group of cells
		unsigned size   : 7; //size of this group of cells
		unsigned corner : 6; //which corners are this group connected to
		unsigned edge   : 6; //which edges are this group connected to
		unsigned local  : 2; //0 for far, 1 for distance 2, 2 for virtual connection, 3 for neighbour
/*/
		uint8_t piece;  //who controls this cell, 0 for none, 1,2 for players
		uint8_t size;   //size of this group of cells
mutable uint16_t parent; //parent for this group of cells
		uint8_t corner; //which corners are this group connected to
		uint8_t edge;   //which edges are this group connected to
		unsigned perm : 4;   //is this a permanent piece or a randomly placed piece?
		unsigned local: 4;  //0 for far, 1 for distance 2, 2 for virtual connection, 3 for neighbour
mutable uint8_t ringdepth; //when doing a ring search, what depth was this position found
//*/

		Cell() : piece(0), size(0), parent(0), corner(0), edge(0), perm(0), local(0), ringdepth(0) { }
		Cell(unsigned int p, unsigned int a, unsigned int s, unsigned int c, unsigned int e, unsigned int l) :
			piece(p), size(s), parent(a), corner(c), edge(e), perm(0), local(l), ringdepth(0) { }

		int numcorners() const { return BitsSetTable64[corner]; }
		int numedges()   const { return BitsSetTable64[edge];   }
	};

	class MoveIterator { //only returns valid moves...
		const Board & board;
		int lineend;
		Move move;
		bool unique;
		HashSet hashes;
	public:
		MoveIterator(const Board & b, bool Unique, bool allowswap) : board(b), lineend(0), move(Move(M_SWAP)), unique(Unique) {
			if(board.outcome >= 0){
				move = Move(0, board.size_d); //already done
			}else if(!allowswap || !board.valid_move(move)){ //check if swap is valid
				if(unique){
					hashes.init(board.movesremain());
					hashes.add(board.test_hash(move, board.toplay()));
				}
				++(*this); //find the first valid move
			}
		}

		const Move & operator * ()  const { return move; }
		const Move * operator -> () const { return & move; }
		bool done() const { return (move.y >= board.get_size_d()); }
		bool operator == (const Board::MoveIterator & rhs) const { return (move == rhs.move); }
		bool operator != (const Board::MoveIterator & rhs) const { return (move != rhs.move); }
		MoveIterator & operator ++ (){ //prefix form
			do{
				move.x++;

				if(move.x >= lineend){
					move.y++;
					if(move.y >= board.get_size_d()) //done
						return *this;

					move.x = board.linestart(move.y);
					lineend = board.lineend(move.y);
				}
			}while(!board.valid_move_fast(move) || (unique && hashes.exists(board.test_hash(move, board.toplay()))));

			if(unique)
				hashes.add(board.test_hash(move, board.toplay()));

			return *this;
		}
		MoveIterator operator ++ (int){ //postfix form, discouraged from being used
			MoveIterator newit(*this);
			++(*this);
			return newit;
		}
	};

private:
	char size; //the length of one side of the hexagon
	char sizem1; //size - 1
	char size_d; //diameter of the board = size*2-1

	short num_cells;
	short nummoves;
	short unique_depth; //update and test rotations/symmetry with less than this many pieces on the board
	Move last;
	char toPlay;
	char outcome; //-3 = unknown, 0 = tie, 1,2 = player win
	char wintype; //0 no win, 1 = edge, 2 = corner, 3 = ring
	bool allowswap;

	vector<Cell> cells;
	Zobrist hash;
	const MoveValid * neighbourlist;

public:
	Board(){
		size = 0;
	}

	Board(int s){
		size = s;
		sizem1 = s - 1;
		size_d = s*2-1;
		last = M_NONE;
		nummoves = 0;
		unique_depth = 5;
		toPlay = 1;
		outcome = -3;
		wintype = 0;
		allowswap = false;
		neighbourlist = get_neighbour_list();
		num_cells = vecsize() - size*sizem1;

		cells.resize(vecsize());

		for(int y = 0; y < size_d; y++){
			for(int x = 0; x < size_d; x++){
				int i = xy(x, y);
				cells[i] = Cell(0, i, 1, (1 << iscorner(x, y)), (1 << isedge(x, y)), 0);
			}
		}
	}

	int memsize() const { return sizeof(Board) + sizeof(Cell)*vecsize(); }

	int get_size_d() const { return size_d; }
	int get_size() const{ return size; }

	int vecsize() const { return size_d*size_d; }
	int numcells() const { return num_cells; }

	int num_moves() const { return nummoves; }
	int movesremain() const { return (won() >= 0 ? 0 : num_cells - nummoves + canswap()); }

	int xy(int x, int y)   const { return   y*size_d +   x; }
	int xy(const Move & m) const { return m.y*size_d + m.x; }
	int xy(const MoveValid & m) const { return m.xy; }

	int xyc(int x, int y)   const { return xy(  x + sizem1,   y + sizem1); }
	int xyc(const Move & m) const { return xy(m.x + sizem1, m.y + sizem1); }

	const Cell * cell(int i)          const { return & cells[i]; }
	const Cell * cell(int x, int y)   const { return cell(xy(x,y)); }
	const Cell * cell(const Move & m) const { return cell(xy(m)); }
	const Cell * cell(const MoveValid & m) const { return cell(m.xy); }


	//assumes valid x,y
	int get(int i)          const { return cells[i].piece; }
	int get(int x, int y)   const { return get(xy(x,y)); }
	int get(const Move & m) const { return get(xy(m)); }
	int get(const MoveValid & m) const { return get(m.xy); }

	int geton(const MoveValid & m) const { return (m.onboard() ? get(m.xy) : 0); }

	int local(const Move & m, char turn) const { return local(xy(m), turn); }
	int local(int i,          char turn) const {
		char localshift = (turn & 2); //0 for p1, 2 for p2
		return ((cells[i].local >> localshift) & 3);
	}


	//assumes x, y are in array bounds
	bool onboard_fast(int x, int y)   const { return (  y -   x < size) && (  x -   y < size); }
	bool onboard_fast(const Move & m) const { return (m.y - m.x < size) && (m.x - m.y < size); }
	//checks array bounds too
	bool onboard(int x, int y)  const { return (  x >= 0 &&   y >= 0 &&   x < size_d &&   y < size_d && onboard_fast(x, y) ); }
	bool onboard(const Move & m)const { return (m.x >= 0 && m.y >= 0 && m.x < size_d && m.y < size_d && onboard_fast(m) ); }
	bool onboard(const MoveValid & m) const { return m.onboard(); }

	void setswap(bool s) { allowswap = s; }
	bool canswap() const { return (nummoves == 1 && toPlay == 2 && allowswap); }

	//assumes x, y are in bounds (meaning no swap) and the game isn't already finished
	bool valid_move_fast(int x, int y)   const { return !get(x,y); }
	bool valid_move_fast(const Move & m) const { return !get(m); }
	//checks array bounds too
	bool valid_move(int x, int y)   const { return (outcome == -3 && onboard(x, y) && !get(x,y)); } //ignores swap rule!
	bool valid_move(const Move & m) const { return (outcome == -3 && ((onboard(m) && !get(m)) || (m == M_SWAP && canswap()))); }

	//iterator through neighbours of a position
	const MoveValid * nb_begin(int x, int y)   const { return nb_begin(xy(x, y)); }
	const MoveValid * nb_begin(const Move & m) const { return nb_begin(xy(m)); }
	const MoveValid * nb_begin(int i)          const { return &neighbourlist[i*18]; }

	const MoveValid * nb_end(int x, int y)   const { return nb_end(xy(x, y)); }
	const MoveValid * nb_end(const Move & m) const { return nb_end(xy(m)); }
	const MoveValid * nb_end(int i)          const { return nb_end(nb_begin(i)); }
	const MoveValid * nb_end(const MoveValid * m) const { return m + 6; }
	const MoveValid * nb_endhood(const MoveValid * m) const { return m + 18; }

	int iscorner(int x, int y) const {
		if(!onboard(x,y))
			return -1;

		int m = sizem1, e = size_d-1;

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

		int m = sizem1, e = size_d-1;

		if(y   == 0 && x != 0 && x != m) return 0;
		if(x-y == m && x != m && x != e) return 1;
		if(x   == e && y != m && y != e) return 2;
		if(y   == e && x != e && x != m) return 3;
		if(y-x == m && x != m && x != 0) return 4;
		if(x   == 0 && y != m && y != 0) return 5;

		return -1;
	}

	MoveValid * get_neighbour_list(){
		if(!staticneighbourlist[(int)size]){
			MoveValid * list = new MoveValid[vecsize()*18];
			MoveValid * a = list;
			for(int y = 0; y < size_d; y++){
				for(int x = 0; x < size_d; x++){
					Move pos(x,y);

					for(int i = 0; i < 18; i++){
						Move loc = pos + neighbours[i];
						*a = MoveValid(loc, (onboard(loc) ? xy(loc) : -1) );
						++a;
					}
				}
			}

			staticneighbourlist[(int)size] = list;
		}

		return staticneighbourlist[(int)size];
	}


	int linestart(int y) const { return (y < size ? 0 : y - sizem1); }
	int lineend(int y)   const { return (y < size ? size + y : size_d); }
	int linelen(int y)   const { return size_d - abs(sizem1 - y); }

	string to_s(bool color, bool hguicoords = false) const {
		string white = "O",
		       black = "@",
		       empty = ".",
		       coord = "",
		       reset = "";
		if(color){
			string esc = "\033";
			reset = esc + "[0m";
			coord = esc + "[1;37m";
			empty = reset + ".";
			white = esc + "[1;33m" + "@"; //yellow
			black = esc + "[1;34m" + "@"; //blue
		}

		string s;
		s += string(size + 3, ' ');
		for(int i = 0; i < size; i++)
			s += " " + coord + to_str(i+1);
		s += "\n";

		for(int y = 0; y < size_d; y++){
			s += string(abs(sizem1 - y) + 2, ' ');
			s += coord + char('A' + y);
			int end = lineend(y);
			for(int x = linestart(y); x < end; x++){
				s += (last == Move(x, y)   ? coord + "[" :
				      last == Move(x-1, y) ? coord + "]" : " ");
				int p = get(x, y);
				if(p == 0) s += empty;
				if(p == 1) s += white;
				if(p == 2) s += black;
			}
			s += (last == Move(end-1, y) ? reset + "]" : " ");
			if(y < sizem1)
				s += coord + to_str(size + y + 1);
			else if(!hguicoords && y > sizem1)
				s += coord + to_str(3*size - y - 1);
			s += '\n';
		}
		if(!hguicoords){
			s += string(size + 3, ' ');
			for(int i = 0; i < size; i++)
				s += " " + coord + to_str(i+1);
			s += "\n";
		}

		s += reset;
		return s;
	}

	void print(bool color = true, bool hguicoords = true) const {
		printf("%s", to_s(color, hguicoords).c_str());
	}

	string boardstr() const {
		string white, black;
		for(int y = 0; y < size_d; y++){
			for(int x = linestart(y); x < lineend(y); x++){
				int p = get(x, y);
				if(p == 1) white += Move(x, y).to_s();
				if(p == 2) black += Move(x, y).to_s();
			}
		}
		return white + ";" + black;
	}

	string won_str() const {
		switch(outcome){
			case -3: return "none";
			case -2: return "black_or_draw";
			case -1: return "white_or_draw";
			case 0:  return "draw";
			case 1:  return "white";
			case 2:  return "black";
		}
		return "unknown";
	}

	char won() const {
		return outcome;
	}

	int win() const{ // 0 for draw or unknown, 1 for win, -1 for loss
		if(outcome <= 0)
			return 0;
		return (outcome == toplay() ? 1 : -1);
	}

	char getwintype() const { return wintype; }

	char toplay() const {
		return toPlay;
	}

	MoveIterator moveit(bool unique = false, int swap = -1) const {
		return MoveIterator(*this, (unique ? nummoves <= unique_depth : false), (swap == -1 ? allowswap : swap));
	}

	void set(const Move & m, bool perm = true){
		last = m;
		Cell * cell = & cells[xy(m)];
		cell->piece = toPlay;
		cell->perm = perm;
		nummoves++;
		update_hash(m, toPlay); //depends on nummoves
		toPlay = 3 - toPlay;
	}

	void unset(const Move & m){ //break win checks, but is a poor mans undo if all you care about is the hash
		toPlay = 3 - toPlay;
		update_hash(m, toPlay);
		nummoves--;
		Cell * cell = & cells[xy(m)];
		cell->piece = 0;
		cell->perm = 0;
	}

	void doswap(){
		for(int y = 0; y < size_d; y++){
			for(int x = linestart(y); x < lineend(y); x++){
				if(get(x,y) != 0){
					cells[xy(x,y)].piece = 2;
					toPlay = 1;
					return;
				}
			}
		}
	}

	int find_group(const MoveValid & m) const { return find_group(m.xy); }
	int find_group(const Move & m) const { return find_group(xy(m)); }
	int find_group(int x, int y)   const { return find_group(xy(x, y)); }
	int find_group(unsigned int i) const {
		unsigned int p = cells[i].parent;
		if(p != i){
			do{
				p = cells[p].parent;
			}while(p != cells[p].parent);
			cells[i].parent = p; //do path compression, but only the current one, not all, to avoid recursion
		}
		return p;
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

	Cell test_cell(const Move & pos) const {
		char turn = toplay();
		int posxy = xy(pos);

		Cell testcell = cells[find_group(pos)];
		for(const MoveValid * i = nb_begin(posxy), *e = nb_end(i); i < e; i++){
			if(i->onboard() && turn == get(i->xy)){
				const Cell * g = & cells[find_group(i->xy)];
				testcell.corner |= g->corner;
				testcell.edge   |= g->edge;
				testcell.size   += g->size; //not quite accurate if it's joining the same group twice
				i++; //skip the next one
			}
		}
		return testcell;
	}

	int test_connectivity(const Move & pos) const {
		Cell testcell = test_cell(pos);
		return testcell.numcorners() + testcell.numedges();
	}

	int test_size(const Move & pos) const {
		Cell testcell = test_cell(pos);
		return testcell.size;
	}

	//check if a position is encirclable by a given player
	//false if it or one of its neighbours are the opponent's and connected to an edge or corner
	bool encirclable(const Move pos, int player) const {
		int otherplayer = 3-player;
		int posxy = xy(pos);

		const Cell * g = & cells[find_group(posxy)];
		if(g->piece == otherplayer && (g->edge || g->corner))
			return false;

		for(const MoveValid * i = nb_begin(posxy), *e = nb_end(i); i < e; i++){
			if(!i->onboard())
				return false;

			const Cell * g = & cells[find_group(i->xy)];
			if(g->piece == otherplayer && (g->edge || g->corner))
				return false;
		}
		return true;
	}

	// do a depth first search for a ring
	// can take a minimum length of the ring, any ring shorter than ringsize is ignored
	// ignores tails on small rings correctly (ie an old 6-ring plus a new stone will still be only a 6-ring)
	// two 6-rings next to each other may count as a bigger ring
	// can be done before or after placing the stone and joining neighbouring groups
	// using a ringsize smaller than a previous ring check could lead to weird results
	bool checkring_df(const Move & pos, const int turn, const int ringsize = 6, const int permsneeded = 0) const {
		const Cell * start = & cells[xy(pos)];
		start->ringdepth = 1;
		bool success = false;
		for(int i = 0; i < 4; i++){ //4 instead of 6 since any ring must have its first endpoint in the first 4
			Move loc = pos + neighbours[i];

			if(!onboard(loc))
				continue;

			const Cell * g = & cells[xy(loc)];

			if(turn != g->piece)
				continue;

			g->ringdepth = 2;
			success = followring(loc, i, turn, 3, ringsize, (permsneeded - g->perm));
			g->ringdepth = 0;

			if(success)
				break;
		}
		start->ringdepth = 0;
		return success;
	}
	// only take the 3 directions that are valid in a ring
	// the backwards directions are either invalid or not part of the shortest loop
	bool followring(const Move & cur, const int & dir, const int & turn, const int & depth, const int & ringsize, const int & permsneeded) const {
		for(int i = 5; i <= 7; i++){
			int nd = (dir + i) % 6;
			Move next = cur + neighbours[nd];

			if(!onboard(next))
				continue;

			const Cell * g = & cells[xy(next)];

			if(g->ringdepth)
				return (depth - g->ringdepth >= ringsize && permsneeded <= 0);

			if(turn != g->piece)
				continue;

			g->ringdepth = depth;
			bool success = followring(next, nd, turn, depth+1, ringsize, (permsneeded - g->perm));
			g->ringdepth = 0;

			if(success)
				return true;
		}
		return false;
	}

	// do an O(1) ring check
	// must be done before placing the stone and joining it with the neighbouring groups
	bool checkring_o1(const Move & pos, const int turn) const {
		static const unsigned char ringdata[64][10] = {
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //000000
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //000001
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //000010
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //000011
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //000100
			{1, 3, 5, 0, 0, 0, 0, 0, 0, 0}, //000101
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //000110
			{3,10,16,15, 0, 0, 0, 0, 0, 0}, //000111
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //001000
			{1, 2, 5, 0, 0, 0, 0, 0, 0, 0}, //001001
			{1, 2, 4, 0, 0, 0, 0, 0, 0, 0}, //001010
			{1, 2, 4, 0, 0, 0, 0, 0, 0, 0}, //001011
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //001100
			{1, 2, 5, 0, 0, 0, 0, 0, 0, 0}, //001101
			{3, 9,15,14, 0, 0, 0, 0, 0, 0}, //001110
			{4,10,16,15, 9,14,15, 0, 0, 0}, //001111
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //010000
			{1, 1, 5, 0, 0, 0, 0, 0, 0, 0}, //010001
			{1, 1, 4, 0, 0, 0, 0, 0, 0, 0}, //010010
			{1, 1, 4, 0, 0, 0, 0, 0, 0, 0}, //010011
			{1, 1, 3, 0, 0, 0, 0, 0, 0, 0}, //010100
			{2, 1, 3, 5, 0, 0, 0, 0, 0, 0}, //010101
			{1, 1, 3, 0, 0, 0, 0, 0, 0, 0}, //010110
			{7,10,16,15, 1, 3, 0, 0, 0, 0}, //010111
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //011000
			{1, 1, 5, 0, 0, 0, 0, 0, 0, 0}, //011001
			{1, 1, 4, 0, 0, 0, 0, 0, 0, 0}, //011010
			{1, 1, 4, 0, 0, 0, 0, 0, 0, 0}, //011011
			{3, 8,14,13, 0, 0, 0, 0, 0, 0}, //011100
			{7, 8,14,13, 1, 5, 0, 0, 0, 0}, //011101
			{4, 9,15,14, 8,13,14, 0, 0, 0}, //011110
			{5,10,16,15, 9,14,15, 8,14,13}, //011111
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //100000
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //100001
			{1, 0, 4, 0, 0, 0, 0, 0, 0, 0}, //100010
			{3,11,17,16, 0, 0, 0, 0, 0, 0}, //100011
			{1, 0, 3, 0, 0, 0, 0, 0, 0, 0}, //100100
			{1, 0, 3, 0, 0, 0, 0, 0, 0, 0}, //100101
			{1, 0, 3, 0, 0, 0, 0, 0, 0, 0}, //100110
			{4,11,17,16,10,15,16, 0, 0, 0}, //100111
			{1, 0, 2, 0, 0, 0, 0, 0, 0, 0}, //101000
			{1, 0, 2, 0, 0, 0, 0, 0, 0, 0}, //101001
			{2, 0, 2, 4, 0, 0, 0, 0, 0, 0}, //101010
			{7,11,17,16, 0, 2, 0, 0, 0, 0}, //101011
			{1, 0, 2, 0, 0, 0, 0, 0, 0, 0}, //101100
			{1, 0, 2, 0, 0, 0, 0, 0, 0, 0}, //101101
			{7, 9,15,14, 0, 2, 0, 0, 0, 0}, //101110
			{5,11,17,16,10,15,16, 9,15,14}, //101111
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //110000
			{3, 6,12,17, 0, 0, 0, 0, 0, 0}, //110001
			{1, 0, 4, 0, 0, 0, 0, 0, 0, 0}, //110010
			{4, 6,12,17,11,16,17, 0, 0, 0}, //110011
			{1, 0, 3, 0, 0, 0, 0, 0, 0, 0}, //110100
			{7, 6,12,17, 0, 3, 0, 0, 0, 0}, //110101
			{1, 0, 3, 0, 0, 0, 0, 0, 0, 0}, //110110
			{5, 6,12,17,11,16,17,10,16,15}, //110111
			{3, 7,13,12, 0, 0, 0, 0, 0, 0}, //111000
			{4, 7,13,12, 6,17,12, 0, 0, 0}, //111001
			{7, 7,13,12, 0, 4, 0, 0, 0, 0}, //111010
			{5, 7,13,12, 6,17,12,11,17,16}, //111011
			{4, 8,14,13, 7,12,13, 0, 0, 0}, //111100
			{5, 8,14,13, 7,12,13, 6,12,17}, //111101
			{5, 9,15,14, 8,13,14, 7,13,12}, //111110
			{6, 0, 0, 0, 0, 0, 0, 0, 0, 0}, //111111
		};

		int bitpattern = 0;
		const MoveValid * s = nb_begin(pos);
		for(const MoveValid * i = s, *e = nb_end(i); i < e; i++){
			bitpattern <<= 1;
			if(i->onboard() && turn == get(i->xy))
				bitpattern |= 1;
		}

		const unsigned char * d = ringdata[bitpattern];

		switch(d[0]){
			case 0: //no ring (000000, 000001, 000011)
				return false;

			case 1: //simple case (000101, 001101, 001011, 011011)
				return (find_group(s[d[1]]) == find_group(s[d[2]]));

			case 2:{ //3 non-neighbours (010101)
				int a = find_group(s[d[1]]), b = find_group(s[d[2]]), c = find_group(s[d[3]]);
				return (a == b || a == c || b == c);
			}

			case 7: //case 1 and 3 (010111)
				if(find_group(s[d[4]]) == find_group(s[d[5]]))
					return true;
				//fall through

			case 3: // 3 neighbours (000111)
				return checkring_back(s[d[1]], s[d[2]], s[d[3]], turn);

			case 4: // 4 neighbours (001111)
				return checkring_back(s[d[1]], s[d[2]], s[d[3]], turn) ||
				       checkring_back(s[d[4]], s[d[5]], s[d[6]], turn);

			case 5: // 5 neighbours (011111)
				return checkring_back(s[d[1]], s[d[2]], s[d[3]], turn) ||
				       checkring_back(s[d[4]], s[d[5]], s[d[6]], turn) ||
				       checkring_back(s[d[7]], s[d[8]], s[d[9]], turn);

			case 6: // 6 neighbours (111111)
				return true; //a ring around this position? how'd that happen

			default:
				return false;
		}
	}
	//checks for 3 more stones, a should be the corner
	bool checkring_back(const MoveValid & a, const MoveValid & b, const MoveValid & c, int turn) const {
		return (a.onboard() && get(a) == turn && get(b) == turn && get(c) == turn);
	}

	hash_t gethash() const {
		return (nummoves > unique_depth ? hash.get(0) : hash.get());
	}

	string hashstr() const {
		static const char hexlookup[] = "0123456789abcdef";
		char buf[19] = "0x";
		hash_t val = gethash();
		for(int i = 15; i >= 0; i--){
			buf[i+2] = hexlookup[val & 15];
			val >>= 4;
		}
		buf[18] = '\0';
		return (char *)buf;
	}

	void update_hash(const Move & pos, int turn){
		if(nummoves > unique_depth){ //simple update, no rotations/symmetry
			hash.update(0, 3*xy(pos) + turn);
			return;
		}

		//mirror is simply flip x,y
		int x = pos.x - sizem1,
		    y = pos.y - sizem1,
		    z = y - x;

//x,y; y,z; z,-x; -x,-y; -y,-z; -z,x
//y,x; z,y; -x,z; -y,-x; -z,-y; x,-z

		hash.update(0,  3*xyc( x,  y) + turn);
		hash.update(1,  3*xyc( y,  z) + turn);
		hash.update(2,  3*xyc( z, -x) + turn);
		hash.update(3,  3*xyc(-x, -y) + turn);
		hash.update(4,  3*xyc(-y, -z) + turn);
		hash.update(5,  3*xyc(-z,  x) + turn);
		hash.update(6,  3*xyc( y,  x) + turn);
		hash.update(7,  3*xyc( z,  y) + turn);
		hash.update(8,  3*xyc(-x,  z) + turn);
		hash.update(9,  3*xyc(-y, -x) + turn);
		hash.update(10, 3*xyc(-z, -y) + turn);
		hash.update(11, 3*xyc( x, -z) + turn);
	}

	hash_t test_hash(const Move & pos) const {
		return test_hash(pos, toplay());
	}

	hash_t test_hash(const Move & pos, int turn) const {
		if(nummoves >= unique_depth) //simple test, no rotations/symmetry
			return hash.test(0, 3*xy(pos) + turn);

		int x = pos.x - sizem1,
		    y = pos.y - sizem1,
		    z = y - x;

		hash_t m = hash.test(0,  3*xyc( x,  y) + turn);
		m = min(m, hash.test(1,  3*xyc( y,  z) + turn));
		m = min(m, hash.test(2,  3*xyc( z, -x) + turn));
		m = min(m, hash.test(3,  3*xyc(-x, -y) + turn));
		m = min(m, hash.test(4,  3*xyc(-y, -z) + turn));
		m = min(m, hash.test(5,  3*xyc(-z,  x) + turn));
		m = min(m, hash.test(6,  3*xyc( y,  x) + turn));
		m = min(m, hash.test(7,  3*xyc( z,  y) + turn));
		m = min(m, hash.test(8,  3*xyc(-x,  z) + turn));
		m = min(m, hash.test(9,  3*xyc(-y, -x) + turn));
		m = min(m, hash.test(10, 3*xyc(-z, -y) + turn));
		m = min(m, hash.test(11, 3*xyc( x, -z) + turn));
		return m;
	}

	unsigned int sympattern(const Move & pos) const { return sympattern(xy(pos)); }
	unsigned int sympattern(int posxy)        const { return pattern_symmetry(pattern(posxy)); }

	unsigned int pattern(const Move & pos) const { return pattern(xy(pos)); }
	unsigned int pattern(int posxy)        const {
		unsigned int p = 0;
		for(const MoveValid * i = nb_begin(posxy), *e = nb_end(i); i < e; i++){
			p <<= 2;
			if(i->onboard())
				p |= cells[i->xy].piece;
			else
				p |= 3;
		}
		return p;
	}

	static unsigned int pattern_invert(unsigned int p){ //switch players
		return ((p & 0xAAA) >> 1) | ((p & 0x555) << 1);
	}
	static unsigned int pattern_rotate(unsigned int p){
		return (((p & 3) << 10) | (p >> 2));
	}
	static unsigned int pattern_mirror(unsigned int p){
		//012345 -> 054321, mirrors along the 0,3 axis to move fewer bits
		return (p & ((3 << 10) | (3 << 4))) | ((p & (3 << 8)) >> 8) | ((p & (3 << 6)) >> 4) | ((p & (3 << 2)) << 4) | ((p & (3 << 0)) << 8);
	}
	static unsigned int pattern_symmetry(unsigned int p){ //takes a pattern and returns the representative version
		unsigned int m = p;                 //012345
		m = min(m, (p = pattern_rotate(p)));//501234
		m = min(m, (p = pattern_rotate(p)));//450123
		m = min(m, (p = pattern_rotate(p)));//345012
		m = min(m, (p = pattern_rotate(p)));//234501
		m = min(m, (p = pattern_rotate(p)));//123450
		m = min(m, (p = pattern_mirror(pattern_rotate(p))));//012345 -> 054321
		m = min(m, (p = pattern_rotate(p)));//105432
		m = min(m, (p = pattern_rotate(p)));//210543
		m = min(m, (p = pattern_rotate(p)));//321054
		m = min(m, (p = pattern_rotate(p)));//432105
		m = min(m, (p = pattern_rotate(p)));//543210
		return m;
	}

	bool move(const Move & pos, bool checkwin = true, bool locality = false, int ringsize = 6, int permring = 0){
		assert(outcome < 0);

		if(!valid_move(pos))
			return false;

		if(pos == M_SWAP){
			doswap();
			return true;
		}

		char turn = toplay();
		char localshift = (turn & 2); //0 for p1, 2 for p2

		set(pos, !permring);

		if(locality){
			for(int i = 6; i < 18; i++){
				MoveScore loc = neighbours[i] + pos;

				if(onboard(loc))
					cells[xy(loc)].local |= (loc.score << localshift);
			}
		}

		int posxy = xy(pos);
		bool islocal = (local(pos, turn) == 3);
		bool alreadyjoined = false; //useful for finding rings
		for(const MoveValid * i = nb_begin(posxy), *e = nb_end(i); i < e; i++){
			if(i->onboard()){
				cells[i->xy].local |= (3 << localshift);
				if(islocal && turn == get(i->xy)){
					alreadyjoined |= join_groups(posxy, i->xy);
					i++; //skip the next one. If it is the same group,
						 //it is already connected and forms a corner, which we can ignore
				}
			}
		}

		if(checkwin){
			Cell * g = & cells[find_group(posxy)];
			if(g->numedges() >= 3){
				outcome = turn;
				wintype = 1;
			}else if(g->numcorners() >= 2){
				outcome = turn;
				wintype = 2;
			}else if(ringsize && alreadyjoined && g->size >= max(6, ringsize) && checkring_df(pos, turn, ringsize, permring)){
				outcome = turn;
				wintype = 3;
			}else if(nummoves == num_cells){
				outcome = 0;
			}
		}
		return true;
	}

	bool test_local(const Move & pos, char turn) const {
		return (local(pos, turn) == 3);
	}

	//test if making this move would win, but don't actually make the move
	int test_win(const Move & pos, char turn = 0, bool checkrings = true) const {
		if(turn == 0)
			turn = toplay();

		if(test_local(pos, turn)){
			int posxy = xy(pos);
			Cell testcell = cells[find_group(posxy)];
			int numgroups = 0;
			for(const MoveValid * i = nb_begin(posxy), *e = nb_end(i); i < e; i++){
				if(i->onboard() && turn == get(i->xy)){
					const Cell * g = & cells[find_group(i->xy)];
					testcell.corner |= g->corner;
					testcell.edge   |= g->edge;
					testcell.size   += g->size;
					i++; //skip the next one
					numgroups++;
				}
			}

			if(testcell.numcorners() >= 2 || testcell.numedges() >= 3 || (checkrings && numgroups >= 2 && testcell.size >= 6 && checkring_o1(pos, turn)))
				return turn;
		}

		if(nummoves+1 == num_cells)
			return 0;

		return -3;
	}
};

