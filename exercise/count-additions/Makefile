run: test.elf
	./test.elf

count_additions.so: count_additions.cc
	clang++ $(CXXFLAGS) -shared -fPIC -o $@ $<

test.orig.bc: test.c
	clang -O0 -emit-llvm -c -o $@ $<

test.transformed.bc: test.orig.bc count_additions.so
	opt -load-pass-plugin ./count_additions.so -passes="count-additions" -o $@ $<

test.elf: test.transformed.bc
	clang -o $@ $<
