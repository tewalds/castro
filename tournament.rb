#!/usr/bin/ruby

	require 'array'

	$parallel = 2;
	$rounds = 1;
	$players = [
		"./castro",
		"./castro-rave-1",
	];
	
	$cmds = [
		"boardsize 4", 
		"time_settings 0 1 0"
	];

#####################################

	$num = $players.length();
	$num_games = $num*($num-1)*$rounds;



def play_game(p1, p2)

	fds = [];
	IO.popen($players[p1], "w+"){|fd1|
		fds << fd1;
	
	IO.popen($players[p2], "w+"){|fd2|
		fds << fd2;
		
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
			print "genmove #{turnstrings[turn]}: ";
			fds[turn].write("genmove #{turnstrings[turn]}\n");
			ret = fds[turn].gets.slice(2, 100).rstrip;
			fds[turn].gets;
			puts ret;

			turn = (turn+1)%2;

			break if(ret == "resign" || ret == "none")

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



	puts "Starting a tournament of #{$rounds} rounds and #{$num_games} games\n";

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

	$outcomes = $games.map_fork($parallel){|n,i,j|
		puts "Game #{n}/#{$num_games}: #{$players[i]} vs #{$players[j]}\n";
		result = play_game(i, j);

		[i,j,result]
	}

	$results = [[0]*$num]*$num
	$outcomes.each{|i,j,result|
		if(result == 1)
			$results[i][j] += 1;
		elsif(result == 2)
			$results[j][i] += 1;
		end
	}
	
##############
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

#	echo "Played $num_games games, Total Time: " . number_format($endtime - $time) . " s, Average Time: " . number_format(($endtime - $time)/$num_games) . " s\n";


