#include "library.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int member_index_by_id(const Library *lib, int id)
{
    size_t i;
    for (i = 0U; i < lib->member_count; i++) {
        if (lib->members[i].id == id) return (int)i;
    }
    return -1;
}

static int member_has_active_transaction(const Library *lib, int id)
{
    size_t i;
    for (i = 0U; i < lib->transaction_count; i++) {
        if (lib->transactions[i].member_id == id &&
            lib->transactions[i].returned == 0) return 1;
    }
    return 0;
}

static int get_member_field(const char *prompt, char *buffer, size_t size)
{
    read_line(prompt, buffer, size);
    if (!text_field_valid(buffer, size)) {
        puts("Value cannot be empty, too long, or contain the | character.");
        return 0;
    }
    return 1;
}

void register_member(Library *lib)
{
    Member member;
    size_t i;
    time_t current_time;
    struct tm *time_info;

    memset(&member, 0, sizeof member);
    member.id = read_int("Member ID (positive number): ", 1, 2147483647);
    for (i = 0U; i < lib->member_count; i++) {
        if (lib->members[i].id == member.id) {
            puts("That Member ID already exists.");
            return;
        }
    }

    if (!get_member_field("Member name: ", member.name, sizeof member.name) ||
        !get_member_field("Phone number: ", member.phone, sizeof member.phone)) {
        return;
    }

    current_time = time(NULL);
    time_info = localtime(&current_time);
    if (time_info == NULL ||
        strftime(member.added_at, sizeof member.added_at,
                 "%Y-%m-%d %H:%M:%S", time_info) == 0U) {
        puts("Unable to record member registration timestamp.");
        return;
    }

    if (!ensure_member_capacity(lib, lib->member_count + 1U)) {
        puts("Not enough memory to register member.");
        return;
    }

    lib->members[lib->member_count++] = member;
    if (!save_members(lib)) {
        lib->member_count--;
        puts("Member was not registered because storage failed.");
        return;
    }
    log_event(lib->username, "member-registered");
    puts("Member registered successfully.");
}

void list_members(const Library *lib)
{
    size_t i;

    puts("\n================ ALL MEMBERS ================");
    printf("%-10s %-25s %-15s %-20s\n", "Member ID", "Name", "Phone", "Added At");
    puts("--------------------------------------------------------------------------------");

    if (lib->member_count == 0U) {
        puts("No members found. Please register a member first.");
        return;
    }

    for (i = 0U; i < lib->member_count; i++) {
        printf("%-10d %-25.25s %-15.15s %-20s\n",
               lib->members[i].id, lib->members[i].name, lib->members[i].phone,
               lib->members[i].added_at[0] != '\0' ? lib->members[i].added_at : "Unknown (legacy record)");
    }
}

void search_members(const Library *lib)
{
    char query[100];
    char id_text[24];
    size_t i;
    int found = 0;

    if (lib->member_count == 0U) {
        puts("No members are stored yet.");
        return;
    }

    read_line("Search by Member ID, name, or phone: ", query, sizeof query);
    for (i = 0U; i < lib->member_count; i++) {
        const Member *m = &lib->members[i];
        (void)snprintf(id_text, sizeof id_text, "%d", m->id);
        if (strcmp(id_text, query) == 0 ||
            contains_ignore_case(m->name, query) ||
            contains_ignore_case(m->phone, query)) {
            printf("Member ID: %d | Name: %s | Phone: %s | Added At: %s\n", m->id, m->name, m->phone, m->added_at[0] != '\0' ? m->added_at : "Unknown (legacy record)");
            found = 1;
        }
    }
    if (!found) puts("No matching member found.");
}

void update_member(Library *lib)
{
    int id = read_int("Member ID to update: ", 1, 2147483647);
    int index = member_index_by_id(lib, id);
    Member old_member;
    Member updated;

    if (index < 0) {
        puts("Member not found.");
        return;
    }

    old_member = lib->members[index];
    updated = old_member;
    if (!get_member_field("New name: ", updated.name, sizeof updated.name) ||
        !get_member_field("New phone number: ", updated.phone, sizeof updated.phone)) {
        return;
    }

    lib->members[index] = updated;
    if (!save_members(lib)) {
        lib->members[index] = old_member;
        puts("Update cancelled because storage failed.");
        return;
    }
    log_event(lib->username, "member-updated");
    puts("Member updated successfully.");
}

void delete_member(Library *lib)
{
    int id = read_int("Member ID to delete: ", 1, 2147483647);
    int index = member_index_by_id(lib, id);
    Member old_member;
    size_t i;

    if (index < 0) {
        puts("Member not found.");
        return;
    }
    if (member_has_active_transaction(lib, id)) {
        puts("Cannot delete this member while they have an unreturned book.");
        return;
    }
    if (!confirm_action("Delete this member? (y/n): ")) {
        puts("Delete cancelled.");
        return;
    }

    old_member = lib->members[index];
    for (i = (size_t)index; i + 1U < lib->member_count; i++) {
        lib->members[i] = lib->members[i + 1U];
    }
    lib->member_count--;

    if (!save_members(lib)) {
        for (i = lib->member_count; i > (size_t)index; i--) {
            lib->members[i] = lib->members[i - 1U];
        }
        lib->members[index] = old_member;
        lib->member_count++;
        puts("Delete cancelled because storage failed.");
        return;
    }
    log_event(lib->username, "member-deleted");
    puts("Member deleted successfully.");
}
