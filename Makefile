.PHONY: clean fresh run gendeps

LDFLAGS   += -lpthread
OBJECTS		= castro.o fileio.o gtpgeneral.o gtpplayer.o gtpsolver.o mtrand.o string.o \
				solverab.o solverpns.o solverpns2.o solverpns_tt.o player.o playeruct.o zobrist.o alarm.o

ifdef DEBUG
	CPPFLAGS	+= -g3 -Wall
else
	CPPFLAGS	+= -O3 -funroll-loops -Wall -march=native
endif

all: castro


builddb: builddb.o string.o
	$(CXX) -o $@ $^ $(LOADLIBES) $(LDLIBS) -lkyotocabinet -lz -lstdc++ -lrt -lpthread -lm -lc

checkproof: checkproof.o string.o zobrist.o solverab.o solverpns.o alarm.o
	$(CXX) -o $@ $^ $(LOADLIBES) $(LDLIBS) -lkyotocabinet -lz -lstdc++ -lrt -lpthread -lm -lc

genhgf: genhgf.o string.o zobrist.o
	$(CXX) -o $@ $^ $(LOADLIBES) $(LDLIBS) -lkyotocabinet -lz -lstdc++ -lrt -lpthread -lm -lc


castro: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)


alarm.o: alarm.cpp alarm.h time.h
builddb.o: builddb.cpp proofdb.h string.h move.h
castro.o: castro.cpp havannahgtp.h gtp.h string.h game.h board.h move.h \
 zobrist.h types.h hashset.h solver.h solverab.h solverpns.h \
 compacttree.h thread.h lbdist.h log.h solverpns2.h solverpns_tt.h \
 player.h time.h depthstats.h mtrand.h weightedrandtree.h
checkproof.o: checkproof.cpp proofdb.h string.h move.h board.h zobrist.h \
 types.h hashset.h solverab.h solver.h solverpns.h compacttree.h thread.h \
 lbdist.h log.h
fileio.o: fileio.cpp fileio.h
genhgf.o: genhgf.cpp proofdb.h string.h move.h board.h zobrist.h types.h \
 hashset.h
gtpgeneral.o: gtpgeneral.cpp havannahgtp.h gtp.h string.h game.h board.h \
 move.h zobrist.h types.h hashset.h solver.h solverab.h solverpns.h \
 compacttree.h thread.h lbdist.h log.h solverpns2.h solverpns_tt.h \
 player.h time.h depthstats.h mtrand.h weightedrandtree.h
gtpplayer.o: gtpplayer.cpp havannahgtp.h gtp.h string.h game.h board.h \
 move.h zobrist.h types.h hashset.h solver.h solverab.h solverpns.h \
 compacttree.h thread.h lbdist.h log.h solverpns2.h solverpns_tt.h \
 player.h time.h depthstats.h mtrand.h weightedrandtree.h fileio.h
gtpsolver.o: gtpsolver.cpp havannahgtp.h gtp.h string.h game.h board.h \
 move.h zobrist.h types.h hashset.h solver.h solverab.h solverpns.h \
 compacttree.h thread.h lbdist.h log.h solverpns2.h solverpns_tt.h \
 player.h time.h depthstats.h mtrand.h weightedrandtree.h
mtrand.o: mtrand.cpp mtrand.h
player.o: player.cpp player.h time.h types.h move.h string.h board.h \
 zobrist.h hashset.h depthstats.h thread.h mtrand.h weightedrandtree.h \
 lbdist.h compacttree.h log.h solverab.h solver.h alarm.h fileio.h
playeruct.o: playeruct.cpp player.h time.h types.h move.h string.h \
 board.h zobrist.h hashset.h depthstats.h thread.h mtrand.h \
 weightedrandtree.h lbdist.h compacttree.h log.h
solverab.o: solverab.cpp solverab.h solver.h board.h move.h string.h \
 zobrist.h types.h hashset.h time.h alarm.h log.h
solverpns2.o: solverpns2.cpp solverpns2.h solver.h board.h move.h \
 string.h zobrist.h types.h hashset.h compacttree.h thread.h lbdist.h \
 log.h time.h alarm.h
solverpns.o: solverpns.cpp solverpns.h solver.h board.h move.h string.h \
 zobrist.h types.h hashset.h compacttree.h thread.h lbdist.h log.h time.h \
 alarm.h
solverpns_tt.o: solverpns_tt.cpp solverpns_tt.h solver.h board.h move.h \
 string.h zobrist.h types.h hashset.h time.h alarm.h log.h
string.o: string.cpp string.h types.h
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

