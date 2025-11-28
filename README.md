# DD2497 Project course in System Security

Project made by:
- Mathias Magnusson <mathm@kth.se>
- Alex Shariat <alexsz@kth.se>
- Mathias Grindsäter <grin@kth.se>
- Zhongmin Hu <zhongmin@kth.se>

## Project Specification - Diversification
Our goal is to design and implement a compiler pass pipeline within the LLVM framework to achieve code diversification. We will also construct a simple program and several simple attacks that can compromise the program, and demonstrate that our diversification pipeline will hinder the attacks.

### Vulnerabilities We Aim to Counter
Attackers often rely on knowing the exact memory layout of data and sequence of instructions in a program. By making each binary unique, an exploit that works on one instance will likely fail on another. As such, our transformation pipeline partly aims to prevent mass exploitation of binaries and code-reuse attacks.

Return Oriented Programming (ROP) is a code reuse attack where the attacker finds gadgets in the program’s memory. Each gadget ends with a return instruction and the attacker chains them together to perform malicious actions. By changing the code layout, addresses, and instruction patterns, attackers cannot predict where the gadgets are stored.

A prerequisite to performing e.g. a ROP attack is to successfully overwrite a stored return address on the stack of a program. Return addresses are often stored at a constant offset from user-controlled variables such as buffers, which make it relatively easy for an attacker to control specific values in the program if it has a buffer over-write. By diversifying the order of stack variables and struct fields and inserting differing amounts of padding we aim to make it harder for an attacker to gain an initial foothold to perform for instance a ROP attack.

Information leak vulnerabilities give attackers an opportunity to gain knowledge about memory layout to use for exploitation payload. By diversifying the structure of each binary, position of functions, basic blocks and instruction sequences, we can significantly reduce information leaks, even if a code pointer in an instance is leaked, it has limited value for attacks on other instances since internal layout differs.

### Testing the Pipeline
We will construct a program that is vulnerable to stack overflow, ROP attacks, and susceptible to memory leaks. With our compiler pass, we aim to show that if an attack is possible in one version of the program, it should not be possible in the other version (at least this possibility should be negligible). We will show and analyze the differences, which passes are related to which changes, and also run a code reuse attack to see the different effects on the two programs.

### Minimal Specifications
The project will consist of a set of code transformations implemented as a plugin to the LLVM compiler infrastructure. The transformations that we aim to implement are:
* Shuffle the order of functions (Code layout diversification)
* CFG Diversification
* Instruction-level Diversification
* Stack variable layout shuffling
* Struct field shuffling
* Inserting randomized NOP sleds
* Mixing with constants (e.g. replacing x = 10 with x = 5; x *= 2)

### Optional Requirements
If we find ourselves with enough time, some additional things we want to implement are:
* Load-time diversification
* Dynamic/runtime diversification
* Check the effect of our diversification passes on different optimization levels
* Measuring the performance degradation our diversifications cause on programs
* Testing that our transformations work when compiling some bigger, open source project.

### Division of Work
The various diversification passes are expected to be rather easy to divide, so different group members can focus on different transformations.

## Running the Project

### Dependencies

[Zig](https://ziglang.org) version 0.15.2
(Optionally) [LLVM](https://www.llvm.org/) version 21

### Build System

The passes in the project are written in C++ but to build and run everything we use the Zig build
system (as opposed to something more common for C++ like a Makefile or CMake). The build script is
defined in `build.zig` and `build.zig.zon`. By default, it will automatically download LLVM (which
can take a while, but it will be cached after the first run), but you can also use an externally
provided LLVM installed by e.g. your package manager, by providing the flag `-Dllvm=...` after
`zig build` in any command. On Arch linux, if you have installed the package `llvm` using pacman,
the correct flag is `-Dllvm=/usr` and this is probably true in most cases.

To compile the project, run `zig build install`. This will create a file called
`libdiversification.so` in the directory `zig-out/` in the project root. This file can be loaded as
a pass plugin using LLVM's `opt`.

To both compile the project and run it on a C file, emitting both the original and altered llvm
bitcode and the final binary, run `zig build run -- path/to/FILENAME.c`. This will create
`FILENAME.orig.bc`, `FILENAME.transformed.bc`, and `FILENAME.elf` in the `zig-out/` directory. To
look at the diff between the original and transformed LLVM IR, you can run something like:

```sh
diff <(llvm-dis zig-out/$filename.orig.bc -o -) <(llvm-dis zig-out/$filename.transformed.bc -o -)
```

Too inspect the assembly of the final binary, you can run

```sh
objdump -Mintel -d zig-out/$filename.elf
```

By default all existing passes are included and ran. To only run one or a few, specify the flag
`-Dpass=NAME` for each pass. The name of the pass is the name of it's source file without the
extension.

### Defining a Pass

Create a file in `passes/` named after you pass, ending with `.cc`. If it is named `very_cool.cc`,
it should contain a function with the signature `bool very_cool_pass(Module &m, std::mt19937_64 &gen)`.
