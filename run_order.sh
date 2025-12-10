zig build run -Dllvm=/usr/lib/llvm-18 \
    -Dpass=func_rand \
    -Dpass=stack_reorder \
    -Dpass=stackpad_rand \
    -Dpass=inst_reorder \
    -Dpass=inst_sub \
    -Dpass=garbage_insert \
    -Dpass=constant_altering \
    -- test/test_comprehensive.c && ./zig-out/test_comprehensive.elf
