// Compile using:
// clang -o simple simple.c

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

int main() {
    char buf[256];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (true) {
        printf("Hello (again), what's your name?\n> ");
        read(STDIN_FILENO, buf, 0x256);
        char *lf = strchr(buf, '\n');
        if (lf) *lf = 0;
        if (strcmp(buf, "quit") == 0) break;
        printf("Nice to meet you %s\n", buf);
    }
    return 0;
}

void win(uint64_t a, uint32_t b) {
    char *flag = getenv("FLAG");
    if (!flag) flag = "Did not find a flag :(";
    if (a == 123 && b == 321) puts(flag);
    exit(0);
}

size_t dingus(size_t seed) {
    size_t a = seed + 1;
    size_t b = seed + 2;
    size_t c = seed + 3;
    size_t d = seed + 4;
    size_t e = seed + 5;
    size_t f = seed + 6;
    size_t g = seed + 7;
    size_t h = seed + 8;
    size_t i = seed + 9;
    size_t j = seed + 10;
    size_t k = seed + 11;
    size_t l = seed + 12;
    size_t m = seed + 13;
    size_t n = seed + 14;

    a += b;
    c ^= d;
    e = (e << 7) | (e >> (64 - 7));
    f *= 9;
    g -= h;
    i += j + k;
    l ^= m;

    return a ^ b ^ c ^ d ^ e ^ f ^ g ^ h ^ i ^ j ^ k ^ l ^ m ^ n;
}
