# Technische Präsentation

```bash
PATH="$PWD/big_tests/sysroot/bin:$PATH" \
	riscv64-linux-gnu-gcc \
	-static \
	examples/helloworld3.c \
	-o examples/helloworld3
ninja -C build all \
	&& ./build/src/translate \
		--debug=true \
		--output-binary=examples/helloworld3.bin \
		--output=examples/helloworld3_translated.s \
		examples/helloworld3 \
	&& gcc \
		-c examples/helloworld3_translated.s \
		-o examples/helloworld3_translated.o \
	&& ld \
		-T src/generator/x86_64/helper/link.ld \
		examples/helloworld3_translated.o \
		build/src/generator/x86_64/helper/libhelper.a \
		-o examples/helloworld3_translated \
	&& ./examples/helloworld3_translated
```

```bash
time examples/helloworld3_native
time examples/helloworld3
time examples/helloworld3_translated
```


```bash
tmux attach-session -r -t default
LC_ALL=de_DE.UTF-8 pympress presentation/build/presentation_16_9.pdf
```
