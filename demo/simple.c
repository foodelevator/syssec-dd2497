#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

int main() {
    char buf[256];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (true) {
        printf("[r]ead, [w]rite or [q]uit? ");
        char c; scanf(" %c", &c);
        if (c == 'q') break;
        printf("how much? ");
        size_t n; scanf("%zu", &n);
        if (n > 0x256) break;
        if (c == 'r') 
            write(STDOUT_FILENO, buf, n);
        else 
            read(STDIN_FILENO, buf, n);
    }
}

void win(uint64_t a, uint32_t b) {
    char *flag = getenv("FLAG");
    if (!flag) flag = "You have won. Very congrats.";
    if (a == 123 && b == 321) puts(flag);
    exit(0);
}

void typical_optimized_function_epilogue() {
    asm volatile (
        ".intel_syntax noprefix\n"
        "pop r12\n"
        "pop r13\n"
        "pop r14\n"
        "pop r15\n"
        "pop rbp\n"
        "ret\n"
    );
}
