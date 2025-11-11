#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

int main() {
    int result = add(5, 3);
    result = multiply(result, 2);
    printf("Result: %d\n", result);
    puts("bye");
    return 0;
}