#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>

void pwnme(void) {
    char buf[0x20];  // 32 bytes to smash

    setvbuf(stdout, NULL, _IONBF, 0);

    puts("write4");
    puts("x86_64");

    memset(buf, 0, sizeof(buf));

    puts("Give give me the input!");
    printf("> ");

    // Vulnerable read: 512 bytes into a 32-byte buffer :(
    read(0, buf, 0x200);

    puts("Thank you!");
}

void print_file(const char *filename) {
    char line[256];
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        fputs(line, stdout);
    }

    fclose(fp);
}

