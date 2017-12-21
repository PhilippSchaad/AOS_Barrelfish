default_target: all
$(VERBOSE).SILENT:

FILENAME="Report"
FLAGS="-synctex=1 -interaction=nonstop"

all: compile read

compile:
	pdflatex ${FLAGS} ${FILENAME}.tex
	bibtex ${FILENAME}.aux
	pdflatex ${FLAGS} ${FILENAME}.tex
	pdflatex ${FLAGS} ${FILENAME}.tex

read:
	evince ${FILENAME}.pdf &

clean:
	find . -name "*.ps" -type f -delete
	find . -name "*.log" -type f -delete
	find . -name "*.aux" -type f -delete
	find . -name "*.out" -type f -delete
	find . -name "*.dvi" -type f -delete
	find . -name "*.bbl" -type f -delete
	find . -name "*.blg" -type f -delete
	rm -rf ${FILENAME}.pdf
	rm -rf ${FILENAME}.blx.bib
	rm -rf ${FILENAME}.toc
	rm -rf ${FILENAME}.run.xml
	rm -rf ${FILENAME}.lof
	rm -rf ${FILENAME}.lot
