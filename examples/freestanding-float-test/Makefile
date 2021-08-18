all:
	riscv64-linux-gnu-gcc-10 -march=rv64g -mabi=lp64 -nostdlib -static ./float_test.c ./start.S -g -o float_test.o
clean:
	rm float_test.o