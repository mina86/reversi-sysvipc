CC			?= gcc
CPPFLAGS	?= -pipe -march=athlon64 -DNDEBUG -DG_DISABLE_ASSERT -s -O2 -fomit-frame-pointer -funit-at-a-time -fno-align-loops -fno-align-labels


all: reversi
doc: reversi.pdf


reversi: src/reversi.o src/server.o src/client.o
	exec $(CC) $(LDFLAGS) -o reversi src/reversi.o src/server.o src/client.o

src/server.o: src/server.c src/reversi.h
src/client.o: src/client.c src/reversi.h
src/reversi.o: src/reversi.c src/reversi.h


doc/reversi.dvi: doc/reversi.tex doc/modules.eps
	cd doc && exec latex -file-line-error -interaction=scrollmode reversi.tex
	cd doc && exec latex -file-line-error -interaction=batchmode  reversi.tex

reversi.pdf: doc/reversi.dvi
	cd doc && dvipdf reversi.dvi && mv reversi.pdf ..

reversi.ps: doc/reversi.dvi
	cd doc && dvips reversi.dvi && mv reversi.ps ..


clean::
	exec rm -f -- reversi reversi.pdf reversi.ps src/*.o
	exec rm -f -- doc/reversi.aux doc/reversi.?o?
