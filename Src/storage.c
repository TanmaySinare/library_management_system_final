#include "library.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/* Replace the target using a backup, so a failed write does not erase old data. */
static int replace_with_backup(const char *temporary, const char *target,
                               const char *backup)
{
    FILE *check;
    int had_target = 0;

    check = fopen(target, "r");
    if (check != NULL) {
        had_target = 1;
        (void)fclose(check);
    } else if (errno != ENOENT) {
        perror(target);
        (void)remove(temporary);
        return 0;
    }

    (void)remove(backup);
    if (had_target && rename(target, backup) != 0) {
        perror("Could not back up the existing data file");
        (void)remove(temporary);
        return 0;
    }

    if (rename(temporary, target) != 0) {
        perror("Could not replace the data file");
        if (had_target) {
            (void)rename(backup, target);
        }
        (void)remove(temporary);
        return 0;
    }

    (void)remove(backup);
    return 1;
}

int save_books(const Library *lib)
{
    FILE *file = fopen("books.tmp", "w");
    size_t i;
    int ok = 1;

    if (file == NULL) {
        perror("Cannot write books.tmp");
        return 0;
    }

    for (i = 0U; i < lib->book_count; i++) {
        const Book *b = &lib->books[i];
        if (fprintf(file, "%d|%s|%s|%s|%d|%d\n", b->id, b->title, b->author,
                    b->category, b->total_copies, b->available_copies) < 0) {
            ok = 0;
            break;
        }
    }
    if (fclose(file) != 0) ok = 0;
    if (!ok) {
        (void)remove("books.tmp");
        puts("Unable to write all book records.");
        return 0;
    }
    return replace_with_backup("books.tmp", BOOKS_FILE, "books.bak");
}

int save_members(const Library *lib)
{
    FILE *file = fopen("members.tmp", "w");
    size_t i;
    int ok = 1;

    if (file == NULL) {
        perror("Cannot write members.tmp");
        return 0;
    }

    for (i = 0U; i < lib->member_count; i++) {
        const Member *m = &lib->members[i];
        if (fprintf(file, "%d|%s|%s|%s\n", m->id, m->name, m->phone, m->added_at) < 0) {
            ok = 0;
            break;
        }
    }
    if (fclose(file) != 0) ok = 0;
    if (!ok) {
        (void)remove("members.tmp");
        puts("Unable to write all member records.");
        return 0;
    }
    return replace_with_backup("members.tmp", MEMBERS_FILE, "members.bak");
}

int save_transactions(const Library *lib)
{
    FILE *file = fopen("transactions.tmp", "w");
    size_t i;
    int ok = 1;

    if (file == NULL) {
        perror("Cannot write transactions.tmp");
        return 0;
    }

    for (i = 0U; i < lib->transaction_count; i++) {
        const Transaction *t = &lib->transactions[i];
        if (fprintf(file, "%d|%d|%d|%s|%s|%s|%d|%.2f\n",
                    t->id, t->book_id, t->member_id, t->issue_date,
                    t->due_date, t->return_date, t->returned, t->fine) < 0) {
            ok = 0;
            break;
        }
    }
    if (fclose(file) != 0) ok = 0;
    if (!ok) {
        (void)remove("transactions.tmp");
        puts("Unable to write all transaction records.");
        return 0;
    }
    return replace_with_backup("transactions.tmp", TRANSACTIONS_FILE,
                               "transactions.bak");
}

static int load_books(Library *lib)
{
    FILE *file = fopen(BOOKS_FILE, "r");
    char line[512];
    unsigned long line_number = 0UL;

    if (file == NULL) {
        if (errno == ENOENT) return 1;
        perror("Cannot open books.txt");
        return 0;
    }

    while (fgets(line, sizeof line, file) != NULL) {
        Book b;
        int used = 0;
        line_number++;
        if (sscanf(line, "%d|%99[^|]|%99[^|]|%49[^|]|%d|%d %n",
                   &b.id, b.title, b.author, b.category,
                   &b.total_copies, &b.available_copies, &used) != 6) {
            fprintf(stderr, "Invalid record in %s at line %lu.\n", BOOKS_FILE, line_number);
            (void)fclose(file);
            return 0;
        }
        while (line[used] == ' ' || line[used] == '\t' ||
               line[used] == '\r' || line[used] == '\n') used++;
        if (line[used] != '\0' || b.id <= 0 || b.total_copies <= 0 ||
            b.available_copies < 0 || b.available_copies > b.total_copies ||
            !text_field_valid(b.title, sizeof b.title) ||
            !text_field_valid(b.author, sizeof b.author) ||
            !text_field_valid(b.category, sizeof b.category) ||
            !ensure_book_capacity(lib, lib->book_count + 1U)) {
            fprintf(stderr, "Invalid or excessive book data in %s at line %lu.\n",
                    BOOKS_FILE, line_number);
            (void)fclose(file);
            return 0;
        }
        lib->books[lib->book_count++] = b;
    }

    if (ferror(file)) {
        perror("Error reading books.txt");
        (void)fclose(file);
        return 0;
    }
    (void)fclose(file);
    return 1;
}

static int load_members(Library *lib)
{
    FILE *file = fopen(MEMBERS_FILE, "r");
    char line[512];
    unsigned long line_number = 0UL;

    if (file == NULL) {
        if (errno == ENOENT) return 1;
        perror("Cannot open members.txt");
        return 0;
    }

    while (fgets(line, sizeof line, file) != NULL) {
        Member m;
        int used = 0;
        int fields;
        line_number++;
        memset(&m, 0, sizeof m);
        fields = sscanf(line, "%d|%99[^|]|%24[^|\r\n]|%19[^\r\n] %n",
                        &m.id, m.name, m.phone, m.added_at, &used);
        /* Accept old records with no timestamp as well as new four-field records. */
        if (fields == 3) {
            used = (int)strlen(line);
            while (used > 0 && (line[used - 1] == '\n' || line[used - 1] == '\r')) used--;
            m.added_at[0] = '\0';
        } else if (fields != 4) {
            fprintf(stderr, "Invalid record in %s at line %lu.\n", MEMBERS_FILE, line_number);
            (void)fclose(file);
            return 0;
        }
        while (line[used] == ' ' || line[used] == '\t' ||
               line[used] == '\r' || line[used] == '\n') used++;
        if (line[used] != '\0' || m.id <= 0 ||
            !text_field_valid(m.name, sizeof m.name) ||
            !text_field_valid(m.phone, sizeof m.phone) ||
            !ensure_member_capacity(lib, lib->member_count + 1U)) {
            fprintf(stderr, "Invalid or excessive member data in %s at line %lu.\n",
                    MEMBERS_FILE, line_number);
            (void)fclose(file);
            return 0;
        }
        lib->members[lib->member_count++] = m;
    }

    if (ferror(file)) {
        perror("Error reading members.txt");
        (void)fclose(file);
        return 0;
    }
    (void)fclose(file);
    return 1;
}

static int load_transactions(Library *lib)
{
    FILE *file = fopen(TRANSACTIONS_FILE, "r");
    char line[512];
    unsigned long line_number = 0UL;

    if (file == NULL) {
        if (errno == ENOENT) return 1;
        perror("Cannot open transactions.txt");
        return 0;
    }

    while (fgets(line, sizeof line, file) != NULL) {
        Transaction t;
        int used = 0;
        line_number++;
        if (sscanf(line, "%d|%d|%d|%10[^|]|%10[^|]|%10[^|]|%d|%lf %n",
                   &t.id, &t.book_id, &t.member_id, t.issue_date, t.due_date,
                   t.return_date, &t.returned, &t.fine, &used) != 8) {
            fprintf(stderr, "Invalid record in %s at line %lu.\n", TRANSACTIONS_FILE, line_number);
            (void)fclose(file);
            return 0;
        }
        while (line[used] == ' ' || line[used] == '\t' ||
               line[used] == '\r' || line[used] == '\n') used++;
        if (line[used] != '\0' || t.id <= 0 || t.book_id <= 0 ||
            t.member_id <= 0 || (t.returned != 0 && t.returned != 1) ||
            t.fine < 0.0 || !valid_date(t.issue_date) || !valid_date(t.due_date) ||
            (t.returned && !valid_date(t.return_date)) ||
            !ensure_transaction_capacity(lib, lib->transaction_count + 1U)) {
            fprintf(stderr, "Invalid or excessive transaction data in %s at line %lu.\n",
                    TRANSACTIONS_FILE, line_number);
            (void)fclose(file);
            return 0;
        }
        lib->transactions[lib->transaction_count++] = t;
    }

    if (ferror(file)) {
        perror("Error reading transactions.txt");
        (void)fclose(file);
        return 0;
    }
    (void)fclose(file);
    return 1;
}

int load_data(Library *lib)
{
    if (lib == NULL) return 0;
    if (!load_books(lib) || !load_members(lib) || !load_transactions(lib)) {
        free_library(lib);
        return 0;
    }
    return 1;
}
