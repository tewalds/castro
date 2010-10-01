
#ifndef _HAVANNAHGTP_H_
#define _HAVANNAHGTP_H_

#include "gtp.h"
#include "game.h"
#include "string.h"
#include "solver.h"
#include "solverab.h"
#include "solverpns.h"
#include "solverpns_heap.h"
#include "solverpns_tt.h"
#include "player.h"
#include "board.h"
#include "move.h"
#include "lbdist.h"

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
		newcallback("pv",              bind(&HavannahGTP::gtp_pv,            this, _1), "Output the principle variation for the player tree as it stands now");
		newcallback("time",            bind(&HavannahGTP::gtp_time,          this, _1), "Set the time limits and the algorithm for per game time");
		newcallback("player_params",   bind(&HavannahGTP::gtp_player_params, this, _1), "Set the algorithm for the player, no args gives options");
		newcallback("all_legal",       bind(&HavannahGTP::gtp_all_legal,     this, _1), "List all legal moves");
		newcallback("history",         bind(&HavannahGTP::gtp_history,       this, _1), "List of played moves");
		newcallback("playgame",        bind(&HavannahGTP::gtp_playgame,      this, _1), "Play a list of moves");
		newcallback("havannah_winner", bind(&HavannahGTP::gtp_winner,        this, _1), "Check the winner of the game");
		newcallback("havannah_solve",  bind(&HavannahGTP::gtp_solve,         this, _1), "Use the default solver: havannah_solve [time] [memory]");
		newcallback("solve_ab",        bind(&HavannahGTP::gtp_solve_ab,      this, _1), "Solve with negamax");
		newcallback("solve_scout",     bind(&HavannahGTP::gtp_solve_scout,   this, _1), "Solve with negascout");
		newcallback("solve_pns",       bind(&HavannahGTP::gtp_solve_pns,     this, _1, 0, false), "Solve with basic proof number search");
		newcallback("solve_pnsab",     bind(&HavannahGTP::gtp_solve_pns,     this, _1, 1, false), "Solve with proof number search, with one ply of alpha-beta");
		newcallback("solve_dfpns",     bind(&HavannahGTP::gtp_solve_pns,     this, _1, 0, true),  "Solve with proof number search, with a depth-first optimization");
		newcallback("solve_dfpnsab",   bind(&HavannahGTP::gtp_solve_pns,     this, _1, 1, true),  "Solve with proof number search, with a depth-first optimization and one ply of alpha-beta");
		newcallback("solve_hpns",      bind(&HavannahGTP::gtp_solve_hpns,    this, _1, 0, false), "Solve with basic proof number search");
		newcallback("solve_hpnsab",    bind(&HavannahGTP::gtp_solve_hpns,    this, _1, 1, false), "Solve with proof number search, with one ply of alpha-beta");
		newcallback("solve_hdfpns",    bind(&HavannahGTP::gtp_solve_hpns,    this, _1, 0, true),  "Solve with proof number search, with a depth-first optimization");
		newcallback("solve_hdfpnsab",  bind(&HavannahGTP::gtp_solve_hpns,    this, _1, 1, true),  "Solve with proof number search, with a depth-first optimization and one ply of alpha-beta");
		newcallback("solve_ttpns",     bind(&HavannahGTP::gtp_solve_pnstt,   this, _1, 0, false), "Solve with basic proof number search");
		newcallback("solve_ttpnsab",   bind(&HavannahGTP::gtp_solve_pnstt,   this, _1, 1, false), "Solve with proof number search, with one ply of alpha-beta");
		newcallback("solve_ttdfpns",   bind(&HavannahGTP::gtp_solve_pnstt,   this, _1, 0, true),  "Solve with proof number search, with a depth-first optimization");
		newcallback("solve_ttdfpnsab", bind(&HavannahGTP::gtp_solve_pnstt,   this, _1, 1, true),  "Solve with proof number search, with a depth-first optimization and one ply of alpha-beta");
	}

	GTPResponse gtp_print(vecstr args){
		return GTPResponse(true, "\n" + game.getboard().to_s());
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

	GTPResponse gtp_swap(vecstr args){
		if(args.size() == 0)
			return GTPResponse(false, "Wrong number of arguments");

		log("swap " + implode(args, " "));

		if(args.size() >= 1)
			allow_swap = from_str<bool>(args[0]);

		string ret = "";
		if(allow_swap) ret += "Swap on";
		else           ret += "Swap off";

		return GTPResponse(true, ret);
	}

	GTPResponse gtp_boardsize(vecstr args){
		if(args.size() != 1)
			return GTPResponse(false, "Current board size: " + to_str(game.getsize()));

		log("boardsize " + args[0]);

		int size = from_str<int>(args[0]);
		if(size < 3 || size > 10)
			return GTPResponse(false, "Size " + to_str(size) + " is out of range.");

		game = HavannahGame(size);
		player.set_board(game.getboard());

		time_remain = time.game;

		return GTPResponse(true);
	}

	GTPResponse gtp_clearboard(vecstr args){
		game.clear();
		player.set_board(game.getboard());

		time_remain = time.game;

		log("clear_board");
		return GTPResponse(true);
	}

	GTPResponse gtp_undo(vecstr args){
		int num = 1;
		if(args.size() >= 1)
			num = from_str<int>(args[0]);

		while(num--){
			game.undo();
			log("undo");
		}
		player.set_board(game.getboard());
		if(verbose)
			return GTPResponse(true, "\n" + game.getboard().to_s());
		else
			return GTPResponse(true);
	}

	GTPResponse gtp_solve(vecstr args){
		log("havannah_solve " + implode(args, " "));

		return gtp_solve_pns(args, 1, true);
	}

	GTPResponse gtp_solve_ab(vecstr args){
		double time = 60;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		SolverAB solve(false);
		solve.solve(game.getboard(), time);

		return GTPResponse(true, solve_str(solve));
	}

	GTPResponse gtp_solve_scout(vecstr args){
		double time = 60;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		SolverAB solve(true);
		solve.solve(game.getboard(), time);

		return GTPResponse(true, solve_str(solve));
	}

	GTPResponse gtp_solve_pns(vecstr args, int ab, bool df){
		double time = 60;
		int mem = mem_allowed;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		if(args.size() >= 2)
			mem = from_str<int>(args[1]);

		SolverPNS solve(ab, df);
		solve.solve(game.getboard(), time, mem);

		return GTPResponse(true, solve_str(solve));
	}

	GTPResponse gtp_solve_hpns(vecstr args, int ab, bool df){
		double time = 60;
		int mem = mem_allowed;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		if(args.size() >= 2)
			mem = from_str<int>(args[1]);

		SolverPNSHeap solve(ab, df);
		solve.solve(game.getboard(), time, mem);

		return GTPResponse(true, solve_str(solve));
	}

	GTPResponse gtp_solve_pnstt(vecstr args, int ab, bool df){
		double time = 60;
		int mem = mem_allowed;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		if(args.size() >= 2)
			mem = from_str<int>(args[1]);

		SolverPNSTT solve(ab, df);
		solve.solve(game.getboard(), time, mem);

		return GTPResponse(true, solve_str(solve));
	}


	string solve_str(const Solver & solve){
		string ret = "";
		ret += solve_str(solve.outcome) + " ";
		ret += move_str(solve.bestmove) + " ";
		ret += to_str(solve.maxdepth) + " ";
		ret += to_str(solve.nodes_seen);
		return ret;
	}

	Move parse_move(const string & str){
		if(str == "swap")
			return Move(M_SWAP);

		Move m;
		m.y = tolower(str[0]) - 'a';
		m.x = atoi(str.c_str() + 1) - 1;

		if(!hguicoords && m.y >= game.getsize())
			m.x += m.y + 1 - game.getsize();

		return m;
	}

	string move_str(int x, int y, int hguic = -1){
		return move_str(Move(x, y), hguic);
	}

	string move_str(Move m, int hguic = -1){
		if(hguic == -1)
			hguic = hguicoords;

		if(m == M_UNKNOWN) return "unknown";
		if(m == M_NONE)    return "none";
		if(m == M_SWAP)    return "swap";
		if(m == M_RESIGN)  return "resign";

		if(!hguic && m.y >= game.getsize())
			m.x -= m.y + 1 - game.getsize();

		return string() + char(m.y + 'a') + to_str(m.x + 1);
	}

	GTPResponse gtp_all_legal(vecstr args){
		string ret;
		for(Board::MoveIterator move = game.getboard().moveit(); !move.done(); ++move)
			ret += move_str(*move) + " ";
		return GTPResponse(true, ret);
	}

	GTPResponse gtp_history(vecstr args){
		string ret;
		vector<Move> hist = game.get_hist();
		for(unsigned int i = 0; i < hist.size(); i++)
			ret += move_str(hist[i]) + " ";
		return GTPResponse(true, ret);
	}



	GTPResponse gtp_time(vecstr args){
		if(args.size() == 0)
			return GTPResponse(true, string("\n") +
				"Update the time settings, eg: time -s 2.5 -m 10 -g 600 -f 1\n" +
				"Method for distributing remaining time, current: " + time.method_name() + " " + to_str(time.param) + "\n" +
				"  -p --percent  Percentage of the remaining time every move            [10.0]\n" +
				"  -e --even     Multiple of even split of the maximum  remaining moves [2.0]\n" +
				"  -s --stats    Multiple of even split of the expected remaining moves [2.0]\n" +
				"Time allocation\n" +
				"  -m --move     Time per move                                          [" + to_str(time.move) + "]\n" +
				"  -g --game     Time per game                                          [" + to_str(time.game) + "]\n" +
				"  -f --flexible Add remaining time per move to remaining time          [" + to_str(time.flexible) + "]\n" +
				"  -i --maxsims  Maximum number of simulations per move                 [" + to_str(time.max_sims) + "]\n" +
				"Current game\n" +
				"  -r --remain   Remaining time for this game                           [" + to_str(time_remain) + "]\n");

		for(unsigned int i = 0; i < args.size(); i++) {
			string arg = args[i];

			if(arg == "-p" || arg == "--percent"){
				time.method = TimeControl::PERCENT;
				time.param = 10;
				if(i+1 < args.size() && from_str<double>(args[i+1]) > 0) time.param = from_str<double>(args[++i]);
			}else if(arg == "-e" || arg == "--even"){
				time.method = TimeControl::EVEN;
				time.param = 2;
				if(i+1 < args.size() && from_str<double>(args[i+1]) > 0) time.param = from_str<double>(args[++i]);
			}else if(arg == "-s" || arg == "--stats"){
				time.method = TimeControl::STATS;
				time.param = 2;
				if(i+1 < args.size() && from_str<double>(args[i+1]) > 0) time.param = from_str<double>(args[++i]);
			}else if((arg == "-m" || arg == "--move") && i+1 < args.size()){
				time.move = from_str<double>(args[++i]);
			}else if((arg == "-g" || arg == "--game") && i+1 < args.size()){
				time.game = from_str<float>(args[++i]);
			}else if((arg == "-f" || arg == "--flexible") && i+1 < args.size()){
				time.flexible = from_str<bool>(args[++i]);
			}else if((arg == "-i" || arg == "--maxsims") && i+1 < args.size()){
				time.max_sims = from_str<int>(args[++i]);
			}else if((arg == "-r" || arg == "--remain") && i+1 < args.size()){
				time_remain = from_str<double>(args[++i]);
			}else{
				return GTPResponse(false, "Missing or unknown parameter");
			}
		}

		return GTPResponse(true);
	}

	double get_time(){
		double ret = 0;

		switch(time.method){
			case TimeControl::PERCENT:
				ret += time.param*time_remain/100;
				break;
			case TimeControl::STATS:
				if(player.gamelen() > 0){
					ret += 2.0*time.param*time_remain / player.gamelen();
					break;
				}//fall back to even
			case TimeControl::EVEN:
				ret += 2.0*time.param*time_remain / game.movesremain();
				break;
		}

		if(ret > time_remain)
			ret = time_remain;

		ret += time.move;

		return ret;
	}

	GTPResponse gtp_move_stats(vecstr args){
		string s = "";
		Player::Node * child = player.root.children.begin(),
		             * childend = player.root.children.end();
		for( ; child != childend; child++){
			s += move_str(child->move, true);
			s += "," + to_str(child->exp.avg(), 2) + "," + to_str(child->exp.num());
			s += "," + to_str(child->rave.avg(), 2) + "," + to_str(child->rave.num());
			if(child->outcome >= 0)
				s += "," + won_str(child->outcome);
			s += "\n";
		}
		return GTPResponse(true, s);
	}

	GTPResponse gtp_pv(vecstr args){
		string pvstr = "";
		vector<Move> pv = player.get_pv();
		for(unsigned int i = 0; i < pv.size(); i++)
			pvstr += move_str(pv[i], true) + " ";
		return GTPResponse(true, pvstr);
	}

	GTPResponse gtp_genmove(vecstr args){
		double use_time = get_time();

		if(args.size() >= 2)
			use_time = from_str<double>(args[1]);

		fprintf(stderr, "time remain: %.1f, time: %.3f, sims: %i\n", time_remain, use_time, time.max_sims);

		player.rootboard.setswap(allow_swap);

		Move best = player.genmove(use_time, time.max_sims);

		if(time.flexible)
			time_remain += time.move - player.time_used;
		else
			time_remain += min(0.0, time.move - player.time_used);

		if(time_remain < 0)
			time_remain = 0;

		log("#genmove " + implode(args, " "));
		log(string("play ") + (game.toplay() == 2 ? 'w' : 'b') + ' ' + move_str(best, false));


		fprintf(stderr, "PV:          %s\n", gtp_pv(vecstr()).response.c_str());
		if(verbose && !player.root.children.empty())
			fprintf(stderr, "Exp-Rave:\n%s\n", gtp_move_stats(vecstr()).response.c_str());

		player.move(best);
		game.move(best);

		return GTPResponse(true, move_str(best));
	}

	GTPResponse gtp_player_params(vecstr args){
		if(args.size() == 0)
			return GTPResponse(true, string("\n") +
				"Set player parameters, eg: player_params -e 1 -f 0 -t 2 -o 1 -p 0\n" +
				"Processing:\n" +
#ifndef SINGLE_THREAD
				"  -t --threads     Number of MCTS threads                            [" + to_str(player.numthreads) + "]\n" +
#endif
				"  -o --ponder      Continue to ponder during the opponents time      [" + to_str(player.ponder) + "]\n" +
				"  -M --maxmem      Max memory in Mb to use for the tree              [" + to_str(player.maxmem) + "]\n" +
				"Final move selection:\n" +
				"  -E --msexplore   Lower bound constant in final move selection      [" + to_str(player.msexplore) + "]\n" +
				"  -F --msrave      Rave factor, 0 for pure exp, -1 # sims, -2 # wins [" + to_str(player.msrave) + "]\n" +
				"Tree traversal:\n" +
				"  -e --explore     Exploration rate for UCT                          [" + to_str(player.explore) + "]\n" +
				"  -f --ravefactor  The rave factor: alpha = rf/(rf + visits)         [" + to_str(player.ravefactor) + "]\n" +
				"  -d --decrrave    Decrease the rave factor over time: rf += d*empty [" + to_str(player.decrrave) + "]\n" +
				"  -a --knowledge   Use knowledge: 0.01*know/sqrt(visits+1)           [" + to_str(player.knowledge) + "]\n" +
				"  -r --userave     Use rave with this probability [0-1]              [" + to_str(player.userave) + "]\n" +
				"  -X --useexplore  Use exploration with this probability [0-1]       [" + to_str(player.useexplore) + "]\n" +
				"  -u --fpurgency   Value to assign to an unplayed move               [" + to_str(player.fpurgency) + "]\n" +
				"Tree building:\n" +
				"  -s --shortrave   Only use moves from short rollouts for rave       [" + to_str(player.shortrave) + "]\n" +
				"  -k --keeptree    Keep the tree from the previous move              [" + to_str(player.keeptree) + "]\n" +
				"  -m --minimax     Backup the minimax proof in the UCT tree          [" + to_str(player.minimax) + "]\n" +
				"  -x --visitexpand Number of visits before expanding a node          [" + to_str(player.visitexpand) + "]\n" +
				"Node initialization knowledge:\n" +
				"  -l --localreply  Give a bonus based on how close a reply is        [" + to_str(player.localreply) + "]\n" +
				"  -y --locality    Give a bonus to stones near other stones          [" + to_str(player.locality) + "]\n" +
				"  -c --connect     Give a bonus to stones connected to edges/corners [" + to_str(player.connect) + "]\n" +
				"  -S --size        Give a bonus based on the size of the group       [" + to_str(player.size) + "]\n" +
				"  -b --bridge      Give a bonus to replying to a bridge probe        [" + to_str(player.bridge) + "]\n" +
				"  -D --distance    Give a bonus to low minimum distance to win       [" + to_str(player.dists) + "]\n" +
				"Rollout policy:\n" +
				"  -h --weightrand  Weight the moves by the rave values at the root   [" + to_str(player.weightedrandom) + "]\n" +
				"  -K --weightknow  Use knowledge in the weighted random values       [" + to_str(player.weightedknow) + "]\n" +
				"  -C --checkrings  Check for rings only this often in rollouts       [" + to_str(player.checkrings) + "]\n" +
				"  -R --ringdepth   Check for rings for this depth in rollouts        [" + to_str(player.checkringdepth) + "]\n" +
				"  -p --pattern     Maintain the virtual connection pattern           [" + to_str(player.rolloutpattern) + "]\n" +
				"  -g --goodreply   Reuse the last good reply (1), remove losses (2)  [" + to_str(player.lastgoodreply) + "]\n" +
				"  -w --instantwin  Look for instant wins (1) and forced replies (2)  [" + to_str(player.instantwin) + "]\n" +
				"  -W --instwindep  How deep to check instant wins, - multiplies size [" + to_str(player.instwindepth) + "]\n"
				);

		for(unsigned int i = 0; i < args.size(); i++) {
			string arg = args[i];

			if((arg == "-t" || arg == "--threads") && i+1 < args.size()){
				player.numthreads = from_str<int>(args[++i]);
				bool p = player.ponder;
				player.set_ponder(false); //stop the threads while resetting them
				player.reset_threads();
				player.set_ponder(p);
			}else if((arg == "-o" || arg == "--ponder") && i+1 < args.size()){
				player.set_ponder(from_str<bool>(args[++i]));
			}else if((arg == "-M" || arg == "--maxmem") && i+1 < args.size()){
				player.set_maxmem(from_str<int>(args[++i]));
			}else if((arg == "-E" || arg == "--msexplore") && i+1 < args.size()){
				player.msexplore = from_str<float>(args[++i]);
			}else if((arg == "-F" || arg == "--msrave") && i+1 < args.size()){
				player.msrave = from_str<float>(args[++i]);
			}else if((arg == "-e" || arg == "--explore") && i+1 < args.size()){
				player.explore = from_str<float>(args[++i]);
			}else if((arg == "-f" || arg == "--ravefactor") && i+1 < args.size()){
				player.ravefactor = from_str<float>(args[++i]);
			}else if((arg == "-d" || arg == "--decrrave") && i+1 < args.size()){
				player.decrrave = from_str<float>(args[++i]);
			}else if((arg == "-a" || arg == "--knowledge") && i+1 < args.size()){
				player.knowledge = from_str<bool>(args[++i]);
			}else if((arg == "-s" || arg == "--shortrave") && i+1 < args.size()){
				player.shortrave = from_str<bool>(args[++i]);
			}else if((arg == "-k" || arg == "--keeptree") && i+1 < args.size()){
				player.keeptree = from_str<bool>(args[++i]);
			}else if((arg == "-m" || arg == "--minimax") && i+1 < args.size()){
				player.minimax = from_str<int>(args[++i]);
			}else if((arg == "-r" || arg == "--userave") && i+1 < args.size()){
				player.userave = from_str<float>(args[++i]);
			}else if((arg == "-X" || arg == "--useexplore") && i+1 < args.size()){
				player.useexplore = from_str<float>(args[++i]);
			}else if((arg == "-u" || arg == "--fpurgency") && i+1 < args.size()){
				player.fpurgency = from_str<float>(args[++i]);
			}else if((arg == "-x" || arg == "--visitexpand") && i+1 < args.size()){
				player.visitexpand = from_str<uint>(args[++i]);
			}else if((arg == "-l" || arg == "--localreply") && i+1 < args.size()){
				player.localreply = from_str<int>(args[++i]);
			}else if((arg == "-y" || arg == "--locality") && i+1 < args.size()){
				player.locality = from_str<int>(args[++i]);
			}else if((arg == "-c" || arg == "--connect") && i+1 < args.size()){
				player.connect = from_str<int>(args[++i]);
			}else if((arg == "-S" || arg == "--size") && i+1 < args.size()){
				player.size = from_str<int>(args[++i]);
			}else if((arg == "-b" || arg == "--bridge") && i+1 < args.size()){
				player.bridge = from_str<int>(args[++i]);
			}else if((arg == "-D" || arg == "--distance") && i+1 < args.size()){
				player.dists = from_str<int>(args[++i]);
			}else if((arg == "-h" || arg == "--weightrand") && i+1 < args.size()){
				player.weightedrandom = from_str<bool>(args[++i]);
			}else if((arg == "-K" || arg == "--weightknow") && i+1 < args.size()){
				player.weightedknow = from_str<bool>(args[++i]);
			}else if((arg == "-C" || arg == "--checkrings") && i+1 < args.size()){
				player.checkrings = from_str<float>(args[++i]);
			}else if((arg == "-R" || arg == "--ringdepth") && i+1 < args.size()){
				player.checkringdepth = from_str<float>(args[++i]);
			}else if((arg == "-p" || arg == "--pattern") && i+1 < args.size()){
				player.rolloutpattern = from_str<bool>(args[++i]);
			}else if((arg == "-g" || arg == "--goodreply") && i+1 < args.size()){
				player.lastgoodreply = from_str<int>(args[++i]);
			}else if((arg == "-w" || arg == "--instantwin") && i+1 < args.size()){
				player.instantwin = from_str<int>(args[++i]);
			}else if((arg == "-W" || arg == "--instwindep") && i+1 < args.size()){
				player.instwindepth = from_str<int>(args[++i]);
			}else{
				return GTPResponse(false, "Missing or unknown parameter");
			}
		}
		return GTPResponse(true);
	}

	GTPResponse play(const string & pos, int toplay){
		if(toplay != game.toplay())
			return GTPResponse(false, "It is the other player's turn!");

		if(game.getboard().won() >= 0)
			return GTPResponse(false, "The game is already over.");

		Move move = parse_move(pos);

		if(!game.valid(move))
			return GTPResponse(false, "Invalid move");

		player.move(move);

		game.move(move);

		log(string("play ") + (toplay == 1 ? 'w' : 'b') + ' ' + move_str(move, false));

		if(verbose)
			return GTPResponse(true, "Placement: " + move_str(move) + ", outcome: " + won_str(game.getboard().won()) + "\n" + game.getboard().to_s());
		else
			return GTPResponse(true);
	}

	GTPResponse gtp_playgame(vecstr args){
		GTPResponse ret(true);

		for(unsigned int i = 0; ret.success && i < args.size(); i++)
			ret = play(args[i], game.toplay());

		return ret;
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
		return GTPResponse(true, won_str(game.getboard().won()));
	}

	GTPResponse gtp_name(vecstr args){
		return GTPResponse(true, "Castro");
	}

	GTPResponse gtp_version(vecstr args){
		return GTPResponse(true, "0.1");
	}

	GTPResponse gtp_verbose(vecstr args){
		if(args.size() >= 1)
			verbose = from_str<bool>(args[0]);
		else
			verbose = !verbose;
		return GTPResponse(true, "Verbose " + to_str(verbose));
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
		str += "Board size:  " + to_str(game.getboard().get_size()) + "\n";
		str += "Board cells: " + to_str(game.getboard().numcells()) + "\n";
		str += "Board vec:   " + to_str(game.getboard().vecsize()) + "\n";
		str += "Board mem:   " + to_str(game.getboard().memsize()) + "\n";

		return GTPResponse(true, str);
	}

	GTPResponse gtp_dists(vecstr args){
		Board board = game.getboard();
		LBDists dists(&board);

		int size = board.get_size();
		int size_d = board.get_size_d();

		string s = "\n";
		s += string(size + 4, ' ');
		for(int i = 0; i < size; i++){
			s += to_str(i+1);
			s += " ";
		}
		s += "\n";

		for(int y = 0; y < size_d; y++){
			s += string(abs(size-1 - y) + 2, ' ');
			s += char('A' + y);
			s += " ";
			for(int x = board.linestart(y); x < board.lineend(y); x++){
				int p = board.get(x, y);
				if(p == 0){
					int d = dists.get(Move(x, y));
					if(d < 10)
						s += to_str(d);
					else
						s += '.';
				}else if(p == 1){
					s += 'W';
				}else if(p == 2){
					s += 'B';
				}
				s += ' ';
			}
			if(y < size-1)
				s += to_str(1 + size + y);
			s += '\n';
		}
		return GTPResponse(true, s);
	}
};

#endif

