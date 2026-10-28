#include "library.h"

#include <stdio.h>
#include <string.h>

/*
 * EDUCATIONAL DEMO AUTHENTICATION ONLY:
 * Replace this hard-coded authentication with an approved password-hashing
 * and account-management solution before any real deployment.
 * Never use these sample credentials on a production system.
 */
Role authenticate_user(char username_out[32])
{
    char username[32];
    char password[64];
    int attempt;

    for (attempt = 1; attempt <= 3; attempt++) {
        puts("\n===== LIBRARY MANAGEMENT SYSTEM LOGIN =====");
        read_line("Username: ", username, sizeof username);
        read_line("Password: ", password, sizeof password);

        if (strcmp(username, "admin") == 0 &&
            strcmp(password, "Admin@123") == 0) {
            (void)strncpy(username_out, username, 31U);
            username_out[31] = '\0';
            log_event(username_out, "login-success role=Administrator");
            return ROLE_ADMIN;
        }

        if (strcmp(username, "librarian") == 0 &&
            strcmp(password, "Librarian@123") == 0) {
            (void)strncpy(username_out, username, 31U);
            username_out[31] = '\0';
            log_event(username_out, "login-success role=Librarian");
            return ROLE_LIBRARIAN;
        }

        log_event(username, "login-failed");
        printf("Invalid credentials. Attempts remaining: %d\n", 3 - attempt);
    }

    puts("Too many unsuccessful attempts. Exiting.");
    return ROLE_NONE;
}
