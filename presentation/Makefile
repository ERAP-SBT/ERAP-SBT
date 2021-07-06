presentation_filename=./src/presentation_16_9
source_dir=./src/

all: pdf

.PHONY: pdflatex
pdf: export TEXINPUTS = ${source_dir}:$TEXINPUTS:
pdf:
	pdflatex -output-directory=./build -halt-on-error ${presentation_filename}.tex

.PHONY: clean
clean:
	rm -f ./build/*
