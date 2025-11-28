#include <stdio.h>

// Imported functions
void pwnme(void);
void print_file(const char *filename);

// We need the compiler to import print_file
// so make it do something stupid :)
void usefulFunction(void) {
    print_file("nonexistent");  
}

// We need the compiler to keep these gadgets for the solution,
// even if the program doesn't use them.
__attribute__((noinline, used, optnone))
void gadget_pool(int choice) {
    switch (choice) {
    case 0:
        __asm__ volatile(
            "pop %r14\n\t"
            "pop %r15\n\t"
            "ret\n\t"
        );
        break;

    case 1:
        __asm__ volatile(
            "mov %r15, (%r14)\n\t"
            "ret\n\t"
        );
        break;

    case 2:
        __asm__ volatile(
            "pop %rdi\n\t"
            "ret\n\t"
        );
        break;

    default:
        break;
    }
}

int main(void) {
    pwnme();
    return 0;
}

