// Compile using:
// clang -O3 -o simple simple.c

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
    if (!flag) flag = "Did not find a flag :(";
    if (a == 123 && b == 321) puts(flag);
    exit(0);
}

size_t very_important_function_that_is_definitely_needed_to_make_this_program_work(size_t seed) {
    size_t a = seed + 50014;
    size_t b = seed + 50015;
    size_t c = seed + 3;
    size_t d = seed + 4;
    size_t e = seed + 5;
    size_t f = seed + 6;
    size_t g = seed + 7;

    a += b;
    c ^= d;
    e = (e << 7) | (e >> (64 - 7));
    f *= 9;

    return a ^ b ^ c ^ d ^ e ^ f ^ g;
}
