
#ifndef _BOARD_H_
#define _BOARD_H_

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
		uint8_t local;  //0 for far, 1 for distance 2, 2 for virtual connection, 3 for neighbour
mutable uint8_t ringdepth; //when doing a ring search, what depth was this position found
//*/

		Cell() : piece(0), size(0), parent(0), corner(0), edge(0), local(0), ringdepth(0) { }
		Cell(unsigned int p, unsigned int a, unsigned int s, unsigned int c, unsigned int e, unsigned int l) :
			piece(p), size(s), parent(a), corner(c), edge(e), local(l), ringdepth(0) { }

		int numcorners(){ return BitsSetTable64[corner]; }
		int numedges()  { return BitsSetTable64[edge];   }
	};

public:
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

	short nummoves;
	short unique_depth; //update and test rotations/symmetry with less than this many pieces on the board
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
		nummoves = 0;
		unique_depth = 5;
		toPlay = 1;
		outcome = -3;
		wintype = 0;
		allowswap = false;
		neighbourlist = get_neighbour_list();

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
	int numcells() const { return vecsize() - size*sizem1; }

	int num_moves() const { return nummoves; }
	int movesremain() const { return (won() >= 0 ? 0 : numcells() - nummoves + canswap()); }

	int xy(int x, int y)   const { return   y*size_d +   x; }
	int xy(const Move & m) const { return m.y*size_d + m.x; }
	int xy(const MoveValid & m) const { return m.xy; }

	int xyc(int x, int y)   const { return xy(  x + sizem1,   y + sizem1); }
	int xyc(const Move & m) const { return xy(m.x + sizem1, m.y + sizem1); }

	//assumes valid x,y
	int get(int i)          const { return cells[i].piece; }
	int get(int x, int y)   const { return get(xy(x,y)); }
	int get(const Move & m) const { return get(xy(m)); }
	int get(const MoveValid & m) const { return get(m.xy); }
	int get(const MoveValid * m) const { return get(m->xy); }

	int local(const Move & m) const { return cells[xy(m)].local; }

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
	const MoveValid * nb_begin(int i)          const { return &neighbourlist[i*6]; }

	const MoveValid * nb_end(int x, int y)   const { return nb_end(xy(x, y)); }
	const MoveValid * nb_end(const Move & m) const { return nb_end(xy(m)); }
	const MoveValid * nb_end(int i)          const { return nb_end(nb_begin(i)); }
	const MoveValid * nb_end(const MoveValid * m) const { return m + 6; }

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
			MoveValid * list = new MoveValid[vecsize()*6];
			MoveValid * a = list;
			for(int y = 0; y < size_d; y++){
				for(int x = 0; x < size_d; x++){
					Move pos(x,y);

					for(int i = 0; i < 6; i++){
						Move loc = pos + neighbours[i];
						if(onboard(loc))
							*a = MoveValid(loc, xy(loc));
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

	string to_s() const {
		string s;
		s += string(size + 4, ' ');
		for(int i = 0; i < size; i++){
			s += to_str(i+1);
			s += " ";
		}
		s += "\n";

		for(int y = 0; y < size_d; y++){
			s += string(abs(sizem1 - y) + 2, ' ');
			s += char('A' + y);
			s += " ";
			for(int x = linestart(y); x < lineend(y); x++){
				int p = get(x, y);
				if(p == 0) s += '.';
				if(p == 1) s += 'O';
				if(p == 2) s += '@';
				s += ' ';
			}
			if(y < size-1)
				s += to_str(1 + size + y);
			s += '\n';
		}
		return s;
	}

	void print() const {
		printf("%s", to_s().c_str());
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

	void set(const Move & m){
		cells[xy(m)].piece = toPlay;
		nummoves++;
		update_hash(m, toPlay); //depends on nummoves
		toPlay = 3 - toPlay;
	}

	void unset(const Move & m){ //break win checks, but is a poor mans undo if all you care about is the hash
		toPlay = 3 - toPlay;
		update_hash(m, toPlay);
		nummoves--;
		cells[xy(m)].piece = 0;
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

	int find_group(const MoveValid * m) const { return find_group(m->xy); }
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

	int test_connectivity(const Move & pos) const {
		char turn = toplay();
		int posxy = xy(pos);

		Cell testcell = cells[find_group(pos)];
		for(const MoveValid * i = nb_begin(posxy), *e = nb_end(i); i < e; i++){
			if(i->onboard() && turn == get(i->xy)){
				const Cell * g = & cells[find_group(i->xy)];
				testcell.corner |= g->corner;
				testcell.edge   |= g->edge;
				i++; //skip the next one
			}
		}
		return testcell.numcorners() + testcell.numedges();
	}

	int test_size(const Move & pos) const {
		char turn = toplay();
		int posxy = xy(pos);

		int size = 1;
		for(const MoveValid * i = nb_begin(posxy), *e = nb_end(i); i < e; i++){
			if(i->onboard() && turn == get(i->xy)){
				size += cells[find_group(i->xy)].size;
				i++; //skip the next one
			}
		}
		return size;
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
	bool checkring_df(const Move & pos, const int turn, const int ringsize = 6) const {
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
			success = followring(loc, i, turn, 3, ringsize);
			g->ringdepth = 0;

			if(success)
				break;
		}
		start->ringdepth = 0;
		return success;
	}
	// only take the 3 directions that are valid in a ring
	// the backwards directions are either invalid or not part of the shortest loop
	bool followring(const Move & cur, const int & dir, const int & turn, const int & depth, const int & ringsize) const {
		for(int i = 5; i <= 7; i++){
			int nd = (dir + i) % 6;
			Move next = cur + neighbours[nd];

			if(!onboard(next))
				continue;

			const Cell * g = & cells[xy(next)];

			if(g->ringdepth)
				return (depth - g->ringdepth >= ringsize);

			if(turn != g->piece)
				continue;

			g->ringdepth = depth;
			bool success = followring(next, nd, turn, depth+1, ringsize);
			g->ringdepth = 0;

			if(success)
				return true;
		}
		return false;
	}

	// do an O(1) ring check
	// must be done before placing the stone and joining it with the neighbouring groups
	bool checkring_o1(const Move & pos, const int turn) const {
		int bitpattern = 0;
		const MoveValid * s = nb_begin(pos);
		for(const MoveValid * i = s, *e = nb_end(i); i < e; i++){
			bitpattern <<= 1;
			if(i->onboard() && turn == get(i->xy))
				bitpattern |= 1;
		}

		const MoveScore * n = neighbours;
		switch(bitpattern){
			case 0b000101: case 0b001101: case 0b011101: case 0b100101: return (find_group(s+3) == find_group(s+5));
			case 0b001010: case 0b011010: case 0b111010: case 0b001011: return (find_group(s+2) == find_group(s+4));
			case 0b010100: case 0b110100: case 0b110101: case 0b010110: return (find_group(s+1) == find_group(s+3));
			case 0b101000: case 0b101001: case 0b101011: case 0b101100: return (find_group(s+0) == find_group(s+2));
			case 0b010001: case 0b010011: case 0b010111: case 0b011001: return (find_group(s+1) == find_group(s+5));
			case 0b100010: case 0b100110: case 0b101110: case 0b110010: return (find_group(s+0) == find_group(s+4));

			case 0b001001: case 0b011011: return (find_group(s+2) == find_group(s+5));
			case 0b010010: case 0b110110: return (find_group(s+1) == find_group(s+4));
			case 0b100100: case 0b101101: return (find_group(s+0) == find_group(s+3));

			//check if two of the tree have the same group
			case 0b010101: return (find_group(s+1) == find_group(s+3) || find_group(s+3) == find_group(s+5) || find_group(s+1) == find_group(s+5));
			case 0b101010: return (find_group(s+0) == find_group(s+2) || find_group(s+2) == find_group(s+4) || find_group(s+0) == find_group(s+4));

			//might have a ring?
			case 0b000111: return (get(pos + n[16]) == turn && get(pos + n[10]) == turn && get(pos + n[15]) == turn);
			case 0b001110: return (get(pos + n[15]) == turn && get(pos + n[ 9]) == turn && get(pos + n[14]) == turn);
			case 0b011100: return (get(pos + n[14]) == turn && get(pos + n[ 8]) == turn && get(pos + n[13]) == turn);
			case 0b111000: return (get(pos + n[13]) == turn && get(pos + n[ 7]) == turn && get(pos + n[12]) == turn);
			case 0b110001: return (get(pos + n[12]) == turn && get(pos + n[ 6]) == turn && get(pos + n[17]) == turn);
			case 0b100011: return (get(pos + n[17]) == turn && get(pos + n[11]) == turn && get(pos + n[16]) == turn);

			case 0b001111: return (get(pos + n[15]) == turn && ((get(pos + n[16]) == turn && get(pos + n[10]) == turn) || (get(pos + n[ 9]) == turn && get(pos + n[14]) == turn)));
			case 0b011110: return (get(pos + n[14]) == turn && ((get(pos + n[15]) == turn && get(pos + n[ 9]) == turn) || (get(pos + n[ 8]) == turn && get(pos + n[13]) == turn)));
			case 0b111100: return (get(pos + n[13]) == turn && ((get(pos + n[14]) == turn && get(pos + n[ 8]) == turn) || (get(pos + n[ 7]) == turn && get(pos + n[12]) == turn)));
			case 0b111001: return (get(pos + n[12]) == turn && ((get(pos + n[13]) == turn && get(pos + n[ 7]) == turn) || (get(pos + n[ 6]) == turn && get(pos + n[17]) == turn)));
			case 0b110011: return (get(pos + n[17]) == turn && ((get(pos + n[12]) == turn && get(pos + n[ 6]) == turn) || (get(pos + n[11]) == turn && get(pos + n[16]) == turn)));
			case 0b100111: return (get(pos + n[16]) == turn && ((get(pos + n[17]) == turn && get(pos + n[11]) == turn) || (get(pos + n[10]) == turn && get(pos + n[15]) == turn)));

			case 0b011111: return ((get(pos + n[15]) == turn && ((get(pos + n[16]) == turn && get(pos + n[10]) == turn) || (get(pos + n[ 9]) == turn && get(pos + n[14]) == turn))) || (get(pos + n[14]) == turn && get(pos + n[ 8]) == turn && get(pos + n[13]) == turn));
			case 0b111110: return ((get(pos + n[14]) == turn && ((get(pos + n[15]) == turn && get(pos + n[ 9]) == turn) || (get(pos + n[ 8]) == turn && get(pos + n[13]) == turn))) || (get(pos + n[13]) == turn && get(pos + n[ 7]) == turn && get(pos + n[12]) == turn));
			case 0b111101: return ((get(pos + n[13]) == turn && ((get(pos + n[14]) == turn && get(pos + n[ 8]) == turn) || (get(pos + n[ 7]) == turn && get(pos + n[12]) == turn))) || (get(pos + n[12]) == turn && get(pos + n[ 6]) == turn && get(pos + n[17]) == turn));
			case 0b111011: return ((get(pos + n[12]) == turn && ((get(pos + n[13]) == turn && get(pos + n[ 7]) == turn) || (get(pos + n[ 6]) == turn && get(pos + n[17]) == turn))) || (get(pos + n[17]) == turn && get(pos + n[11]) == turn && get(pos + n[16]) == turn));
			case 0b110111: return ((get(pos + n[17]) == turn && ((get(pos + n[12]) == turn && get(pos + n[ 6]) == turn) || (get(pos + n[11]) == turn && get(pos + n[16]) == turn))) || (get(pos + n[16]) == turn && get(pos + n[10]) == turn && get(pos + n[15]) == turn));
			case 0b101111: return ((get(pos + n[16]) == turn && ((get(pos + n[17]) == turn && get(pos + n[11]) == turn) || (get(pos + n[10]) == turn && get(pos + n[15]) == turn))) || (get(pos + n[15]) == turn && get(pos + n[ 9]) == turn && get(pos + n[14]) == turn));

			case 0b111111: //a ring around this position? how'd that happen
				return true;

			case 0b000000:
			case 0b000001: case 0b000010: case 0b000100: case 0b001000: case 0b010000: case 0b100000:
			case 0b000011: case 0b000110: case 0b001100: case 0b011000: case 0b110000: case 0b100001:
			default:
				return false;

		}
	}

	hash_t gethash() const {
		return (nummoves > unique_depth ? hash.get(0) : hash.get());
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

	bool move(const Move & pos, bool checkwin = true, bool locality = false, int ringsize = 6){
		assert(outcome < 0);

		if(!valid_move(pos))
			return false;

		if(pos == M_SWAP){
			doswap();
			return true;
		}

		char turn = toplay();

		set(pos);

		if(locality){
			for(int i = 6; i < 18; i++){
				MoveScore loc = neighbours[i] + pos;

				if(onboard(loc))
					cells[xy(loc)].local |= loc.score;
			}
		}

		int posxy = xy(pos);
		bool local = (cells[posxy].local == 3);
		bool alreadyjoined = false; //useful for finding rings
		for(const MoveValid * i = nb_begin(posxy), *e = nb_end(i); i < e; i++){
			if(i->onboard()){
				cells[i->xy].local = 3;
				if(local && turn == get(i->xy)){
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
			}else if(ringsize && alreadyjoined && g->size >= max(6, ringsize) && checkring_df(pos, turn, ringsize)){
				outcome = turn;
				wintype = 3;
			}else if(nummoves == numcells()){
				outcome = 0;
			}
		}
		return true;
	}

	bool test_local(const Move & pos) const {
		return (cells[xy(pos)].local == 3);
	}

	//test if making this move would win, but don't actually make the move
	int test_win(const Move & pos, char turn = 0) const {
		int posxy = xy(pos);
		if(cells[posxy].local != 3)
			return -3;

		if(turn == 0)
			turn = toplay();

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

		if(testcell.numcorners() >= 2 || testcell.numedges() >= 3 || (numgroups >= 2 && testcell.size >= 6 && checkring_o1(pos, turn)))
			return turn;

		if(nummoves+1 == numcells())
			return 0;

		return -3;
	}
};

#endif

