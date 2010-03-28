
#ifndef _GAME_H_
#define _GAME_H_

#include "board.h"
#include "gtp.h"
#include "string.h"
#include <cctype>

class HavannahGame {
	vector<Move> hist;
	int size;

public:

	HavannahGame(int s = 8){
		size = s;
	}

	int getsize() const {
		return size;
	}

	Board getboard() const {
		Board board(size);
		for(int i = 0; i < hist.size(); i++)
			board.move(hist[i]);
		return board;
	}
	
	void clear(){
		hist.clear();
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

