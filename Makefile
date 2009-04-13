all: reversi
doc: reversi.pdf


reversi: src/reversi
	exec mv src/reversi .

src/reversi::
	$(MAKE) -C src reversi


reversi.pdf: doc/reversi.pdf
	exec mv doc/reversi.pdf .

doc/reversi.pdf::
	$(MAKE) -C doc reversi.pdf

reversi.ps: doc/reversi.ps
	exec mv doc/reversi.ps .

doc/reversi.ps::
	$(MAKE) -C doc reversi.ps


clean::
	exec $(MAKE) -C src clean
	exec $(MAKE) -C doc clean
	exec rm -f -- reversi reversi.*
