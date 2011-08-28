
//build with: g++ builddb.cpp -o builddb -lkyotocabinet -lz -lstdc++ -lrt -lpthread -lm -lc string.o

#include <string>
#include <iostream>

using namespace std;


#include "proofdb.h"
#include "board.h"
#include "solverab.h"
#include "solverpns.h"

ProofDB<MCTSRec> mctsdb;
ProofDB<PNSRec>  pnsdb;
SolverAB solverab;
SolverPNS solverpns;
int maxdepth = 40;
int verbose = 0;

//mctsoutcome: -3 = unknown, -4 = not recorded, -5 = all; returns: -3 = unknown, -4 = timeout
int16_t recurse(const Board & board, const Move & prevmove, int mctsoutcome, int depth, double timelimit){
	PNSRec pns(board.boardstr());
	if(pnsdb.get(pns) && pns.outcome >= 0) //already proven
		return pns.outcome;

	MCTSRec mcts(board.gethash());

	if(verbose)
		cerr << string(depth, ' ') << "recurse(" << board.boardstr() << ";" << mcts.key() << "," << prevmove.to_s() << "," << mctsoutcome << "," << depth << ")";

	if(mctsdb.get(mcts)){
		if(verbose)
			cerr << " -> MCTSRec<" << mcts.key() << " : " << mcts.value() << ">\n";

		if(depth == maxdepth || (mctsoutcome != -5 && mcts.outcome != mctsoutcome))
			return -3;

		if(mcts.outcome == -3)
			cerr << "Found mcts.outcome == -3: " << pns.key() << "\n";
	}else{
		if(mctsoutcome > -4){
			if(verbose) cerr << "\n";
			return -3;
		}

		solverab.set_board(board);
		solverpns.set_board(board);

		string solvedby;
		uint64_t nodes;
		double time_used;
		do {
			solverab.solve(timelimit);

			if(solverab.outcome >= 0){
				pns.outcome = solverab.outcome;
				nodes = solverab.nodes_seen;
				time_used = solverab.time_used;
				solvedby = "ab";
				break;
			}

			solverpns.solve(timelimit);

			if(solverpns.outcome >= 0){
				pns.outcome = solverpns.outcome;
				nodes = solverpns.nodes_seen;
				time_used = solverpns.time_used;
				solvedby = "pns";
				break;
			}

			if(verbose)
				cerr << " -> MCTSRec not found, solve failed in " << timelimit << "\n";

			return -4;
		}while(0);

		pnsdb.set(pns);

		if(verbose)
			cerr << " -> MCTSRec not found, solved in " << timelimit << "\n";

		cout << "\n" << string(depth, ' ') << "(;" << (board.toplay() == 2 ? "W" : "B") << "[" << prevmove.to_s() << "]";
		cout << "C[" << solvedby << ": " << pns.outcome << ", nodes: " << nodes << ", time: " << to_str(time_used, 3) << "])";
		cout.flush();

		return pns.outcome;
	}

	cout << "\n" << string(depth, ' ') << "(;" << (board.toplay() == 2 ? "W" : "B") << "[" << prevmove.to_s() << "]C[mcts: " << mcts.outcome << ", sims: " << mcts.work << "]";
	cout.flush();

	assert(pns.outcome <= -3);

	double limit = 0.002;
	for(int i = 0; i <= 5 && pns.outcome <= -3; ){
		int nextmcts = -5;
		bool foundtimeout = false;
		switch(i){
			case 0: nextmcts = board.toplay();     break; // look for known wins first
			case 1: nextmcts = -4;                 break; // solve the easy cases, may be a win in there
			case 2: nextmcts = -3;                 break; // then unknowns, which are likely collisions that may be a win
			case 3: nextmcts = -4;                 break; // if unknows aren't wins, then try to prove the rest of the unrecorded cases, they need solving anyway
			case 4: nextmcts = 0;                  break; // get the draws out of the way
			case 5: nextmcts = 3 - board.toplay(); break; // finally check the losses
		}

		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			Board next = board;
			next.move(*move);

			int16_t ret = recurse(next, *move, nextmcts, depth+1, limit);
			if(ret == board.toplay()){
				pns.outcome = ret;
				break;
			}else if(ret == 0){
				pns.outcome = 0;
			}else if(ret == -4){
				foundtimeout = true;
			} //unknown just falls through, and loss is implicit in pns.outcome initialization
		}

		if(foundtimeout && ((i == 1 && limit < 1) || i == 3)){
			limit *= 5;
		}else{
			i++;
		}
	}

	cout << "\n" << string(depth, ' ') << ")";
	cout.flush();

	if(pns.outcome < 0) //no draws or wins, must be a loss
		pns.outcome = 3 - board.toplay();
	pnsdb.set(pns);
	return pns.outcome;
}

int main(int argc, char** argv) {
	if(argc < 3){
		cout << "Usage: " << argv[0] << " <mctsdb> <pnsdb>\n";
		return 0;
	}

	// create the database object
	mctsdb.open(argv[1]);
	pnsdb.open(argv[2]);

	solverab.set_memlimit(0);//10 << 20);
	solverpns.set_memlimit(2000 << 20);

	cout << "(;FF[4]SZ[4]";

	//setup the rootboard
	Board rootboard(4);
	for(int i = 3; i < argc; i++) {
		string arg = argv[i];

		if(arg == "-v"){
			verbose = true;
		}else{
			Move m(arg);
			cout << ";" << (rootboard.toplay() == 1 ? "W" : "B") << "[" << m.to_s() << "]";
			rootboard.move(m);
		}
	}

	for(Board::MoveIterator move = rootboard.moveit(true); !move.done(); ++move){
		Board next = rootboard;
		next.move(*move);
		recurse(next, *move, -5, 1, 1);
	}

	cout << ")";

	pnsdb.close();
	mctsdb.close();

	return 0;
}

