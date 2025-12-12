## Everything manually

```sh
/usr/lib/llvm18/bin/clang -o simple simple.c
python solve1.py
```

## Find rop chain automatically

```sh
zig build -Dllvm=/usr/lib/llvm18 \
    -Dpass=func_rand \
    run --verbose -- simple.c \
    && mv ../zig-out/simple.elf simple
python solve2.py
```

## Find stack offsets manually

```sh
zig build -Dllvm=/usr/lib/llvm18 \
    -Dpass=func_rand \
    -Dpass=stack_reorder \
    -Dpass=stackpad_rand \
    -Dpass=inst_reorder \
    -Dpass=inst_sub \
    -Dpass=garbage_insert \
    -Dpass=constant_altering \
    run --verbose -- simple.c \
    && mv ../zig-out/simple.elf simple
python solve3.py
```
