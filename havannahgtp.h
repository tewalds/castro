
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
	enum time_control_t { TIME_PERCENT, TIME_EVEN, TIME_STATS };
	time_control_t time_control;
	double time_param;
	double time_remain;
	double time_per_move;
	int max_runs;
	int mem_allowed;
	bool allow_swap;

	Player player;

	HavannahGTP(FILE * i = stdin, FILE * o = stdout, FILE * l = NULL){
		GTPclient(i, o, l);

		player.set_board(game.getboard());
		player.start_threads();

		verbose = false;
		hguicoords = false;
		time_control = TIME_STATS;
		time_param  = 2;
		time_remain = 120;
		time_per_move = 0;
		mem_allowed = 1000;
		max_runs = 0;
		allow_swap = true;

		newcallback("name",            bind(&HavannahGTP::gtp_name,          this, _1), "Name of the program");
		newcallback("version",         bind(&HavannahGTP::gtp_version,       this, _1), "Version of the program");
		newcallback("verbose",         bind(&HavannahGTP::gtp_verbose,       this, _1), "Enable verbose mode");
		newcallback("debug",           bind(&HavannahGTP::gtp_debug,         this, _1), "Enable debug mode");
		newcallback("hguicoords",      bind(&HavannahGTP::gtp_hguicoords,    this, _1), "Switch coordinate systems to match HavannahGui");
		newcallback("gridcoords",      bind(&HavannahGTP::gtp_gridcoords,    this, _1), "Switch coordinate systems to match Little Golem");
		newcallback("showboard",       bind(&HavannahGTP::gtp_print,         this, _1), "Show the board");
		newcallback("print",           bind(&HavannahGTP::gtp_print,         this, _1), "Alias for showboard");
		newcallback("clear_board",     bind(&HavannahGTP::gtp_clearboard,    this, _1), "Clear the board, but keep the size");
		newcallback("boardsize",       bind(&HavannahGTP::gtp_boardsize,     this, _1), "Clear the board, set the board size");
		newcallback("swap",            bind(&HavannahGTP::gtp_swap,          this, _1), "Enable/disable swap: swap <0|1>");
		newcallback("time_settings",   bind(&HavannahGTP::gtp_time_settings, this, _1), "Set the time limits: time_settings [secs per game] [secs per move] [sims per move]");
		newcallback("play",            bind(&HavannahGTP::gtp_play,          this, _1), "Place a stone: play <color> <location>");
		newcallback("white",           bind(&HavannahGTP::gtp_playwhite,     this, _1), "Place a white stone: white <location>");
		newcallback("black",           bind(&HavannahGTP::gtp_playblack,     this, _1), "Place a black stone: black <location>");
		newcallback("undo",            bind(&HavannahGTP::gtp_undo,          this, _1), "Undo one or more moves: undo [amount to undo]");
		newcallback("genmove",         bind(&HavannahGTP::gtp_genmove,       this, _1), "Generate a move: genmove [color] [time] [memory]");
		newcallback("time_control",    bind(&HavannahGTP::gtp_time_control,  this, _1), "Set the algorithm for per game time, no args gives options");
		newcallback("player_params",   bind(&HavannahGTP::gtp_player_params, this, _1), "Set the algorithm for the player, no args gives options");
		newcallback("player_stats",    bind(&HavannahGTP::gtp_player_stats,  this, _1), "Gives some information about the last move");
		newcallback("all_legal",       bind(&HavannahGTP::gtp_all_legal,     this, _1), "List all legal moves");
		newcallback("history",         bind(&HavannahGTP::gtp_history,       this, _1), "List of played moves");
		newcallback("havannah_winner", bind(&HavannahGTP::gtp_winner,        this, _1), "Check the winner of the game");
		newcallback("havannah_solve",  bind(&HavannahGTP::gtp_solve,         this, _1), "Use the default solver: havannah_solve [time] [memory]");
		newcallback("solve_ab",        bind(&HavannahGTP::gtp_solve_ab,      this, _1), "Solve with negamax");
		newcallback("solve_scout",     bind(&HavannahGTP::gtp_solve_scout,   this, _1), "Solve with negascout");
		newcallback("solve_pns",       bind(&HavannahGTP::gtp_solve_pns,     this, _1), "Solve with basic proof number search");
		newcallback("solve_pnsab",     bind(&HavannahGTP::gtp_solve_pnsab,   this, _1), "Solve with proof number search, with one ply of alpha-beta");
		newcallback("solve_dfpnsab",   bind(&HavannahGTP::gtp_solve_dfpnsab, this, _1), "Solve with proof number search, with a depth-first optimization");
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


	GTPResponse gtp_time_settings(vecstr args){
		if(args.size() == 0)
			return GTPResponse(false, "Wrong number of arguments");

		log("time_settings " + implode(args, " "));

		if(args.size() >= 1) time_remain   = from_str<double>(args[0]);
		if(args.size() >= 2) time_per_move = from_str<double>(args[1]);
		if(args.size() >= 3) max_runs      = from_str<int>(args[2]);

		return GTPResponse(true);
	}

	GTPResponse gtp_boardsize(vecstr args){
		if(args.size() != 1)
			return GTPResponse(false, "Wrong number of arguments");

		log("boardsize " + args[0]);

		int size = from_str<int>(args[0]);
		if(size < 3 || size > 10)
			return GTPResponse(false, "Size " + to_str(size) + " is out of range.");

		game = HavannahGame(size);
		player.set_board(game.getboard());

		return GTPResponse(true);
	}

	GTPResponse gtp_clearboard(vecstr args){
		game.clear();
		player.set_board(game.getboard());

		log("clear_board");
		return GTPResponse(true);
	}

	GTPResponse gtp_undo(vecstr args){
		int num = 1;
		if(args.size() >= 1)
			num = from_str<int>(args[0]);

		while(num--){
			game.undo();
			player.set_board(game.getboard(-1));
			log("undo");
		}
		if(verbose)
			return GTPResponse(true, "\n" + game.getboard().to_s());
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
		solve.solve_ab(game.getboard(), time);

		return GTPResponse(true, solve_str(solve));
	}

	GTPResponse gtp_solve_scout(vecstr args){
		double time = 60;

		if(args.size() >= 1)
			time = from_str<double>(args[0]);

		Solver solve;
		solve.solve_scout(game.getboard(), time);

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
		solve.solve_pns(game.getboard(), time, mem);

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
		solve.solve_pnsab(game.getboard(), time, mem);

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
		solve.solve_dfpnsab(game.getboard(), time, mem);

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
		m.x = tolower(str[0]) - 'a';
		m.y = atoi(str.c_str() + 1) - 1;

		if(!hguicoords && m.x >= game.getsize())
			m.y += m.x + 1 - game.getsize();

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

		if(!hguic && m.x >= game.getsize())
			m.y -= m.x + 1 - game.getsize();

		return string() + char(m.x + 'a') + to_str(m.y + 1);
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

	GTPResponse gtp_time_control(vecstr args){
		bool matched = false;
		if(args.size() >= 1){
			if(     args[0] == "-p" || args[0] == "--percent"){ time_control = TIME_PERCENT; time_param = 10; matched = true; }
			else if(args[0] == "-e" || args[0] == "--even"   ){ time_control = TIME_EVEN;    time_param = 2;  matched = true; }
			else if(args[0] == "-s" || args[0] == "--stats"  ){ time_control = TIME_STATS;   time_param = 2;  matched = true; }
		}

		string current;
		if(time_control == TIME_PERCENT) current = "percent";
		if(time_control == TIME_EVEN)    current = "even";
		if(time_control == TIME_STATS)   current = "stats";

		if(matched){
			if(args.size() >= 2)
				time_param = from_str<double>(args[1]);
			return GTPResponse(true, current + " " + to_str(time_param));
		}

		return GTPResponse(true, string("\n") +
			"Choose the time control to use for the player, eg: time_control -e 2.5\n" +
			"Current time control: " + current + " " + to_str(time_param) + "\n" +
			"  -p --percent  Percentage of the remaining time every move                          [10.0]\n" +
			"  -e --even     Multiple of even split of the maximum  remaining moves (1.0 is even) [2.0]\n" +
			"  -s --stats    Multiple of even split of the expected remaining moves (1.0 is even) [2.0]\n" 
			);
	}

	double get_time(){
		double time = 0;

		switch(time_control){
			case TIME_PERCENT:
				time += time_param*time_remain/100;
				break;
			case TIME_STATS:
				if(player.gamelen() > 0){
					time += 2.0*time_param*time_remain / player.gamelen();
					break;
				}//fall back to even
			case TIME_EVEN:
				time += 2.0*time_param*time_remain / game.movesremain();
				break;
		}

		if(time > time_remain)
			time = time_remain;

		time += time_per_move;

		return time;
	}

	GTPResponse gtp_genmove(vecstr args){
		double time = get_time();
		int mem = mem_allowed;

		if(args.size() >= 2)
			time = from_str<double>(args[1]);

		if(args.size() >= 3)
			mem = from_str<int>(args[2]);

		fprintf(stderr, "time left: %.1f, max time: %.3f, max runs: %i\n", time_remain, time, max_runs);

		player.move(game.get_last());
		player.rootboard.setswap(allow_swap);

		Move best = player.genmove(time, max_runs, mem);

		time_remain += time_per_move - player.time_used;
		if(time_remain < 0)
			time_remain = 0;

		game.move(best);

		string pvstr = "";
		vector<Move> pv = player.get_pv();
		for(unsigned int i = 0; i < pv.size(); i++)
			pvstr += move_str(pv[i], true) + " ";
		fprintf(stderr, "PV:          %s\n", pvstr.c_str());

		log("#genmove " + implode(args, " "));
		log(string("play ") + (game.toplay() == 2 ? 'w' : 'b') + ' ' + move_str(best, false));

		return GTPResponse(true, move_str(best));
	}

	GTPResponse gtp_player_params(vecstr args){
		if(args.size() == 0)
			return GTPResponse(true, string("\n") +
				"Set player parameters, eg: player_params -e 3 -r 40 -t 0.1 -p 0\n" +
				"  -d --defaults    Reset all the parameters to size dependent defaults\n" +
				"Tree traversal:\n" +
				"  -e --explore     Exploration rate for UCT                          [" + to_str(player.explore) + "]\n" +
				"  -f --ravefactor  The rave factor: alpha = rf/(rf + visits)         [" + to_str(player.ravefactor) + "]\n" +
				"  -a --knowfactor  The knowledge factor: kf*know/sqrt(visits+1)      [" + to_str(player.knowfactor) + "]\n" +
				"  -r --ravescale   Scale the rave values from 2 - 0 instead all 1    [" + to_str(player.ravescale) + "]\n" +
				"  -o --opmoves     Treat good opponent moves as good moves for you   [" + to_str(player.opmoves) + "]\n" +
				"  -i --skiprave    Skip using rave values once in this many times    [" + to_str(player.skiprave) + "]\n" +
				"  -s --shortrave   Only use moves from short rollouts for rave       [" + to_str(player.shortrave) + "]\n" +
				"  -k --keeptree    Keep the tree from the previous move              [" + to_str(player.keeptree) + "]\n" +
				"  -m --minimax     Backup the minimax proof in the UCT tree          [" + to_str(player.minimax) + "]\n" +
				"  -u --fpurgency   Value to assign to an unplayed move               [" + to_str(player.fpurgency) + "]\n" +
				"  -x --visitexpand Number of visits before expanding a node          [" + to_str(player.visitexpand) + "]\n" +
				"Node initialization knowledge:\n" +
				"  -l --localreply  Give a bonus based on how close a reply is        [" + to_str(player.localreply) + "]\n" +
				"  -y --locality    Give a bonus to stones near other stones          [" + to_str(player.locality) + "]\n" +
				"  -c --connect     Give a bonus to stones connected to edges/corners [" + to_str(player.connect) + "]\n" +
				"  -b --bridge      Give a bonus to replying to a bridge probe        [" + to_str(player.bridge) + "]\n" +
				"Rollout policy:\n" +
				"  -h --weightrand  Weight the moves by the rave values at the root   [" + to_str(player.weightedrandom) + "]\n" +
				"  -p --pattern     Maintain the virtual connection pattern           [" + to_str(player.rolloutpattern) + "]\n" +
				"  -g --goodreply   Reuse the last good reply (1), remove losses (2)  [" + to_str(player.lastgoodreply) + "]\n" +
				"  -w --instantwin  Look for instant wins (1) and forced replies (2)  [" + to_str(player.instantwin) + "]\n"
				);

		for(unsigned int i = 0; i < args.size(); i++) {
			string arg = args[i];

			if(arg == "-d" || arg == "--defaults"){
				player.set_default_params();
			}else if((arg == "-e" || arg == "--explore") && i+1 < args.size()){
				player.explore = from_str<float>(args[++i]);
				player.defaults = false;
			}else if((arg == "-f" || arg == "--ravefactor") && i+1 < args.size()){
				player.ravefactor = from_str<float>(args[++i]);
				player.defaults = false;
			}else if((arg == "-a" || arg == "--knowfactor") && i+1 < args.size()){
				player.knowfactor = from_str<float>(args[++i]);
				player.defaults = false;
			}else if((arg == "-r" || arg == "--ravescale") && i+1 < args.size()){
				player.ravescale = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-o" || arg == "--opmoves") && i+1 < args.size()){
				player.opmoves = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-i" || arg == "--skiprave") && i+1 < args.size()){
				player.skiprave = from_str<int>(args[++i]);
				player.defaults = false;
			}else if((arg == "-s" || arg == "--shortrave") && i+1 < args.size()){
				player.shortrave = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-k" || arg == "--keeptree") && i+1 < args.size()){
				player.keeptree = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-m" || arg == "--minimax") && i+1 < args.size()){
				player.minimax = from_str<int>(args[++i]);
				player.defaults = false;
			}else if((arg == "-u" || arg == "--fpurgency") && i+1 < args.size()){
				player.fpurgency = from_str<float>(args[++i]);
				player.defaults = false;
			}else if((arg == "-x" || arg == "--visitexpand") && i+1 < args.size()){
				player.visitexpand = from_str<uint>(args[++i]);
				player.defaults = false;
			}else if((arg == "-l" || arg == "--localreply") && i+1 < args.size()){
				player.localreply = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-y" || arg == "--locality") && i+1 < args.size()){
				player.locality = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-c" || arg == "--connect") && i+1 < args.size()){
				player.connect = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-b" || arg == "--bridge") && i+1 < args.size()){
				player.bridge = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-h" || arg == "--weightrand") && i+1 < args.size()){
				player.weightedrandom = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-p" || arg == "--pattern") && i+1 < args.size()){
				player.rolloutpattern = from_str<bool>(args[++i]);
				player.defaults = false;
			}else if((arg == "-g" || arg == "--goodreply") && i+1 < args.size()){
				player.lastgoodreply = from_str<int>(args[++i]);
				player.defaults = false;
			}else if((arg == "-w" || arg == "--instantwin") && i+1 < args.size()){
				player.instantwin = from_str<int>(args[++i]);
				player.defaults = false;
			}else{
				return GTPResponse(false, "Missing or unknown parameter");
			}
		}
		return GTPResponse(true);
	}

	GTPResponse gtp_player_stats(vecstr args){
		string s;
		s += "Nodes: " + to_str(player.nodes) + "\n";
		s += player.rootboard.to_s();

		s += "Exp-Rave: ";
		for(Player::Node * child = player.root.children.begin(); child != player.root.children.end(); child++){
			s += move_str(child->move, true) + "-";
			s += to_str(child->exp.avg(), 2) + "/" + to_str(child->exp.num()) + "-";
			s += to_str(child->rave.avg(), 2) + "/" + to_str(child->rave.num()) + " ";
		}
		s += "\n";

		return GTPResponse(true, s);
	}

	GTPResponse play(const string & pos, int toplay){
		if(toplay != game.toplay())
			return GTPResponse(false, "It is the other player's turn!");

		if(game.getboard().won() >= 0)
			return GTPResponse(false, "The game is already over.");

		Move move = parse_move(pos);

		if(!game.valid(move))
			return GTPResponse(false, "Invalid move");

		player.move(game.get_last());

		game.move(move);

		log(string("play ") + (toplay == 1 ? 'w' : 'b') + ' ' + move_str(move, false));

		if(verbose)
			return GTPResponse(true, "Placement: " + move_str(move) + ", outcome: " + won_str(game.getboard().won()) + "\n" + game.getboard().to_s());
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
		return GTPResponse(true, won_str(game.getboard().won()));
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
		str += "Board size:  " + to_str(game.getboard().get_size()) + "\n";
		str += "Board cells: " + to_str(game.getboard().numcells()) + "\n";
		str += "Board vec:   " + to_str(game.getboard().vecsize()) + "\n";
		str += "Board mem:   " + to_str(game.getboard().memsize()) + "\n";

		return GTPResponse(true, str);
	}
};

#endif

