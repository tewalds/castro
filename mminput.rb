#!/usr/bin/ruby

class GTPClient
	def initialize(cmdline, newline = "\n")
		@io=IO.popen(cmdline,'w+')
		@sep = newline + newline
	end
	def cmd(c)
		return [true, ""] if c.strip == ""
		@io.puts c.strip
		res = @io.gets(@sep).strip.split(' ', 2)
#		puts "> #{c} ==> #{res.join ' '}"
		res[0] = (res[0] == '=')
		return res
	end
	def close
		@io.close
	end
end

size = (ARGV[0] || 5).to_i

gtp = GTPClient.new("./castro")

puts "! 4328"
puts "27"

puts "19 dist-p1"
puts "19 dist-p2"
puts "6 neighbours-dist-1-empty"
puts "6 neighbours-dist-1-p1"
puts "6 neighbours-dist-1-p2"
puts "6 neighbours-dist-1-edge"
puts "6 neighbours-dist-2-empty"
puts "6 neighbours-dist-2-p1"
puts "6 neighbours-dist-2-p2"
puts "6 neighbours-dist-2-edge"
puts "6 neighbours-vc-empty"
puts "6 neighbours-vc-p1"
puts "6 neighbours-vc-p2"
puts "6 neighbours-vc-edge"
puts "19 distwin-p1"
puts "19 distwin-p2"
puts "6 connect-p1"
puts "6 connect-p2"
puts "20 groupsize-p1"
puts "20 groupsize-p2"
puts "4 groups-p1"
puts "4 groups-p2"
puts "6 form1b-p1"
puts "6 form1b-p2"
puts "6 form2b-p1"
puts "6 form2b-p2"
puts "4096 6-pattern"
puts "!"

linenum = 0;
prefix = $0;

$stdin.each_line {|line|
	$0 = "#{prefix} - #{linenum}"
	linenum += 1;

	moves = line.strip.split

	gtp.cmd("boardsize #{size}")

	moves.each{|move|
		patterns = gtp.cmd("features")[1]
		patterns = Hash[patterns.strip.split("\n").map{|l| l.strip.split(" ", 2) }]

		puts "#"
		puts patterns[move]
		puts patterns.values

		gtp.cmd("playgame #{move}")
	}

	if(linenum % 100 == 0)
		gtp.cmd("quit")
		gtp.close
		gtp = GTPClient.new("./castro")
	end
}

gtp.cmd("quit")
gtp.close

