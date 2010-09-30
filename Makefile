.PHONY: clean fresh run gendeps

LDFLAGS   += -lpthread
OBJECTS		= castro.o mtrand.o string.o solverab.o solverpns.o solverpns_heap.o solverpns_tt.o player.o playeruct.o zobrist.o

ifdef DEBUG
	CPPFLAGS	+= -g3 -Wall
else
	CPPFLAGS	+= -O3 -funroll-loops -Wall
endif

all: castro

castro: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

castro.o: castro.cpp havannahgtp.h gtp.h string.h game.h board.h move.h \
 zobrist.h types.h hashset.h solver.h solverab.h time.h timer.h thread.h \
 solverpns.h solverpns_heap.h solverpns_tt.h player.h depthstats.h \
 mtrand.h weightedrandtree.h lbdist.h
mtrand.o: mtrand.cpp mtrand.h
player.o: player.cpp player.h types.h move.h board.h string.h zobrist.h \
 hashset.h time.h depthstats.h thread.h mtrand.h weightedrandtree.h \
 lbdist.h solverab.h timer.h solver.h
playeruct.o: playeruct.cpp player.h types.h move.h board.h string.h \
 zobrist.h hashset.h time.h depthstats.h thread.h mtrand.h \
 weightedrandtree.h lbdist.h
solverab.o: solverab.cpp solverab.h time.h timer.h thread.h board.h \
 move.h string.h zobrist.h types.h hashset.h solver.h
solverpns.o: solverpns.cpp solverpns.h time.h timer.h thread.h board.h \
 move.h string.h zobrist.h types.h hashset.h solver.h solverab.h
solverpns_heap.o: solverpns_heap.cpp solverpns_heap.h time.h timer.h \
 thread.h board.h move.h string.h zobrist.h types.h hashset.h solver.h \
 solverab.h
solverpns_tie.o: solverpns_tie.cpp solverpns_tie.h time.h timer.h \
 thread.h board.h move.h string.h zobrist.h types.h hashset.h solver.h \
 solverab.h
solverpns_tt.o: solverpns_tt.cpp solverpns_tt.h time.h timer.h thread.h \
 board.h move.h string.h zobrist.h types.h hashset.h solver.h solverab.h
string.o: string.cpp string.h
zobrist.o: zobrist.cpp zobrist.h types.h


gendeps:
	ls *.cpp -1 | xargs -L 1 cpp -M -MM

clean:
	rm -f castro *.o

fresh: clean all

run: all
	./castro

tar: clean
	cd ..; tar zcf castro.tgz castro \
		--exclude castro/.git \
		--exclude castro/.gitignore \
		--exclude castro/papers \
		--exclude castro/games \
		--exclude castro/littlegolem*

profile:
	valgrind --tool=callgrind ./castro

