#include <stdio.h>

void fun() {
    int number;

    printf("Enter an integer: ");
    scanf("%d", &number);

    // loop 10 times and perform an arithmetic operation
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 4; j++) {
            number += j;
        }
        number *= 2;
    }

    // print the result
    printf("Result: %d\n", number);

    return 0;
}

int main(int argc, char const *argv[])
{
    fun();
    return 0;
}
