.PHONY: clean fresh run gendeps

LDFLAGS   += -lpthread
OBJECTS		= castro.o string.o time.o solverab.o solverscout.o solverpns.o solverpnsab.o solverdfpnsab.o player.o playeruct.o

ifdef DEBUG
	CPPFLAGS	+= -g3 -Wall
else
	CPPFLAGS	+= -O3 -funroll-loops
endif

all: castro

castro: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)


castro.o: castro.cpp havannahgtp.h gtp.h string.h game.h board.h move.h \
 solver.h time.h timer.h thread.h player.h depthstats.h
player.o: player.cpp player.h move.h board.h string.h time.h timer.h thread.h depthstats.h solver.h weightedrandtree.h
playeruct.o: playeruct.cpp player.h move.h board.h string.h time.h timer.h thread.h depthstats.h solver.h weightedrandtree.h
solverab.o: solverab.cpp solver.h time.h timer.h thread.h board.h move.h string.h
solverdfpnsab.o: solverdfpnsab.cpp solver.h time.h timer.h thread.h board.h move.h string.h
solverpnsab.o: solverpnsab.cpp solver.h time.h timer.h thread.h board.h move.h string.h
solverpns.o: solverpns.cpp solver.h time.h timer.h thread.h board.h move.h string.h
solverscout.o: solverscout.cpp solver.h time.h timer.h thread.h board.h move.h string.h
string.o: string.cpp string.h
time.o: time.cpp time.h


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

