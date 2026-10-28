#include "library.h"

#include <stdio.h>
#include <string.h>

static void print_menu(Role role)
{
    puts("\n========== LIBRARY MANAGEMENT SYSTEM ==========");
    puts("1.  Add book");
    puts("2.  View all books");
    puts("3.  Search books");
    puts("4.  Update book");
    if (role == ROLE_ADMIN) puts("5.  Delete book [Administrator only]");
    else puts("5.  Delete book [Administrator only - restricted]");
    puts("6.  Register member");
    puts("7.  View all members");
    puts("8.  Search members");
    puts("9.  Update member");
    if (role == ROLE_ADMIN) puts("10. Delete member [Administrator only]");
    else puts("10. Delete member [Administrator only - restricted]");
    puts("11. Issue book");
    puts("12. Return book");
    puts("13. View transactions");
    puts("14. View fine details");
    puts("0.  Exit");
}

int main(void)
{
    Library library;
    int choice;
    Role role;

    memset(&library, 0, sizeof library);

    puts("Library Management System");
    puts("Data is stored in text files in the current project directory.");

    if (!load_data(&library)) {
        puts("Could not load stored data. Check the data files before continuing.");
        free_library(&library);
        return 1;
    }

    role = authenticate_user(library.username);
    if (role == ROLE_NONE) {
        free_library(&library);
        return 1;
    }
    library.role = role;
    printf("Welcome, %s (%s).\n", library.username, role_name(role));

    do {
        print_menu(library.role);
        choice = read_int("Enter your choice: ", 0, 14);

        switch (choice) {
            case 1: add_book(&library); break;
            case 2: list_books(&library); break;
            case 3: search_books(&library); break;
            case 4: update_book(&library); break;
            case 5:
                if (library.role != ROLE_ADMIN) {
                    puts("Access denied. Only administrators can delete books.");
                    log_event(library.username, "unauthorized-book-delete-attempt");
                } else {
                    delete_book(&library);
                }
                break;
            case 6: register_member(&library); break;
            case 7: list_members(&library); break;
            case 8: search_members(&library); break;
            case 9: update_member(&library); break;
            case 10:
                if (library.role != ROLE_ADMIN) {
                    puts("Access denied. Only administrators can delete members.");
                    log_event(library.username, "unauthorized-member-delete-attempt");
                } else {
                    delete_member(&library);
                }
                break;
            case 11: issue_book(&library); break;
            case 12: return_book(&library); break;
            case 13: list_transactions(&library); break;
            case 14: show_fine_details(&library); break;
            case 0:
                puts("Exiting Library Management System.");
                log_event(library.username, "logout");
                break;
            default:
                puts("Invalid menu option.");
                break;
        }
    } while (choice != 0);

    free_library(&library);
    return 0;
}
