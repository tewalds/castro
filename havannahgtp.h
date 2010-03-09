
#ifndef _HAVANNAHGTP_H_
#define _HAVANNAHGTP_H_

#include "gtp.h"
#include "game.h"
#include "string.h"
#include "solver.h"
#include "player.h"
#include "board.h"
#include "move.h"

class HavannahGTP : public GTPclient {
	HavannahGame game;

public:
	bool verbose;
	bool hguicoords;
	double time_remain;
	double time_per_move;
	int mem_allowed;

	Player player;

	HavannahGTP(FILE * i = stdin, FILE * o = stdout, FILE * l = NULL){
		GTPclient(i, o, l);

		verbose = false;
		hguicoords = false;
		time_remain = 120;
		time_per_move = 0;
		mem_allowed = 1000;

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
		newcallback("solve_ab",        bind(&HavannahGTP::gtp_solve_ab,   this, _1));
		newcallback("solve_scout",     bind(&HavannahGTP::gtp_solve_scout,this, _1));
		newcallback("solve_pns",       bind(&HavannahGTP::gtp_solve_pns,  this, _1));
		newcallback("solve_pnsab",     bind(&HavannahGTP::gtp_solve_pnsab,this, _1));
		newcallback("solve_dfpnsab",   bind(&HavannahGTP::gtp_solve_dfpnsab,this, _1));
		newcallback("all_legal",       bind(&HavannahGTP::gtp_all_legal,  this, _1));
		newcallback("top_moves",       bind(&HavannahGTP::gtp_top_moves,  this, _1));
		newcallback("time_settings",   bind(&HavannahGTP::gtp_time_settings, this, _1));
		newcallback("genmove",         bind(&HavannahGTP::gtp_genmove,    this, _1));
		newcallback("player_params",   bind(&HavannahGTP::gtp_player_params, this, _1));
	}

	GTPResponse gtp_print(vecstr args){
		return GTPResponse(true, "\n" + game.getboard()->to_s());
	}

	string won_str(int outcome) const {
		switch(outcome){
			case -1: return "none";
			case  0: return "draw";
			case  1: return "white";
			case  2: return "black";
			default: return "unknown";
		}
	}

	string solve_str(int outcome) const {
		switch(outcome){
			case -2: return "black_or_draw";
			case -1: return "white_or_draw";
			case  0: return "draw";
			case  1: return "white";
			case  2: return "black";
			default: return "unknown";
		}
	}

	GTPResponse gtp_time_settings(vecstr args){
		if(args.size() == 0)
			return GTPResponse(false, "Wrong number of arguments");

		log("time_settings " + implode(args, " "));

		time_remain = from_str<double>(args[0]);

		if(args.size() >= 2)
			time_per_move = from_str<double>(args[1]);

		return GTPResponse(true);
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
		log("havannah_solve " + implode(args, " "));

		return gtp_solve_pnsab(args);
	}

	GTPResponse gtp_solve_ab(vecstr args){
		double time = 60;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		Solver solve;
		solve.solve_ab(*(game.getboard()), time);

		return GTPResponse(true, solve_str(solve));
	}

	GTPResponse gtp_solve_scout(vecstr args){
		double time = 60;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		Solver solve;
		solve.solve_scout(*(game.getboard()), time);

		return GTPResponse(true, solve_str(solve));
	}

	GTPResponse gtp_solve_pns(vecstr args){
		double time = 60;
		int mem = mem_allowed;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		if(args.size() >= 2)
			mem = from_str<int>(args[1]);

		Solver solve;
		solve.solve_pns(*(game.getboard()), time, mem);

		return GTPResponse(true, solve_str(solve));
	}

	GTPResponse gtp_solve_pnsab(vecstr args){
		double time = 60;
		int mem = mem_allowed;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		if(args.size() >= 2)
			mem = from_str<int>(args[1]);

		Solver solve;
		solve.solve_pnsab(*(game.getboard()), time, mem);

		return GTPResponse(true, solve_str(solve));
	}

	GTPResponse gtp_solve_dfpnsab(vecstr args){
		double time = 60;
		int mem = mem_allowed;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		if(args.size() >= 2)
			mem = from_str<int>(args[1]);

		Solver solve;
		solve.solve_dfpnsab(*(game.getboard()), time, mem);

		return GTPResponse(true, solve_str(solve));
	}

	string solve_str(const Solver & solve){
		string ret = "";
		ret += solve_str(solve.outcome) + " ";
		ret += move_str(solve.X, solve.Y) + " ";
		ret += to_str(solve.maxdepth) + " ";
		ret += to_str(solve.nodes);
		return ret;
	}

	void parse_move(const string & str, int & x, int & y){
		x = tolower(str[0]) - 'a';
		y = atoi(str.c_str() + 1) - 1;

		if(!hguicoords && x >= game.getsize())
			y += x + 1 - game.getsize();
	}

	string move_str(Move & m, int hguic = -1){
		return move_str(m.x, m.y, hguic);
	}

	string move_str(int x, int y, int hguic = -1){
		if(hguic == -1)
			hguic = hguicoords;

		if(x == -1)
			return "none";
		if(x == -2)
			return "resign";

		if(!hguic && x >= game.getsize())
			y -= x + 1 - game.getsize();

		return string() + char(x + 'a') + to_str(y+1);
	}

	GTPResponse gtp_all_legal(vecstr args){
		MoveScore moves[game.getboard()->vecsize()];
		int num = game.getboard()->get_moves(moves);

		string ret;
		for(int i = 0; i < num; i++)
			ret += move_str(moves[i].x, moves[i].y) + " ";
		return GTPResponse(true, ret);
	}

	GTPResponse gtp_top_moves(vecstr args){
		MoveScore moves[game.getboard()->vecsize()];
		int num = game.getboard()->get_moves(moves, true);

		string ret;
		for(int i = 0; i < num; i++)
			ret += move_str(moves[i].x, moves[i].y) + " " + to_str(moves[i].score) + " ";
		return GTPResponse(true, ret);
	}

	GTPResponse gtp_genmove(vecstr args){
		double time = 4*time_remain / game.movesremain();
		if(time > time_remain)
			time = time_remain;
		time += time_per_move;
		int mem = mem_allowed;

		if(args.size() >= 2)
			time = from_str<double>(args[1]);

		if(args.size() >= 3)
			mem = from_str<int>(args[2]);

		fprintf(stderr, "time left: %.1f, max time: %.3f\n", time_remain, time);

		player.play_uct(*(game.getboard()), time, mem);

		time_remain += time_per_move - player.time_used;

		game.move(player.bestmove);

		string pv = "";
		for(int i = 0; i < player.principle_variation.size(); i++)
			pv += move_str(player.principle_variation[i], true) + " ";
		fprintf(stderr, "PV:          %s\n", pv.c_str());

		return GTPResponse(true, move_str(player.bestmove));
	}

	GTPResponse gtp_player_params(vecstr args){
		if(args.size() == 0)
			return GTPResponse(true, string("\n") +
				"Set player parameters, eg: player_params -e 3 -r 40 -t 0.1 -p 0\n" +
				"  -e --explore     Exploration rate                                  [" + to_str(player.explore) + "]\n" +
				"  -f --ravefactor  The rave factor: alpha = rf/(rf + visits)         [" + to_str(player.ravefactor) + "]\n" +
				"  -r --ravescale   Scale the rave values from 2 - 0 instead all 1    [" + to_str(player.ravescale) + "]\n" +
				"  -a --raveall     Assign a value of 0.5 to unplayed positions       [" + to_str(player.raveall) + "]\n" +
				"  -k --skiprave    Skip using rave values once in this many times    [" + to_str(player.skiprave) + "]\n" +
				"  -t --prooftime   Fraction of time to spend proving the node        [" + to_str(player.prooftime) + "]\n" +
				"  -s --proofscore  Number of visits to give based on a partial proof [" + to_str(player.proofscore) + "]\n" +
				"  -p --pattern     Use the virtual connection pattern in roll outs   [" + to_str(player.rolloutpattern) + "]\n" );

		for(int i = 0; i < args.size(); i++) {
			string arg = args[i];

			if((arg == "-e" || arg == "--explore") && i+1 < args.size()){
				player.explore = from_str<double>(args[++i]);
			}else if((arg == "-f" || arg == "--ravefactor") && i+1 < args.size()){
				player.ravefactor = from_str<int>(args[++i]);
			}else if((arg == "-r" || arg == "--ravescale") && i+1 < args.size()){
				player.ravescale = from_str<bool>(args[++i]);
			}else if((arg == "-a" || arg == "--raveall") && i+1 < args.size()){
				player.raveall = from_str<bool>(args[++i]);
			}else if((arg == "-k" || arg == "--skiprave") && i+1 < args.size()){
				player.skiprave = from_str<int>(args[++i]);
			}else if((arg == "-t" || arg == "--prooftime") && i+1 < args.size()){
				player.prooftime = from_str<double>(args[++i]);
			}else if((arg == "-s" || arg == "--proofscore") && i+1 < args.size()){
				player.proofscore = from_str<int>(args[++i]);
			}else if((arg == "-p" || arg == "--pattern") && i+1 < args.size()){
				player.rolloutpattern = from_str<bool>(args[++i]);
			}else{
				return GTPResponse(false, "Missing or unknown parameter");
			}
		}
		return GTPResponse(true);
	}



	GTPResponse play(const string & pos, int toplay){
		int x, y;
		parse_move(pos, x, y);
		if(!game.getboard()->onboard2(x, y))
			return GTPResponse(false, "Move out of bounds");

		if(!game.move(x, y, toplay)){
			if(game.getboard()->won() >= 0)
				return GTPResponse(false, "Game already over");
			else
				return GTPResponse(false, "Invalid placement, already taken");
		}

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

