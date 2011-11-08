#!/usr/bin/ruby


$infile = nil  # read an mminput file
$outfile = nil # output the mapped file to here
$mapfile = nil # output the mapping to here
$maxpatterns = nil # limit the number of patterns to this, increasing freqlimit if needed
$freqlimit = 100 # keep all patterns that show up at least this many times
$keep = false  # should patterns below this frequency be replaced by a generic pattern or just removed



while(ARGV.length > 0)
	arg = ARGV.shift
	case arg
	when "-k", "--keep"     then $keep          = true;
	when "-n", "--num"      then $maxpatterns   = ARGV.shift.to_i;
	when "-f", "--freq"     then $freqlimit     = ARGV.shift.to_i;
	when "-i", "--infile"   then $infile        = ARGV.shift
	when "-o", "--outfile"  then $outfile       = ARGV.shift
	when "-m", "--mapfile"  then $mapfile       = ARGV.shift
	when "-h", "--help"     then
		puts "Map an mm input file to and have a smaller set of patterns."
		puts "Usage: #{$0} [-k] [-n num] [-e exp] -i infile -o outfile -m mapfile"
		puts "  -k --keep     Replace the low frequency patterns with a generic pattern"
		puts "  -n --num      Limit the number of patterns to this many"
		puts "  -f --freq     Only keep patterns with this frequency or greater [#{$freqlimit}]"
		puts "  -i --infile   Input file"
		puts "  -o --outfile  Output file"
		puts "  -m --mapfile  Output mapping between patterns and pattern number"
		puts "  -h --help     Print this help"
		exit;
	else
		puts "Unknown argument #{arg}"
		exit
	end
end

if !$infile || !$outfile || !$mapfile
	puts "Missing arguments, -h for help"
	exit
end


freq = Hash.new(0)

passedheader = false
File.open($infile){|infile|
	infile.each_line {|line|
		line = line.strip

		if line == "!"
			passedheader = true
			next
		end
		next unless passedheader
		next if line == "#"

		freq[line.to_i] += 1;
	}
}

arr = freq.to_a
arr = arr.sort{|a, b| -(a[1] <=> b[1]) }
arr = arr.select{|a| a[1] >= $freqlimit }
arr = arr[0..$maxpatterns] if $maxpatterns

num = arr.length + 1

i = 1
File.open($mapfile, 'w'){|mapfile|
	arr.each{|a|
#		puts "#{a[0]} #{a[1]}"
		mapfile.puts "#{i} #{a[0]}"
		a[1] = i
		i += 1
	}
}

map = Hash[arr]

File.open($outfile, 'w'){|outfile|
	outfile.puts "! #{num}"
	outfile.puts "1"
	outfile.puts "#{num} 18-pattern"
	outfile.puts "!"


	passedheader = false
	firstblock = false
	skipblock = false
	File.open($infile){|infile|
		infile.each_line {|line|
			line = line.strip

			if line == "!"
				passedheader = true
				next
			end
			next unless passedheader

			if(line == "#")
				firstblock = true
				next
			end

			i = line.to_i

			if(firstblock)
				skipblock = (map[i] == nil && !$keep)
				firstblock = false
				outfile.puts "#" unless skipblock
			end

			next if skipblock

			m = map[i]
			if m
				outfile.puts m.to_s
			elsif $keep
				outfile.puts '0'
			end
		}
	}
}

