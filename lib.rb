
def timer
	start = Time.now
	yield
	return Time.now - start
end

class GTPClient
	def initialize(cmdline)
		@io=IO.popen(cmdline,'w+')
	end
	def cmd(c)
		@io.puts c.strip
		return @io.gets("\n\n")
	end
	def close
		@io.close
	end
end


class Float
	def round(digits = 0)
		if(digits > 0)
			p = (10**digits).to_f
			f = self * p
			return (f+0.5).floor/p if f > 0.0
			return (f-0.5).ceil/p  if f < 0.0
			return 0.0
		else
			return (self+0.5).floor if self > 0.0
			return (self-0.5).ceil  if self < 0.0
			return 0
		end
	end
end

class Array
	#map_fork runs the block once for each value, each in it's own process.
	# It takes a maximum concurrency, or nil for all at the same time
	# Clearly the blocks can't affect anything in the parent process.
	def map_fork(concurrency, &block)
		if(concurrency == 1)
			return self.map(&block);
		end

		children = []
		results = []
		socks = {}

		#create as a proc since it is called several times, 
		# but is not useful outside of this function, and needs the local scope.
		read_socks_func = proc {
			while(socks.length > 0 && (readsocks = IO::select(socks.keys, nil, nil, 0.1)))
				readsocks.first.each{|sock|
					rd = sock.read
					if(rd.nil? || rd.length == 0)
						results[socks[sock]] = Marshal.load(results[socks[sock]]);
						socks.delete(sock);
					else
						results[socks[sock]] ||= ""
						results[socks[sock]] += rd
					end
				}
			end
		}

		self.each_with_index {|val, index|
			rd, wr = IO.pipe

			children << fork {
				rd.close
				result = block.call(val)
				wr.write(Marshal.dump(result))
				wr.sync
				wr.close
				exit;
			}

			wr.close
			socks[rd] = index;

			#if needed, wait for a previous process to exit
			if(concurrency)
				begin
					begin
						read_socks_func.call
		
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
				read_socks_func.call

				while(pid = Process.wait(-1, Process::WNOHANG))
					children.delete(pid);
				end
			end while(children.length >= 0 && sleep(0.1))
		rescue SystemCallError
			children = []
		end


		read_socks_func.call

		return results
	end
end


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

