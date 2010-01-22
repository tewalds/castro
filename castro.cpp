
#include "board.h"
#include "gtp.h"
#include "string.h"
#include <cctype>

Board * board;
bool verbose = false;

GTPResponse gtp_print(vecstr args){
	return GTPResponse(true, "\n" + board->to_s());
}

GTPResponse gtp_boardsize(vecstr args){
	if(args.size() != 1)
		return GTPResponse(false, "Wrong number of arguments");

	int size = atoi(args[0].c_str());
	if(size < 3 || size > 10)
		return GTPResponse(false, "Size " + to_str(size) + " is out of range.");

	delete board;
	board = new Board(size);
	return GTPResponse(true);
}

GTPResponse gtp_clearboard(vecstr args){
	int size = board->getsize();
	delete board;
	board = new Board(size);
	return GTPResponse(true);
}

void parse_move(const string & str, int & x, int & y){
	x = tolower(str[0]) - 'a';
	y = atoi(str.c_str() + 1) - 1;
}
GTPResponse play(const string & pos, int toplay){
	int x, y;
	parse_move(pos, x, y);
	if(!board->onboard2(x, y))
		return GTPResponse(false, "Move out of bounds");

	if(!board->move(x, y, toplay))
		return GTPResponse(false, "Invalid placement, already taken");

	if(verbose)
		return GTPResponse(true, "Placement: " + to_str(x) + "," + to_str(y) + ", outcome: " + board->won_str() + "\n" + board->to_s());
	else
		return GTPResponse(true);
}

GTPResponse gtp_play(vecstr args){
	if(args.size() != 2)
		return GTPResponse(false, "Wrong number of arguments");

	char toplay = 0;
	switch(tolower(args[0][0])){
		case 'w': toplay = 1; break;
		case 'b': toplay = 2; break;
		default:
			return GTPResponse(false, "Invalid player selection");
	}

	return play(args[1], toplay);
}

GTPResponse gtp_playwhite(vecstr args){
	if(args.size() != 1)
		return GTPResponse(false, "Wrong number of arguments");

	return play(args[0], 1);
}

GTPResponse gtp_playblack(vecstr args){
	if(args.size() != 1)
		return GTPResponse(false, "Wrong number of arguments");

	return play(args[0], 2);
}

GTPResponse gtp_winner(vecstr args){
	return GTPResponse(true, board->won_str());
}

GTPResponse gtp_name(vecstr args){
	return GTPResponse(true, "Castro");
}

GTPResponse gtp_version(vecstr args){
	return GTPResponse(true, "0.1");
}

GTPResponse gtp_verbose(vecstr args){
	verbose = true;
	return GTPResponse(true, "Verbose on");
}

int main(){

	board = new Board(8);

	GTPclient gtp;
	gtp.newcallback("name",        gtp_name);
	gtp.newcallback("version",     gtp_version);
	gtp.newcallback("verbose",     gtp_verbose);
	gtp.newcallback("print",       gtp_print);
	gtp.newcallback("clear_board", gtp_clearboard);
	gtp.newcallback("boardsize",   gtp_boardsize);
	gtp.newcallback("play",        gtp_play);
	gtp.newcallback("white",       gtp_playwhite);
	gtp.newcallback("black",       gtp_playblack);
	gtp.newcallback("havannah_winner", gtp_winner);
	
	gtp.run();

	return 1;
}
