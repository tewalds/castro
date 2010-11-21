#!/usr/bin/ruby -W0

$sizes    = [4,5,6,7,8,9,10]
$parallel = 1
$memory   = 100
$time     = 5
$solver   = 'pnstt'
$params   = '-e 0 -a 1 -d 1'
$exec     = './castro'
$base     = 'solver'

	while(ARGV.length > 0)
		arg = ARGV.shift
		case arg
		when "-s", "--sizes"    then $sizes    = ARGV.shift.split(',').map{|a| a.to_i };
		when "-p", "--parallel" then $parallel = ARGV.shift.to_i;
		when "-m", "--memory"   then $memory   = ARGV.shift.to_i;
		when "-t", "--time"     then $time     = ARGV.shift.to_f;
		when "-o", "--solver"   then $solver   = ARGV.shift;
		when "-a", "--params"   then $params   = ARGV.shift;
		when "-e", "--exec"     then $exec     = ARGV.shift;
		when "-b", "--base"     then $base     = ARGV.shift;
		when "-h", "--help"     then
			puts "Generate tests for the solver"
			puts "Usage: #{$0} [<options>]"
			puts "  -s --sizes    Comma separated list of boardsizes   [#{$sizes.join(',')}]"
			puts "  -p --parallel Number of tests to run in parallel   [#{$parallel}]"
			puts "  -m --memory   Max memory for the player and solver [#{$memory}]"
			puts "  -t --time     Time limit per move or solve         [#{$time}]"
			puts "  -o --solver   Which solver to use                  [#{$solver}]"
			puts "  -a --params   Parameters for the solver            [#{$params}]"
			puts "  -e --exec     Program to run                       [#{$exec}]"
			puts "  -b --base     Where to save the tests              [#{$base}]"
			puts "  -h --help     Print this help"
			exit;
		else
			puts "Unknown argument: #{arg}"
			exit;
		end
	end

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

loop_fork($parallel){
	$sizes.each{|size|
		begin
			puts "Starting size #{size}"

			gtp = GTPClient.new($exec);
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
			gtp.cmd("time -r 0 -g 0 -m #{$time} -f 0")
			gtp.cmd("player_params -M #{$memory}");
			gtp.cmd("#{$solver}_params -m #{$memory} -e 0 -a 1");

			log = "hguicoords\nswap 0\nboardsize #{size}\nplaygame";

			loop{
				move = gtp.cmd("genmove")[2..-3] #trim off the = and newlines
				log << " " << move;

				solve = gtp.cmd("player_solved").split[1];
				if(solve != 'unknown')
					log << "\n#solved by player\n#solved as #{solve}\n"
					break
				end

				solve = gtp.cmd("#{$solver}_solve #{$time}").split[1];
				if(solve != 'unknown')
					log << "\n#solved by #{$solver}\n#solved as #{solve}\n"
					break
				end
			}

			gtp.cmd("quit");
			gtp.close

			log << "\n";

			num = nums[size];
			num += 1 while(File.exists?("#{$base}/#{size}/#{num}.tst"))
			File.open("#{$base}/#{size}/#{num}.tst", "w"){|f|
				f.write(log)
			}
			nums[size] = num+1;

			puts "------------------------------------------------------------------";
			puts "#{$base}/#{size}/#{num}.tst : "
			puts log
			puts "------------------------------------------------------------------";
		rescue => e
			p e.message
			p e.backtrace
		end
	}
}


