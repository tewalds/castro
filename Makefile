.PHONY: clean fresh run gendeps

OBJECTS		= castro.o string.o time.o solverab.o solverpns.o solverpnsab.o

ifdef DEBUG
	CPPFLAGS	+= -g3 -Wall
else
	CPPFLAGS	+= -O3 -funroll-loops
endif

all: castro

castro: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)


castro.o: castro.cpp havannahgtp.h gtp.h string.h game.h board.h solver.h time.h
solverab.o: solverab.cpp solver.h time.h board.h
solverpns.o: solverpns.cpp solver.h time.h board.h
solverpnsab.o: solverpnsab.cpp solver.h time.h board.h
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
	cd ..; tar zcf castro.tgz castro --exclude castro/.git --exclude castro/.gitignore

profile:
	valgrind --tool=callgrind ./castro

