#include "library.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void read_line(const char *prompt, char *buffer, size_t buffer_size)
{
    size_t length;
    int ch;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }

    printf("%s", prompt);
    if (fgets(buffer, (int)buffer_size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }

    length = strlen(buffer);
    if (length > 0U && buffer[length - 1U] == '\n') {
        buffer[length - 1U] = '\0';
    } else {
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* Discard the remainder of an overlong input line. */
        }
    }

    length = strlen(buffer);
    if (length > 0U && buffer[length - 1U] == '\r') {
        buffer[length - 1U] = '\0';
    }
}

int read_int(const char *prompt, int min_value, int max_value)
{
    char buffer[128];
    char *end = NULL;
    long value;
    size_t i;

    for (;;) {
        read_line(prompt, buffer, sizeof buffer);

        i = 0U;
        while (isspace((unsigned char)buffer[i])) {
            i++;
        }
        if (buffer[i] == '\0') {
            puts("Please enter a number.");
            continue;
        }

        value = strtol(buffer, &end, 10);
        if (end == buffer) {
            puts("Invalid number. Try again.");
            continue;
        }

        while (isspace((unsigned char)*end)) {
            end++;
        }
        if (*end != '\0' || value < min_value || value > max_value) {
            printf("Enter a value between %d and %d.\n", min_value, max_value);
            continue;
        }
        return (int)value;
    }
}

/* Fields use a pipe-delimited text format, so pipes and control characters are rejected. */
int text_field_valid(const char *value, size_t maximum_length)
{
    size_t i;
    size_t length;

    if (value == NULL) {
        return 0;
    }
    length = strlen(value);
    if (length == 0U || length >= maximum_length) {
        return 0;
    }

    for (i = 0U; i < length; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (ch == '|' || ch == '\n' || ch == '\r' || ch < 32U) {
            return 0;
        }
    }
    return 1;
}

int contains_ignore_case(const char *text, const char *query)
{
    size_t i;
    size_t j;

    if (text == NULL || query == NULL) {
        return 0;
    }
    if (*query == '\0') {
        return 1;
    }

    for (i = 0U; text[i] != '\0'; i++) {
        for (j = 0U; query[j] != '\0'; j++) {
            if (text[i + j] == '\0' ||
                tolower((unsigned char)text[i + j]) !=
                tolower((unsigned char)query[j])) {
                break;
            }
        }
        if (query[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

int valid_date(const char *date)
{
    int year;
    int month;
    int day;
    char extra;
    struct tm value;
    time_t normalized;

    size_t i;
    if (date == NULL || strlen(date) != 10U ||
        date[4] != '-' || date[7] != '-') {
        return 0;
    }
    for (i = 0U; i < 10U; i++) {
        if (i != 4U && i != 7U && !isdigit((unsigned char)date[i])) {
            return 0;
        }
    }
    if (sscanf(date, "%4d-%2d-%2d%c", &year, &month, &day, &extra) != 3) {
        return 0;
    }
    if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    memset(&value, 0, sizeof value);
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = 12;
    value.tm_isdst = -1;
    normalized = mktime(&value);

    if (normalized == (time_t)-1) {
        return 0;
    }
    return value.tm_year == year - 1900 &&
           value.tm_mon == month - 1 &&
           value.tm_mday == day;
}

/* Civil-date ordinal calculation makes date subtraction independent of DST. */
static long date_ordinal(int year, unsigned int month, unsigned int day)
{
    long era;
    unsigned int year_of_era;
    unsigned int adjusted_month;
    unsigned int day_of_year;
    unsigned int day_of_era;

    year -= (month <= 2U) ? 1 : 0;
    era = (year >= 0 ? year : year - 399) / 400;
    year_of_era = (unsigned int)(year - (int)(era * 400));
    adjusted_month = (unsigned int)((int)month + ((month > 2U) ? -3 : 9));
    day_of_year = (153U * adjusted_month + 2U) / 5U + day - 1U;
    day_of_era = year_of_era * 365U + year_of_era / 4U -
                 year_of_era / 100U + day_of_year;
    return era * 146097L + (long)day_of_era;
}

long days_between_dates(const char *start_date, const char *end_date, int *ok)
{
    int y1, m1, d1;
    int y2, m2, d2;

    if (ok == NULL) {
        return 0L;
    }
    *ok = 0;

    if (!valid_date(start_date) || !valid_date(end_date) ||
        sscanf(start_date, "%d-%d-%d", &y1, &m1, &d1) != 3 ||
        sscanf(end_date, "%d-%d-%d", &y2, &m2, &d2) != 3) {
        return 0L;
    }

    *ok = 1;
    return date_ordinal(y2, (unsigned int)m2, (unsigned int)d2) -
           date_ordinal(y1, (unsigned int)m1, (unsigned int)d1);
}

void get_today(char output[DATE_LEN])
{
    time_t now = time(NULL);
    struct tm *local = localtime(&now);

    if (local == NULL) {
        strcpy(output, "2000-01-01");
        return;
    }
    (void)strftime(output, DATE_LEN, "%Y-%m-%d", local);
}

int confirm_action(const char *prompt)
{
    char answer[16];

    read_line(prompt, answer, sizeof answer);
    return answer[0] == 'y' || answer[0] == 'Y';
}

void log_event(const char *username, const char *event)
{
    FILE *file = fopen("library.log", "a");
    time_t now;
    struct tm *local;
    char timestamp[32];

    if (file == NULL) {
        /* Logging failure must not crash the core application. */
        return;
    }

    now = time(NULL);
    local = localtime(&now);
    if (local != NULL) {
        (void)strftime(timestamp, sizeof timestamp, "%Y-%m-%d %H:%M:%S", local);
    } else {
        strcpy(timestamp, "unknown-time");
    }

    /* Never log passwords or other authentication secrets. */
    (void)fprintf(file, "%s | user=%s | event=%s\n",
                  timestamp,
                  (username != NULL && username[0] != '\0') ? username : "unknown",
                  (event != NULL) ? event : "unspecified");
    (void)fclose(file);
}

const char *role_name(Role role)
{
    switch (role) {
        case ROLE_ADMIN: return "Administrator";
        case ROLE_LIBRARIAN: return "Librarian";
        default: return "Unauthenticated";
    }
}

int ensure_book_capacity(Library *lib, size_t needed)
{
    Book *temp;
    size_t new_capacity;

    if (needed <= lib->book_capacity) return 1;
    new_capacity = (lib->book_capacity == 0U) ? 8U : lib->book_capacity;
    while (new_capacity < needed) {
        if (new_capacity > ((size_t)-1) / 2U) return 0;
        new_capacity *= 2U;
    }
    if (new_capacity > ((size_t)-1) / sizeof(Book)) return 0;
    temp = (Book *)realloc(lib->books, new_capacity * sizeof(Book));
    if (temp == NULL) return 0;
    lib->books = temp;
    lib->book_capacity = new_capacity;
    return 1;
}

int ensure_member_capacity(Library *lib, size_t needed)
{
    Member *temp;
    size_t new_capacity;

    if (needed <= lib->member_capacity) return 1;
    new_capacity = (lib->member_capacity == 0U) ? 8U : lib->member_capacity;
    while (new_capacity < needed) {
        if (new_capacity > ((size_t)-1) / 2U) return 0;
        new_capacity *= 2U;
    }
    if (new_capacity > ((size_t)-1) / sizeof(Member)) return 0;
    temp = (Member *)realloc(lib->members, new_capacity * sizeof(Member));
    if (temp == NULL) return 0;
    lib->members = temp;
    lib->member_capacity = new_capacity;
    return 1;
}

int ensure_transaction_capacity(Library *lib, size_t needed)
{
    Transaction *temp;
    size_t new_capacity;

    if (needed <= lib->transaction_capacity) return 1;
    new_capacity = (lib->transaction_capacity == 0U) ? 8U : lib->transaction_capacity;
    while (new_capacity < needed) {
        if (new_capacity > ((size_t)-1) / 2U) return 0;
        new_capacity *= 2U;
    }
    if (new_capacity > ((size_t)-1) / sizeof(Transaction)) return 0;
    temp = (Transaction *)realloc(lib->transactions, new_capacity * sizeof(Transaction));
    if (temp == NULL) return 0;
    lib->transactions = temp;
    lib->transaction_capacity = new_capacity;
    return 1;
}

void free_library(Library *lib)
{
    if (lib == NULL) return;
    free(lib->books);
    free(lib->members);
    free(lib->transactions);
    lib->books = NULL;
    lib->members = NULL;
    lib->transactions = NULL;
    lib->book_count = lib->member_count = lib->transaction_count = 0U;
    lib->book_capacity = lib->member_capacity = lib->transaction_capacity = 0U;
}
