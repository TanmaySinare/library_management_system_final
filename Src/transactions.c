#include "library.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int book_index_by_id(const Library *lib, int id)
{
    size_t i;
    for (i = 0U; i < lib->book_count; i++) {
        if (lib->books[i].id == id) return (int)i;
    }
    return -1;
}

static int member_exists(const Library *lib, int id)
{
    size_t i;
    for (i = 0U; i < lib->member_count; i++) {
        if (lib->members[i].id == id) return 1;
    }
    return 0;
}

static int transaction_index_by_id(const Library *lib, int id)
{
    size_t i;
    for (i = 0U; i < lib->transaction_count; i++) {
        if (lib->transactions[i].id == id) return (int)i;
    }
    return -1;
}

static int next_transaction_id(const Library *lib)
{
    size_t i;
    int max_id = 0;

    for (i = 0U; i < lib->transaction_count; i++) {
        if (lib->transactions[i].id > max_id) max_id = lib->transactions[i].id;
    }
    if (max_id >= INT_MAX) return 0;
    return max_id + 1;
}

void issue_book(Library *lib)
{
    int book_id;
    int member_id;
    int book_index;
    int date_ok;
    long difference;
    char today[DATE_LEN];
    char due_date[DATE_LEN];
    Book old_book;
    Transaction transaction;

    book_id = read_int("Book ID: ", 1, 2147483647);
    member_id = read_int("Member ID: ", 1, 2147483647);

    if (!member_exists(lib, member_id)) {
        puts("Member does not exist. Register the member first.");
        return;
    }

    book_index = book_index_by_id(lib, book_id);
    if (book_index < 0) {
        puts("Book not found.");
        return;
    }
    if (lib->books[book_index].available_copies <= 0) {
        puts("No copies are available for issue.");
        return;
    }

    get_today(today);
    printf("Issue date: %s\n", today);
    read_line("Due date (YYYY-MM-DD): ", due_date, sizeof due_date);

    if (!valid_date(due_date)) {
        puts("Invalid due date. Use YYYY-MM-DD.");
        return;
    }
    difference = days_between_dates(today, due_date, &date_ok);
    if (!date_ok || difference < 0L) {
        puts("Due date cannot be earlier than the issue date.");
        return;
    }

    if (lib->transaction_count >= 2147483646U) {
        puts("Transaction limit reached.");
        return;
    }
    if (!ensure_transaction_capacity(lib, lib->transaction_count + 1U)) {
        puts("Not enough memory to create transaction.");
        return;
    }

    memset(&transaction, 0, sizeof transaction);
    transaction.id = next_transaction_id(lib);
    if (transaction.id <= 0) {
        puts("No transaction IDs are available.");
        return;
    }
    transaction.book_id = book_id;
    transaction.member_id = member_id;
    (void)strncpy(transaction.issue_date, today, DATE_LEN - 1U);
    (void)strncpy(transaction.due_date, due_date, DATE_LEN - 1U);
    (void)strcpy(transaction.return_date, "-");
    transaction.returned = 0;
    transaction.fine = 0.0;

    old_book = lib->books[book_index];
    lib->books[book_index].available_copies--;
    lib->transactions[lib->transaction_count++] = transaction;

    if (!save_books(lib)) {
        lib->books[book_index] = old_book;
        lib->transaction_count--;
        puts("Issue cancelled because book storage failed.");
        return;
    }
    if (!save_transactions(lib)) {
        lib->books[book_index] = old_book;
        lib->transaction_count--;
        (void)save_books(lib);
        puts("Issue cancelled because transaction storage failed.");
        return;
    }

    log_event(lib->username, "book-issued");
    printf("Book issued successfully. Transaction ID: %d\n", transaction.id);
}

void return_book(Library *lib)
{
    int transaction_id;
    int transaction_index;
    int book_index;
    int date_ok;
    long overdue_days;
    char actual_return_date[DATE_LEN];
    Transaction old_transaction;
    Book old_book;

    transaction_id = read_int("Transaction ID: ", 1, 2147483647);
    transaction_index = transaction_index_by_id(lib, transaction_id);

    if (transaction_index < 0) {
        puts("Transaction not found.");
        return;
    }
    if (lib->transactions[transaction_index].returned) {
        puts("This transaction is already marked as returned.");
        return;
    }

    book_index = book_index_by_id(lib, lib->transactions[transaction_index].book_id);
    if (book_index < 0) {
        puts("The related book record is missing. Please contact the administrator.");
        return;
    }

    read_line("Actual return date (YYYY-MM-DD): ", actual_return_date, sizeof actual_return_date);
    if (!valid_date(actual_return_date)) {
        puts("Invalid return date. Use YYYY-MM-DD.");
        return;
    }

    overdue_days = days_between_dates(lib->transactions[transaction_index].issue_date,
                                      actual_return_date, &date_ok);
    if (!date_ok || overdue_days < 0L) {
        puts("Return date cannot be earlier than the issue date.");
        return;
    }
    overdue_days = days_between_dates(lib->transactions[transaction_index].due_date,
                                      actual_return_date, &date_ok);
    if (!date_ok) {
        puts("Could not calculate overdue days because a stored date is invalid.");
        return;
    }
    if (overdue_days < 0L) overdue_days = 0L;

    old_transaction = lib->transactions[transaction_index];
    old_book = lib->books[book_index];

    lib->transactions[transaction_index].returned = 1;
    (void)strncpy(lib->transactions[transaction_index].return_date,
                  actual_return_date, DATE_LEN - 1U);
    lib->transactions[transaction_index].fine = (double)overdue_days * FINE_PER_DAY;
    if (lib->books[book_index].available_copies >= lib->books[book_index].total_copies) {
        lib->transactions[transaction_index] = old_transaction;
        puts("Inventory is inconsistent. Return cancelled.");
        return;
    }
    lib->books[book_index].available_copies++;

    if (!save_books(lib)) {
        lib->transactions[transaction_index] = old_transaction;
        lib->books[book_index] = old_book;
        puts("Return cancelled because book storage failed.");
        return;
    }
    if (!save_transactions(lib)) {
        lib->transactions[transaction_index] = old_transaction;
        lib->books[book_index] = old_book;
        (void)save_books(lib);
        puts("Return cancelled because transaction storage failed.");
        return;
    }

    log_event(lib->username, "book-returned");
    printf("Book returned successfully.\nOverdue days: %ld\nFine: Rs. %.2f\n",
           overdue_days, lib->transactions[transaction_index].fine);
}

void list_transactions(const Library *lib)
{
    size_t i;

    puts("\n================ TRANSACTION HISTORY ================");
    printf("%-6s %-8s %-10s %-12s %-12s %-12s %-10s %-10s\n",
           "TX ID", "Book ID", "Member ID", "Issue Date", "Due Date",
           "Return Date", "Status", "Fine");
    puts("-------------------------------------------------------------------------------------");

    if (lib->transaction_count == 0U) {
        puts("No transactions found.");
        return;
    }

    for (i = 0U; i < lib->transaction_count; i++) {
        const Transaction *t = &lib->transactions[i];
        printf("%-6d %-8d %-10d %-12s %-12s %-12s %-10s Rs.%-7.2f\n",
               t->id, t->book_id, t->member_id, t->issue_date, t->due_date,
               t->returned ? t->return_date : "-",
               t->returned ? "Returned" : "Issued", t->fine);
    }
}

void show_fine_details(const Library *lib)
{
    size_t i;
    int found = 0;
    double total = 0.0;

    printf("\nFine rate: Rs. %.2f per overdue day\n", FINE_PER_DAY);
    puts("================ FINE DETAILS ================");

    for (i = 0U; i < lib->transaction_count; i++) {
        const Transaction *t = &lib->transactions[i];
        if (t->returned) {
            printf("Transaction %d | Member %d | Book %d | Fine: Rs. %.2f\n",
                   t->id, t->member_id, t->book_id, t->fine);
            total += t->fine;
            found = 1;
        }
    }

    if (!found) puts("No returned-book fine records found.");
    printf("Total recorded fines: Rs. %.2f\n", total);
}
