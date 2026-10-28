# Library Management System - build, test, coverage and analysis targets

CC       := gcc
CXX      := g++
CSTD     := -std=c11
CXXSTD   := -std=c++17
WARN     := -Wall -Wextra -Wpedantic -Wconversion -Wshadow
CFLAGS   ?= $(CSTD) $(WARN) -O0 -g
CXXFLAGS ?= $(CXXSTD) -Wall -Wextra -Wpedantic -O0 -g
INCLUDES := -Iinclude
GTEST_LIBS ?= -lgtest -lgtest_main -pthread

SRC_DIR      := Src
INCLUDE_DIR  := include
TEST_DIR     := Test
BUILD_DIR    := Built
COVERAGE_DIR := Coverage

C_SOURCES := $(SRC_DIR)/main.c \
             $(SRC_DIR)/auth.c \
             $(SRC_DIR)/books.c \
             $(SRC_DIR)/members.c \
             $(SRC_DIR)/storage.c \
             $(SRC_DIR)/transactions.c \
             $(SRC_DIR)/util.c

# Exclude main.c from tests to avoid linking a second main() function.
MODULE_SOURCES := $(filter-out $(SRC_DIR)/main.c,$(C_SOURCES))
APP_OBJECTS    := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
TEST_OBJECTS   := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(MODULE_SOURCES))
TEST_SOURCE    := $(TEST_DIR)/library_all_function_unit_tests.cpp
APP            := $(BUILD_DIR)/library
GTEST          := $(BUILD_DIR)/library_tests
GTEST_SAN      := $(BUILD_DIR)/library_tests_sanitize
GTEST_COV      := $(BUILD_DIR)/library_tests_coverage
COV_APP        := $(BUILD_DIR)/library_coverage_app
COV_MAIN_OBJ   := $(BUILD_DIR)/cov_main.o

.PHONY: all run test gtest coverage sanitize valgrind helgrind cppcheck misra clean help directories

all: $(APP)

directories:
	mkdir -p $(BUILD_DIR) $(COVERAGE_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/library.h | directories
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/library.h | directories
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(APP): $(APP_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

run: $(APP)
	./$(APP)

$(GTEST): $(TEST_SOURCE) $(TEST_OBJECTS) $(INCLUDE_DIR)/library.h | directories
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TEST_SOURCE) $(TEST_OBJECTS) $(GTEST_LIBS) -o $@

test gtest: $(GTEST)
	./$(GTEST)

# Build and run tests with AddressSanitizer and UndefinedBehaviorSanitizer.
sanitize: | directories
	$(CXX) $(CXXSTD) $(WARN) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $(INCLUDES) \
	  $(TEST_SOURCE) $(MODULE_SOURCES) $(GTEST_LIBS) -o $(GTEST_SAN)
	ASAN_OPTIONS=detect_leaks=1 ./$(GTEST_SAN)

valgrind: $(GTEST) | directories
	command -v valgrind >/dev/null 2>&1 || { echo 'ERROR: Valgrind is not installed.'; exit 1; }
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 \
	  --log-file=$(COVERAGE_DIR)/valgrind_report.txt ./$(GTEST)

helgrind: $(GTEST) | directories
	command -v valgrind >/dev/null 2>&1 || { echo 'ERROR: Valgrind is not installed.'; exit 1; }
	valgrind --tool=helgrind --error-exitcode=1 --log-file=$(COVERAGE_DIR)/helgrind_report.txt ./$(GTEST)

cppcheck: | directories
	command -v cppcheck >/dev/null 2>&1 || { echo 'ERROR: Cppcheck is not installed.'; exit 1; }
	cppcheck --enable=all --inconclusive --std=c11 --language=c --inline-suppr \
	  --suppress=missingIncludeSystem --output-file=$(COVERAGE_DIR)/cppcheck_report.txt \
	  $(INCLUDES) $(C_SOURCES)
	@echo 'Cppcheck report: $(COVERAGE_DIR)/cppcheck_report.txt'

# MISRA addon availability and rule coverage depend on the installed Cppcheck package.
misra: | directories
	command -v cppcheck >/dev/null 2>&1 || { echo 'ERROR: Cppcheck is not installed.'; exit 1; }
	cppcheck --addon=misra.py --enable=all --inconclusive --std=c11 --language=c \
	  --output-file=$(COVERAGE_DIR)/misra_report.txt $(INCLUDES) $(C_SOURCES)
	@echo 'MISRA screening report: $(COVERAGE_DIR)/misra_report.txt'
	@echo 'This report alone does not certify full MISRA C:2012 compliance.'

# Compile the tested modules with coverage instrumentation, then run GoogleTest.
$(BUILD_DIR)/cov_%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/library.h | directories
	$(CC) $(CFLAGS) $(INCLUDES) --coverage -c $< -o $@

COV_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/cov_%.o,$(MODULE_SOURCES))
COV_APP_OBJECTS := $(COV_MAIN_OBJ) $(COV_OBJECTS)

$(GTEST_COV): $(TEST_SOURCE) $(COV_OBJECTS) $(INCLUDE_DIR)/library.h | directories
	$(CXX) $(CXXFLAGS) $(INCLUDES) --coverage $(TEST_SOURCE) $(COV_OBJECTS) $(GTEST_LIBS) -o $@

$(COV_APP): $(COV_APP_OBJECTS)
	$(CC) $(CFLAGS) --coverage $^ -o $@

coverage:
	$(MAKE) clean
	$(MAKE) $(GTEST_COV) $(COV_APP)
	./$(GTEST_COV)
	sh $(TEST_DIR)/coverage_smoke.sh
	gcov -b -c -o $(BUILD_DIR) $(BUILD_DIR)/cov_*.gcno > $(COVERAGE_DIR)/gcov_report.txt
	mv -f ./*.gcov $(COVERAGE_DIR)/ 2>/dev/null || true
	@echo 'Coverage report: $(COVERAGE_DIR)/gcov_report.txt (C source modules only; inspect .gcov files for uncovered lines)'

clean:
	rm -f $(BUILD_DIR)/*.o $(APP) $(GTEST) $(GTEST_SAN) $(GTEST_COV) $(COV_APP)
	rm -f $(BUILD_DIR)/*.gcda $(BUILD_DIR)/*.gcno $(SRC_DIR)/*.gcda $(SRC_DIR)/*.gcno
	rm -f *.gcda *.gcno *.gcov $(COVERAGE_DIR)/*.gcov $(COVERAGE_DIR)/*_report.txt

help:
	@echo 'make             Build the application'
	@echo 'make run         Build and run the application'
	@echo 'make test        Build and run GoogleTest tests (requires GoogleTest)'
	@echo 'make sanitize    Run tests with AddressSanitizer and UBSan'
	@echo 'make valgrind    Run tests under Valgrind (requires Valgrind + GoogleTest)'
	@echo 'make helgrind    Run tests under Helgrind (requires Valgrind + GoogleTest)'
	@echo 'make cppcheck    Generate a Cppcheck report'
	@echo 'make misra       Generate a MISRA addon screening report'
	@echo 'make coverage    Run tests and generate gcov coverage data'
	@echo 'make clean       Remove generated build and report files'
