#!/usr/bin/ruby

# queue runner used to solve size 4
# reads game histories from solver.work, keeps progress in solver.progress
# outputs complete histories to solver.complete
# outputs hgf files to /usr/data/tewalds/
# exits when solver.work is empty

# doesn't use file locking, so has a race condition, but this is unlikely to happen
# outputs move statistics once per hour

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

loop {
	work = File.open("solver.work"){|f| f.readlines.map{|a| a.strip }.select{|a| a.length > 0 } }
	break if work.length == 0
	moves = work.shift
	File.open("solver.work", "w"){|f| f.write(work.join("\n") + "\n") }

	progress = File.open("solver.progress"){|f| f.readlines.map{|a| a.strip }.select{|a| a.length > 0 } }
	progress.push moves + "," + `hostname`.strip + "," + Time.now.to_s
	File.open("solver.progress", "w"){|f| f.write(progress.join("\n") + "\n") }

	gtp = GTPClient.new("./castro")
	gtp.cmd("boardsize 4")
	gtp.cmd("playgame #{moves}")
	gtp.cmd("player_params -P 1 -t 8 -M 30000 --gcsolved 10000 -e 0 -f 500 -d 200 -g 1")
	gtp.cmd("player_params -T 1") if moves["c3"] || moves["d4"]

	puts gtp.cmd("print")[1]

	start = Time.now
	sims = 0;
	loop{
		$0 = "#{moves} - #{sims} - start: #{start}, cur: #{Time.now}"
		puts Time.now
		res = gtp.cmd("player_solve 3600")[1]
		puts res
		res = res.split
		sims += res[3].to_i
		puts gtp.cmd("move_stats")[1]
		break if res[0] != "unknown"
	}
	movename = moves.gsub(" ", "-")
	gtp.cmd("player_hgf /usr/data/tewalds/#{movename}-1000.hgf 1000")
	gtp.cmd("player_hgf /usr/data/tewalds/#{movename}-10000.hgf 10000")
	gtp.cmd("player_hgf /usr/data/tewalds/#{movename}-100000.hgf 100000")
	gtp.cmd("player_hgf /usr/data/tewalds/#{movename}-1000000.hgf 1000000")
	gtp.cmd("player_hgf /usr/data/tewalds/#{movename}-10000000.hgf 10000000")
	gtp.cmd("player_hgf /usr/data/tewalds/#{movename}-100000000.hgf 100000000")
	gtp.cmd("quit")
	gtp.close

	progress = File.open("solver.progress"){|f| f.readlines.map{|a| a.strip }.select{|a| a.length > 0 } }
	progress = progress.select{|a| a[moves] == nil }
	File.open("solver.progress", "w"){|f| f.write(progress.join("\n") + "\n") }

	complete = File.open("solver.complete"){|f| f.readlines.map{|a| a.strip }.select{|a| a.length > 0 } }
	complete.push moves + "," + `hostname`.strip + "," + Time.now.to_s
	File.open("solver.complete", "w"){|f| f.write(complete.join("\n") + "\n") }
}

puts "No work left!"

