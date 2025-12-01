# ROP write4

## What is it?
This is basically ROP Emporium 4th challenge `write4`. 
The challenge is to use three gadgets to add a string into memory (`.bss` here) - the string is the flag filename - and to make the program call `print_file`.  

However, since only the executables are available on ROP Emporium and we want to try these things with our passes,
this is a reversed engineered high-level language version.

I have uploaded both the executables on which you can try the payload directly. The C programs are for
the passes. If recompiled, one has to rewrite the addresses.

The payload simply fills the stack like this (stack expanding down view):

```
func addr to print_file
.bss addr
addr to gadget: pop rdi; ret
addr to gadget: mov [r14], r15; ret which writes the str into .bss
flag.txt string (file name) (goes into r15)
Addr of .bss (goes into r14)
RIP -> gadget pop r14; pop r15; ret
RDP (overwrite)
32 byte buffer
…
```

Note that `canary` and `pic` is off for `write4`:
```
mathias@mathias-comp:$ rabin2 -I write4
arch     x86
baddr    0x400000
binsz    14019
bintype  elf
bits     64
canary   false
class    ELF64
compiler GCC: (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0 Ubuntu clang version 18.1.3 (1ubuntu1)
crypto   false
endian   little
havecode true
intrp    /lib64/ld-linux-x86-64.so.2
laddr    0x0
lang     c
linenum  true
lsyms    true
machine  AMD x86-64 architecture
nx       true
os       linux
pic      false
relocs   true
relro    partial
rpath    .
sanitize false
static   false
stripped false
subsys   linux
va       true
```

## Compile C -> Executable 

1. First compile the library file (`.so`)
```
clang -O0 -fPIC -fno-stack-protector -shared libwrite4.c -o libwrite4.so
```

2. Then compile the main program, make sure it links with the library.
```
clang -O0 -no-pie -fno-stack-protector write4.c -L. -lwrite4 -Wl,-rpath,. -o write4
```

## Solution
It should print the flag.