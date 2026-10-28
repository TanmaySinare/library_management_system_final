# Library Management System (C)

## Folder structure

- `Src/`: C implementation files.
- `include/`: public header (`library.h`).
- `Test/`: GoogleTest unit tests (`library_all_function_unit_tests.cpp`).
- `Built/`: generated application, test executable and object files.
- `Coverage/`: generated Cppcheck, MISRA-screening, Valgrind and gcov reports.
- `books.txt`, `members.txt`, `transactions.txt`: application data files.
- `library.log`: application event log.
- `Makefile`: build and analysis commands.

There is intentionally no `Source code/` folder. The Makefile is in the project root.

## Build and run

```sh
make
make run
```

Demo credentials (educational use only): `admin` / `Admin@123` or `librarian` / `Librarian@123`.

## Member registration timestamp

When a member is registered, the local system date/time is stored as `YYYY-MM-DD HH:MM:SS`. The `Added At` column is displayed in **View All Members**. The timestamp is persisted as the fourth field in `members.txt`. Legacy three-field member rows are still loaded, and their timestamp is shown as `Unknown (legacy record)` because the original time cannot be reconstructed.

## Tests and analysis

```sh
make test       # GoogleTest; requires g++ and GoogleTest development package
make valgrind   # requires Valgrind and GoogleTest
make cppcheck   # requires Cppcheck
make misra      # requires Cppcheck with its MISRA addon installed
make coverage   # runs tests and writes gcov coverage data
make clean
```

Reports are written to `Coverage/`.

## Important quality/compliance note

These commands provide build, unit-test, memory-analysis, static-analysis and coverage workflows. They do **not** guarantee full MISRA C:2012 compliance or 100% statement/branch/function coverage. MISRA compliance requires reviewing every applicable rule, compiler/tool versions, project configuration and documented deviations. Coverage is measured only for code executed by the available tests; additional tests are needed for uncovered lines and branches. The test and analysis targets must be run in an environment where the required tools are installed.


## GoogleTest coverage

Run `make coverage` to cleanly rebuild the coverage-instrumented test executable, run the GoogleTest suite, and write the gcov summary to `Coverage/gcov_report.txt`. Annotated `*.gcov` source files are placed in `Coverage/`. The test translation unit includes `Src/main.c` under a renamed entry-point symbol so the application menu can also be exercised by integration-style GoogleTests. Coverage percentages are measured results; a passing test suite does not guarantee a 95% or 100% score.
