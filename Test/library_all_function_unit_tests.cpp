/*
 * GoogleTest unit-test suite for the uploaded Library Management System.
 *
 * Run this test executable from a disposable copy of the project.
 * The fixture changes to a fresh temporary directory per test, so text-file
 * writes do not overwrite the project's real books.txt/members.txt/transactions.txt.
 *
 * Build C files as C object files, then link them with this C++ file and
 * GoogleTest. EXCLUDE main.c to avoid a duplicate main().
 *
 * Linux / GitHub Codespaces test harness (uses POSIX stdin redirection).
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

extern "C" {
#include "library.h"
}

/* Include the application entry point with a different name so it can be
 * exercised by GoogleTest while gtest_main supplies the real process main.
 * This also instruments Src/main.c for the coverage build. */
#define main library_application_main
#include "../Src/main.c"
#undef main

static std::string g_temp_dir;
static char g_original_dir[PATH_MAX];

/* Create a filesystem socket node so fopen() fails with an error other than
 * ENOENT; this reliably exercises storage open-error handling on Linux. */
static int create_unix_socket_path(const char *path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un address = {};
    size_t path_length;
    int result;

    if (fd < 0 || path == nullptr) {
        if (fd >= 0) (void)close(fd);
        return 0;
    }
    path_length = std::strlen(path);
    if (path_length >= sizeof(address.sun_path)) {
        (void)close(fd);
        return 0;
    }
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path, path_length + 1U);
    (void)unlink(path);
    result = bind(fd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address));
    (void)close(fd);
    if (result != 0) (void)unlink(path);
    return result == 0 ? 1 : 0;
}

class LibraryTestEnvironment : public ::testing::Test {
protected:
    Library lib{};

    void SetUp() override {
        ASSERT_NE(getcwd(g_original_dir, sizeof(g_original_dir)), nullptr);
        char pattern[] = "/tmp/library_gtest_XXXXXX";
        char *dir = mkdtemp(pattern);
        ASSERT_NE(dir, nullptr);
        g_temp_dir = dir;
        ASSERT_EQ(chdir(g_temp_dir.c_str()), 0);
        std::memset(&lib, 0, sizeof(lib));
        std::strncpy(lib.username, "testuser", sizeof(lib.username) - 1);
        lib.role = ROLE_ADMIN;
    }

    void TearDown() override {
        free_library(&lib);
        (void)chdir(g_original_dir);
        if (!g_temp_dir.empty()) {
            std::string cmd = "rm -rf \"" + g_temp_dir + "\"";
            (void)std::system(cmd.c_str());
            g_temp_dir.clear();
        }
    }

    void SetInput(const std::string &input) {
        FILE *f = std::tmpfile();
        ASSERT_NE(f, nullptr);
        ASSERT_EQ(std::fwrite(input.data(), 1, input.size(), f), input.size());
        std::rewind(f);
        ASSERT_NE(dup2(fileno(f), STDIN_FILENO), -1);
        std::fclose(f);
        clearerr(stdin);
    }

    void RestoreInput() {
        // Most tests consume all supplied input; stdin is reset before each test
        // by the test runner process. Do not call further interactive functions
        // after the input stream has been exhausted.
    }

    void AddBook(int id = 101, const char *title = "Wings Of Fire",
                 int total = 2, int available = 2) {
        ASSERT_EQ(ensure_book_capacity(&lib, lib.book_count + 1), 1);
        Book &b = lib.books[lib.book_count++];
        std::memset(&b, 0, sizeof(b));
        b.id = id;
        std::strncpy(b.title, title, sizeof(b.title) - 1);
        std::strncpy(b.author, "A P J Abdul Kalam", sizeof(b.author) - 1);
        std::strncpy(b.category, "Biography", sizeof(b.category) - 1);
        b.total_copies = total;
        b.available_copies = available;
    }

    void AddMember(int id = 201) {
        ASSERT_EQ(ensure_member_capacity(&lib, lib.member_count + 1), 1);
        Member &m = lib.members[lib.member_count++];
        std::memset(&m, 0, sizeof(m));
        m.id = id;
        std::strncpy(m.name, "Rahul Patil", sizeof(m.name) - 1);
        std::strncpy(m.phone, "9876543210", sizeof(m.phone) - 1);
    }

    void AddTransaction(int id = 1, int book_id = 101, int member_id = 201,
                        int returned = 0) {
        ASSERT_EQ(ensure_transaction_capacity(&lib, lib.transaction_count + 1), 1);
        Transaction &t = lib.transactions[lib.transaction_count++];
        std::memset(&t, 0, sizeof(t));
        t.id = id;
        t.book_id = book_id;
        t.member_id = member_id;
        std::strcpy(t.issue_date, "2026-10-19");
        std::strcpy(t.due_date, "2026-10-29");
        std::strcpy(t.return_date, returned ? "2026-10-30" : "-");
        t.returned = returned;
        t.fine = returned ? 5.0 : 0.0;
    }
};

/* ------------------------------ util.c ------------------------------ */

TEST_F(LibraryTestEnvironment, ValidDateAcceptsCorrectDate) {
    EXPECT_EQ(valid_date("2026-11-02"), 1);
}
TEST_F(LibraryTestEnvironment, ValidDateRejectsBadFormat) {
    EXPECT_EQ(valid_date("2026-11-2"), 0);
    EXPECT_EQ(valid_date("20261102"), 0);
    EXPECT_EQ(valid_date(nullptr), 0);
}
TEST_F(LibraryTestEnvironment, ValidDateRejectsImpossibleDates) {
    EXPECT_EQ(valid_date("2026-02-30"), 0);
    EXPECT_EQ(valid_date("2026-13-01"), 0);
    EXPECT_EQ(valid_date("2026-00-01"), 0);
}
TEST_F(LibraryTestEnvironment, ValidDateLeapYears) {
    EXPECT_EQ(valid_date("2028-02-29"), 1);
    EXPECT_EQ(valid_date("2027-02-29"), 0);
}
TEST_F(LibraryTestEnvironment, TextFieldValidation) {
    EXPECT_EQ(text_field_valid("Wings Of Fire", 100), 1);
    EXPECT_EQ(text_field_valid("", 100), 0);
    EXPECT_EQ(text_field_valid(nullptr, 100), 0);
    EXPECT_EQ(text_field_valid("bad|field", 100), 0);
    EXPECT_EQ(text_field_valid("12345", 5), 0);
}
TEST_F(LibraryTestEnvironment, CaseInsensitiveSearch) {
    EXPECT_EQ(contains_ignore_case("Wings Of Fire", "wINGS"), 1);
    EXPECT_EQ(contains_ignore_case("Wings Of Fire", "Java"), 0);
    EXPECT_EQ(contains_ignore_case("anything", ""), 1);
    EXPECT_EQ(contains_ignore_case(nullptr, "x"), 0);
    EXPECT_EQ(contains_ignore_case("abc", nullptr), 0);
}
TEST_F(LibraryTestEnvironment, DateDifferenceAndOkFlag) {
    int ok = 0;
    EXPECT_EQ(days_between_dates("2026-10-29", "2026-11-02", &ok), 4);
    EXPECT_EQ(ok, 1);
    EXPECT_EQ(days_between_dates("2026-11-02", "2026-10-29", &ok), -4);
    EXPECT_EQ(ok, 1);
    EXPECT_EQ(days_between_dates("bad", "2026-11-02", &ok), 0);
    EXPECT_EQ(ok, 0);
    EXPECT_EQ(days_between_dates("2026-01-01", "2026-01-02", nullptr), 0);
}
TEST_F(LibraryTestEnvironment, TodayUsesIsoDateFormat) {
    char today[DATE_LEN] = {};
    get_today(today);
    ASSERT_EQ(std::strlen(today), 10U);
    EXPECT_EQ(today[4], '-');
    EXPECT_EQ(today[7], '-');
    EXPECT_EQ(valid_date(today), 1);
}
TEST_F(LibraryTestEnvironment, RoleNames) {
    EXPECT_STREQ(role_name(ROLE_ADMIN), "Administrator");
    EXPECT_STREQ(role_name(ROLE_LIBRARIAN), "Librarian");
    EXPECT_STREQ(role_name(ROLE_NONE), "Unauthenticated");
}
TEST_F(LibraryTestEnvironment, MemoryCapacityAndFree) {
    ASSERT_EQ(ensure_book_capacity(&lib, 1), 1);
    ASSERT_EQ(ensure_member_capacity(&lib, 1), 1);
    ASSERT_EQ(ensure_transaction_capacity(&lib, 1), 1);
    EXPECT_GE(lib.book_capacity, 1U);
    EXPECT_GE(lib.member_capacity, 1U);
    EXPECT_GE(lib.transaction_capacity, 1U);
    free_library(&lib);
    EXPECT_EQ(lib.books, nullptr);
    EXPECT_EQ(lib.members, nullptr);
    EXPECT_EQ(lib.transactions, nullptr);
    EXPECT_EQ(lib.book_count, 0U);
    EXPECT_EQ(lib.member_count, 0U);
    EXPECT_EQ(lib.transaction_count, 0U);
}
TEST_F(LibraryTestEnvironment, FreeLibraryAcceptsNull) {
    free_library(nullptr);
}
TEST_F(LibraryTestEnvironment, ConfirmActionYesAndNo) {
    SetInput("y\n");
    EXPECT_EQ(confirm_action("Confirm? "), 1);
    SetInput("n\n");
    EXPECT_EQ(confirm_action("Confirm? "), 0);
}
TEST_F(LibraryTestEnvironment, ReadLineReadsText) {
    char buffer[32] = {};
    SetInput("hello world\n");
    read_line("Text: ", buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "hello world");
}
TEST_F(LibraryTestEnvironment, ReadIntRetriesUntilValid) {
    SetInput("abc\n5\n");
    EXPECT_EQ(read_int("Number: ", 1, 10), 5);
}
TEST_F(LibraryTestEnvironment, LogEventCreatesLog) {
    log_event("tester", "test-event");
    std::ifstream in(LOG_FILE);
    ASSERT_TRUE(in.good());
    std::stringstream contents;
    contents << in.rdbuf();
    EXPECT_NE(contents.str().find("user=tester"), std::string::npos);
    EXPECT_NE(contents.str().find("event=test-event"), std::string::npos);
}

/* ----------------------------- auth.c -------------------------------- */

TEST_F(LibraryTestEnvironment, AdminAuthenticationSucceeds) {
    char username[32] = {};
    SetInput("admin\nAdmin@123\n");
    EXPECT_EQ(authenticate_user(username), ROLE_ADMIN);
    EXPECT_STREQ(username, "admin");
}
TEST_F(LibraryTestEnvironment, LibrarianAuthenticationSucceeds) {
    char username[32] = {};
    SetInput("librarian\nLibrarian@123\n");
    EXPECT_EQ(authenticate_user(username), ROLE_LIBRARIAN);
    EXPECT_STREQ(username, "librarian");
}
TEST_F(LibraryTestEnvironment, AuthenticationFailsAfterThreeAttempts) {
    char username[32] = {};
    SetInput("wrong\nbad\nwrong\nbad\nwrong\nbad\n");
    EXPECT_EQ(authenticate_user(username), ROLE_NONE);
}

/* ----------------------------- storage.c ----------------------------- */

TEST_F(LibraryTestEnvironment, SavesAndLoadsRecords) {
    AddBook();
    AddMember();
    AddTransaction();
    ASSERT_EQ(save_books(&lib), 1);
    ASSERT_EQ(save_members(&lib), 1);
    ASSERT_EQ(save_transactions(&lib), 1);

    Library loaded{};
    ASSERT_EQ(load_data(&loaded), 1);
    ASSERT_EQ(loaded.book_count, 1U);
    ASSERT_EQ(loaded.member_count, 1U);
    ASSERT_EQ(loaded.transaction_count, 1U);
    EXPECT_EQ(loaded.books[0].id, 101);
    EXPECT_EQ(loaded.members[0].id, 201);
    EXPECT_EQ(loaded.transactions[0].id, 1);
    free_library(&loaded);
}
TEST_F(LibraryTestEnvironment, MissingDataFilesLoadAsEmpty) {
    EXPECT_EQ(load_data(&lib), 1);
    EXPECT_EQ(lib.book_count, 0U);
    EXPECT_EQ(lib.member_count, 0U);
    EXPECT_EQ(lib.transaction_count, 0U);
}
TEST_F(LibraryTestEnvironment, MalformedBookFileIsRejected) {
    std::ofstream out(BOOKS_FILE);
    out << "bad record\n";
    out.close();
    EXPECT_EQ(load_data(&lib), 0);
}
TEST_F(LibraryTestEnvironment, MalformedMemberFileIsRejected) {
    std::ofstream out(MEMBERS_FILE);
    out << "not|enough\n";
    out.close();
    EXPECT_EQ(load_data(&lib), 0);
}
TEST_F(LibraryTestEnvironment, MalformedTransactionFileIsRejected) {
    std::ofstream out(TRANSACTIONS_FILE);
    out << "bad transaction\n";
    out.close();
    EXPECT_EQ(load_data(&lib), 0);
}
TEST_F(LibraryTestEnvironment, NullLibraryLoadFails) {
    EXPECT_EQ(load_data(nullptr), 0);
}

/* ------------------------------ books.c ------------------------------ */

TEST_F(LibraryTestEnvironment, AddBookStoresValidBook) {
    SetInput("102\nClean Code\nRobert Martin\nProgramming\n3\n");
    add_book(&lib);
    ASSERT_EQ(lib.book_count, 1U);
    EXPECT_EQ(lib.books[0].id, 102);
    EXPECT_STREQ(lib.books[0].title, "Clean Code");
    EXPECT_EQ(lib.books[0].total_copies, 3);
    EXPECT_EQ(lib.books[0].available_copies, 3);
}
TEST_F(LibraryTestEnvironment, AddBookRejectsDuplicateId) {
    AddBook(101);
    SetInput("101\n");
    add_book(&lib);
    EXPECT_EQ(lib.book_count, 1U);
}
TEST_F(LibraryTestEnvironment, AddBookRejectsInvalidField) {
    SetInput("102\nBad|Title\n");
    add_book(&lib);
    EXPECT_EQ(lib.book_count, 0U);
}
TEST_F(LibraryTestEnvironment, ListBooksHandlesEmptyAndPopulated) {
    list_books(&lib);
    AddBook();
    list_books(&lib);
}
TEST_F(LibraryTestEnvironment, SearchBooksHandlesEmptyFoundAndMissing) {
    search_books(&lib);
    AddBook();
    SetInput("wings\n");
    search_books(&lib);
    SetInput("missing title\n");
    search_books(&lib);
    SetInput("101\n");
    search_books(&lib);
}
TEST_F(LibraryTestEnvironment, UpdateBookHandlesMissingAndSuccess) {
    SetInput("999\n");
    update_book(&lib);
    AddBook();
    SetInput("101\nNew Title\nNew Author\nNew Category\n4\n");
    update_book(&lib);
    EXPECT_STREQ(lib.books[0].title, "New Title");
    EXPECT_EQ(lib.books[0].total_copies, 4);
}
TEST_F(LibraryTestEnvironment, UpdateBookRejectsTotalBelowIssuedCopies) {
    AddBook(101, "Book", 3, 1);
    SetInput("101\nNew Title\nNew Author\nNew Category\n1\n");
    update_book(&lib);
    EXPECT_EQ(lib.books[0].total_copies, 3);
}
TEST_F(LibraryTestEnvironment, DeleteBookHandlesMissingCancelAndSuccess) {
    SetInput("999\n");
    delete_book(&lib);
    AddBook();
    SetInput("101\nn\n");
    delete_book(&lib);
    EXPECT_EQ(lib.book_count, 1U);
    SetInput("101\ny\n");
    delete_book(&lib);
    EXPECT_EQ(lib.book_count, 0U);
}
TEST_F(LibraryTestEnvironment, DeleteBookRejectsActiveTransaction) {
    AddBook();
    AddMember();
    AddTransaction();
    SetInput("101\n");
    delete_book(&lib);
    EXPECT_EQ(lib.book_count, 1U);
}

/* ----------------------------- members.c ----------------------------- */

TEST_F(LibraryTestEnvironment, RegisterMemberStoresValidMember) {
    SetInput("202\nMaya Shah\n9876543210\n");
    register_member(&lib);
    ASSERT_EQ(lib.member_count, 1U);
    EXPECT_EQ(lib.members[0].id, 202);
    EXPECT_STREQ(lib.members[0].name, "Maya Shah");
    ASSERT_EQ(std::strlen(lib.members[0].added_at), 19U);
    EXPECT_EQ(lib.members[0].added_at[4], '-');
    EXPECT_EQ(lib.members[0].added_at[7], '-');
    EXPECT_EQ(lib.members[0].added_at[10], ' ');
    EXPECT_EQ(lib.members[0].added_at[13], ':');
    EXPECT_EQ(lib.members[0].added_at[16], ':');
}
TEST_F(LibraryTestEnvironment, RegisterMemberRejectsDuplicateId) {
    AddMember(201);
    SetInput("201\n");
    register_member(&lib);
    EXPECT_EQ(lib.member_count, 1U);
}
TEST_F(LibraryTestEnvironment, RegisterMemberRejectsInvalidField) {
    SetInput("202\nBad|Name\n");
    register_member(&lib);
    EXPECT_EQ(lib.member_count, 0U);
}
TEST_F(LibraryTestEnvironment, ListMembersHandlesEmptyAndPopulated) {
    list_members(&lib);
    AddMember();
    list_members(&lib);
}
TEST_F(LibraryTestEnvironment, SearchMembersHandlesEmptyFoundAndMissing) {
    search_members(&lib);
    AddMember();
    SetInput("201\n");
    search_members(&lib);
    SetInput("Rahul\n");
    search_members(&lib);
    SetInput("missing\n");
    search_members(&lib);
}
TEST_F(LibraryTestEnvironment, UpdateMemberHandlesMissingAndSuccess) {
    SetInput("999\n");
    update_member(&lib);
    AddMember();
    SetInput("201\nMaya Shah\n9123456789\n");
    update_member(&lib);
    EXPECT_STREQ(lib.members[0].name, "Maya Shah");
    EXPECT_STREQ(lib.members[0].phone, "9123456789");
}
TEST_F(LibraryTestEnvironment, DeleteMemberHandlesMissingCancelAndSuccess) {
    SetInput("999\n");
    delete_member(&lib);
    AddMember();
    SetInput("201\nn\n");
    delete_member(&lib);
    EXPECT_EQ(lib.member_count, 1U);
    SetInput("201\ny\n");
    delete_member(&lib);
    EXPECT_EQ(lib.member_count, 0U);
}
TEST_F(LibraryTestEnvironment, DeleteMemberRejectsActiveLoan) {
    AddBook();
    AddMember();
    AddTransaction();
    SetInput("201\n");
    delete_member(&lib);
    EXPECT_EQ(lib.member_count, 1U);
}

/* --------------------------- transactions.c --------------------------- */

TEST_F(LibraryTestEnvironment, IssueBookSuccess) {
    AddBook();
    AddMember();
    SetInput("101\n201\n2099-12-31\n");
    issue_book(&lib);
    ASSERT_EQ(lib.transaction_count, 1U);
    EXPECT_EQ(lib.books[0].available_copies, 1);
    EXPECT_EQ(lib.transactions[0].returned, 0);
}
TEST_F(LibraryTestEnvironment, IssueBookRejectsUnknownMember) {
    AddBook();
    SetInput("101\n999\n");
    issue_book(&lib);
    EXPECT_EQ(lib.transaction_count, 0U);
}
TEST_F(LibraryTestEnvironment, IssueBookRejectsUnknownBook) {
    AddMember();
    SetInput("999\n201\n");
    issue_book(&lib);
    EXPECT_EQ(lib.transaction_count, 0U);
}
TEST_F(LibraryTestEnvironment, IssueBookRejectsUnavailableCopy) {
    AddBook(101, "Book", 1, 0);
    AddMember();
    SetInput("101\n201\n");
    issue_book(&lib);
    EXPECT_EQ(lib.transaction_count, 0U);
}
TEST_F(LibraryTestEnvironment, IssueBookRejectsInvalidDueDate) {
    AddBook();
    AddMember();
    SetInput("101\n201\n2026-02-30\n");
    issue_book(&lib);
    EXPECT_EQ(lib.transaction_count, 0U);
}
TEST_F(LibraryTestEnvironment, ReturnBookCalculatesFine) {
    AddBook(101, "Book", 2, 1);
    AddMember();
    AddTransaction();
    SetInput("1\n2026-11-02\n");
    return_book(&lib);
    EXPECT_EQ(lib.transactions[0].returned, 1);
    EXPECT_DOUBLE_EQ(lib.transactions[0].fine, 20.0);
    EXPECT_EQ(lib.books[0].available_copies, 2);
}
TEST_F(LibraryTestEnvironment, ReturnBookRejectsUnknownTransaction) {
    AddBook();
    SetInput("999\n");
    return_book(&lib);
    EXPECT_EQ(lib.transaction_count, 0U);
}
TEST_F(LibraryTestEnvironment, ReturnBookRejectsAlreadyReturnedTransaction) {
    AddBook();
    AddTransaction(1, 101, 201, 1);
    SetInput("1\n");
    return_book(&lib);
    EXPECT_EQ(lib.transactions[0].returned, 1);
}
TEST_F(LibraryTestEnvironment, ReturnBookRejectsInvalidDate) {
    AddBook(101, "Book", 2, 1);
    AddTransaction();
    SetInput("1\n2026-02-30\n");
    return_book(&lib);
    EXPECT_EQ(lib.transactions[0].returned, 0);
}
TEST_F(LibraryTestEnvironment, ListTransactionsHandlesEmptyAndPopulated) {
    list_transactions(&lib);
    AddTransaction();
    list_transactions(&lib);
}
TEST_F(LibraryTestEnvironment, FineDetailsHandlesNoFineAndReturnedFine) {
    show_fine_details(&lib);
    AddTransaction(1, 101, 201, 1);
    show_fine_details(&lib);
}


/* ---------------------- Additional edge-case tests ---------------------- */

/* Exercise main.c menu rendering and routing from the real application entry point. */
TEST_F(LibraryTestEnvironment, MainRoutesAdminMenuOperations) {
    const std::string input =
        "admin\nAdmin@123\n"
        "1\n301\nBook Title\nWriter Name\nFiction\n1\n"
        "2\n"
        "3\nBook Title\n"
        "4\n301\nUpdated Book\nUpdated Writer\nFiction\n2\n"
        "6\n401\nMember Name\n9876543210\n"
        "7\n"
        "8\n401\n"
        "9\n401\nUpdated Member\n9123456780\n"
        "11\n301\n401\n2099-12-31\n"
        "13\n"
        "12\n1\n2100-01-05\n"
        "14\n"
        "10\n401\ny\n"
        "5\n301\ny\n"
        "0\n";
    SetInput(input);
    EXPECT_EQ(library_application_main(), 0);
}

TEST_F(LibraryTestEnvironment, MainRoutesLibrarianRestrictedDeleteOptions) {
    SetInput("librarian\nLibrarian@123\n5\n10\n0\n");
    EXPECT_EQ(library_application_main(), 0);
}

TEST_F(LibraryTestEnvironment, MainRejectsMalformedStoredData) {
    {
        std::ofstream out(BOOKS_FILE);
        ASSERT_TRUE(out.good());
        out << "malformed\n";
    }
    EXPECT_EQ(library_application_main(), 1);
}

TEST_F(LibraryTestEnvironment, ReadLineHandlesNullZeroSizeEofCrLfAndLongInput) {
    char buffer[8] = "old";
    read_line("", nullptr, sizeof(buffer));
    read_line("", buffer, 0U);
    EXPECT_STREQ(buffer, "old");

    SetInput("");
    read_line("", buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "");

    SetInput("hello\r\n");
    read_line("", buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "hello");

    char small[5] = {};
    SetInput("abcdefgh\nnext\n");
    read_line("", small, sizeof(small));
    EXPECT_STREQ(small, "abcd");
    read_line("", buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "next");
}

TEST_F(LibraryTestEnvironment, ReadIntHandlesEmptyRangeTrailingSpacesAndOverflow) {
    SetInput("\n999999999999999999999999999999999\n0\n11\n 5 \n");
    EXPECT_EQ(read_int("", 1, 10), 5);
}

TEST_F(LibraryTestEnvironment, TextFieldRejectsControlCharactersAndAcceptsShortBoundary) {
    EXPECT_EQ(text_field_valid("ok", 3U), 1);
    EXPECT_EQ(text_field_valid("line\nfeed", 20U), 0);
    EXPECT_EQ(text_field_valid("carriage\rreturn", 20U), 0);
    EXPECT_EQ(text_field_valid("tab\tfield", 20U), 0);
    const char control[] = {'a', '\x01', 'b', '\0'};
    EXPECT_EQ(text_field_valid(control, 8U), 0);
}

TEST_F(LibraryTestEnvironment, ValidDateRejectsYearAndDayBoundaries) {
    EXPECT_EQ(valid_date("1899-12-31"), 0);
    EXPECT_EQ(valid_date("1900-01-01"), 1);
    EXPECT_EQ(valid_date("2026-01-32"), 0);
    EXPECT_EQ(valid_date("2026-aa-01"), 0);
    EXPECT_EQ(valid_date("2026-01-01x"), 0);
}

TEST_F(LibraryTestEnvironment, MemoryCapacityExpandsBeyondInitialAllocation) {
    ASSERT_EQ(ensure_book_capacity(&lib, 1U), 1);
    const size_t initial_books = lib.book_capacity;
    ASSERT_EQ(ensure_book_capacity(&lib, initial_books), 1);
    ASSERT_EQ(ensure_book_capacity(&lib, initial_books + 1U), 1);
    EXPECT_GT(lib.book_capacity, initial_books);

    ASSERT_EQ(ensure_member_capacity(&lib, 20U), 1);
    ASSERT_EQ(ensure_transaction_capacity(&lib, 20U), 1);
    EXPECT_GE(lib.member_capacity, 20U);
    EXPECT_GE(lib.transaction_capacity, 20U);
}

TEST_F(LibraryTestEnvironment, LogEventHandlesNullValuesAndLogOpenFailure) {
    log_event(nullptr, nullptr);
    {
        std::ifstream in(LOG_FILE);
        ASSERT_TRUE(in.good());
        std::stringstream contents;
        contents << in.rdbuf();
        EXPECT_NE(contents.str().find("user=unknown"), std::string::npos);
        EXPECT_NE(contents.str().find("event=unspecified"), std::string::npos);
    }
    ASSERT_EQ(unlink(LOG_FILE), 0);
    ASSERT_EQ(mkdir(LOG_FILE, 0700), 0);
    EXPECT_NO_THROW(log_event("tester", "ignored-because-log-is-directory"));
}

TEST_F(LibraryTestEnvironment, StorageLoadsLegacyMemberTimestampAndRoundTripsTimestamp) {
    {
        std::ofstream out(MEMBERS_FILE);
        out << "203|Legacy Member|9876543210\n";
    }
    ASSERT_EQ(load_data(&lib), 1);
    ASSERT_EQ(lib.member_count, 1U);
    EXPECT_STREQ(lib.members[0].added_at, "");
    std::memcpy(lib.members[0].added_at, "2026-10-28 12:34:56", 20U);
    ASSERT_EQ(save_members(&lib), 1);
    Library reloaded{};
    ASSERT_EQ(load_data(&reloaded), 1);
    ASSERT_EQ(reloaded.member_count, 1U);
    EXPECT_STREQ(reloaded.members[0].added_at, "2026-10-28 12:34:56");
    free_library(&reloaded);
}

TEST_F(LibraryTestEnvironment, StorageRejectsInvalidBookRanges) {
    const char *records[] = {
        "0|Title|Author|Category|1|1\n",
        "1|Title|Author|Category|0|0\n",
        "1|Title|Author|Category|2|-1\n",
        "1|Title|Author|Category|2|3\n",
        "1||Author|Category|1|1\n",
        "1|Title|Author|Category|1|1|extra\n"
    };
    for (const char *record : records) {
        std::ofstream out(BOOKS_FILE, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << record;
        out.close();
        Library loaded{};
        EXPECT_EQ(load_data(&loaded), 0) << record;
        EXPECT_EQ(loaded.book_count, 0U);
        free_library(&loaded);
    }
}

TEST_F(LibraryTestEnvironment, StorageRejectsInvalidMemberValuesAndExtraFields) {
    const char *records[] = {
        "0|Name|123|2026-10-28 12:00:00\n",
        "1||123|2026-10-28 12:00:00\n",
        "1|Name|123|2026-10-28 12:00:00|extra\n",
        "1|Name|123|2026-10-28 12:00:00|tail\n"
    };
    for (const char *record : records) {
        std::ofstream out(MEMBERS_FILE, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << record;
        out.close();
        Library loaded{};
        EXPECT_EQ(load_data(&loaded), 0) << record;
        free_library(&loaded);
    }
}

TEST_F(LibraryTestEnvironment, StorageRejectsInvalidTransactionValues) {
    const char *records[] = {
        "0|101|201|2026-10-19|2026-10-29|-|0|0\n",
        "1|0|201|2026-10-19|2026-10-29|-|0|0\n",
        "1|101|0|2026-10-19|2026-10-29|-|0|0\n",
        "1|101|201|bad-date|2026-10-29|-|0|0\n",
        "1|101|201|2026-10-19|2026-10-29|-|2|0\n",
        "1|101|201|2026-10-19|2026-10-29|-|0|-1\n",
        "1|101|201|2026-10-19|2026-10-29|bad-date|1|0\n",
        "1|101|201|2026-10-19|2026-10-29|-|0|0|extra\n"
    };
    for (const char *record : records) {
        std::ofstream out(TRANSACTIONS_FILE, std::ios::trunc);
        ASSERT_TRUE(out.good());
        out << record;
        out.close();
        Library loaded{};
        EXPECT_EQ(load_data(&loaded), 0) << record;
        free_library(&loaded);
    }
}

TEST_F(LibraryTestEnvironment, StorageClearsPreviouslyLoadedDataAfterLaterFailure) {
    {
        std::ofstream books(BOOKS_FILE);
        books << "101|Title|Author|Category|2|2\n";
        std::ofstream members(MEMBERS_FILE);
        members << "invalid-member-row\n";
    }
    EXPECT_EQ(load_data(&lib), 0);
    EXPECT_EQ(lib.books, nullptr);
    EXPECT_EQ(lib.members, nullptr);
    EXPECT_EQ(lib.book_count, 0U);

    {
        std::ofstream books(BOOKS_FILE, std::ios::trunc);
        books << "101|Title|Author|Category|2|2\n";
        std::ofstream members(MEMBERS_FILE, std::ios::trunc);
        members << "201|Member|1234567890|2026-10-28 12:00:00\n";
        std::ofstream tx(TRANSACTIONS_FILE);
        tx << "invalid-transaction\n";
    }
    EXPECT_EQ(load_data(&lib), 0);
    EXPECT_EQ(lib.books, nullptr);
    EXPECT_EQ(lib.members, nullptr);
    EXPECT_EQ(lib.transactions, nullptr);
}

TEST_F(LibraryTestEnvironment, SaveFunctionsHandleTemporaryPathFailures) {
    ASSERT_EQ(mkdir("books.tmp", 0700), 0);
    EXPECT_EQ(save_books(&lib), 0);
    ASSERT_EQ(rmdir("books.tmp"), 0);

    ASSERT_EQ(mkdir("members.tmp", 0700), 0);
    EXPECT_EQ(save_members(&lib), 0);
    ASSERT_EQ(rmdir("members.tmp"), 0);

    ASSERT_EQ(mkdir("transactions.tmp", 0700), 0);
    EXPECT_EQ(save_transactions(&lib), 0);
    ASSERT_EQ(rmdir("transactions.tmp"), 0);
}

TEST_F(LibraryTestEnvironment, StorageBackupFailurePreservesExistingTarget) {
    {
        std::ofstream out(BOOKS_FILE);
        out << "original-data\n";
    }
    ASSERT_EQ(mkdir("books.bak", 0700), 0);
    {
        std::ofstream marker("books.bak/keep.txt");
        marker << "keep";
    }
    EXPECT_EQ(save_books(&lib), 0);
    std::ifstream in(BOOKS_FILE);
    ASSERT_TRUE(in.good());
    std::string first_line;
    std::getline(in, first_line);
    EXPECT_EQ(first_line, "original-data");
}

TEST_F(LibraryTestEnvironment, AddBookRejectsInvalidAuthorAndCategory) {
    SetInput("302\nGood title\nBad|Author\n");
    add_book(&lib);
    EXPECT_EQ(lib.book_count, 0U);
    SetInput("303\nGood title\nGood author\nBad|Category\n");
    add_book(&lib);
    EXPECT_EQ(lib.book_count, 0U);
}

TEST_F(LibraryTestEnvironment, AddBookRollsBackWhenStorageFails) {
    ASSERT_EQ(mkdir("books.tmp", 0700), 0);
    SetInput("304\nTitle\nAuthor\nCategory\n2\n");
    add_book(&lib);
    EXPECT_EQ(lib.book_count, 0U);
}

TEST_F(LibraryTestEnvironment, SearchBooksMatchesAuthorAndCategory) {
    AddBook(101, "Title", 2, 2);
    SetInput("abdul\n");
    search_books(&lib);
    SetInput("biography\n");
    search_books(&lib);
    SetInput("9999999999999999999999999999\n");
    search_books(&lib);
}

TEST_F(LibraryTestEnvironment, UpdateBookRestoresOriginalWhenStorageFails) {
    AddBook(101, "Original", 2, 1);
    ASSERT_EQ(mkdir("books.tmp", 0700), 0);
    SetInput("101\nChanged\nChanged Author\nNew Category\n4\n");
    update_book(&lib);
    EXPECT_STREQ(lib.books[0].title, "Original");
    EXPECT_EQ(lib.books[0].total_copies, 2);
    EXPECT_EQ(lib.books[0].available_copies, 1);
}

TEST_F(LibraryTestEnvironment, DeleteBookShiftsRecordsAndRestoresOnStorageFailure) {
    AddBook(101, "First");
    AddBook(102, "Second");
    SetInput("101\ny\n");
    delete_book(&lib);
    ASSERT_EQ(lib.book_count, 1U);
    EXPECT_EQ(lib.books[0].id, 102);

    AddBook(103, "Third");
    ASSERT_EQ(mkdir("books.tmp", 0700), 0);
    SetInput("102\ny\n");
    delete_book(&lib);
    EXPECT_EQ(lib.book_count, 2U);
    EXPECT_EQ(lib.books[0].id, 102);
}

TEST_F(LibraryTestEnvironment, RegisterMemberRejectsInvalidPhoneAndStoresTimestamp) {
    SetInput("202\nMaya Shah\nBad|Phone\n");
    register_member(&lib);
    EXPECT_EQ(lib.member_count, 0U);

    SetInput("203\nMaya Shah\n9876543210\n");
    register_member(&lib);
    ASSERT_EQ(lib.member_count, 1U);
    ASSERT_EQ(std::strlen(lib.members[0].added_at), 19U);
    EXPECT_EQ(valid_date(lib.members[0].added_at), 0);  // timestamp contains time fields
    EXPECT_EQ(valid_date(std::string(lib.members[0].added_at, 10U).c_str()), 1);
}

TEST_F(LibraryTestEnvironment, RegisterMemberRollsBackWhenStorageFails) {
    ASSERT_EQ(mkdir("members.tmp", 0700), 0);
    SetInput("204\nName\n9876543210\n");
    register_member(&lib);
    EXPECT_EQ(lib.member_count, 0U);
}

TEST_F(LibraryTestEnvironment, UpdateMemberRejectsInvalidFieldsAndRestoresOnStorageFailure) {
    AddMember(201);
    SetInput("201\nBad|Name\n");
    update_member(&lib);
    EXPECT_STREQ(lib.members[0].name, "Rahul Patil");

    ASSERT_EQ(mkdir("members.tmp", 0700), 0);
    SetInput("201\nChanged Name\n9123456789\n");
    update_member(&lib);
    EXPECT_STREQ(lib.members[0].name, "Rahul Patil");
}

TEST_F(LibraryTestEnvironment, DeleteMemberShiftsRecordsAndRestoresOnStorageFailure) {
    AddMember(201);
    AddMember(202);
    SetInput("201\ny\n");
    delete_member(&lib);
    ASSERT_EQ(lib.member_count, 1U);
    EXPECT_EQ(lib.members[0].id, 202);

    AddMember(203);
    ASSERT_EQ(mkdir("members.tmp", 0700), 0);
    SetInput("202\ny\n");
    delete_member(&lib);
    EXPECT_EQ(lib.member_count, 2U);
    EXPECT_EQ(lib.members[0].id, 202);
}

TEST_F(LibraryTestEnvironment, IssueBookRejectsPastDueDateAndExhaustedTransactionIds) {
    AddBook();
    AddMember();
    SetInput("101\n201\n2000-01-01\n");
    issue_book(&lib);
    EXPECT_EQ(lib.transaction_count, 0U);
    EXPECT_EQ(lib.books[0].available_copies, 2);

    AddTransaction(INT_MAX, 101, 201, 1);
    SetInput("101\n201\n2099-12-31\n");
    issue_book(&lib);
    EXPECT_EQ(lib.transaction_count, 1U);
    EXPECT_EQ(lib.books[0].available_copies, 2);
}

TEST_F(LibraryTestEnvironment, IssueBookRollsBackIfBookOrTransactionSaveFails) {
    AddBook();
    AddMember();
    ASSERT_EQ(mkdir("books.tmp", 0700), 0);
    SetInput("101\n201\n2099-12-31\n");
    issue_book(&lib);
    EXPECT_EQ(lib.transaction_count, 0U);
    EXPECT_EQ(lib.books[0].available_copies, 2);
    ASSERT_EQ(rmdir("books.tmp"), 0);

    ASSERT_EQ(mkdir("transactions.tmp", 0700), 0);
    SetInput("101\n201\n2099-12-31\n");
    issue_book(&lib);
    EXPECT_EQ(lib.transaction_count, 0U);
    EXPECT_EQ(lib.books[0].available_copies, 2);
}

TEST_F(LibraryTestEnvironment, ReturnBookHandlesMissingBookAndPreIssueReturnDate) {
    AddTransaction(1, 999, 201, 0);
    SetInput("1\n");
    return_book(&lib);
    EXPECT_EQ(lib.transactions[0].returned, 0);

    lib.transactions[0].book_id = 101;
    AddBook(101, "Book", 2, 1);
    SetInput("1\n2026-10-18\n");
    return_book(&lib);
    EXPECT_EQ(lib.transactions[0].returned, 0);
}

TEST_F(LibraryTestEnvironment, ReturnBookNoFineOnOrBeforeDueDate) {
    AddBook(101, "Book", 2, 1);
    AddMember();
    AddTransaction();
    SetInput("1\n2026-10-29\n");
    return_book(&lib);
    EXPECT_EQ(lib.transactions[0].returned, 1);
    EXPECT_DOUBLE_EQ(lib.transactions[0].fine, 0.0);
    EXPECT_EQ(lib.books[0].available_copies, 2);
}

TEST_F(LibraryTestEnvironment, ReturnBookRejectsInconsistentInventory) {
    AddBook(101, "Book", 2, 2);
    AddTransaction();
    SetInput("1\n2026-11-02\n");
    return_book(&lib);
    EXPECT_EQ(lib.transactions[0].returned, 0);
    EXPECT_EQ(lib.books[0].available_copies, 2);
}

TEST_F(LibraryTestEnvironment, ReturnBookRollsBackWhenStorageFails) {
    AddBook(101, "Book", 2, 1);
    AddTransaction();
    ASSERT_EQ(mkdir("books.tmp", 0700), 0);
    SetInput("1\n2026-11-02\n");
    return_book(&lib);
    EXPECT_EQ(lib.transactions[0].returned, 0);
    EXPECT_EQ(lib.books[0].available_copies, 1);
    ASSERT_EQ(rmdir("books.tmp"), 0);

    ASSERT_EQ(mkdir("transactions.tmp", 0700), 0);
    SetInput("1\n2026-11-02\n");
    return_book(&lib);
    EXPECT_EQ(lib.transactions[0].returned, 0);
    EXPECT_EQ(lib.books[0].available_copies, 1);
}


TEST_F(LibraryTestEnvironment, StorageReadErrorsAreHandledForEachDataFile) {
    ASSERT_EQ(mkdir(BOOKS_FILE, 0700), 0);
    EXPECT_EQ(load_data(&lib), 0);
    EXPECT_EQ(lib.books, nullptr);
    ASSERT_EQ(rmdir(BOOKS_FILE), 0);

    ASSERT_EQ(mkdir(MEMBERS_FILE, 0700), 0);
    EXPECT_EQ(load_data(&lib), 0);
    EXPECT_EQ(lib.members, nullptr);
    ASSERT_EQ(rmdir(MEMBERS_FILE), 0);

    ASSERT_EQ(mkdir(TRANSACTIONS_FILE, 0700), 0);
    EXPECT_EQ(load_data(&lib), 0);
    EXPECT_EQ(lib.transactions, nullptr);
    ASSERT_EQ(rmdir(TRANSACTIONS_FILE), 0);
}

TEST_F(LibraryTestEnvironment, StorageOpenErrorsAreHandled) {
    ASSERT_EQ(create_unix_socket_path(BOOKS_FILE), 1);
    EXPECT_EQ(load_data(&lib), 0);
    EXPECT_EQ(unlink(BOOKS_FILE), 0);

    ASSERT_EQ(create_unix_socket_path(MEMBERS_FILE), 1);
    EXPECT_EQ(load_data(&lib), 0);
    EXPECT_EQ(unlink(MEMBERS_FILE), 0);

    ASSERT_EQ(create_unix_socket_path(TRANSACTIONS_FILE), 1);
    EXPECT_EQ(load_data(&lib), 0);
    EXPECT_EQ(unlink(TRANSACTIONS_FILE), 0);
}

TEST_F(LibraryTestEnvironment, SaveFunctionsDetectDeviceWriteFailures) {
    /* Fill stdio's buffer so fprintf itself sees the failed /dev/full write,
     * rather than only discovering it during fclose. */
    for (int i = 0; i < 200; ++i) {
        AddBook(1000 + i, "Book", 2, 2);
    }
    ASSERT_EQ(symlink("/dev/full", "books.tmp"), 0);
    EXPECT_EQ(save_books(&lib), 0);

    for (int i = 0; i < 200; ++i) {
        AddMember(2000 + i);
    }
    ASSERT_EQ(symlink("/dev/full", "members.tmp"), 0);
    EXPECT_EQ(save_members(&lib), 0);

    for (int i = 0; i < 200; ++i) {
        AddTransaction(3000 + i, 101, 201, 0);
    }
    ASSERT_EQ(symlink("/dev/full", "transactions.tmp"), 0);
    EXPECT_EQ(save_transactions(&lib), 0);
}

TEST_F(LibraryTestEnvironment, BackupTargetOpenFailureRemovesTemporaryFile) {
    AddBook();
    ASSERT_EQ(create_unix_socket_path(BOOKS_FILE), 1);
    EXPECT_EQ(save_books(&lib), 0);
    EXPECT_EQ(access("books.tmp", F_OK), -1);
    EXPECT_EQ(unlink(BOOKS_FILE), 0);
}
