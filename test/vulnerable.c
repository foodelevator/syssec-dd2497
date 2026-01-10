#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Secret flag that should only be accessible via exploit
#define SECRET_FLAG "FLAG{d1v3rs1f1c4t10n_st0ps_r0p_4tt4cks}"

// Global variables for different passes to manipulate
int global_auth_level = 0;
int global_access_count = 0;
char global_username[32] = "guest";

// ===== HELPER FUNCTIONS (increase gadget surface) =====

int check_permissions(int level) {
    int threshold = 100;
    int bonus = 5;
    int penalty = 10;
    int result = 0;
    
    if (level > threshold) {
        result = level + bonus;
    } else {
        result = level - penalty;
    }
    
    return result;
}

void update_stats(int value) {
    int local_a = value;
    int local_b = value * 2;
    int local_c = value + 7;
    int local_d = 0;
    
    for (int i = 0; i < 5; i++) {
        local_d = local_a + local_b;
        local_a = local_d - local_c;
        local_b = local_a * 3;
    }
    
    global_access_count = local_d;
}

int calculate_score(int base, int multiplier) {
    int score = 0;
    int temp1 = base + 10;
    int temp2 = multiplier * 2;
    int temp3 = temp1 + temp2;
    
    for (int i = 0; i < 3; i++) {
        score = score + temp3;
        temp3 = temp3 - 1;
    }
    
    return score;
}

// ===== TARGET FUNCTION (the goal of the exploit) =====

void print_flag() {
    printf("\n");
    printf("========================================\n");
    printf("    CONGRATULATIONS! ACCESS GRANTED!   \n");
    printf("========================================\n");
    printf("\n");
    printf("    Secret Flag: %s\n", SECRET_FLAG);
    printf("\n");
    printf("========================================\n");
    printf("\n");
    
    // Could also spawn a shell instead:
    // system("/bin/sh");
}

void admin_backdoor() {
    // Another potential target for ROP
    printf("Admin backdoor activated!\n");
    printf("Executing system command...\n");
    system("/bin/sh");
}

// ===== VULNERABLE FUNCTIONS =====

void process_input(char *input) {
    char buffer[64];  // Small buffer - vulnerable!
    char temp[32];
    int counter = 0;
    int validation = 0;
    
    printf("Processing your input...\n");
    
    // VULNERABILITY 1: strcpy without bounds checking
    strcpy(buffer, input);
    
    // Some processing to add stack complexity
    for (int i = 0; i < 10; i++) {
        counter = counter + i;
        validation = validation + (i * 2);
    }
    
    printf("Input processed: %s\n", buffer);
    printf("Validation code: %d\n", validation);
}

void authenticate_user() {
    char username[32];
    char password[64];  // Vulnerable buffer
    int auth_code = 0;
    int session_id = 0;
    int timestamp = 0;
    
    printf("\n=== User Authentication ===\n");
    printf("Username: ");
    fflush(stdout);
    
    // VULNERABILITY 2: scanf without width specifier - unsafe!
    scanf("%s", username);
    
    printf("Password: ");
    fflush(stdout);
    
    // VULNERABILITY 3: scanf without width specifier - unsafe!
    scanf("%s", password);
    
    // Fake authentication logic
    auth_code = strlen(username) + strlen(password);
    session_id = auth_code * 17;
    timestamp = session_id + 1234;
    
    update_stats(auth_code);
    
    if (strcmp(username, "admin") == 0 && strcmp(password, "secretpass123") == 0) {
        printf("Access granted!\n");
        global_auth_level = 100;
    } else {
        printf("Access denied.\n");
        global_auth_level = 0;
    }
}

void read_user_data() {
    char input_buffer[128];  // Vulnerable
    char name[64];
    char email[64];
    int age = 0;
    int count = 0;
    
    printf("\n=== User Registration ===\n");
    printf("Please enter your data (name email age): ");
    fflush(stdout);
    
    // VULNERABILITY 4: Dangerous scanf that could overflow
    scanf("%s %s %d", name, email, &age);
    
    // Some processing
    for (int i = 0; i < age % 20; i++) {
        count = count + i;
    }
    
    printf("Registered: %s (%s), Age: %d\n", name, email, age);
}

void handle_message() {
    char message[100];
    char response[50];
    int msg_length = 0;
    int response_code = 0;
    
    printf("\n=== Message Handler ===\n");
    printf("Enter your message: ");
    fflush(stdout);
    
    // VULNERABILITY 5: fgets but then unsafe operations
    fgets(message, sizeof(message), stdin);
    
    // Remove newline
    message[strcspn(message, "\n")] = 0;
    
    msg_length = strlen(message);
    
    // VULNERABILITY 6: sprintf without bounds checking
    sprintf(response, "You said: %s", message);
    
    printf("%s\n", response);
    
    response_code = calculate_score(msg_length, 3);
    printf("Response code: %d\n", response_code);
}

// ===== MENU SYSTEM =====

void print_menu() {
    printf("\n");
    printf("================================\n");
    printf("    VULNERABLE CTF CHALLENGE    \n");
    printf("================================\n");
    printf("1. Authenticate\n");
    printf("2. Register User\n");
    printf("3. Send Message\n");
    printf("4. Check Permissions\n");
    printf("5. Exit\n");
    printf("================================\n");
    printf("Choice: ");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    int choice = 0;
    int running = 1;
    char input[256];  // Vulnerable buffer
    int loop_counter = 0;
    
    // Disable buffering for easier interaction
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);
    
    printf("\n");
    printf("Welcome to the Vulnerable Authentication System!\n");
    printf("(This program is intentionally vulnerable for educational purposes)\n");
    
    // Some initial processing with loops for loop passes to affect
    for (int i = 0; i < 10; i++) {
        loop_counter = loop_counter + i;
        for (int j = 0; j < 5; j++) {
            loop_counter = loop_counter + (i * j);
        }
    }
    
    while (running) {
        print_menu();
        
        // VULNERABILITY 7: Direct read without bounds checking
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        choice = atoi(input);
        
        switch (choice) {
            case 1:
                authenticate_user();
                break;
            case 2:
                read_user_data();
                break;
            case 3:
                handle_message();
                break;
            case 4: {
                int perm_level = check_permissions(global_auth_level);
                printf("Permission level: %d\n", perm_level);
                printf("Access count: %d\n", global_access_count);
                break;
            }
            case 5:
                printf("Goodbye!\n");
                running = 0;
                break;
            default:
                printf("Invalid choice!\n");
                break;
        }
    }
    
    return 0;
}
