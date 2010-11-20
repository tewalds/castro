#!/usr/bin/ruby -W0

require 'lib.rb'

nums = {
	4 => 1,
	5 => 1,
	6 => 1,
	7 => 1,
	8 => 1,
	9 => 1,
	10 => 1,
}

loop{
	(4..10).each{|size|
		begin
			puts "Starting size #{size}"

			gtp = GTPClient.new("./castro");
			gtp.cmd("hguicoords")
			gtp.cmd("swap 0")
			gtp.cmd("boardsize #{size}")
			case size
				when 4 then gtp.cmd('player_params -g 1')
				when 6 then gtp.cmd('player_params -R 50')
				when 7 then gtp.cmd('player_params -R 60 -p 1')
				when 8 then gtp.cmd('player_params -R 70 -p 1')
				when 9 then gtp.cmd('player_params -R 100 -p 1')
				when 10 then gtp.cmd('player_params -R 130 -p 1')
			end
			gtp.cmd("time -r 0 -g 0 -m 5 -f 0")
			gtp.cmd("player_params -M 100");
			gtp.cmd("pnstt_params -m 100 -e 0 -a 1");

			log = "hguicoords\nswap 0\nboardsize #{size}\nplaygame";

			loop{
				move = gtp.cmd("genmove")
				move = move[2..-3] #trim off the = and newlines
				log << " " << move;

				solve = gtp.cmd("player_solved").split[1];
				if(solve != 'unknown')
					log << "\n#solved by player\n#solved as #{solve}\n"
					break
				end

				solve = gtp.cmd("pnstt_solve 5").split[1];
				if(solve != 'unknown')
					log << "\n#solved by solver\n#solved as #{solve}\n"
					break
				end
			}

			log << "\n";

			num = nums[size];
			num += 1 while(File.exists?("solver/#{size}/#{num}.tst"))
			File.open("solver/#{size}/#{num}.tst", "w"){|f|
				f.write(log)
			}
			nums[size] = num+1;

			puts "------------------------------------------------------------------";
			puts "solver/#{size}/#{num}.tst : "
			puts log
			puts "------------------------------------------------------------------";
		rescue => e
			p e.message
			p e.backtrace
		end
	}
}


