
#ifndef _GAME_H_
#define _GAME_H_

#include "board.h"
#include "gtp.h"
#include "string.h"
#include <cctype>

class HavannahGame {
	vector<Board> hist;
	int size;

public:

	HavannahGame(int s = 8){
		size = s;
		hist.push_back(Board(size));
	}

	int getsize(){
		return size;
	}

	Board * getboard(){
		return & hist[hist.size()-1];
	}
	
	void clear(){
		while(undo()) ;
	}
	
	bool undo(){
		if(hist.size() <= 1)
			return false;

		hist.pop_back();
		return true;
	}

	int movesremain() const {
		return hist[hist.size()-1].movesremain();
	}

	bool move(Move & m, int toplay = -1){
		Board b = *getboard();
		if(b.move(m, toplay)){
			hist.push_back(b);
			return true;
		}
		return false;
	}
};

#endif

