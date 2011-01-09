
#include "solverpns.h"
#include "solverab.h"

#include "time.h"
#include "timer.h"
#include "log.h"

void SolverPNS::solve(double time){
	if(rootboard.won() >= 0){
		outcome = rootboard.won();
		return;
	}

	timeout = false;
	Timer timer(time, bind(&SolverPNS::timedout, this));
	Time start;

	logerr("max nodes: " + to_str(memlimit/sizeof(PNSNode)) + ", max memory: " + to_str(memlimit/(1024*1024)) + " Mb\n");

	run_pns();

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

	logerr("Finished in " + to_str((Time() - start)*1000, 0) + " msec\n");
}

void SolverPNS::run_pns(){
	while(!timeout && root.phi != 0 && root.delta != 0){
		if(!pns(rootboard, &root, 0, INF32/2, INF32/2)){
			int64_t before = nodes;
			garbage_collect(&root);
			ctmem.compact();

			logerr("Garbage collection cleaned up " + to_str(before - nodes) + " nodes, " +
				to_str(ctmem.meminuse()/(1024*1024)) +  " of " + to_str(memlimit/(1024*1024)) + " Mb still in use\n");

			if(ctmem.meminuse() > memlimit*0.99)
				break;
		}
	}
}

bool SolverPNS::pns(const Board & board, PNSNode * node, int depth, uint32_t tp, uint32_t td){
	if(depth > maxdepth)
		maxdepth = depth;

	if(node->children.empty()){
		if(ctmem.memalloced() >= memlimit)
			return false;

		int numnodes = board.movesremain();
		nodes += node->alloc(numnodes, ctmem);
		nodes_seen += numnodes;

		if(lbdist)
			dists.run(&board);

		int i = 0;
		for(Board::MoveIterator move = board.moveit(true); !move.done(); ++move){
			int abval, pd = 1; // alpha-beta value, phi & delta value

			if(ab){
				Board next = board;
				next.move(*move, false, false);

				SolverAB solveab(false);
				abval = -solveab.negamax(next, ab, -2, 2);
				nodes_seen += solveab.nodes_seen;
			}else{
				int won = board.test_win(*move);
				abval = (won > 0) + (won >= 0);
			}

			if(lbdist && abval == 0)
				pd = dists.get(*move);

			node->children[i] = PNSNode(*move).abval(abval, board.toplay(), ties, pd);

			i++;
		}
		for(; i < numnodes; i++) //add losses to the end so they aren't considered, only used when unique is true
			node->children[i] = PNSNode(Move(M_NONE), 0, LOSS);

		updatePDnum(node);

		return true;
	}
	
	bool mem;
	do{
		PNSNode * child = node->children.begin(),
		        * child2 = node->children.begin(),
		        * childend = node->children.end();

		uint32_t tpc, tdc;

		if(df){
			for(PNSNode * i = node->children.begin(); i != childend; i++){
				if(i->delta <= child->delta){
					child2 = child;
					child = i;
				}else if(i->delta < child2->delta){
					child2 = i;
				}
			}

			tpc = min(INF32/2, (td + child->phi - node->delta));
			tdc = min(tp, (uint32_t)(child2->delta*(1.0 + epsilon) + 1));
		}else{
			tpc = tdc = 0;
			while(child->delta != node->phi)
				child++;
		}

		Board next = board;
		next.move(child->move, false, false);
		mem = pns(next, child, depth + 1, tpc, tdc);

		if(child->phi == 0 || child->delta == 0)
			nodes -= child->dealloc(ctmem);

		if(updatePDnum(node) && !df)
			break;

	}while(!timeout && mem && (!df || (node->phi < tp && node->delta < td)));

	return mem;
}

bool SolverPNS::updatePDnum(PNSNode * node){
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

//removes the children of any node whos children are all unproven and have no children
bool SolverPNS::garbage_collect(PNSNode * node){
	if(node->children.empty())
		return (node->phi != 0 && node->delta != 0);

	PNSNode * i = node->children.begin();
	PNSNode * end = node->children.end();

	bool collect = true;
	for( ; i != end; i++)
		collect &= garbage_collect(i);

	if(collect)
		nodes -= node->dealloc(ctmem);

	return false;
}
