
//build with: g++ builddb.cpp -o builddb -lkyotocabinet -lz -lstdc++ -lrt -lpthread -lm -lc string.o

#include <string>
#include <iostream>

using namespace std;

#include "proofdb.h"
#include "board.h"

ProofDB<PNSRec>  pnsdb;
int maxdepth = 1000;

//mctsoutcome: -3 = unknown, -4 = not recorded, -5 = all; returns: -3 = unknown, -4 = timeout
int recurse(const Board & board, const Move & prevmove, int outcome, int depth){
	PNSRec pns(board.boardstr());
	if(!pnsdb.get(pns) || (outcome != -5 && pns.outcome != outcome))
		return -3;

	cout << "\n" << string(depth, ' ') << "(;" << (board.toplay() == 2 ? "W" : "B") << "[" << prevmove.to_s() << "]" + pns.hgfcomment();
	cout.flush();

	bool leaf = true;
	for(int i = 0; i <= 2 && depth <= maxdepth; i++){
		int nextmcts = -5;
		switch(i){
			case 0: nextmcts = board.toplay();     break; // look for known wins first
			case 1: nextmcts = 0;                  break; // get the draws out of the way
			case 2: nextmcts = 3 - board.toplay(); break; // finally check the losses
		}

		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			Board next = board;
			next.move(*move);

			int ret = recurse(next, *move, nextmcts, depth+1);

			if(ret >= 0)
				leaf = false;

			if(ret == board.toplay()){
				i = 100; //break outer loop
				break;
			}
		}
	}

	if(!leaf)
		cout << "\n";

	cout << string(depth, ' ') << ")";
	cout.flush();

	return pns.outcome;
}

int main(int argc, char** argv) {
	if(argc < 3){
		cout << "Usage: " << argv[0] << " <pnsdb> [-d depth] [<moves ...>]\n";
		return 0;
	}

	// create the database object
	pnsdb.open(argv[1], false);

	cout << "(;FF[4]SZ[4]\n";

	//setup the rootboard
	Board rootboard(4);
	for(int i = 2; i < argc; i++) {
		string arg = argv[i];

		if(arg == "-d"){
			maxdepth = from_str<int>(argv[++i]);
			continue;
		}

		Move m(arg);
		rootboard.move(m);

		cout << ";" << (rootboard.toplay() == 2 ? "W" : "B") << "[" << m.to_s() << "]";
		PNSRec pns(rootboard.boardstr());
		if(pnsdb.get(pns))
			cout << pns.hgfcomment();
		cout << "\n";
	}

	cout.flush();

	for(Board::MoveIterator move = rootboard.moveit(true); !move.done(); ++move){
		Board next = rootboard;
		next.move(*move);
		recurse(next, *move, -5, 1);
	}

	cout << "\n)\n";

	pnsdb.close();

	return 0;
}

