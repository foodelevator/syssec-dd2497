#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global variables to exercise different passes
int global_counter = 0;
int global_result = 0;

// Helper function with arithmetic operations
int calculate_value(int a, int b, int c) {
    int temp1 = a + b;
    int temp2 = b * c;
    int temp3 = temp1 - temp2;
    int temp4 = temp3 + 100;
    int temp5 = temp4 * 2;
    
    if (temp5 > 500) {
        temp5 = temp5 / 2;
    } else {
        temp5 = temp5 + 50;
    }
    
    return temp5;
}

// Function with nested loops
void process_matrix(int rows, int cols) {
    int sum = 0;
    int product = 1;
    int buffer[10];
    int i, j, k;
    
    // Initialize buffer with constants
    for (i = 0; i < 10; i++) {
        buffer[i] = i * 5 + 3;
    }
    
    // Nested loop structure
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            int value = (i * cols) + j;
            sum = sum + value;
            product = product * 2;
            
            if (value % 3 == 0) {
                sum = sum + 7;
            } else if (value % 5 == 0) {
                sum = sum - 2;
            } else {
                sum = sum + 1;
            }
            
            // Inner computation
            for (k = 0; k < 3; k++) {
                int temp = value + k;
                temp = temp * 11;
                temp = temp - 13;
                buffer[k % 10] = temp;
            }
        }
    }
    
    global_result = sum + product;
}

// Function with multiple branches
int categorize_number(int num) {
    int x = num;
    int y = num * 2;
    int z = num + 10;
    int result = 0;
    
    if (x < 10) {
        result = x + y + z;
    } else if (x < 50) {
        result = x * y - z;
    } else if (x < 100) {
        result = (x + y) / 2;
    } else {
        result = x - y + z;
    }
    
    // Additional computation
    int temp_a = result + 25;
    int temp_b = result - 10;
    int temp_c = temp_a * temp_b;
    
    return temp_c % 1000;
}

// Function with array operations
void manipulate_array(int *arr, int size) {
    int index = 0;
    int multiplier = 3;
    int offset = 7;
    int accumulator = 0;
    
    while (index < size) {
        arr[index] = arr[index] * multiplier + offset;
        accumulator = accumulator + arr[index];
        
        if (accumulator > 100) {
            accumulator = accumulator / 2;
        }
        
        index = index + 1;
    }
    
    global_counter = accumulator;
}

// Function with switch statement
char get_grade(int score) {
    char grade = 'F';
    int bonus = 0;
    int penalty = 0;
    
    switch (score / 10) {
        case 10:
        case 9:
            grade = 'A';
            bonus = 5;
            break;
        case 8:
            grade = 'B';
            bonus = 3;
            break;
        case 7:
            grade = 'C';
            bonus = 1;
            break;
        case 6:
            grade = 'D';
            penalty = 2;
            break;
        default:
            grade = 'F';
            penalty = 5;
            break;
    }
    
    int adjusted = score + bonus - penalty;
    global_result = adjusted;
    
    return grade;
}

// Recursive function
int fibonacci(int n) {
    int a = 0;
    int b = 1;
    int c = 0;
    
    if (n <= 0) {
        return 0;
    } else if (n == 1) {
        return 1;
    }
    
    a = fibonacci(n - 1);
    b = fibonacci(n - 2);
    c = a + b;
    
    return c;
}

// Function with complex control flow
int complex_computation(int input) {
    int local1 = input;
    int local2 = input * 2;
    int local3 = input + 5;
    int local4 = 0;
    int local5 = 0;
    int local6 = 0;
    int local7 = 0;
    int i = 0;
    
    for (i = 0; i < 5; i++) {
        local4 = local1 + local2;
        local5 = local4 * local3;
        
        if (local5 > 100) {
            local6 = local5 - 50;
            local7 = local6 / 2;
        } else {
            local6 = local5 + 25;
            local7 = local6 * 3;
        }
        
        local1 = local7 % 100;
        local2 = local1 + 17;
        local3 = local2 - 8;
    }
    
    return local1 + local2 + local3 + local4 + local5 + local6 + local7;
}

// Main function
int main(int argc, char *argv[]) {
    int numbers[20];
    int i = 0;
    int total = 0;
    int average = 0;
    int max_value = 0;
    int min_value = 1000;
    
    printf("Starting complex sample program...\n");
    
    // Initialize array
    for (i = 0; i < 20; i++) {
        numbers[i] = (i * 7 + 13) % 50;
    }
    
    // Call various functions
    process_matrix(4, 5);
    printf("Global result after matrix: %d\n", global_result);
    
    manipulate_array(numbers, 20);
    printf("Global counter: %d\n", global_counter);
    
    // Process array elements
    for (i = 0; i < 20; i++) {
        int value = calculate_value(numbers[i], i, 3);
        total = total + value;
        
        if (value > max_value) {
            max_value = value;
        }
        if (value < min_value) {
            min_value = value;
        }
        
        int category = categorize_number(value);
        if (category % 2 == 0) {
            total = total + 10;
        }
        
        char grade = get_grade(value % 100);
        if (grade == 'A' || grade == 'B') {
            total = total + 20;
        }
    }
    
    average = total / 20;
    
    printf("Total: %d, Average: %d\n", total, average);
    printf("Max: %d, Min: %d\n", max_value, min_value);
    
    // Compute fibonacci
    int fib_result = fibonacci(8);
    printf("Fibonacci(8): %d\n", fib_result);
    
    // Complex computation
    int complex_result = complex_computation(42);
    printf("Complex computation: %d\n", complex_result);
    
    printf("Program completed successfully.\n");
    
    return 0;
}
