#!/usr/bin/ruby -W0

$sizes  = [4]
$num    = 10 
$time   = 10
$memory = 100
$parallel = 1
$solver = 'pns'
$params = '-e 0 -a 1 -d 1'
$exec   = './castro'

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
		when "-h", "--help"     then
			puts "Run a solver over a bunch of test cases to "
			puts "Usage: #{$0} [<options>]"
			puts "  -s --sizes    Comma separated list of boardsizes to test [#{$sizes.join(',')}]"
			puts "  -n --num      The number of tests to run per board size  [#{$num}]"
			puts "  -p --parallel Number of tests to run in parallel         [#{$parallel}]"
			puts "  -m --memory   Max memory that the solver can use         [#{$memory}]"
			puts "  -t --time     Time limit per solve                       [#{$time}]"
			puts "  -o --solver   Which solver to use                        [#{$solver}]"
			puts "  -a --params   Parameters for the solver                  [#{$params}]"
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
		tests << "solver/#{size}/#{n}.tst"
	}
}

results = tests.map_fork($parallel){|test|
	gtp = GTPClient.new("#{$exec} -f #{test}")
	gtp.cmd("#{$solver}_params #{$params} -m #{$memory}")

	result = nil;
	time = timer { result = gtp.cmd("#{$solver}_solve #{$time}")[2..-3] }
	gtp.cmd("quit")
	gtp.close

	[test, time, *(result.split)]
}

solved = 0
total = 0
solvedtime = 0.0
totaltime = 0.0

results.each{|result|
	puts result.join(' ')

	total += 1
	totaltime += result[1]

	if(result[2] != 'unknown')
		solved += 1
		solvedtime += result[1]
	end
}

puts "           solved    total"
puts "problems #{solved.to_s.rjust(8)} #{total.to_s.rjust(8)}"
puts "time sec #{solvedtime.round(1).to_s.rjust(8)} #{totaltime.round(1).to_s.rjust(8)}"

