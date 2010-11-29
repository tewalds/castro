.PHONY: clean fresh run gendeps

LDFLAGS   += -lpthread
OBJECTS		= castro.o gtpgeneral.o gtpplayer.o gtpsolver.o mtrand.o string.o \
				solverab.o solverpns.o solverpns_tt.o player.o playeruct.o zobrist.o

ifdef DEBUG
	CPPFLAGS	+= -g3 -Wall
else
	CPPFLAGS	+= -O3 -funroll-loops -Wall
endif

all: castro

castro: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

castro.o: castro.cpp havannahgtp.h gtp.h string.h game.h board.h move.h \
 zobrist.h types.h hashset.h solver.h solverab.h solverpns.h children.h \
 thread.h lbdist.h solverpns_tt.h player.h time.h depthstats.h mtrand.h \
 weightedrandtree.h compacttree.h
gtpgeneral.o: gtpgeneral.cpp havannahgtp.h gtp.h string.h game.h board.h \
 move.h zobrist.h types.h hashset.h solver.h solverab.h solverpns.h \
 children.h thread.h lbdist.h solverpns_tt.h player.h time.h depthstats.h \
 mtrand.h weightedrandtree.h compacttree.h
gtpplayer.o: gtpplayer.cpp havannahgtp.h gtp.h string.h game.h board.h \
 move.h zobrist.h types.h hashset.h solver.h solverab.h solverpns.h \
 children.h thread.h lbdist.h solverpns_tt.h player.h time.h depthstats.h \
 mtrand.h weightedrandtree.h compacttree.h
gtpsolver.o: gtpsolver.cpp havannahgtp.h gtp.h string.h game.h board.h \
 move.h zobrist.h types.h hashset.h solver.h solverab.h solverpns.h \
 children.h thread.h lbdist.h solverpns_tt.h player.h time.h depthstats.h \
 mtrand.h weightedrandtree.h compacttree.h
mtrand.o: mtrand.cpp mtrand.h
player.o: player.cpp player.h types.h move.h string.h board.h zobrist.h \
 hashset.h time.h depthstats.h thread.h mtrand.h weightedrandtree.h \
 lbdist.h compacttree.h solverab.h solver.h timer.h
playeruct.o: playeruct.cpp player.h types.h move.h string.h board.h \
 zobrist.h hashset.h time.h depthstats.h thread.h mtrand.h \
 weightedrandtree.h lbdist.h compacttree.h
solverab.o: solverab.cpp solverab.h solver.h board.h move.h string.h \
 zobrist.h types.h hashset.h time.h timer.h thread.h
solverpns.o: solverpns.cpp solverpns.h solver.h board.h move.h string.h \
 zobrist.h types.h hashset.h children.h thread.h lbdist.h solverab.h \
 time.h timer.h
solverpns_tt.o: solverpns_tt.cpp solverpns_tt.h solver.h board.h move.h \
 string.h zobrist.h types.h hashset.h solverab.h time.h timer.h thread.h
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

