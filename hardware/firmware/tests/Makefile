# Makefile for Neural Load Ring Firmware Tests
#
# Host simulation build for unit testing.
# Uses GCC to compile firmware code for desktop testing.
#
# Usage:
#   make          - Build and run tests
#   make test     - Run tests only
#   make clean    - Clean build artifacts
#   make verbose  - Build with verbose output

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g
CFLAGS += -I../src
CFLAGS += -I../src/wellness_feedback
CFLAGS += -DHOST_TEST  # Enable host-specific code paths

# Math library
LDFLAGS = -lm

# Output
BUILD_DIR = build
TARGET = $(BUILD_DIR)/run_tests

# Sources
TEST_MAIN = unit_tests.c

# Test files (included via #include)
TEST_FILES = \
    test_framework.h \
    test_signature_feel.c \
    test_cue_processor.c \
    test_cue_to_signature.c

# Source files (included via #include)
SRC_FILES = \
    ../src/wellness_feedback/signature_feel.h \
    ../src/wellness_feedback/signature_feel.c \
    ../src/wellness_feedback/cue_processor.h \
    ../src/wellness_feedback/cue_processor.c \
    ../src/wellness_feedback/cue_to_signature.h \
    ../src/wellness_feedback/cue_to_signature.c

.PHONY: all test clean verbose

all: $(TARGET)
	@echo ""
	@echo "Running tests..."
	@echo ""
	@./$(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(TARGET): $(BUILD_DIR) $(TEST_MAIN) $(TEST_FILES) $(SRC_FILES)
	@echo "Compiling firmware tests..."
	$(CC) $(CFLAGS) -o $(TARGET) $(TEST_MAIN) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

test: $(TARGET)
	@./$(TARGET)

verbose: CFLAGS += -DVERBOSE_TEST
verbose: clean $(TARGET)
	@./$(TARGET)

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete."

# Help target
help:
	@echo "Neural Load Ring Firmware Test Build"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build and run tests (default)"
	@echo "  test     - Run tests without rebuild"
	@echo "  clean    - Remove build artifacts"
	@echo "  verbose  - Build and run with verbose output"
	@echo "  help     - Show this message"
