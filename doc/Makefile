all: reversi.pdf


reversi.dvi:reversi.tex modules.eps
	exec latex -file-line-error -interaction=scrollmode reversi.tex
	exec latex -file-line-error -interaction=batchmode  reversi.tex

reversi.pdf: reversi.dvi
	exec dvipdf reversi.dvi

reversi.ps: reversi.dvi
	exec dvips reversi.dvi


clean::
	exec rm -f -- reversi.[!t]* reversi.t[!e]* doc/reversi.te[!x]*
