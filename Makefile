CC = gcc

BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
GENERATED_DIR = $(BUILD_DIR)/generated
USE_BOOTSTRAP ?= 1
VERSION_HEADER ?= $(GENERATED_DIR)/version.h
VERSION_HEADER_DIR = $(patsubst %/,%,$(dir $(VERSION_HEADER)))

CFLAGS = -O2
CFLAGS_ALL = -std=c99 -g $(CFLAGS)
CPPFLAGS =
LDFLAGS = -g
LIBS = -lgit2

MAIN_BIN = $(BUILD_DIR)/screw-up
BOOTSTRAP_BIN = $(BUILD_DIR)/screw-up-bootstrap
ANALYZER_TESTS_BIN = $(BUILD_DIR)/analyzer_tests
CLI_TESTS_BIN = $(BUILD_DIR)/cli_tests

CORE_OBJS = \
	$(OBJ_DIR)/analyzer.o \
	$(OBJ_DIR)/format.o \
	$(OBJ_DIR)/git_operations.o \
	$(OBJ_DIR)/logger.o \
	$(OBJ_DIR)/util_buffer.o \
	$(OBJ_DIR)/util_map.o \
	$(OBJ_DIR)/util_str.o \
	$(OBJ_DIR)/util_vec.o \
	$(OBJ_DIR)/value.o

CLI_MAIN_OBJ = $(OBJ_DIR)/cli_main.o
CLI_BOOTSTRAP_OBJ = $(OBJ_DIR)/cli_bootstrap.o
CLI_VERSION_OBJ = $(OBJ_DIR)/cli_version.o

TEST_UTILS_OBJ = $(OBJ_DIR)/test_utils.o
ANALYZER_TESTS_OBJ = $(OBJ_DIR)/analyzer_tests.o
CLI_TESTS_OBJ = $(OBJ_DIR)/cli_tests.o

.PHONY: all clean test bootstrap

all: $(MAIN_BIN)

bootstrap: $(BOOTSTRAP_BIN)

test: $(ANALYZER_TESTS_BIN) $(CLI_TESTS_BIN)
	$(ANALYZER_TESTS_BIN)
	$(CLI_TESTS_BIN)

clean:
	rm -rf $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(VERSION_HEADER_DIR):
	mkdir -p $(VERSION_HEADER_DIR)

$(OBJ_DIR)/analyzer.o: src/analyzer.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/analyzer.c -o $(OBJ_DIR)/analyzer.o

$(OBJ_DIR)/format.o: src/format.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/format.c -o $(OBJ_DIR)/format.o

$(OBJ_DIR)/git_operations.o: src/git_operations.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/git_operations.c -o $(OBJ_DIR)/git_operations.o

$(OBJ_DIR)/logger.o: src/logger.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/logger.c -o $(OBJ_DIR)/logger.o

$(OBJ_DIR)/util_buffer.o: src/util_buffer.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/util_buffer.c -o $(OBJ_DIR)/util_buffer.o

$(OBJ_DIR)/util_map.o: src/util_map.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/util_map.c -o $(OBJ_DIR)/util_map.o

$(OBJ_DIR)/util_str.o: src/util_str.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/util_str.c -o $(OBJ_DIR)/util_str.o

$(OBJ_DIR)/util_vec.o: src/util_vec.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/util_vec.c -o $(OBJ_DIR)/util_vec.o

$(OBJ_DIR)/value.o: src/value.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/value.c -o $(OBJ_DIR)/value.o

$(OBJ_DIR)/cli_main.o: src/cli_main.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -c src/cli_main.c -o $(OBJ_DIR)/cli_main.o

$(OBJ_DIR)/cli_bootstrap.o: src/cli.c $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS_ALL) -DSCREW_UP_BOOTSTRAP=1 -c src/cli.c -o $(OBJ_DIR)/cli_bootstrap.o

$(OBJ_DIR)/cli_version.o: src/cli.c $(VERSION_HEADER) $(OBJ_DIR)
	$(CC) -I$(VERSION_HEADER_DIR) $(CPPFLAGS) $(CFLAGS_ALL) -DSCREW_UP_USE_VERSION_HEADER=1 -c src/cli.c -o $(OBJ_DIR)/cli_version.o

$(OBJ_DIR)/test_utils.o: tests/test_utils.c $(OBJ_DIR)
	$(CC) -Itests $(CPPFLAGS) $(CFLAGS_ALL) -c tests/test_utils.c -o $(OBJ_DIR)/test_utils.o

$(OBJ_DIR)/analyzer_tests.o: tests/analyzer_tests.c $(OBJ_DIR)
	$(CC) -Itests $(CPPFLAGS) $(CFLAGS_ALL) -c tests/analyzer_tests.c -o $(OBJ_DIR)/analyzer_tests.o

$(OBJ_DIR)/cli_tests.o: tests/cli_tests.c $(VERSION_HEADER) $(OBJ_DIR)
	$(CC) -I$(VERSION_HEADER_DIR) -Itests $(CPPFLAGS) $(CFLAGS_ALL) -c tests/cli_tests.c -o $(OBJ_DIR)/cli_tests.o

$(BOOTSTRAP_BIN): $(CORE_OBJS) $(CLI_BOOTSTRAP_OBJ) $(CLI_MAIN_OBJ)
	$(CC) $(LDFLAGS) -o $(BOOTSTRAP_BIN) \
		$(CORE_OBJS) $(CLI_BOOTSTRAP_OBJ) $(CLI_MAIN_OBJ) $(LIBS)

ifeq ($(USE_BOOTSTRAP),1)
$(VERSION_HEADER): src/version.h.in $(BOOTSTRAP_BIN) $(VERSION_HEADER_DIR)
	$(BOOTSTRAP_BIN) format -i src/version.h.in $(VERSION_HEADER)
endif

$(MAIN_BIN): $(CORE_OBJS) $(CLI_VERSION_OBJ) $(CLI_MAIN_OBJ)
	$(CC) $(LDFLAGS) -o $(MAIN_BIN) \
		$(CORE_OBJS) $(CLI_VERSION_OBJ) $(CLI_MAIN_OBJ) $(LIBS)

$(ANALYZER_TESTS_BIN): $(CORE_OBJS) $(ANALYZER_TESTS_OBJ) $(TEST_UTILS_OBJ)
	$(CC) $(LDFLAGS) -o $(ANALYZER_TESTS_BIN) \
		$(CORE_OBJS) $(ANALYZER_TESTS_OBJ) $(TEST_UTILS_OBJ) $(LIBS)

$(CLI_TESTS_BIN): $(CORE_OBJS) $(CLI_VERSION_OBJ) $(CLI_TESTS_OBJ) $(TEST_UTILS_OBJ)
	$(CC) $(LDFLAGS) -o $(CLI_TESTS_BIN) \
		$(CORE_OBJS) $(CLI_VERSION_OBJ) $(CLI_TESTS_OBJ) $(TEST_UTILS_OBJ) $(LIBS)
