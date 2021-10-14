presentation_filename=./src/presentation_4_3
source_dir=./src/

all: pdf

.PHONY: pdflatex
pdf: export TEXINPUTS = ${source_dir}:$TEXINPUTS:
pdf: | build
	pdflatex -output-directory=./build -halt-on-error ${presentation_filename}.tex

build:
	mkdir -p build

.PHONY: clean
clean:
	rm -rf ./build
