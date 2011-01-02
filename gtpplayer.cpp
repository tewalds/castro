

#include "havannahgtp.h"


GTPResponse HavannahGTP::gtp_time(vecstr args){
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

double HavannahGTP::get_time(){
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

GTPResponse HavannahGTP::gtp_move_stats(vecstr args){
	string s = "";

	Player::Node * node = &(player.root);

	for(unsigned int i = 0; i < args.size(); i++){
		Move m(args[i]);
		Player::Node * c = node->children.begin(),
		             * cend = node->children.end();
		for(; c != cend; c++){
			if(c->move == m){
				node = c;
				break;
			}
		}
	}

	Player::Node * child = node->children.begin(),
	             * childend = node->children.end();
	for( ; child != childend; child++){
		if(child->move == M_NONE)
			continue;

		s += move_str(child->move, true);
		s += "," + to_str(child->exp.avg(), 2) + "," + to_str(child->exp.num());
		s += "," + to_str(child->rave.avg(), 2) + "," + to_str(child->rave.num());
		s += "," + to_str(child->know);
		if(child->outcome >= 0)
			s += "," + won_str(child->outcome);
		s += "\n";
	}
	return GTPResponse(true, s);
}

GTPResponse HavannahGTP::gtp_player_solve(vecstr args){
	double use_time = get_time();

	if(args.size() >= 1)
		use_time = from_str<double>(args[0]);

	logerr("time remain: " + to_str(time_remain, 1) + ", time: " + to_str(use_time, 3) + ", sims: " + to_str(time.max_sims) + "\n");

	player.rootboard.setswap(allow_swap);

	Player::Node * ret = player.genmove(use_time, time.max_sims);
	Move best = M_RESIGN;
	if(ret)
		best = ret->move;

	if(time.flexible)
		time_remain += time.move - player.time_used;
	else
		time_remain += min(0.0, time.move - player.time_used);

	if(time_remain < 0)
		time_remain = 0;

	int toplay = player.rootboard.toplay();

	DepthStats gamelen, treelen;
	int runs = 0;
	DepthStats wintypes[2][4];
	for(unsigned int i = 0; i < player.threads.size(); i++){
		gamelen += player.threads[i]->gamelen;
		treelen += player.threads[i]->treelen;
		runs += player.threads[i]->runs;

		for(int a = 0; a < 2; a++)
			for(int b = 0; b < 4; b++)
				wintypes[a][b] += player.threads[i]->wintypes[a][b];

		player.threads[i]->reset();
	}

	string stats = "Finished " + to_str(runs) + " runs in " + to_str(player.time_used*1000, 0) + " msec: " + to_str(runs/player.time_used, 0) + " Games/s\n";
	if(runs > 0){
		stats += "Game length: " + gamelen.to_s() + "\n";
		stats += "Tree depth:  " + treelen.to_s() + "\n";
		stats += "Win Types:   ";
		stats += "P1: f " + to_str(wintypes[0][1].num) + ", b " + to_str(wintypes[0][2].num) + ", r " + to_str(wintypes[0][3].num) + "; ";
		stats += "P2: f " + to_str(wintypes[1][1].num) + ", b " + to_str(wintypes[1][2].num) + ", r " + to_str(wintypes[1][3].num) + "\n";

		if(verbose){
			stats += "P1 fork:     " + wintypes[0][1].to_s() + "\n";
			stats += "P1 bridge:   " + wintypes[0][2].to_s() + "\n";
			stats += "P1 ring:     " + wintypes[0][3].to_s() + "\n";
			stats += "P2 fork:     " + wintypes[1][1].to_s() + "\n";
			stats += "P2 bridge:   " + wintypes[1][2].to_s() + "\n";
			stats += "P2 ring:     " + wintypes[1][3].to_s() + "\n";
		}
	}

	if(ret){
		stats += "Move Score:  " + to_str(ret->exp.avg()) + "\n";

		if(ret->outcome >= 0){
			stats += "Solved as a ";
			if(ret->outcome == toplay) stats += "win";
			else if(ret->outcome == 0) stats += "draw";
			else                       stats += "loss";
			stats += "\n";
		}
	}

	stats += "PV:          " + gtp_pv(vecstr()).response + "\n";

	if(verbose && !player.root.children.empty())
		stats += "Exp-Rave:\n" + gtp_move_stats(vecstr()).response + "\n";

	logerr(stats);

	Solver s;
	if(ret){
		s.outcome = (ret->outcome >= 0 ? ret->outcome : -3);
		s.bestmove = ret->move;
		s.maxdepth = gamelen.maxdepth;
		s.nodes_seen = runs;
	}else{
		s.outcome = 3-toplay;
		s.bestmove = M_RESIGN;
		s.maxdepth = 0;
		s.nodes_seen = 0;
	}

	return GTPResponse(true, solve_str(s));
}


GTPResponse HavannahGTP::gtp_player_solved(vecstr args){
	string s = "";
	Player::Node * child = player.root.children.begin(),
	             * childend = player.root.children.end();
	int toplay = player.rootboard.toplay();
	int best = 0;
	for( ; child != childend; child++){
		if(child->move == M_NONE)
			continue;

		if(child->outcome == toplay)
			return GTPResponse(true, won_str(toplay));
		else if(child->outcome < 0)
			best = 2;
		else if(child->outcome == 0)
			best = 1;
	}
	if(best == 2) return GTPResponse(true, won_str(-3));
	if(best == 1) return GTPResponse(true, won_str(0));
	return GTPResponse(true, won_str(3 - toplay));
}

GTPResponse HavannahGTP::gtp_pv(vecstr args){
	string pvstr = "";
	vector<Move> pv = player.get_pv();
	for(unsigned int i = 0; i < pv.size(); i++)
		pvstr += move_str(pv[i], true) + " ";
	return GTPResponse(true, pvstr);
}

GTPResponse HavannahGTP::gtp_genmove(vecstr args){
	if(player.rootboard.won() >= 0)
		return GTPResponse(true, "resign");

	double use_time = get_time();

	if(args.size() >= 2)
		use_time = from_str<double>(args[1]);

	logerr("time remain: " + to_str(time_remain, 1) + ", time: " + to_str(use_time, 3) + ", sims: " + to_str(time.max_sims) + "\n");

	player.rootboard.setswap(allow_swap);

	Player::Node * ret = player.genmove(use_time, time.max_sims);
	Move best = player.root.bestmove;

	if(time.flexible)
		time_remain += time.move - player.time_used;
	else
		time_remain += min(0.0, time.move - player.time_used);

	if(time_remain < 0)
		time_remain = 0;

	log("#genmove " + implode(args, " "));
	log(string("play ") + (game.toplay() == 2 ? 'w' : 'b') + ' ' + move_str(best, false));

	int toplay = player.rootboard.toplay();

	DepthStats gamelen, treelen;
	int runs = 0;
	DepthStats wintypes[2][4];
	for(unsigned int i = 0; i < player.threads.size(); i++){
		gamelen += player.threads[i]->gamelen;
		treelen += player.threads[i]->treelen;
		runs += player.threads[i]->runs;

		for(int a = 0; a < 2; a++)
			for(int b = 0; b < 4; b++)
				wintypes[a][b] += player.threads[i]->wintypes[a][b];

		player.threads[i]->reset();
	}

	string stats = "Finished " + to_str(runs) + " runs in " + to_str(player.time_used*1000, 0) + " msec: " + to_str(runs/player.time_used, 0) + " Games/s\n";
	if(runs > 0){
		stats += "Game length: " + gamelen.to_s() + "\n";
		stats += "Tree depth:  " + treelen.to_s() + "\n";
		stats += "Win Types:   ";
		stats += "P1: f " + to_str(wintypes[0][1].num) + ", b " + to_str(wintypes[0][2].num) + ", r " + to_str(wintypes[0][3].num) + "; ";
		stats += "P2: f " + to_str(wintypes[1][1].num) + ", b " + to_str(wintypes[1][2].num) + ", r " + to_str(wintypes[1][3].num) + "\n";

		if(verbose){
			stats += "P1 fork:     " + wintypes[0][1].to_s() + "\n";
			stats += "P1 bridge:   " + wintypes[0][2].to_s() + "\n";
			stats += "P1 ring:     " + wintypes[0][3].to_s() + "\n";
			stats += "P2 fork:     " + wintypes[1][1].to_s() + "\n";
			stats += "P2 bridge:   " + wintypes[1][2].to_s() + "\n";
			stats += "P2 ring:     " + wintypes[1][3].to_s() + "\n";
		}
	}

	if(ret)
		stats += "Move Score:  " + to_str(ret->exp.avg()) + "\n";

	if(player.root.outcome != -3){
		stats += "Solved as a ";
		if(player.root.outcome == 0)             stats += "draw";
		else if(player.root.outcome == toplay)   stats += "win";
		else if(player.root.outcome == 3-toplay) stats += "loss";
		else if(player.root.outcome == -toplay)  stats += "win or draw";
		else if(player.root.outcome == toplay-3) stats += "loss or draw";
		stats += "\n";
	}

	stats += "PV:          " + gtp_pv(vecstr()).response + "\n";

	if(verbose && !player.root.children.empty())
		stats += "Exp-Rave:\n" + gtp_move_stats(vecstr()).response + "\n";

	logerr(stats);

	game.move(best);
	player.move(best);
	solverab.move(best);
	solverpns.move(best);
	solverpnstt.move(best);

	return GTPResponse(true, move_str(best));
}

GTPResponse HavannahGTP::gtp_player_params(vecstr args){
	if(args.size() == 0)
		return GTPResponse(true, string("\n") +
			"Set player parameters, eg: player_params -e 1 -f 0 -t 2 -o 1 -p 0\n" +
			"Processing:\n" +
#ifndef SINGLE_THREAD
			"  -t --threads     Number of MCTS threads                            [" + to_str(player.numthreads) + "]\n" +
#endif
			"  -o --ponder      Continue to ponder during the opponents time      [" + to_str(player.ponder) + "]\n" +
			"  -M --maxmem      Max memory in Mb to use for the tree              [" + to_str(player.maxmem/(1024*1024)) + "]\n" +
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
			"  -O --rollouts    Number of rollouts to run per simulation          [" + to_str(player.rollouts) + "]\n" +
			"  -I --dynwiden    Dynamic widening, consider log_wid(exp) children  [" + to_str(player.dynwiden) + "]\n" +
			"Tree building:\n" +
			"  -s --shortrave   Only use moves from short rollouts for rave       [" + to_str(player.shortrave) + "]\n" +
			"  -k --keeptree    Keep the tree from the previous move              [" + to_str(player.keeptree) + "]\n" +
			"  -m --minimax     Backup the minimax proof in the UCT tree          [" + to_str(player.minimax) + "]\n" +
			"  -T --detectdraw  Detect draws once no win is possible at all       [" + to_str(player.detectdraw) + "]\n" +
			"  -x --visitexpand Number of visits before expanding a node          [" + to_str(player.visitexpand) + "]\n" +
			"  -P --symmetry    Prune symmetric moves, good for proof, not play   [" + to_str(player.prunesymmetry) + "]\n" +
			"  -L --logproof    Log proven nodes hashes and outcomes to this file [" + player.solved_logname + "]\n" +
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

	string errs;
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
			player.maxmem = from_str<uint64_t>(args[++i])*1024*1024;
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
		}else if((arg == "-T" || arg == "--detectdraw") && i+1 < args.size()){
			player.detectdraw = from_str<bool>(args[++i]);
		}else if((arg == "-P" || arg == "--symmetry") && i+1 < args.size()){
			player.prunesymmetry = from_str<bool>(args[++i]);
		}else if((arg == "-L" || arg == "--logproof") && i+1 < args.size()){
			if(player.setlogfile(args[++i]) == 0)
				errs += "Can't set the log file\n";
		}else if((arg == "-r" || arg == "--userave") && i+1 < args.size()){
			player.userave = from_str<float>(args[++i]);
		}else if((arg == "-X" || arg == "--useexplore") && i+1 < args.size()){
			player.useexplore = from_str<float>(args[++i]);
		}else if((arg == "-u" || arg == "--fpurgency") && i+1 < args.size()){
			player.fpurgency = from_str<float>(args[++i]);
		}else if((arg == "-O" || arg == "--rollouts") && i+1 < args.size()){
			player.rollouts = from_str<int>(args[++i]);
		}else if((arg == "-I" || arg == "--dynwiden") && i+1 < args.size()){
			player.dynwiden = from_str<float>(args[++i]);
			player.logdynwiden = std::log(player.dynwiden);
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
	return GTPResponse(true, errs);
}

