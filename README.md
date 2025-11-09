# DD2497 Project course in System Security

Project made by:
- Mathias Magnusson <mathm@kth.se>
- Alex Shariat <alexsz@kth.se>
- Mathias Grindsäter <grin@kth.se>
- Zhongmin Hu <zhongmin@kth.se>

## Project Specification - Diversification
Our goal is to design and implement a compiler pass pipeline within the LLVM framework to achieve code diversification. We will also construct a simple program and several simple attacks that can compromise the program, and demonstrate that our diversification pipeline will hinder the attacks.

## Vulnerabilities We Aim to Counter
Attackers often rely on knowing the exact memory layout of data and sequence of instructions in a program. By making each binary unique, an exploit that works on one instance will likely fail on another. As such, our transformation pipeline partly aims to prevent mass exploitation of binaries and code-reuse attacks.

Return Oriented Programming (ROP) is a code reuse attack where the attacker finds gadgets in the program’s memory. Each gadget ends with a return instruction and the attacker chains them together to perform malicious actions. By changing the code layout, addresses, and instruction patterns, attackers cannot predict where the gadgets are stored.

A prerequisite to performing e.g. a ROP attack is to successfully overwrite a stored return address on the stack of a program. Return addresses are often stored at a constant offset from user-controlled variables such as buffers, which make it relatively easy for an attacker to control specific values in the program if it has a buffer over-write. By diversifying the order of stack variables and struct fields and inserting differing amounts of padding we aim to make it harder for an attacker to gain an initial foothold to perform for instance a ROP attack.

Information leak vulnerabilities give attackers an opportunity to gain knowledge about memory layout to use for exploitation payload. By diversifying the structure of each binary, position of functions, basic blocks and instruction sequences, we can significantly reduce information leaks, even if a code pointer in an instance is leaked, it has limited value for attacks on other instances since internal layout differs.

## Testing the Pipeline
We will construct a program that is vulnerable to stack overflow, ROP attacks, and susceptible to memory leaks. With our compiler pass, we aim to show that if an attack is possible in one version of the program, it should not be possible in the other version (at least this possibility should be negligible). We will show and analyze the differences, which passes are related to which changes, and also run a code reuse attack to see the different effects on the two programs.

## Minimal Specifications
The project will consist of a set of code transformations implemented as a plugin to the LLVM compiler infrastructure. The transformations that we aim to implement are:
* Shuffle the order of functions (Code layout diversification)
* CFG Diversification
* Instruction-level Diversification
* Stack variable layout shuffling
* Struct field shuffling
* Inserting randomized NOP sleds
* Mixing with constants (e.g. replacing x = 10 with x = 5; x *= 2)

## Optional Requirements
If we find ourselves with enough time, some additional things we want to implement are:
* Load-time diversification
* Dynamic/runtime diversification
* Check the effect of our diversification passes on different optimization levels
* Measuring the performance degradation our diversifications cause on programs
* Testing that our transformations work when compiling some bigger, open source project.

## Division of Work
The various diversification passes are expected to be rather easy to divide, so different group members can focus on different transformations. 






