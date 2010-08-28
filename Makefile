.PHONY: clean fresh run gendeps

LDFLAGS   += -lpthread
OBJECTS		= castro.o string.o solverab.o solverpns.o solverpnsab.o solverdfpnsab.o player.o playeruct.o

ifdef DEBUG
	CPPFLAGS	+= -g3 -Wall
else
	CPPFLAGS	+= -O3 -funroll-loops
endif

all: castro

castro: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

castro.o: castro.cpp havannahgtp.h gtp.h string.h game.h board.h move.h \
 solver.h solverpns.h time.h timer.h thread.h solverab.h player.h types.h depthstats.h
player.o: player.cpp player.h types.h move.h board.h string.h time.h depthstats.h thread.h solverab.h timer.h solver.h
playeruct.o: playeruct.cpp player.h types.h move.h board.h string.h time.h depthstats.h thread.h weightedrandtree.h
solverab.o: solverab.cpp solverab.h time.h timer.h thread.h board.h move.h string.h solver.h
solverdfpnsab.o: solverdfpnsab.cpp solverpns.h time.h timer.h thread.h board.h move.h string.h solver.h solverab.h
solverpnsab.o: solverpnsab.cpp solverpns.h time.h timer.h thread.h board.h move.h string.h solver.h solverab.h
solverpns.o: solverpns.cpp solverpns.h time.h timer.h thread.h board.h move.h string.h solver.h
string.o: string.cpp string.h


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

