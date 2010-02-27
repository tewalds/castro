#!/usr/bin/ruby

	$parallel = 1;
	$rounds = 1;
	$players = [];
	$time_per_game = 100;
	$time_per_move = 0;
	$boardsize = 4;
	$players = [];
	$cmds = [];

	while(ARGV.length > 0)
		arg = ARGV.shift
		case arg
		when "-p", "--parallel" then $parallel      = ARGV.shift.to_i;
		when "-r", "--rounds"   then $rounds        = ARGV.shift.to_i;
		when "-g", "--timegame" then $time_per_game = ARGV.shift.to_f;
		when "-m", "--timemove" then $time_per_move = ARGV.shift.to_f;
		when "-s", "--size"     then $boardsize     = ARGV.shift.to_i;
		when "-c", "--cmd"      then $cmds << ARGV.shift;
		when "-h", "--help"     then
			puts "Run a round robin tournament between players that all understand GTP."
			puts "Usage: #{$0} [<options>] players..."
			puts "  -p --parallel Number of games to run in parallel [#{$parallel}]"
			puts "  -r --rounds   Number of rounds to play [#{$rounds}]"
			puts "  -g --timegame Time given per game [#{$time_per_game}]"
			puts "  -m --timemove Time given per move [#{$time_per_move}]"
			puts "  -s --size     Board size [#{$boardsize}]"
			puts "  -s --cmd      Send an arbitrary GTP command at the beginning of the game"
			puts "  -h --help     Print this help"
			exit;
		else
			$players << arg
		end
	end

	$cmds << "boardsize #{$boardsize}" if $boardsize > 0
	$cmds << "time_settings #{$time_per_game} #{$time_per_move} 0" if $time_per_game > 0 || $time_per_move > 0

	$num = $players.length();
	$num_games = $num*($num-1)*$rounds;



def play_game(p1, p2)
#start the programs
	fds = [];
	IO.popen($players[p1], "w+"){|fd1|
		fds << fd1;
	
	IO.popen($players[p2], "w+"){|fd2|
		fds << fd2;

	#send the initial commands
		$cmds.each{|cmd|
			puts cmd;
			
			fd1.write(cmd+"\n");
			fd1.gets; fd1.gets;
			
			fd2.write(cmd+"\n");
			fd2.gets; fd2.gets;
		}

		turnstrings = ["white","black"];

		turn = 0;
		loop{
			#ask for a move
			print "genmove #{turnstrings[turn]}: ";
			fds[turn].write("genmove #{turnstrings[turn]}\n");
			ret = fds[turn].gets.slice(2, 100).rstrip;
			fds[turn].gets;
			puts ret;

			turn = (turn+1)%2;

			break if(ret == "resign" || ret == "none")

			#pass the move to the other player
			fds[turn].write("play #{turnstrings[turn]} #{ret}\n");
			fds[turn].gets;
			fds[turn].gets;
		}

		fd1.write("quit\n");
		fd2.write("quit\n");

		return turn+1;
	}
	}
end

def timer
	start = Time.now
	yield
	return Time.now - start
end

class Array
	#map_fork runs the block once for each value, each in it's own process.
	# It takes a maximum concurrency, or nil for all at the same time
	# Clearly the blocks can't affect anything in the parent process.
	def map_fork(concurrency, &block)
		if(concurrency == 1)
			return self.map(&block);
		end

		children = []
		results = []
		socks = {}

		#create as a proc since it is called several times, 
		# but is not useful outside of this function, and needs the local scope.
		read_socks_func = proc {
			while(socks.length > 0 && (readsocks = IO::select(socks.keys, nil, nil, 0.1)))
				readsocks.first.each{|sock|
					rd = sock.read
					if(rd.nil? || rd.length == 0)
						results[socks[sock]] = Marshal.load(results[socks[sock]]);
						socks.delete(sock);
					else
						results[socks[sock]] ||= ""
						results[socks[sock]] += rd
					end
				}
			end
		}

		self.each_with_index {|val, index|
			rd, wr = IO.pipe

			children << fork {
				rd.close
				result = block.call(val)
				wr.write(Marshal.dump(result))
				wr.sync
				wr.close
				exit;
			}

			wr.close
			socks[rd] = index;

			#if needed, wait for a previous process to exit
			if(concurrency)
				begin
					begin
						read_socks_func.call
		
						while(pid = Process.wait(-1, Process::WNOHANG))
							children.delete(pid);
						end
					end while(children.length >= concurrency && sleep(0.1))
				rescue SystemCallError
					children = []
				end
			end
		}

		#wait for all processes to finish before returning
		begin
			begin
				read_socks_func.call

				while(pid = Process.wait(-1, Process::WNOHANG))
					children.delete(pid);
				end
			end while(children.length >= 0 && sleep(0.1))
		rescue SystemCallError
			children = []
		end


		read_socks_func.call

		return results
	end
end

	puts "Starting a tournament of #{$rounds} rounds and #{$num_games} games\n";

#queue up the games
	$games = [];
	n = 1;
	(0...$rounds).each{|r|
		(0...$num).each{|i|
			(0...$num).each{|j|
				if(i != j)
					$games << [n,i,j]
					n += 1;
				end
			}
		}
	}

#play the games
	time = timer {
		$outcomes = $games.map_fork($parallel){|n,i,j|
			puts "Game #{n}/#{$num_games}: #{$players[i]} vs #{$players[j]}\n";
			$0 = "Game #{n}/#{$num_games}: #{$players[i]} vs #{$players[j]}"

			result = play_game(i, j);

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
	puts "\n\n";
	(0...$num).each{|i|
		puts "Player #{i+1}: #{$players[i]}";
	}
	puts "\n";

#win vs loss matrix
	puts "Win vs Loss Matrix:";
	print "   ";
	(1..$num).each{|i| print i.to_s.rjust(3) }
	puts ""

	(0...$num).each{|i|
		wins = 0;
		losses = 0;
		print (i+1).to_s.rjust(2) + ":"
		(0...$num).each{|j|
			if(i == j)
				print "   ";
			else
				print $results[i][j].to_s.rjust(3);
				wins += $results[i][j];
				losses += $results[j][i];
			end
		}
		puts " : #{wins} wins, #{losses} losses, #{(($num-1)*2*$rounds-wins-losses)} ties";
	}

	puts "Played #{$num_games} games, Total Time: #{time.to_i} s";

