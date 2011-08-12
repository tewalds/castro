

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

	solverab.solve(time);

	return GTPResponse(true, solve_str(solverab));
}

GTPResponse HavannahGTP::gtp_solve_ab_params(vecstr args){
	if(args.size() == 0)
		return GTPResponse(true, string("\n") +
			"Update the alpha-beta solver settings, eg: ab_params -m 100 -s 1 -d 3\n"
			"  -m --memory   Memory limit in Mb (0 to disable the TT)           [" + to_str(solverab.memlimit) + "]\n"
			"  -s --scout    Whether to scout ahead for the true minimax value  [" + to_str(solverab.scout) + "]\n"
			"  -d --depth    Starting depth                                     [" + to_str(solverab.startdepth) + "]\n"
			);

	for(unsigned int i = 0; i < args.size(); i++) {
		string arg = args[i];

		if((arg == "-m" || arg == "--memory") && i+1 < args.size()){
			int mem = from_str<int>(args[++i]);
			solverab.set_memlimit(mem);
		}else if((arg == "-s" || arg == "--scout") && i+1 < args.size()){
			solverab.scout = from_str<bool>(args[++i]);
		}else if((arg == "-d" || arg == "--depth") && i+1 < args.size()){
			solverab.startdepth = from_str<int>(args[++i]);
		}else{
			return GTPResponse(false, "Missing or unknown parameter");
		}
	}

	return true;
}

GTPResponse HavannahGTP::gtp_solve_ab_stats(vecstr args){
	string s = "";

	Board board = game.getboard();
	for(unsigned int i = 0; i < args.size(); i++)
		board.move(Move(args[i]));

	int value;
	for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
		value = solverab.tt_get(board.test_hash(*move));

		s += move->to_s() + "," + to_str(value) + "\n";
	}
	return GTPResponse(true, s);
}

GTPResponse HavannahGTP::gtp_solve_ab_clear(vecstr args){
	solverab.clear_mem();
	return true;
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
			"  -m --memory   Memory limit in Mb                                       [" + to_str(solverpns.memlimit/(1024*1024)) + "]\n"
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
			uint64_t mem = from_str<uint64_t>(args[++i]);
			if(mem < 1) return GTPResponse(false, "Memory can't be less than 1mb");
			solverpns.set_memlimit(mem*(1024*1024));
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

	SolverPNS::PNSNode * node = &(solverpns.root);

	for(unsigned int i = 0; i < args.size(); i++){
		Move m(args[i]);
		SolverPNS::PNSNode * c = node->children.begin(),
		                   * cend = node->children.end();
		for(; c != cend; c++){
			if(c->move == m){
				node = c;
				break;
			}
		}
	}

	SolverPNS::PNSNode * child = node->children.begin(),
	                   * childend = node->children.end();
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


GTPResponse HavannahGTP::gtp_solve_pns2(vecstr args){
	double time = 60;

	if(args.size() >= 1)
		time = from_str<double>(args[0]);

	solverpns2.solve(time);

	return GTPResponse(true, solve_str(solverpns2));
}

GTPResponse HavannahGTP::gtp_solve_pns2_params(vecstr args){
	if(args.size() == 0)
		return GTPResponse(true, string("\n") +
			"Update the pns solver settings, eg: pns_params -m 100 -s 0 -d 1 -e 0.25 -a 2 -l 0\n"
			"  -m --memory   Memory limit in Mb                                       [" + to_str(solverpns2.memlimit/(1024*1024)) + "]\n"
			"  -s --ties     Which side to assign ties to, 0 = handle, 1 = p1, 2 = p2 [" + to_str(solverpns2.ties) + "]\n"
			"  -t --threads  How many threads to run                                  [" + to_str(solverpns2.numthreads) + "]\n"
//			"  -o --ponder   Ponder in the background
			"  -d --df       Use depth-first thresholds                               [" + to_str(solverpns2.df) + "]\n"
			"  -e --epsilon  How big should the threshold be                          [" + to_str(solverpns2.epsilon) + "]\n"
			"  -a --abdepth  Run an alpha-beta search of this size at each leaf       [" + to_str(solverpns2.ab) + "]\n"
			"  -l --lbdist   Initialize with the lower bound on distance to win       [" + to_str(solverpns2.lbdist) + "]\n"
			);

	for(unsigned int i = 0; i < args.size(); i++) {
		string arg = args[i];

		if((arg == "-t" || arg == "--threads") && i+1 < args.size()){
			solverpns2.numthreads = from_str<int>(args[++i]);
			solverpns2.reset_threads();
		}else if((arg == "-m" || arg == "--memory") && i+1 < args.size()){
			uint64_t mem = from_str<uint64_t>(args[++i]);
			if(mem < 1) return GTPResponse(false, "Memory can't be less than 1mb");
			solverpns2.set_memlimit(mem*(1024*1024));
		}else if((arg == "-s" || arg == "--ties") && i+1 < args.size()){
			solverpns2.ties = from_str<int>(args[++i]);
			solverpns2.clear_mem();
		}else if((arg == "-d" || arg == "--df") && i+1 < args.size()){
			solverpns2.df = from_str<bool>(args[++i]);
		}else if((arg == "-e" || arg == "--epsilon") && i+1 < args.size()){
			solverpns2.epsilon = from_str<float>(args[++i]);
		}else if((arg == "-a" || arg == "--abdepth") && i+1 < args.size()){
			solverpns2.ab = from_str<int>(args[++i]);
		}else if((arg == "-l" || arg == "--lbdist") && i+1 < args.size()){
			solverpns2.lbdist = from_str<bool>(args[++i]);
		}else{
			return GTPResponse(false, "Missing or unknown parameter");
		}
	}

	return true;
}

GTPResponse HavannahGTP::gtp_solve_pns2_stats(vecstr args){
	string s = "";

	SolverPNS2::PNSNode * node = &(solverpns2.root);

	for(unsigned int i = 0; i < args.size(); i++){
		Move m(args[i]);
		SolverPNS2::PNSNode * c = node->children.begin(),
		                    * cend = node->children.end();
		for(; c != cend; c++){
			if(c->move == m){
				node = c;
				break;
			}
		}
	}

	SolverPNS2::PNSNode * child = node->children.begin(),
	                    * childend = node->children.end();
	for( ; child != childend; child++){
		if(child->move == M_NONE)
			continue;

		s += child->move.to_s() + "," + to_str(child->phi) + "," + to_str(child->delta) + "\n";
	}
	return GTPResponse(true, s);
}

GTPResponse HavannahGTP::gtp_solve_pns2_clear(vecstr args){
	solverpns2.clear_mem();
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
			"  -c --copy     Try to copy a proof to this many siblings, <0 quit early [" + to_str(solverpnstt.copyproof) + "]\n"
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
		}else if((arg == "-c" || arg == "--copy") && i+1 < args.size()){
			solverpnstt.copyproof = from_str<int>(args[++i]);
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
	for(unsigned int i = 0; i < args.size(); i++)
		board.move(Move(args[i]));

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

