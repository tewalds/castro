#!/usr/bin/ruby

$infile      = nil
$outfile     = nil
$size        = nil
$maxpatterns = nil
$freqlimit   = 100
$keep        = false


while(ARGV.length > 0)
	arg = ARGV.shift
	case arg
	when "-k", "--keep"     then $keep          = true;
	when "-n", "--num"      then $maxpatterns   = ARGV.shift.to_i;
	when "-f", "--freq"     then $freqlimit     = ARGV.shift.to_i;
	when "-s", "--size"     then $size          = ARGV.shift.to_i;
	when "-i", "--infile"   then $infile        = ARGV.shift
	when "-o", "--outfile"  then $outfile       = ARGV.shift
	when "-h", "--help"     then
		puts "Map an mm input file to and have a smaller set of patterns."
		puts "Usage: #{$0} [-k] [-n num] [-f freq] -s size -i infile -o outfile"
		puts "  -k --keep     Replace the low frequency patterns with a generic pattern"
		puts "  -n --num      Limit the number of patterns to this many"
		puts "  -f --freq     Only keep patterns with this frequency or greater [#{$freqlimit}]"
		puts "  -s --size     Boardsize"
		puts "  -i --infile   Input file of games"
		puts "  -o --outfile  Output file"
		puts "  -h --help     Print this help"
		exit;
	else
		puts "Unknown argument #{arg}"
		exit
	end
end


if !$infile || !$outfile || !$size
	puts "Missing arguments, -h for help"
	exit
end

`make mm`
`./mminput.rb #{$size} < #{$infile} > mm.input.#{$size}`
mmmapargs = ""
mmmapargs << " -k" if $keep
mmmapargs << " -n #{$maxpatterns}" if $maxpatterns
mmmapargs << " -f #{$freqlimit}" if $freqlimit
`./mmmap.rb #{mmmapargs} -i mm.input.#{$size} -m mm.map.#{$size} -o mm.mapped.#{$size}`
`./mm < mm.mapped.#{$size} > mm.mappedweights.#{$size}`
`./mmunmap.rb -i mm.mappedweights.#{$size} -m mm.map.#{$size} -o mm.weights.#{$size}`

