
#ifndef _HAVANNAHGTP_H_
#define _HAVANNAHGTP_H_

#include "gtp.h"
#include "game.h"
#include "string.h"
#include "solver.h"
#include "solverab.h"
#include "solverpns.h"
#include "solverpns_tt.h"
#include "player.h"
#include "board.h"
#include "move.h"

struct TimeControl {
	enum Method { PERCENT, EVEN, STATS };
	Method method; //method to use to distribute the remaining time
	double param;  //param for the method, such as the percentage used or multiple of even
	double game;
	double move;
	bool   flexible; //whether time_per_move can be saved for future moves
	int    max_sims;

	TimeControl(){
		method   = STATS;
		param    = 2;
		game     = 120;
		move     = 0;
		flexible = true;
		max_sims = 0;
	}

	string method_name(){
		switch(method){
			case PERCENT: return "percent";
			case EVEN:    return "even";
			case STATS:   return "stats";
			default:      return "WTF? unknown time control method";
		}
	}
};

class HavannahGTP : public GTPclient {
	HavannahGame game;

public:
	bool verbose;
	bool hguicoords;

	TimeControl time;
	double      time_remain; //time remaining for this game

	int mem_allowed;
	bool allow_swap;

	Player player;

	SolverAB    solverab;
	SolverPNS   solverpns;
	SolverPNSTT solverpnstt;

	HavannahGTP(FILE * i = stdin, FILE * o = stdout, FILE * l = NULL){
		GTPclient(i, o, l);

		verbose = false;
		hguicoords = false;

		time_remain = time.game;

		mem_allowed = 1000;
		allow_swap = false;

		player.set_board(game.getboard());

		newcallback("name",            bind(&HavannahGTP::gtp_name,          this, _1), "Name of the program");
		newcallback("version",         bind(&HavannahGTP::gtp_version,       this, _1), "Version of the program");
		newcallback("verbose",         bind(&HavannahGTP::gtp_verbose,       this, _1), "Enable verbose mode");
		newcallback("debug",           bind(&HavannahGTP::gtp_debug,         this, _1), "Enable debug mode");
		newcallback("hguicoords",      bind(&HavannahGTP::gtp_hguicoords,    this, _1), "Switch coordinate systems to match HavannahGui");
		newcallback("gridcoords",      bind(&HavannahGTP::gtp_gridcoords,    this, _1), "Switch coordinate systems to match Little Golem");
		newcallback("showboard",       bind(&HavannahGTP::gtp_print,         this, _1), "Show the board");
		newcallback("print",           bind(&HavannahGTP::gtp_print,         this, _1), "Alias for showboard");
		newcallback("dists",           bind(&HavannahGTP::gtp_dists,         this, _1), "Similar to print, but shows minimum win distances");
		newcallback("clear_board",     bind(&HavannahGTP::gtp_clearboard,    this, _1), "Clear the board, but keep the size");
		newcallback("clear",           bind(&HavannahGTP::gtp_clearboard,    this, _1), "Alias for clear_board");
		newcallback("boardsize",       bind(&HavannahGTP::gtp_boardsize,     this, _1), "Clear the board, set the board size");
		newcallback("swap",            bind(&HavannahGTP::gtp_swap,          this, _1), "Enable/disable swap: swap <0|1>");
		newcallback("play",            bind(&HavannahGTP::gtp_play,          this, _1), "Place a stone: play <color> <location>");
		newcallback("white",           bind(&HavannahGTP::gtp_playwhite,     this, _1), "Place a white stone: white <location>");
		newcallback("black",           bind(&HavannahGTP::gtp_playblack,     this, _1), "Place a black stone: black <location>");
		newcallback("undo",            bind(&HavannahGTP::gtp_undo,          this, _1), "Undo one or more moves: undo [amount to undo]");
		newcallback("genmove",         bind(&HavannahGTP::gtp_genmove,       this, _1), "Generate a move: genmove [color] [time]");
		newcallback("move_stats",      bind(&HavannahGTP::gtp_move_stats,    this, _1), "Output the move stats for the player tree as it stands now");
		newcallback("player_solved",   bind(&HavannahGTP::gtp_player_solved, this, _1), "Output whether the player solved the current node");
		newcallback("pv",              bind(&HavannahGTP::gtp_pv,            this, _1), "Output the principle variation for the player tree as it stands now");
		newcallback("time",            bind(&HavannahGTP::gtp_time,          this, _1), "Set the time limits and the algorithm for per game time");
		newcallback("player_params",   bind(&HavannahGTP::gtp_player_params, this, _1), "Set the algorithm for the player, no args gives options");
		newcallback("all_legal",       bind(&HavannahGTP::gtp_all_legal,     this, _1), "List all legal moves");
		newcallback("history",         bind(&HavannahGTP::gtp_history,       this, _1), "List of played moves");
		newcallback("playgame",        bind(&HavannahGTP::gtp_playgame,      this, _1), "Play a list of moves");
		newcallback("winner",          bind(&HavannahGTP::gtp_winner,        this, _1), "Check the winner of the game");
		newcallback("havannah_winner", bind(&HavannahGTP::gtp_winner,        this, _1), "Alias for winner");
		newcallback("solve_ab",        bind(&HavannahGTP::gtp_solve_ab,      this, _1), "Solve with negamax");
		newcallback("solve_scout",     bind(&HavannahGTP::gtp_solve_scout,   this, _1), "Solve with negascout");

		newcallback("pns_solve",       bind(&HavannahGTP::gtp_solve_pns,        this, _1),  "Solve with proof number search and an explicit tree");
		newcallback("pns_params",      bind(&HavannahGTP::gtp_solve_pns_params, this, _1),  "Set Parameters for PNS");
		newcallback("pns_stats",       bind(&HavannahGTP::gtp_solve_pns_stats,  this, _1),  "Output the stats for the PNS solver");
		newcallback("pns_clear",       bind(&HavannahGTP::gtp_solve_pns_clear,  this, _1),  "Stop the solver and release the memory");

		newcallback("pnstt_solve",     bind(&HavannahGTP::gtp_solve_pnstt,        this, _1),  "Solve with proof number search and a transposition table of fixed size");
		newcallback("pnstt_params",    bind(&HavannahGTP::gtp_solve_pnstt_params, this, _1),  "Set Parameters for PNSTT");
		newcallback("pnstt_stats",     bind(&HavannahGTP::gtp_solve_pnstt_stats,  this, _1),  "Outupt the stats for the PNSTT solver");
		newcallback("pnstt_clear",     bind(&HavannahGTP::gtp_solve_pnstt_clear,  this, _1),  "Stop the solver and release the memory");
	}

	GTPResponse gtp_print(vecstr args);
	string won_str(int outcome) const;
	GTPResponse gtp_swap(vecstr args);
	GTPResponse gtp_boardsize(vecstr args);
	GTPResponse gtp_clearboard(vecstr args);
	GTPResponse gtp_undo(vecstr args);
	Move parse_move(const string & str);
	string move_str(int x, int y, int hguic = -1);
	string move_str(Move m, int hguic = -1);
	GTPResponse gtp_all_legal(vecstr args);
	GTPResponse gtp_history(vecstr args);
	GTPResponse play(const string & pos, int toplay);
	GTPResponse gtp_playgame(vecstr args);
	GTPResponse gtp_play(vecstr args);
	GTPResponse gtp_playwhite(vecstr args);
	GTPResponse gtp_playblack(vecstr args);
	GTPResponse gtp_winner(vecstr args);
	GTPResponse gtp_name(vecstr args);
	GTPResponse gtp_version(vecstr args);
	GTPResponse gtp_verbose(vecstr args);
	GTPResponse gtp_hguicoords(vecstr args);
	GTPResponse gtp_gridcoords(vecstr args);
	GTPResponse gtp_debug(vecstr args);
	GTPResponse gtp_dists(vecstr args);

	GTPResponse gtp_time(vecstr args);
	double get_time();
	GTPResponse gtp_move_stats(vecstr args);
	GTPResponse gtp_player_solved(vecstr args);
	GTPResponse gtp_pv(vecstr args);
	GTPResponse gtp_genmove(vecstr args);
	GTPResponse gtp_player_params(vecstr args);

	string solve_str(int outcome) const;
	string solve_str(const Solver & solve);
	GTPResponse gtp_solve_ab(vecstr args);
	GTPResponse gtp_solve_scout(vecstr args);

	GTPResponse gtp_solve_pns(vecstr args);
	GTPResponse gtp_solve_pns_params(vecstr args);
	GTPResponse gtp_solve_pns_stats(vecstr args);
	GTPResponse gtp_solve_pns_clear(vecstr args);

	GTPResponse gtp_solve_pnstt(vecstr args);
	GTPResponse gtp_solve_pnstt_params(vecstr args);
	GTPResponse gtp_solve_pnstt_stats(vecstr args);
	GTPResponse gtp_solve_pnstt_clear(vecstr args);
};

#endif

