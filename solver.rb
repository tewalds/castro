#!/usr/bin/ruby -W0

$sizes  = [4]
$num    = 10 
$time   = 10
$memory = 100
$parallel = 1
$solver = 'pns'
$params = '-e 0 -a 1 -d 1'
$exec   = './castro'
$base   = 'solver'

	while(ARGV.length > 0)
		arg = ARGV.shift
		case arg
		when "-s", "--sizes"    then $sizes    = ARGV.shift.split(',').map{|a| a.to_i };
		when "-n", "--num"      then $num      = ARGV.shift.to_i;
		when "-m", "--memory"   then $memory   = ARGV.shift.to_i;
		when "-p", "--parallel" then $parallel = ARGV.shift.to_i;
		when "-t", "--time"     then $time     = ARGV.shift.to_f;
		when "-o", "--solver"   then $solver   = ARGV.shift;
		when "-a", "--params"   then $params   = ARGV.shift;
		when "-e", "--exec"     then $exec     = ARGV.shift;
		when "-b", "--base"     then $base     = ARGV.shift;
		when "-h", "--help"     then
			puts "Run a solver over a bunch of test cases"
			puts "Usage: #{$0} [<options>]"
			puts "  -s --sizes    Comma separated list of boardsizes to test [#{$sizes.join(',')}]"
			puts "  -n --num      The number of tests to run per board size  [#{$num}]"
			puts "  -p --parallel Number of tests to run in parallel         [#{$parallel}]"
			puts "  -m --memory   Max memory that the solver can use         [#{$memory}]"
			puts "  -t --time     Time limit per solve                       [#{$time}]"
			puts "  -o --solver   Which solver to use                        [#{$solver}]"
			puts "  -a --params   Parameters for the solver                  [#{$params}]"
			puts "  -e --exec     Program to run                             [#{$exec}]"
			puts "  -b --base     Which test set to run                      [#{$base}]"
			puts "  -h --help     Print this help"
			exit;
		else
			puts "Unknown argument: #{arg}"
			exit;
		end
	end

require 'lib.rb'

tests = []

$sizes.each{|size|
	(1..$num).each{|n|
		tests << "#{$base}/#{size}/#{n}.tst"
	}
}

results = tests.map_fork($parallel){|test|
	$stderr.puts test

	result = 'unknown unknown 0 0'
	time = $time
	begin
		gtp = GTPClient.new("#{$exec} -f #{test}")
		sleep(0.1)
		gtp.cmd("#{$solver}_params #{$params} -m #{$memory}")

		time = timer { result = gtp.cmd("#{$solver}_solve #{$time}")[2..-3] }
		gtp.cmd("quit")
		gtp.close
	rescue
		gtp.close
	end

	[test, time, *(result.split)]
}

#[test, time, outcome, move, depth, states]

problems = [0, 0]
time     = [0.0, 0.0]
states   = [0, 0]

results.each{|result|
	puts result.join(' ')

	problems[0] += 1
	time[0]     += result[1]
	states[0]   += result[5].to_i

	if(result[2] != 'unknown')
		problems[1] += 1
		time[1]     += result[1]
		states[1]   += result[5].to_i
	end
}

puts
puts "             solved      total"
puts "problems #{problems[1].to_s.rjust(10)} #{problems[0].to_s.rjust(10)}"
puts "time sec #{time[1].round(1).to_s.rjust(10)} #{time[0].round(1).to_s.rjust(10)}"
puts "states   #{states[1].to_s.rjust(10)} #{states[0].to_s.rjust(10)}"

