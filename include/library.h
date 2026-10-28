#ifndef LIBRARY_H
#define LIBRARY_H

#include <stddef.h>

#define TIMESTAMP_LEN 20

#define TITLE_LEN 100
#define AUTHOR_LEN 100
#define CATEGORY_LEN 50
#define MEMBER_NAME_LEN 100
#define PHONE_LEN 25
#define DATE_LEN 11
#define FINE_PER_DAY 5.0

#define BOOKS_FILE "books.txt"
#define MEMBERS_FILE "members.txt"
#define TRANSACTIONS_FILE "transactions.txt"
#define LOG_FILE "library.log"

typedef enum {
    ROLE_NONE = 0,
    ROLE_LIBRARIAN,
    ROLE_ADMIN
} Role;

typedef struct {
    int id;
    char title[TITLE_LEN];
    char author[AUTHOR_LEN];
    char category[CATEGORY_LEN];
    int total_copies;
    int available_copies;
} Book;

typedef struct {
    int id;
    char name[MEMBER_NAME_LEN];
    char phone[PHONE_LEN];
    char added_at[TIMESTAMP_LEN];
} Member;

typedef struct {
    int id;
    int book_id;
    int member_id;
    char issue_date[DATE_LEN];
    char due_date[DATE_LEN];
    char return_date[DATE_LEN]; /* "-" means not returned yet */
    int returned;
    double fine;
} Transaction;

typedef struct {
    Book *books;
    size_t book_count;
    size_t book_capacity;

    Member *members;
    size_t member_count;
    size_t member_capacity;

    Transaction *transactions;
    size_t transaction_count;
    size_t transaction_capacity;

    Role role;
    char username[32];
} Library;

/* util.c */
void read_line(const char *prompt, char *buffer, size_t buffer_size);
int read_int(const char *prompt, int min_value, int max_value);
int text_field_valid(const char *value, size_t maximum_length);
int contains_ignore_case(const char *text, const char *query);
int valid_date(const char *date);
long days_between_dates(const char *start_date, const char *end_date, int *ok);
void get_today(char output[DATE_LEN]);
int confirm_action(const char *prompt);
void log_event(const char *username, const char *event);
const char *role_name(Role role);
int ensure_book_capacity(Library *lib, size_t needed);
int ensure_member_capacity(Library *lib, size_t needed);
int ensure_transaction_capacity(Library *lib, size_t needed);
void free_library(Library *lib);

/* auth.c */
Role authenticate_user(char username_out[32]);

/* storage.c */
int load_data(Library *lib);
int save_books(const Library *lib);
int save_members(const Library *lib);
int save_transactions(const Library *lib);

/* books.c */
void add_book(Library *lib);
void list_books(const Library *lib);
void search_books(const Library *lib);
void update_book(Library *lib);
void delete_book(Library *lib);

/* members.c */
void register_member(Library *lib);
void list_members(const Library *lib);
void search_members(const Library *lib);
void update_member(Library *lib);
void delete_member(Library *lib);

/* transactions.c */
void issue_book(Library *lib);
void return_book(Library *lib);
void list_transactions(const Library *lib);
void show_fine_details(const Library *lib);

#endif
