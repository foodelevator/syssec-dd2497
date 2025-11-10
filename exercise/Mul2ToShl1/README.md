# Mul2ToShl1

This transformation pass transforms multiplications with 2 to a 1 step left shift.
More specifically: `%mul = mul i32 %x, 2` to `%shl = shl i32 %x, 1`

`test.c` contains:
```C
int a(int x){ return x * 2; }
int b(int x){ return 2 * x; }     
int c(int x){ return x * 4; }     // Should not be transformed!
```

To generate test.ll run: `clang -O0 -Xclang -disable-O0-optnone -S -emit-llvm test.c -o test.ll`
Note that we diasble optnone because we use `-O0` which generate `optnone` but also want to use our pass later.
We note the `mul` operand:
```arduino
define dso_local i32 @a(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = mul nsw i32 %3, 2
  ret i32 %4
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @b(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = mul nsw i32 2, %3
  ret i32 %4
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @c(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = mul nsw i32 %3, 4
  ret i32 %4
}

```

Build like this:
`mkdir build && cd build`  
`cmake -DLLVM_DIR=$(llvm-config --cmakedir) ..`  
`cmake --build . --config RelWithDebInfo`
`opt -load-pass-plugin=./Mul2ToShl1.so -passes="function(Mul2ToShl1)" -S ../test.ll -o ../out.ll`

If we open `../out.ll` we can see that the first two `mul` has been transformed to `shl`:
```arduino
define dso_local i32 @a(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = shl i32 %3, 1
  ret i32 %4
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @b(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = shl i32 %3, 1
  ret i32 %4
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @c(i32 noundef %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = mul nsw i32 %3, 4
```

