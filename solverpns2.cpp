
#include "solverpns2.h"

#include "time.h"
#include "alarm.h"
#include "log.h"

void SolverPNS2::solve(double time){
	if(rootboard.won() >= 0){
		outcome = rootboard.won();
		return;
	}

	start_threads();

	timeout = false;
	Alarm timer(time, std::tr1::bind(&SolverPNS2::timedout, this));
	Time start;

//	logerr("max memory: " + to_str(memlimit/(1024*1024)) + " Mb\n");

	//wait for the timer to stop them
	runbarrier.wait();
	CAS(threadstate, Thread_Wait_End, Thread_Wait_Start);
	assert(threadstate == Thread_Wait_Start);

	if(root.phi == 0 && root.delta == LOSS){ //look for the winning move
		for(PNSNode * i = root.children.begin() ; i != root.children.end(); i++){
			if(i->delta == 0){
				bestmove = i->move;
				break;
			}
		}
		outcome = rootboard.toplay();
	}else if(root.phi == 0 && root.delta == DRAW){ //look for the move to tie
		for(PNSNode * i = root.children.begin() ; i != root.children.end(); i++){
			if(i->delta == DRAW){
				bestmove = i->move;
				break;
			}
		}
		outcome = 0;
	}else if(root.delta == 0){ //loss
		bestmove = M_NONE;
		outcome = 3 - rootboard.toplay();
	}else{ //unknown
		bestmove = M_UNKNOWN;
		outcome = -3;
	}

	time_used = Time() - start;
}

void SolverPNS2::SolverThread::run(){
	while(true){
		switch(solver->threadstate){
		case Thread_Cancelled:  //threads should exit
			return;

		case Thread_Wait_Start: //threads are waiting to start
		case Thread_Wait_Start_Cancelled:
			solver->runbarrier.wait();
			CAS(solver->threadstate, Thread_Wait_Start, Thread_Running);
			CAS(solver->threadstate, Thread_Wait_Start_Cancelled, Thread_Cancelled);
			break;

		case Thread_Wait_End:   //threads are waiting to end
			solver->runbarrier.wait();
			CAS(solver->threadstate, Thread_Wait_End, Thread_Wait_Start);
			break;

		case Thread_Running:    //threads are running
			if(solver->root.terminal()){ //solved
				CAS(solver->threadstate, Thread_Running, Thread_Wait_End);
				break;
			}
			if(solver->ctmem.memalloced() >= solver->memlimit){ //out of memory, start garbage collection
				CAS(solver->threadstate, Thread_Running, Thread_GC);
				break;
			}

			pns(solver->rootboard, &solver->root, 0, INF32/2, INF32/2);
			break;

		case Thread_GC:         //one thread is running garbage collection, the rest are waiting
		case Thread_GC_End:     //once done garbage collecting, go to wait_end instead of back to running
			if(solver->gcbarrier.wait()){
				logerr("Starting solver GC with limit " + to_str(solver->gclimit) + " ... ");

				Time starttime;
				solver->garbage_collect(& solver->root);

				Time gctime;
				solver->ctmem.compact(1.0, 0.75);

				Time compacttime;
				logerr(to_str(100.0*solver->ctmem.meminuse()/solver->memlimit, 1) + " % of tree remains - " +
					to_str((gctime - starttime)*1000, 0)  + " msec gc, " + to_str((compacttime - gctime)*1000, 0) + " msec compact\n");

				if(solver->ctmem.meminuse() >= solver->memlimit/2)
					solver->gclimit = (unsigned int)(solver->gclimit*1.3);
				else if(solver->gclimit > 5)
					solver->gclimit = (unsigned int)(solver->gclimit*0.9); //slowly decay to a minimum of 5

				CAS(solver->threadstate, Thread_GC,     Thread_Running);
				CAS(solver->threadstate, Thread_GC_End, Thread_Wait_End);
			}
			solver->gcbarrier.wait();
			break;
		}
	}
}

void SolverPNS2::timedout() {
	CAS(threadstate, Thread_Running, Thread_Wait_End);
	CAS(threadstate, Thread_GC, Thread_GC_End);
	timeout = true;
}

string SolverPNS2::statestring(){
	switch(threadstate){
	case Thread_Cancelled:  return "Thread_Wait_Cancelled";
	case Thread_Wait_Start: return "Thread_Wait_Start";
	case Thread_Wait_Start_Cancelled: return "Thread_Wait_Start_Cancelled";
	case Thread_Running:    return "Thread_Running";
	case Thread_GC:         return "Thread_GC";
	case Thread_GC_End:     return "Thread_GC_End";
	case Thread_Wait_End:   return "Thread_Wait_End";
	}
	return "Thread_State_Unknown!!!";
}

void SolverPNS2::stop_threads(){
	if(threadstate != Thread_Wait_Start){
		timedout();
		runbarrier.wait();
		CAS(threadstate, Thread_Wait_End, Thread_Wait_Start);
		assert(threadstate == Thread_Wait_Start);
	}
}

void SolverPNS2::start_threads(){
	assert(threadstate == Thread_Wait_Start);
	runbarrier.wait();
	CAS(threadstate, Thread_Wait_Start, Thread_Running);
}

void SolverPNS2::reset_threads(){ //start and end with threadstate = Thread_Wait_Start
	assert(threadstate == Thread_Wait_Start);

//wait for them to all get to the barrier
	assert(CAS(threadstate, Thread_Wait_Start, Thread_Wait_Start_Cancelled));
	runbarrier.wait();

//make sure they exited cleanly
	for(unsigned int i = 0; i < threads.size(); i++)
		threads[i]->join();

	threads.clear();

	threadstate = Thread_Wait_Start;

	runbarrier.reset(numthreads + 1);
	gcbarrier.reset(numthreads);

//start new threads
	for(int i = 0; i < numthreads; i++)
		threads.push_back(new SolverThread(this));
}


bool SolverPNS2::SolverThread::pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td){
	iters++;
	if(solver->maxdepth < depth)
		solver->maxdepth = depth;

	if(node->children.empty()){
		if(node->terminal())
			return true;

		if(solver->ctmem.memalloced() >= solver->memlimit)
			return false;

		if(!node->children.lock())
			return false;

		int numnodes = board.movesremain();
		CompactTree<PNSNode>::Children temp;
		temp.alloc(numnodes, solver->ctmem);

		if(solver->lbdist)
			dists.run(&board);

		int i = 0;
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			int outcome, pd;

			if(solver->ab){
				Board next = board;
				next.move(*move, false, false);

				pd = 0;
				outcome = (solver->ab == 1 ? solve1ply(next, pd) : solve2ply(next, pd));
				PLUS(solver->nodes_seen, pd);
			}else{
				outcome = board.test_win(*move);
				pd = 1;
			}

			if(solver->lbdist && outcome < 0)
				pd = dists.get(*move);

			temp[i] = PNSNode(*move).outcome(outcome, board.toplay(), solver->ties, pd);

			i++;
		}
		temp.shrink(i); //if symmetry, there may be extra moves to ignore
		node->children.swap(temp);
		assert(temp.unlock());

		PLUS(solver->nodes_seen, i);

		updatePDnum(node);

		return true;
	}

	bool mem;
	do{
		PNSNode * child = node->children.begin(),
		        * child2 = node->children.begin(),
		        * childend = node->children.end();

		uint32_t tpc, tdc;

		if(solver->df){
			for(PNSNode * i = node->children.begin(); i != childend; i++){
				if(i->refdelta() <= child->refdelta()){
					child2 = child;
					child = i;
				}else if(i->refdelta() < child2->refdelta()){
					child2 = i;
				}
			}

			tpc = min(INF32/2, (td + child->phi - node->delta));
			tdc = min(tp, (uint32_t)(child2->delta*(1.0 + solver->epsilon) + 1));
		}else{
			tpc = tdc = 0;
			for(PNSNode * i = node->children.begin(); i != childend; i++)
				if(child->refdelta() > i->refdelta())
					child = i;
		}

		Board next = board;
		next.move(child->move, false, false);

		child->ref();
		uint64_t itersbefore = iters;
		mem = pns(next, child, depth + 1, tpc, tdc);
		child->deref();
		PLUS(child->work, iters - itersbefore);

		if(updatePDnum(node) && !solver->df)
			break;

	}while(!solver->timeout && mem && (!solver->df || (node->phi < tp && node->delta < td)));

	return mem;
}

bool SolverPNS2::SolverThread::updatePDnum(PNSNode * node){
	PNSNode * i = node->children.begin();
	PNSNode * end = node->children.end();

	uint32_t min = i->delta;
	uint64_t sum = 0;

	bool win = false;
	for( ; i != end; i++){
		win |= (i->phi == LOSS);
		sum += i->phi;
		if( min > i->delta)
			min = i->delta;
	}

	if(win)
		sum = LOSS;
	else if(sum >= INF32)
		sum = INF32;

	if(min == node->phi && sum == node->delta){
		return false;
	}else{
		if(sum == 0 && min == DRAW){
			node->phi = 0;
			node->delta = DRAW;
		}else{
			node->phi = min;
			node->delta = sum;
		}
		return true;
	}
}

//removes the children of any node with less than limit work
void SolverPNS2::garbage_collect(PNSNode * node){
	PNSNode * child = node->children.begin();
	PNSNode * end = node->children.end();

	for( ; child != end; child++){
		if(child->terminal()){ //solved
			//log heavy nodes?
			PLUS(nodes, -child->dealloc(ctmem));
		}else if(child->work < gclimit){ //low work, ignore solvedness since it's trivial to re-solve
			PLUS(nodes, -child->dealloc(ctmem));
		}else if(child->children.num() > 0){
			garbage_collect(child);
		}
	}
}

