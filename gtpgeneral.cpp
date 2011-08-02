
#include "havannahgtp.h"
#include "lbdist.h"

GTPResponse HavannahGTP::gtp_echo(vecstr args){
	return GTPResponse(true, implode(args, " "));
}

GTPResponse HavannahGTP::gtp_print(vecstr args){
	return GTPResponse(true, "\n" + game.getboard().to_s(colorboard));
}

string HavannahGTP::won_str(int outcome) const {
	switch(outcome){
		case -3: return "none";
		case -2: return "black_or_draw";
		case -1: return "white_or_draw";
		case 0:  return "draw";
		case 1:  return "white";
		case 2:  return "black";
		default: return "unknown";
	}
}

GTPResponse HavannahGTP::gtp_swap(vecstr args){
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

GTPResponse HavannahGTP::gtp_boardsize(vecstr args){
	if(args.size() != 1)
		return GTPResponse(false, "Current board size: " + to_str(game.getsize()));

	log("boardsize " + args[0]);

	int size = from_str<int>(args[0]);
	if(size < 3 || size > 10)
		return GTPResponse(false, "Size " + to_str(size) + " is out of range.");

	game = HavannahGame(size);
	player.set_board(game.getboard());
	solverab.set_board(game.getboard());
	solverpns.set_board(game.getboard());
	solverpns2.set_board(game.getboard());
	solverpnstt.set_board(game.getboard());

	time_remain = time.game;

	return GTPResponse(true);
}

GTPResponse HavannahGTP::gtp_clearboard(vecstr args){
	game.clear();
	player.set_board(game.getboard());
	solverab.set_board(game.getboard());
	solverpns.set_board(game.getboard());
	solverpns2.set_board(game.getboard());
	solverpnstt.set_board(game.getboard());

	time_remain = time.game;

	log("clear_board");
	return GTPResponse(true);
}

GTPResponse HavannahGTP::gtp_undo(vecstr args){
	int num = 1;
	if(args.size() >= 1)
		num = from_str<int>(args[0]);

	while(num--){
		game.undo();
		log("undo");
	}
	player.set_board(game.getboard());
	solverab.set_board(game.getboard());
	solverpns.set_board(game.getboard());
	solverpns2.set_board(game.getboard());
	solverpnstt.set_board(game.getboard(), false);
	if(verbose >= 2)
		logerr(game.getboard().to_s(colorboard) + "\n");
	return GTPResponse(true);
}

Move HavannahGTP::parse_move(const string & str){
	return Move(str, !hguicoords * game.getsize());
}

string HavannahGTP::move_str(int x, int y, int hguic){
	return move_str(Move(x, y), hguic);
}

string HavannahGTP::move_str(Move m, int hguic){
	if(hguic == -1)
		hguic = hguicoords;

	return m.to_s(!hguic * game.getsize());
}

GTPResponse HavannahGTP::gtp_all_legal(vecstr args){
	string ret;
	for(Board::MoveIterator move = game.getboard().moveit(); !move.done(); ++move)
		ret += move_str(*move) + " ";
	return GTPResponse(true, ret);
}

GTPResponse HavannahGTP::gtp_history(vecstr args){
	string ret;
	vector<Move> hist = game.get_hist();
	for(unsigned int i = 0; i < hist.size(); i++)
		ret += move_str(hist[i]) + " ";
	return GTPResponse(true, ret);
}

GTPResponse HavannahGTP::play(const string & pos, int toplay){
	if(toplay != game.toplay())
		return GTPResponse(false, "It is the other player's turn!");

	if(game.getboard().won() >= 0)
		return GTPResponse(false, "The game is already over.");

	Move move = parse_move(pos);

	if(!game.valid(move))
		return GTPResponse(false, "Invalid move");

	game.move(move);
	player.move(move);
	solverab.move(move);
	solverpns.move(move);
	solverpns2.move(move);
	solverpnstt.move(move);

	log(string("play ") + (toplay == 1 ? 'w' : 'b') + ' ' + move_str(move, false));

	if(verbose >= 2)
		logerr("Placement: " + move_str(move) + ", outcome: " + game.getboard().won_str() + "\n" + game.getboard().to_s(colorboard));

	return GTPResponse(true);
}

GTPResponse HavannahGTP::gtp_playgame(vecstr args){
	GTPResponse ret(true);

	for(unsigned int i = 0; ret.success && i < args.size(); i++)
		ret = play(args[i], game.toplay());

	return ret;
}

GTPResponse HavannahGTP::gtp_play(vecstr args){
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

GTPResponse HavannahGTP::gtp_playwhite(vecstr args){
	if(args.size() != 1)
		return GTPResponse(false, "Wrong number of arguments");

	return play(args[0], 1);
}

GTPResponse HavannahGTP::gtp_playblack(vecstr args){
	if(args.size() != 1)
		return GTPResponse(false, "Wrong number of arguments");

	return play(args[0], 2);
}

GTPResponse HavannahGTP::gtp_winner(vecstr args){
	log("havannah_winner");
	return GTPResponse(true, game.getboard().won_str());
}

GTPResponse HavannahGTP::gtp_name(vecstr args){
	return GTPResponse(true, "Castro");
}

GTPResponse HavannahGTP::gtp_version(vecstr args){
	return GTPResponse(true, "0.1");
}

GTPResponse HavannahGTP::gtp_verbose(vecstr args){
	if(args.size() >= 1)
		verbose = from_str<int>(args[0]);
	else
		verbose = !verbose;
	return GTPResponse(true, "Verbose " + to_str(verbose));
}

GTPResponse HavannahGTP::gtp_colorboard(vecstr args){
	if(args.size() >= 1)
		colorboard = from_str<int>(args[0]);
	else
		colorboard = !colorboard;
	return GTPResponse(true, "Color " + to_str(colorboard));
}

GTPResponse HavannahGTP::gtp_extended(vecstr args){
	if(args.size() >= 1)
		genmoveextended = from_str<bool>(args[0]);
	else
		genmoveextended = !genmoveextended;
	return GTPResponse(true, "extended " + to_str(genmoveextended));
}

GTPResponse HavannahGTP::gtp_hguicoords(vecstr args){
	hguicoords = true;
	return GTPResponse(true);
}
GTPResponse HavannahGTP::gtp_gridcoords(vecstr args){
	hguicoords = false;
	return GTPResponse(true);
}

GTPResponse HavannahGTP::gtp_debug(vecstr args){
	string str = "\n";
	str += "Board size:  " + to_str(game.getboard().get_size()) + "\n";
	str += "Board cells: " + to_str(game.getboard().numcells()) + "\n";
	str += "Board vec:   " + to_str(game.getboard().vecsize()) + "\n";
	str += "Board mem:   " + to_str(game.getboard().memsize()) + "\n";

	return GTPResponse(true, str);
}

GTPResponse HavannahGTP::gtp_dists(vecstr args){
	Board board = game.getboard();
	LBDists dists(&board);

	int side = 0;
	if(args.size() >= 1){
		switch(tolower(args[0][0])){
			case 'w': side = 1; break;
			case 'b': side = 2; break;
			default:
				return GTPResponse(false, "Invalid player selection");
		}
	}

	int size = board.get_size();
	int size_d = board.get_size_d();

	string s = "\n";
	s += string(size + 3, ' ');
	for(int i = 0; i < size; i++)
		s += " " + to_str(i+1);
	s += "\n";

	string white = "O", black = "@";
	if(colorboard){
		string esc = "\033", reset = esc + "[0m";
		white = esc + "[1;33m" + "@" + reset; //yellow
		black = esc + "[1;34m" + "@" + reset; //blue
	}

	for(int y = 0; y < size_d; y++){
		s += string(abs(size-1 - y) + 2, ' ');
		s += char('A' + y);
		for(int x = board.linestart(y); x < board.lineend(y); x++){
			int p = board.get(x, y);
			s += ' ';
			if(p == 0){
				int d = (side ? dists.get(Move(x, y), side) : dists.get(Move(x, y)));
				if(d < 10)
					s += to_str(d);
				else
					s += '.';
			}else if(p == 1){
				s += white;
			}else if(p == 2){
				s += black;
			}
		}
		if(y < size-1)
			s += " " + to_str(1 + size + y);
		s += '\n';
	}
	return GTPResponse(true, s);
}

