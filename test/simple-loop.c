#include <stdio.h>

void fun() {
    int number;

    printf("Enter an integer: ");
    scanf("%d", &number);

    // loop 10 times and perform an arithmetic operation
    for (int i = 0; i < 10; i++) {
        number += 5;
    }

    // print the result, should be the original number + 50
    printf("Result: %d\n", number);

    return 0;
}

int main(int argc, char const *argv[])
{
    fun();
    return 0;
}
