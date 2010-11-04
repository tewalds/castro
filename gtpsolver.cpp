

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

GTPResponse HavannahGTP::gtp_solve_pns(vecstr args, int ab, bool df){
	double time = 60;
	int mem = mem_allowed;

	if(args.size() >= 1)
		time = from_str<double>(args[0]);

	if(args.size() >= 2)
		mem = from_str<int>(args[1]);

	SolverPNS solve(ab, df);
	solve.set_board(game.getboard());
	solve.set_memlimit(mem);
	solve.solve(time);

	return GTPResponse(true, solve_str(solve));
}

GTPResponse HavannahGTP::gtp_solve_pnstt(vecstr args, int ab, bool df){
	double time = 60;
	int mem = mem_allowed;

	if(args.size() >= 1)
		time = from_str<double>(args[0]);

	if(args.size() >= 2)
		mem = from_str<int>(args[1]);

	SolverPNSTT solve(ab, df);
	solve.set_board(game.getboard());
	solve.set_memlimit(mem);
	solve.solve(time);

	return GTPResponse(true, solve_str(solve));
}

