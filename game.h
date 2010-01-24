
#ifndef _GAME_H_
#define _GAME_H_

#include "board.h"
#include "gtp.h"
#include "string.h"
#include <cctype>

class HavannahGame {
	vector<Board> hist;

public:

	HavannahGame(int size = 8){
		hist.push_back(Board(size));
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

	bool move(int x, int y, int toplay){
		Board b = *getboard();
		if(b.move(x, y, toplay)){
			hist.push_back(b);
			return true;
		}
		return false;
	}
};

#endif

