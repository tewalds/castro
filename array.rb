require 'thread'

class Array

	#each_fork runs the block once for each value, each in it's own process.
	# It takes a maximum concurrency, or nil for all at the same time
	# Clearly the blocks can't affect anything in the parent process.
	# It is useful to get a simple speed up on things like migration scripts.
	def each_fork(concurrency, &block)
		if(concurrency == 1)
			return self.each(&block);
		end
	
		children = []

		self.each {|val|
			children << fork {
				block.call(val)
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

	#map_fork runs the block once for each value, each in it's own process.
	# It takes a maximum concurrency, or nil for all at the same time
	# Clearly the blocks can't affect anything in the parent process.
	# It is useful to get a simple speed up on things like migration scripts.
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
	alias_method :collect_fork, :map_fork


	#each_thread runs the block once for each value, each in its own thread
	# It takes a maximum concurrency, or nil for all at the same time
	# Since threads are so expensive and can't cross processor boundaries, 
	# this is only only suggested for IO bound applications.
	# Clearly the block shouldn't share values between runs, or should wrap them
	# in a mutex/semaphore
	# It is implemented as a thread pool that pulls jobs out of the array.
	def each_thread(concurrency, &block)
		if(!concurrency || self.length < concurrency)
			concurrency = self.length
		end

	#don't spawn threads if it's not needed, they're slow!
		if(concurrency == 1)
			return self.each(&block);
		end


		i = 0;
		threads = [];
		semaphore = Mutex.new();

		concurrency.times {
			threads << Thread.new(){
				val = nil
				done = false
				while(!done)
					semaphore.synchronize {
						if(i < self.length)
							done = true;
						else
							val = self[i]
							i += 1;
						end
					}

					if(!done)
						block.call(val)
					end
				end
			}
		}

		#wait for all threads to finish before returning
		threads.each { |thread|
			thread.join();
		}

		return self
	end

	#map_thread/collect_thread runs the block once for each value, each in its own thread, 
	# building an array of return values as it goes.
	# It takes a maximum concurrency, or nil for all at the same time
	# Since threads are so expensive and can't cross processor boundaries, 
	# this is only only suggested for IO bound applications.
	# Clearly the block shouldn't share values between runs, or should wrap them
	# in a mutex/semaphore.
	# It is implemented as a thread pool that pulls jobs out of the array.
	def map_thread(concurrency, &block)
		if(!concurrency || self.length < concurrency)
			concurrency = self.length;
		end

	#don't spawn threads if it's not needed, they're slow!
		if(concurrency == 1)
			return self.map(&block);
		end

		results = [];

		i = 0;
		threads = [];
		semaphore = Mutex.new();

		concurrency.times {
			threads << Thread.new(){
				index = nil;
				done = false;
				while(!done)
					semaphore.synchronize {
						if(i < self.length)
							done = true;
						else
							index = i;
							results[index] = nil; #make sure it's defined here, so synchronize isn't needed below
							i += 1;
						end
					}

					if(!done)
						results[index] = block.call(self[index]);
					end
				end
			}
		}

		#wait for all threads to finish before returning
		threads.each { |thread|
			thread.join();
		}

		return results;
	end
	alias_method :collect_thread, :map_thread
end
