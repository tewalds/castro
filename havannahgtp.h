
#ifndef _HAVANNAHGTP_H_
#define _HAVANNAHGTP_H_

#include "gtp.h"
#include "game.h"
#include "string.h"
//#include "solver.h"
#include "pnssolver.h"
#include "absolver.h"
#include "board.h"

class HavannahGTP : public GTPclient {
	HavannahGame game;

public:
	bool verbose;
	bool hguicoords;

	HavannahGTP(FILE * i = stdin, FILE * o = stdout, FILE * l = NULL){
		GTPclient(i, o, l);

		verbose = false;
		hguicoords = false;

		newcallback("name",            bind(&HavannahGTP::gtp_name,       this, _1));
		newcallback("version",         bind(&HavannahGTP::gtp_version,    this, _1));
		newcallback("verbose",         bind(&HavannahGTP::gtp_verbose,    this, _1));
		newcallback("debug",           bind(&HavannahGTP::gtp_debug,      this, _1));
		newcallback("hguicoords",      bind(&HavannahGTP::gtp_hguicoords, this, _1));
		newcallback("gridcoords",      bind(&HavannahGTP::gtp_gridcoords, this, _1));
		newcallback("print",           bind(&HavannahGTP::gtp_print,      this, _1));
		newcallback("showboard",       bind(&HavannahGTP::gtp_print,      this, _1));
		newcallback("clear_board",     bind(&HavannahGTP::gtp_clearboard, this, _1));
		newcallback("boardsize",       bind(&HavannahGTP::gtp_boardsize,  this, _1));
		newcallback("play",            bind(&HavannahGTP::gtp_play,       this, _1));
		newcallback("white",           bind(&HavannahGTP::gtp_playwhite,  this, _1));
		newcallback("black",           bind(&HavannahGTP::gtp_playblack,  this, _1));
		newcallback("undo",            bind(&HavannahGTP::gtp_undo,       this, _1));
		newcallback("havannah_winner", bind(&HavannahGTP::gtp_winner,     this, _1));
		newcallback("havannah_solve",  bind(&HavannahGTP::gtp_solve,      this, _1));
		newcallback("all_legal",       bind(&HavannahGTP::gtp_all_legal,  this, _1));
		newcallback("top_moves",       bind(&HavannahGTP::gtp_top_moves,  this, _1));
		newcallback("genmove",         bind(&HavannahGTP::gtp_genmove,    this, _1));
	}

	GTPResponse gtp_print(vecstr args){
		return GTPResponse(true, "\n" + game.getboard()->to_s());
	}

	string won_str(int outcome) const {
		switch(outcome){
			case -1: return "none";
			case 0:  return "draw";
			case 1:  return "white";
			case 2:  return "black";
		}
		return "unknown";
	}

	GTPResponse gtp_boardsize(vecstr args){
		if(args.size() != 1)
			return GTPResponse(false, "Wrong number of arguments");

		log("boardsize " + args[0]);

		int size = atoi(args[0].c_str());
		if(size < 3 || size > 10)
			return GTPResponse(false, "Size " + to_str(size) + " is out of range.");

		game = HavannahGame(size);
		return GTPResponse(true);
	}

	GTPResponse gtp_clearboard(vecstr args){
		game.clear();
		log("clear_board");
		return GTPResponse(true);
	}

	GTPResponse gtp_undo(vecstr args){
		game.undo();
		log("undo");
		if(verbose)
			return GTPResponse(true, "\n" + game.getboard()->to_s());
		else
			return GTPResponse(true);
	}

	GTPResponse gtp_solve(vecstr args){
		double time = 1000000;
		int mem = 2000;

		if(args.size() >= 1)
			time = atof(args[0].c_str());
		
		if(args.size() >= 2)
			mem = atoi(args[1].c_str());

		log("havannah_solve " + to_str(time) + " " + to_str(mem));

//		PNSSolver solve(*(game.getboard()), time, mem);
		ABSolver solve(*(game.getboard()), time, mem);

		string ret = "";
		ret += (solve.outcome == -1 ? string("unknown") : won_str(solve.outcome)) + " ";
		ret += move_str(solve.x, solve.y) + " ";
		ret += to_str(solve.maxdepth) + " ";
		ret += to_str(solve.nodes);
		return GTPResponse(true, ret);
	}

	void parse_move(const string & str, int & x, int & y){
		x = tolower(str[0]) - 'a';
		y = atoi(str.c_str() + 1) - 1;

		if(!hguicoords && x >= game.getsize())
			y += x + 1 - game.getsize();
	}

	string move_str(int x, int y, int hguic = -1){
		if(hguic == -1)
			hguic = hguicoords;

		if(x == -1)
			return "none";

		if(!hguic && x >= game.getsize())
			y -= x + 1 - game.getsize();

		return string() + char(x + 'a') + to_str(y+1);
	}

	GTPResponse gtp_all_legal(vecstr args){
		Move moves[game.getboard()->vecsize()];
		int num = game.getboard()->get_moves(moves);

		string ret;
		for(int i = 0; i < num; i++)
			ret += move_str(moves[i].x, moves[i].y) + " ";
		return GTPResponse(true, ret);
	}

	GTPResponse gtp_top_moves(vecstr args){
		Move moves[game.getboard()->vecsize()];
		int num = game.getboard()->get_moves(moves, true);

		string ret;
		for(int i = 0; i < num; i++)
			ret += move_str(moves[i].x, moves[i].y) + " " + to_str(moves[i].score) + " ";
		return GTPResponse(true, ret);
	}

	GTPResponse gtp_genmove(vecstr args){
		Move moves[game.getboard()->vecsize()];
		int num = game.getboard()->get_moves(moves, true);
		if(num){
			game.move(moves[0].x, moves[0].y);
			return GTPResponse(true, move_str(moves[0].x, moves[0].y));
		}else
			return GTPResponse(false);
	}


	GTPResponse play(const string & pos, int toplay){
		int x, y;
		parse_move(pos, x, y);
		if(!game.getboard()->onboard2(x, y))
			return GTPResponse(false, "Move out of bounds");

		if(!game.move(x, y, toplay))
			return GTPResponse(false, "Invalid placement, already taken");

		log(string("play ") + (toplay == 1 ? 'w' : 'b') + ' ' + move_str(x, y, false));

		if(verbose)
			return GTPResponse(true, "Placement: " + move_str(x, y) + ", outcome: " + game.getboard()->won_str() + "\n" + game.getboard()->to_s());
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
		log("havannah_winner");
		return GTPResponse(true, game.getboard()->won_str());
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

	GTPResponse gtp_hguicoords(vecstr args){
		hguicoords = true;
		return GTPResponse(true);
	}
	GTPResponse gtp_gridcoords(vecstr args){
		hguicoords = false;
		return GTPResponse(true);
	}

	GTPResponse gtp_debug(vecstr args){
		string str = "\n";
		str += "Board size:  " + to_str(game.getboard()->get_size()) + "\n";
		str += "Board cells: " + to_str(game.getboard()->numcells()) + "\n";
		str += "Board vec:   " + to_str(game.getboard()->vecsize()) + "\n";
		str += "Board mem:   " + to_str(game.getboard()->memsize()) + "\n";

		return GTPResponse(true, str);
	}
};

#endif

