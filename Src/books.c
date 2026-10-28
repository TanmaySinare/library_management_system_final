#include "library.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int book_index_by_id(const Library *lib, int id)
{
    size_t i;
    for (i = 0U; i < lib->book_count; i++) {
        if (lib->books[i].id == id) return (int)i;
    }
    return -1;
}

static int book_has_active_transaction(const Library *lib, int id)
{
    size_t i;
    for (i = 0U; i < lib->transaction_count; i++) {
        if (lib->transactions[i].book_id == id &&
            lib->transactions[i].returned == 0) return 1;
    }
    return 0;
}

static int get_required_field(const char *prompt, char *buffer, size_t size)
{
    read_line(prompt, buffer, size);
    if (!text_field_valid(buffer, size)) {
        puts("Value cannot be empty, too long, or contain the | character.");
        return 0;
    }
    return 1;
}

void add_book(Library *lib)
{
    Book book;
    size_t i;

    book.id = read_int("Book ID (positive number): ", 1, 2147483647);
    for (i = 0U; i < lib->book_count; i++) {
        if (lib->books[i].id == book.id) {
            puts("That Book ID already exists.");
            return;
        }
    }

    if (!get_required_field("Title: ", book.title, sizeof book.title) ||
        !get_required_field("Author: ", book.author, sizeof book.author) ||
        !get_required_field("Category: ", book.category, sizeof book.category)) {
        return;
    }

    book.total_copies = read_int("Total copies: ", 1, 1000000);
    book.available_copies = book.total_copies;

    if (!ensure_book_capacity(lib, lib->book_count + 1U)) {
        puts("Not enough memory to add the book.");
        return;
    }

    lib->books[lib->book_count++] = book;
    if (!save_books(lib)) {
        lib->book_count--;
        puts("Book was not added because storage failed.");
        return;
    }

    log_event(lib->username, "book-added");
    puts("Book added successfully.");
}

void list_books(const Library *lib)
{
    size_t i;

    puts("\n================ ALL BOOKS ================");
    printf("%-8s %-25s %-20s %-15s %-8s %-10s\n",
           "Book ID", "Title", "Author", "Category", "Total", "Available");
    puts("----------------------------------------------------------------------------------------");

    if (lib->book_count == 0U) {
        puts("No books found. Please add a book first.");
        return;
    }

    for (i = 0U; i < lib->book_count; i++) {
        const Book *b = &lib->books[i];
        printf("%-8d %-25.25s %-20.20s %-15.15s %-8d %-10d\n",
               b->id, b->title, b->author, b->category,
               b->total_copies, b->available_copies);
    }
}

void search_books(const Library *lib)
{
    char query[100];
    size_t i;
    int found = 0;
    int numeric_id = 0;
    char *end = NULL;
    long maybe_id;

    if (lib->book_count == 0U) {
        puts("No books are stored yet.");
        return;
    }

    read_line("Search by Book ID, title, author, or category: ", query, sizeof query);
    maybe_id = strtol(query, &end, 10);
    if (query[0] != '\0' && end != query && *end == '\0' &&
        maybe_id > 0 && maybe_id <= 2147483647L) {
        numeric_id = (int)maybe_id;
    }

    puts("\nSearch results:");
    for (i = 0U; i < lib->book_count; i++) {
        const Book *b = &lib->books[i];
        if ((numeric_id != 0 && b->id == numeric_id) ||
            contains_ignore_case(b->title, query) ||
            contains_ignore_case(b->author, query) ||
            contains_ignore_case(b->category, query)) {
            printf("ID: %d | Title: %s | Author: %s | Category: %s | Available: %d/%d\n",
                   b->id, b->title, b->author, b->category,
                   b->available_copies, b->total_copies);
            found = 1;
        }
    }
    if (!found) puts("No matching book found.");
}

void update_book(Library *lib)
{
    int id = read_int("Book ID to update: ", 1, 2147483647);
    int index = book_index_by_id(lib, id);
    Book old_book;
    Book updated;
    int issued;

    if (index < 0) {
        puts("Book not found.");
        return;
    }

    old_book = lib->books[index];
    updated = old_book;
    printf("Current title: %s\n", old_book.title);

    if (!get_required_field("New title: ", updated.title, sizeof updated.title) ||
        !get_required_field("New author: ", updated.author, sizeof updated.author) ||
        !get_required_field("New category: ", updated.category, sizeof updated.category)) {
        return;
    }

    issued = old_book.total_copies - old_book.available_copies;
    updated.total_copies = read_int("New total copies: ", 1, 1000000);
    if (updated.total_copies < issued) {
        puts("Total copies cannot be less than the number currently issued.");
        return;
    }
    updated.available_copies = updated.total_copies - issued;

    lib->books[index] = updated;
    if (!save_books(lib)) {
        lib->books[index] = old_book;
        puts("Update cancelled because storage failed.");
        return;
    }
    log_event(lib->username, "book-updated");
    puts("Book updated successfully.");
}

void delete_book(Library *lib)
{
    int id = read_int("Book ID to delete: ", 1, 2147483647);
    int index = book_index_by_id(lib, id);
    Book old_book;
    size_t i;

    if (index < 0) {
        puts("Book not found.");
        return;
    }
    if (book_has_active_transaction(lib, id)) {
        puts("Cannot delete this book while it has an active issue transaction.");
        return;
    }
    if (!confirm_action("Delete this book? (y/n): ")) {
        puts("Delete cancelled.");
        return;
    }

    old_book = lib->books[index];
    for (i = (size_t)index; i + 1U < lib->book_count; i++) {
        lib->books[i] = lib->books[i + 1U];
    }
    lib->book_count--;

    if (!save_books(lib)) {
        for (i = lib->book_count; i > (size_t)index; i--) {
            lib->books[i] = lib->books[i - 1U];
        }
        lib->books[index] = old_book;
        lib->book_count++;
        puts("Delete cancelled because storage failed.");
        return;
    }
    log_event(lib->username, "book-deleted");
    puts("Book deleted successfully.");
}
