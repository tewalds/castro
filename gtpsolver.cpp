

#include "havannahgtp.h"

string HavannahGTP::solve_str(int outcome) const {
	switch(outcome){
		case -2: return "black_or_draw";
		case -1: return "white_or_draw";
		case  0: return "draw";
		case  1: return "white";
		case  2: return "black";
		default: return "unknown";
	}
}

string HavannahGTP::solve_str(const Solver & solve){
	string ret = "";
	ret += solve_str(solve.outcome) + " ";
	ret += move_str(solve.bestmove) + " ";
	ret += to_str(solve.maxdepth) + " ";
	ret += to_str(solve.nodes_seen);
	return ret;
}

GTPResponse HavannahGTP::gtp_solve_ab(vecstr args){
	double time = 60;

	if(args.size() >= 1)
		time = from_str<double>(args[0]);

	SolverAB solve(false);
	solve.set_board(game.getboard());
	solve.solve(time);

	return GTPResponse(true, solve_str(solve));
}

GTPResponse HavannahGTP::gtp_solve_scout(vecstr args){
	double time = 60;

	if(args.size() >= 1)
		time = from_str<double>(args[0]);

	SolverAB solve(true);
	solve.set_board(game.getboard());
	solve.solve(time);

	return GTPResponse(true, solve_str(solve));
}

GTPResponse HavannahGTP::gtp_solve_pns(vecstr args){
	double time = 60;

	if(args.size() >= 1)
		time = from_str<double>(args[0]);

	solverpns.solve(time);

	return GTPResponse(true, solve_str(solverpns));
}

GTPResponse HavannahGTP::gtp_solve_pns_params(vecstr args){
	if(args.size() == 0)
		return GTPResponse(true, string("\n") +
			"Update the pns solver settings, eg: pns_params -m 100 -s 0 -d 1 -e 0.25 -a 2 -l 0\n"
			"  -m --memory   Memory limit in Mb                                       [" + to_str(solverpns.memlimit) + "]\n"
			"  -s --ties     Which side to assign ties to, 0 = handle, 1 = p1, 2 = p2 [" + to_str(solverpns.ties) + "]\n"
//			"  -t --threads  How many threads to run
//			"  -o --ponder   Ponder in the background
			"  -d --df       Use depth-first thresholds                               [" + to_str(solverpns.df) + "]\n"
			"  -e --epsilon  How big should the threshold be                          [" + to_str(solverpns.epsilon) + "]\n"
			"  -a --abdepth  Run an alpha-beta search of this size at each leaf       [" + to_str(solverpns.ab) + "]\n"
			"  -l --lbdist   Initialize with the lower bound on distance to win       [" + to_str(solverpns.lbdist) + "]\n"
			);

	for(unsigned int i = 0; i < args.size(); i++) {
		string arg = args[i];

		if((arg == "-m" || arg == "--memory") && i+1 < args.size()){
			int mem = from_str<int>(args[++i]);
			if(mem < 1) return GTPResponse(false, "Memory can't be less than 1mb");
			solverpns.set_memlimit(mem);
		}else if((arg == "-s" || arg == "--ties") && i+1 < args.size()){
			solverpns.ties = from_str<int>(args[++i]);
			solverpns.clear_mem();
		}else if((arg == "-d" || arg == "--df") && i+1 < args.size()){
			solverpns.df = from_str<bool>(args[++i]);
		}else if((arg == "-e" || arg == "--epsilon") && i+1 < args.size()){
			solverpns.epsilon = from_str<float>(args[++i]);
		}else if((arg == "-a" || arg == "--abdepth") && i+1 < args.size()){
			solverpns.ab = from_str<int>(args[++i]);
		}else if((arg == "-l" || arg == "--lbdist") && i+1 < args.size()){
			solverpns.lbdist = from_str<bool>(args[++i]);
		}else{
			return GTPResponse(false, "Missing or unknown parameter");
		}
	}

	return true;
}

GTPResponse HavannahGTP::gtp_solve_pns_stats(vecstr args){
	string s = "";
	SolverPNS::PNSNode * child = solverpns.root.children.begin(),
	                   * childend = solverpns.root.children.end();
	for( ; child != childend; child++){
		if(child->move == M_NONE)
			continue;

		s += child->move.to_s() + "," + to_str(child->phi) + "," + to_str(child->delta) + "\n";
	}
	return GTPResponse(true, s);
}

GTPResponse HavannahGTP::gtp_solve_pns_clear(vecstr args){
	solverpns.clear_mem();
	return true;
}

GTPResponse HavannahGTP::gtp_solve_pnstt(vecstr args){
	double time = 60;

	if(args.size() >= 1)
		time = from_str<double>(args[0]);

	solverpnstt.solve(time);

	return GTPResponse(true, solve_str(solverpnstt));
}

GTPResponse HavannahGTP::gtp_solve_pnstt_params(vecstr args){
	if(args.size() == 0)
		return GTPResponse(true, string("\n") +
			"Update the pnstt solver settings, eg: pnstt_params -m 100 -s 0 -d 1 -e 0.25 -a 2 -l 0\n"
			"  -m --memory   Memory limit in Mb                                       [" + to_str(solverpnstt.memlimit) + "]\n"
			"  -s --ties     Which side to assign ties to, 0 = handle, 1 = p1, 2 = p2 [" + to_str(solverpnstt.ties) + "]\n"
//			"  -t --threads  How many threads to run
//			"  -o --ponder   Ponder in the background
			"  -d --df       Use depth-first thresholds                               [" + to_str(solverpnstt.df) + "]\n"
			"  -e --epsilon  How big should the threshold be                          [" + to_str(solverpnstt.epsilon) + "]\n"
			"  -a --abdepth  Run an alpha-beta search of this size at each leaf       [" + to_str(solverpnstt.ab) + "]\n"
//			"  -l --lbdist   Initialize with the lower bound on distance to win       [" + to_str(solverpnstt.lbdist) + "]\n"
			);

	for(unsigned int i = 0; i < args.size(); i++) {
		string arg = args[i];

		if((arg == "-m" || arg == "--memory") && i+1 < args.size()){
			int mem = from_str<int>(args[++i]);
			if(mem < 1) return GTPResponse(false, "Memory can't be less than 1mb");
			solverpnstt.set_memlimit(mem);
		}else if((arg == "-s" || arg == "--ties") && i+1 < args.size()){
			solverpnstt.ties = from_str<int>(args[++i]);
			solverpnstt.clear_mem();
		}else if((arg == "-d" || arg == "--df") && i+1 < args.size()){
			solverpnstt.df = from_str<bool>(args[++i]);
		}else if((arg == "-e" || arg == "--epsilon") && i+1 < args.size()){
			solverpnstt.epsilon = from_str<float>(args[++i]);
		}else if((arg == "-a" || arg == "--abdepth") && i+1 < args.size()){
			solverpnstt.ab = from_str<int>(args[++i]);
//		}else if((arg == "-l" || arg == "--lbdist") && i+1 < args.size()){
//			solverpnstt.lbdist = from_str<bool>(args[++i]);
		}else{
			return GTPResponse(false, "Missing or unknown parameter");
		}
	}

	return true;
}

GTPResponse HavannahGTP::gtp_solve_pnstt_stats(vecstr args){
	string s = "";

	Board board = game.getboard();
	SolverPNSTT::PNSNode * child = NULL;
	for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
		child = solverpnstt.tt(board, *move);

		s += move->to_s() + "," + to_str(child->phi) + "," + to_str(child->delta) + "\n";
	}
	return GTPResponse(true, s);
}

GTPResponse HavannahGTP::gtp_solve_pnstt_clear(vecstr args){
	solverpnstt.clear_mem();
	return true;
}

