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

def timer
	start = Time.now
	yield
	return Time.now - start
end

require 'net/http'

expectedtime = 11.0 # expect 11 seconds
benchtime = timer { system($player, "-f", "test/speed4.tst"); }

puts "benchtime: " + benchtime.inspect

exit if benchtime < 1.0

time_factor = benchtime / expectedtime

puts "time_factor: " + time_factor.inspect


def loop_fork(concurrency, &block)
	if(concurrency == 1)
		loop &block
	end

	children = []

	loop {
		children << fork {
			loop {
				block.call
			}
			exit;
		}

		#if needed, wait for a previous process to exit
		if(concurrency)
			begin
				begin
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
			while(pid = Process.wait(-1, Process::WNOHANG))
				children.delete(pid);
			end
		end while(children.length > 0 && sleep(0.1))
	rescue SystemCallError
		children = []
	end

	return self
end



n = 0;
loop_fork($parallel) {
	n += 1;
	begin
		req = Net::HTTP.get(URI.parse("#{$url}/api/getwork"));

		req = req.strip.split("\n").map{|s| s.split(" ", 2) }
		req = Hash[*(req.flatten)]

		tp = req['tp'].split(" ")
		req['tp'] = "#{(tp[0].to_f*time_factor).to_f} #{(tp[1].to_f*time_factor).to_f} #{tp[2]}"

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

		fds = [nil];
		IO.popen($player, "w+"){|fd1|
			fds << fd1;

		IO.popen($player, "w+"){|fd2|
			fds << fd2;

			log = "";

			$cmds = [
				"boardsize #{req['sp']}",
				"time_settings #{req['tp']}",
				"player_params #{req['bp']}",
			]


			#send the initial commands
			$cmds.each{|cmd|
				puts cmd;
				log << cmd + "\n"

				fd1.write(cmd+"\n");
				fd1.gets; fd1.gets;

				fd2.write(cmd+"\n");
				fd2.gets; fd2.gets;
			}

			puts "player_params #{req['pp']}";
			fds[player].write("player_params #{req['pp']}\n");
			fds[player].gets
			fds[player].gets

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
					fds[turn].write("genmove #{turnstrings[turn]}\n");
					ret = fds[turn].gets.slice(2, 100).rstrip;
					fds[turn].gets;
				}
				puts ret

				break if(ret == "resign" || ret == "none")

				turn = 3-turn;

				#pass the move to the other player
				fds[turn].write("play #{turnstrings[3-turn]} #{ret}\n");
				fds[turn].gets;
				fds[turn].gets;

				log << "play #{turnstrings[3-turn]} #{ret}\n"
				log << "# took #{time} seconds\n\n"
			}
			}

			fd1.write("havannah_winner\n");
			ret = fd1.gets.slice(2, 100).rstrip;
			fd1.gets;
			turn = turnstrings.index(ret);

			log << "# Winner: #{ret}\n"
			log << "# Total time: #{totaltime} seconds\n"

			fd1.write("quit\n");
			fd2.write("quit\n");

			result = {
            	"baseline" => req['b'],
            	"player" => req['p'],
            	"size" => req['s'],
            	"time" => req['t'],
            	"outcome" => (turn == 0 ? turn : (turn == player ? 2 : 1)), # tie = 0, loss = 1, win = 2
            	"log" => log,
            	}

			res = Net::HTTP.post_form(URI.parse("#{$url}/api/submit"), result);
		}
		}
	rescue
		print "An error occurred: ",$!, "\n"
	end
}

