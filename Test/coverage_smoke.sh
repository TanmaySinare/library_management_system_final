#!/bin/sh
# Exercise main.c menu routing through the real coverage-instrumented app.
# The app is run from a temporary directory so tests never modify the project data.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
APP="$ROOT/Built/library_coverage_app"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM
(
    cd "$TMP_DIR"
    printf '%s\n' \
      'admin' 'Admin@123' \
      '1' '301' 'Book Title' 'Writer Name' 'Fiction' '1' \
      '2' '3' 'Book Title' \
      '4' '301' 'Updated Book' 'Updated Writer' 'Fiction' '2' \
      '6' '401' 'Member Name' '9876543210' \
      '7' '8' '401' '9' '401' 'Updated Member' '9123456780' \
      '11' '301' '401' '2099-12-31' '13' \
      '12' '1' '2100-01-05' '14' \
      '10' '401' 'y' '5' '301' 'y' '0' | "$APP"

    printf '%s\n' 'librarian' 'Librarian@123' '5' '10' '0' | "$APP"

    # Exercise main's data-load error exit path (expected nonzero return).
    printf '%s\n' 'malformed book record' > books.txt
    if "$APP" </dev/null; then
        echo 'Expected application to reject malformed stored data.' >&2
        exit 1
    fi
    rm -f books.txt
    if printf '%s\n' 'wrong' 'bad' 'wrong' 'bad' 'wrong' 'bad' | "$APP"; then
        echo 'Expected application to reject three failed login attempts.' >&2
        exit 1
    fi
)
