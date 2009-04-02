CC			?= gcc
CPPFLAGS	?= -pipe -march=athlon64 -DNDEBUG -DG_DISABLE_ASSERT -s -O2 -fomit-frame-pointer -funit-at-a-time -fno-align-loops -fno-align-labels

all: reversi
doc: reversi.pdf

server.o: server.c reversi.h
client.o: client.c reversi.h
reversi.o: reversi.c reversi.h

reversi: reversi.o server.o client.o

reversi.dvi: reversi.tex
	latex reversi.tex

reversi.pdf: reversi.dvi
	dvipdf reversi.dvi

reversi.ps: reversi.dvi
	dvips reversi.dvi


clean::
	rm -f -- *.o reversi reversi.dvi reversi.pdf reversi.ps
