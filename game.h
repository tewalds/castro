
#ifndef _GAME_H_
#define _GAME_H_

#include "board.h"
#include "move.h"
#include "string.h"
#include <cctype>

class HavannahGame {
	vector<Move> hist;
	int size;

public:

	HavannahGame(int s = 8){
		size = s;
		hist.push_back(Move(M_NONE));
	}

	int getsize() const {
		return size;
	}

	const vector<Move> & get_hist() const {
		return hist;
	}

	Board getboard() const {
		Board board(size);
		for(int i = 0; i < hist.size(); i++)
			board.move(hist[i]);
		return board;
	}
	
	int len() const {
		return hist.size();
	}

	void clear(){
		hist.clear();
		hist.push_back(Move(M_NONE));
	}
	
	bool undo(){
		if(hist.size() <= 1)
			return false;

		hist.pop_back();
		return true;
	}

	int movesremain() const {
		return getboard().movesremain();
	}

	int toplay() const {
		return getboard().toplay();
	}

	bool move(const Move & m){
		if(getboard().move(m)){
			hist.push_back(m);
			return true;
		}
		return false;
	}
};

#endif

