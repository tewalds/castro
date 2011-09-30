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

puts "! 4096"
puts "1"
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
		patterns = gtp.cmd("patterns")[1]
		patterns = Hash[patterns.strip.split("\n").map{|l| l.strip.split }]

		puts "#"
		puts patterns[move]
		puts patterns.values

		gtp.cmd("playgame #{move}")
	}
}

gtp.cmd("quit")
gtp.close

