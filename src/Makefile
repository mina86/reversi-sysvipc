CC			?= gcc
CPPFLAGS	?= -pipe -march=athlon64 -DNDEBUG -DG_DISABLE_ASSERT -s -O2 -fomit-frame-pointer -funit-at-a-time -fno-align-loops -fno-align-labels

all: reversi


reversi: reversi.o server.o client.o

src/server.o: src/server.c src/reversi.h
src/client.o: src/client.c src/reversi.h
src/reversi.o: src/reversi.c src/reversi.h


clean:
	exec rm -f -- *.o reversi
