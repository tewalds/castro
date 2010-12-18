#!/usr/bin/ruby

	$against = false;
	$parallel = 1;
	$rounds = 1;
	$players = [];
	$time_per_game = 100;
	$time_per_move = 0;
	$max_runs = 0;
	$boardsize = 4;
	$players = [];
	$cmds = [];
	$msg = nil;
	$log = nil;

	while(ARGV.length > 0)
		arg = ARGV.shift
		case arg
		when "-a", "--against"  then $against       = true;
		when "-p", "--parallel" then $parallel      = ARGV.shift.to_i;
		when "-r", "--rounds"   then $rounds        = ARGV.shift.to_i;
		when "-g", "--timegame" then $time_per_game = ARGV.shift.to_f;
		when "-t", "--timemove" then $time_per_move = ARGV.shift.to_f;
		when "-m", "--maxruns"  then $max_runs      = ARGV.shift.to_i;
		when "-s", "--size"     then $boardsize     = ARGV.shift.to_i;
		when "-c", "--cmd"      then $cmds << ARGV.shift;
		when "-z", "--msg"      then $msg = ARGV.shift;
		when "-l", "--log"      then $log = ARGV.shift;
		when "-h", "--help"     then
			puts "Run a round robin tournament between players that all understand GTP."
			puts "Usage: #{$0} [<options>] players..."
			puts "  -a --against  The first player is the control and all others play it but not each other"
			puts "  -p --parallel Number of games to run in parallel [#{$parallel}]"
			puts "  -r --rounds   Number of rounds to play [#{$rounds}]"
			puts "  -g --timegame Time given per game [#{$time_per_game}]"
			puts "  -t --timemove Time given per move [#{$time_per_move}]"
			puts "  -m --maxruns  Simulations per move [#{$max_runs}]"
			puts "  -s --size     Board size [#{$boardsize}]"
			puts "  -c --cmd      Send an arbitrary GTP command at the beginning of the game"
			puts "  -z --msg      Output a message of what this is testing with the results"
			puts "  -l --log      Log the games to this directory, disabled by default"
			puts "  -h --help     Print this help"
			exit;
		else
			$players << arg
		end
	end

	$cmds << "time -g #{$time_per_game} -m #{$time_per_move} -i #{$max_runs} -f 0" if $time_per_game > 0 || $time_per_move > 0 || $max_runs > 0
	$cmds << "boardsize #{$boardsize}" if $boardsize > 0

	$num = $players.length();
	$num_games = 0;
	$interrupt = false;

trap("SIGTERM") { $interrupt = true; }
trap("SIGINT")  { $interrupt = true; }
trap("SIGHUP")  { $interrupt = true; }

require 'lib.rb'

def play_game(n, p1, p2)
#start the programs
	gtp = [nil, nil, nil];
	begin
		gtp[1] = GTPClient.new($players[p1])
		gtp[2] = GTPClient.new($players[p2])

		log = nil;
		if($log)
			log = File.open("#{$log}/#{n}-#{p1}-#{p2}", "w");
			log.write("# Game #{n} between  #{$players[p1]} vs #{$players[p2]}\n")
		end

	#send the initial commands
		$cmds.each{|cmd|
			puts cmd;
			log.write(cmd+"\n") if log

			gtp[1].cmd(cmd);
			gtp[2].cmd(cmd);
		}

		turnstrings = ["draw","white","black"];

		turn = 1;
		i = 1;
		ret = nil;
		totaltime = timer {
		loop{
			$0 = "Game #{n}/#{$num_games} move #{i}: #{$players[p1]} vs #{$players[p2]}"
			i += 1;

			#ask for a move
			print "genmove #{turnstrings[turn]}: ";
			time = timer {
				ret = gtp[turn].cmd("genmove #{turnstrings[turn]}\n")[2..-3];
			}
			puts ret

			break if(ret == "resign" || ret == "none")

			turn = 3-turn;

			#pass the move to the other player
			gtp[turn].cmd("play #{turnstrings[3-turn]} #{ret}");

			log.write("play #{turnstrings[3-turn]} #{ret}\n") if log
			log.write("# took #{time} seconds\n\n") if log
		}
		}

		ret = gtp[1].cmd("havannah_winner")[2..-3];
		turn = turnstrings.index(ret);

		log.write("# Winner: #{ret}, #{$players[turn]}\n") if log
		log.write("# Total time: #{totaltime} seconds\n") if log

		gtp[1].cmd("quit");
		gtp[2].cmd("quit");

		gtp[1].close
		gtp[2].close

		return turn;
	rescue
		gtp[1].close if gtp[1]
		gtp[2].close if gtp[2]
		return 0;
	end
end


#queue up the games
	$games = [];
	n = 0;
	if($against)
		(0...$rounds).each{|r|
			(1...$num).each{|i|
				$games << [n+1, 0, i]
				$games << [n+2, i, 0]
				n += 2;
			}
		}
	else
		(0...$rounds).each{|r|
			(0...$num).each{|i|
				(0...$num).each{|j|
					if(i != j)
						n += 1;
						$games << [n,i,j]
					end
				}
			}
		}
	end
	$num_games = n;

#play the games
	puts "Starting a tournament of #{$rounds} rounds and #{$num_games} games\n";
	if($time_per_game > 0 && $time_per_move == 0 && $max_runs == 0)
		expected_time = $num_games*2*$time_per_game/$parallel
		puts "Max expected time: #{expected_time} seconds, Ends before: " + (Time.now + expected_time).to_s;
	end
	puts "\n"

	time = timer {
		$outcomes = $games.map_fork($parallel){|n,i,j|
			result = ($interrupt ? 0 : play_game(n, i, j));
			[i,j,result]
		}
	}

#compute the results
	$results = []
	$num.times{ $results << [0]*$num }
	$outcomes.each{|i,j,result|
		if(result == 1)
			$results[i][j] += 1;
		elsif(result == 2)
			$results[j][i] += 1;
		end
	}
	
#start output
	out = "\n\n";
	out << "#{$msg}\n" if $msg;
	(0...$num).each{|i|
		out << "Player #{i+1}: #{$players[i]}\n";
	}
	puts "\n";

#win vs loss matrix
	out << "Win vs Loss Matrix:\n";
	out << "   ";
	(1..$num).each{|i| out << i.to_s.rjust(3) }
	out << "\n"

	(0...$num).each{|i|
		wins = 0;
		losses = 0;
		out << (i+1).to_s.rjust(2) + ":"
		(0...$num).each{|j|
			if(i == j)
				out << "   ";
			else
				out << $results[i][j].to_s.rjust(3);
				wins += $results[i][j];
				losses += $results[j][i];
			end
		}
		ties = ($against && i > 0 ? 1 : $num-1)*2*$rounds - wins - losses;
		out << " : #{wins} wins, #{losses} losses, #{ties} ties\n";
	}

	out << "Played #{$num_games} games, Total Time: #{time.to_i} s\n";

	print out

	if($log)
		File.open("#{$log}/players", "w"){|fp|
			$players.each{|p|
				fp.write("#{p}\n")
			}
		}

		File.open("#{$log}/results", "w"){|fp|
			$outcomes.each{|i,j,r| fp.write("#{i},#{j},#{r}\n") }
		}

		File.open("#{$log}/summary", "w"){|fp|
			fp.write(out);
		}
	end

