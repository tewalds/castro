#!/usr/bin/ruby

	$parallel = 1;
	$time_limit = 0;
	$player = "./castro"
	$url = "http://havannah.ewalds.ca";

	while(ARGV.length > 0)
		arg = ARGV.shift
		case arg
		when "-p", "--parallel"  then $parallel   = ARGV.shift.to_i;
		when "-t", "--timelimit" then $time_limit = ARGV.shift.to_f;
		when "-u", "--url"       then $url        = ARGV.shirt;
		when "-h", "--help"      then
			puts "Run a worker process that plays games between players that understand GTP"
			puts "based on a database of players, baselines, sizes and times"
			puts "Usage: #{$0} [<options>]"
			puts "  -p --parallel   Number of games to run in parallel [#{$parallel}]"
			puts "  -t --timelimit  Run the worker for this long before exiting [#{$time_limit}]"
			puts "  -u --url        Get work to do from this url [#{$url}]"
			puts "  -h --help       Print this help"
			exit;
		else
			puts "Unknown argument #{arg}"
			exit;
		end
	end


require 'lib.rb'
require 'net/http'

expectedtime = 11.0 # expect 11 seconds
benchtime = timer { system($player, "-f", "test/speed4.tst"); }

puts "benchtime: " + benchtime.inspect

exit if benchtime < 1.0

time_factor = benchtime / expectedtime

puts "time_factor: " + time_factor.inspect


n = 0;
loop_fork($parallel) {
	n += 1;
	gtp = [nil, nil, nil]
	begin
		req = Net::HTTP.get(URI.parse("#{$url}/api/getwork"));

		req = req.strip.split("\n").map{|s| s.split(" ", 2) }
		req = Hash[*(req.flatten)]

		tp = req['tp'].split(" ")
		req['tp'] = "-g #{(tp[0].to_f*time_factor).to_f} -m #{(tp[1].to_f*time_factor).to_f} -s #{tp[2]}"

puts req.inspect

#		baselines.baseline AS b,
#		baselines.params AS bp,
#		players.player AS p,
#		players.params AS pp,
#		sizes.size AS s,
#		sizes.params AS sp,
#		times.time AS t,
#		times.params AS tp

		player = rand(2) + 1;

		gtp[1] = GTPClient.new($player)
		gtp[2] = GTPClient.new($player)


		log = "";

		$cmds = [
			"time #{req['tp']}",
			"boardsize #{req['sp']}",
			"player_params #{req['bp']}",
		]


		#send the initial commands
		$cmds.each{|cmd|
			puts cmd;
			log << cmd + "\n"

			gtp[1].cmd(cmd);
			gtp[2].cmd(cmd);
		}

		puts "player_params #{req['pp']}";
		gtp[player].cmd("player_params #{req['pp']}");

		turnstrings = ["draw","white","black"];

		turn = 1;
		i = 1;
		ret = nil;
		totaltime = timer {
		loop{
			$0 = "Game #{n} move #{i}, size #{req['sp']}"
			i += 1;

			#ask for a move
			print "genmove #{turnstrings[turn]}: ";
			time = timer {
				ret = gtp[turn].cmd("genmove #{turnstrings[turn]}")[2..-3];
			}
			puts ret

			break if(ret == "resign" || ret == "none")

			turn = 3-turn;

			#pass the move to the other player
			gtp[turn].cmd("play #{turnstrings[3-turn]} #{ret}");

			log << "play #{turnstrings[3-turn]} #{ret}\n"
			log << "# took #{time} seconds\n\n"
		}
		}

		ret = gtp[1].cmd("havannah_winner")[2..-3];
		turn = turnstrings.index(ret);

		log << "# Winner: #{ret}\n"
		log << "# Total time: #{totaltime} seconds\n"

		gtp[1].cmd("quit");
		gtp[2].cmd("quit");

		gtp[1].close;
		gtp[2].close;

		result = {
			"baseline" => req['b'],
			"player" => req['p'],
			"size" => req['s'],
			"time" => req['t'],
			"outcome" => (turn == 0 ? turn : (turn == player ? 2 : 1)), # tie = 0, loss = 1, win = 2
			"log" => log,
		}

		res = Net::HTTP.post_form(URI.parse("#{$url}/api/submit"), result);
	rescue
		gtp[1].close if gtp[1]
		gtp[2].close if gtp[2]
		print "An error occurred: ",$!, "\n"
		sleep(1);
	end
}

