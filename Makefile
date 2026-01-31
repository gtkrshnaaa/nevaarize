# Nevaarize Makefile
# Native JIT Compiler Build System

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -O3 -march=native -fno-rtti -fno-stack-protector
CXXFLAGS_DEBUG := -std=c++23 -Wall -Wextra -Wpedantic -g -O0 -DDEBUG

# Directories
SRC_DIR := core/src
STDLIB_SRC := stdlib/src
INC_DIR := core/include
STDLIB_INC := stdlib/include
BUILD_DIR = build
BIN_DIR := bin

# Source files
SOURCES_CORE := $(wildcard $(SRC_DIR)/*.cpp)
SOURCES_STDLIB := $(wildcard $(STDLIB_SRC)/*.cpp)
OBJECTS_CORE := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES_CORE))
OBJECTS_STDLIB := $(patsubst $(STDLIB_SRC)/%.cpp,$(BUILD_DIR)/stdlib/%.o,$(SOURCES_STDLIB))
OBJECTS := $(OBJECTS_CORE) $(OBJECTS_STDLIB)
OBJECTS_DEBUG_CORE := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/debug/%.o,$(SOURCES_CORE))
OBJECTS_DEBUG_STDLIB := $(patsubst $(STDLIB_SRC)/%.cpp,$(BUILD_DIR)/debug/stdlib/%.o,$(SOURCES_STDLIB))
OBJECTS_DEBUG := $(OBJECTS_DEBUG_CORE) $(OBJECTS_DEBUG_STDLIB)

# Target
TARGET := $(BIN_DIR)/nevaarize
TARGET_DEBUG := $(BIN_DIR)/nevaarize-debug

# Include path
INCLUDES := -I$(INC_DIR) -I$(STDLIB_INC)

# Default target
.PHONY: all
all: release

# Release build
.PHONY: release
release: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@
	@echo "✓ Build complete: $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/stdlib/%.o: $(STDLIB_SRC)/%.cpp | $(BUILD_DIR)/stdlib
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Debug build
.PHONY: debug
debug: $(TARGET_DEBUG)

$(TARGET_DEBUG): $(OBJECTS_DEBUG) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS_DEBUG) $(OBJECTS_DEBUG) -o $@
	@echo "✓ Debug build complete: $@"

$(BUILD_DIR)/debug/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)/debug
	$(CXX) $(CXXFLAGS_DEBUG) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/debug/stdlib/%.o: $(STDLIB_SRC)/%.cpp | $(BUILD_DIR)/debug/stdlib
	$(CXX) $(CXXFLAGS_DEBUG) $(INCLUDES) -c $< -o $@

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/stdlib:
	mkdir -p $(BUILD_DIR)/stdlib

$(BUILD_DIR)/debug:
	mkdir -p $(BUILD_DIR)/debug

$(BUILD_DIR)/debug/stdlib:
	mkdir -p $(BUILD_DIR)/debug/stdlib

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "✓ Clean complete"

# Rebuild
.PHONY: rebuild
rebuild: clean all

# Run interpreter
.PHONY: run
run: $(TARGET)
	./$(TARGET)

# Run with script
.PHONY: run-script
run-script: $(TARGET)
	@if [ -z "$(SCRIPT)" ]; then \
		echo "Usage: make run-script SCRIPT=path/to/script.nva"; \
	else \
		./$(TARGET) $(SCRIPT); \
	fi

# Run examples
.PHONY: run-examples
run-examples: $(TARGET)
	@echo "Running examples..."
	@for f in examples/**/*.nva; do \
		if [ -f "$$f" ]; then \
			echo "=== $$f ==="; \
			./$(TARGET) "$$f" || true; \
			echo ""; \
		fi; \
	done

# Run benchmarks
.PHONY: benchmark
benchmark: $(TARGET)
	@echo "Running benchmarks..."
	@for f in examples/06_benchmarks/*.nva; do \
		if [ -f "$$f" ]; then \
			echo "=== $$f ==="; \
			./$(TARGET) "$$f"; \
			echo ""; \
		fi; \
	done

# Install to /usr/local/bin
.PHONY: install
install: clean release
	install -m 755 $(TARGET) /usr/local/bin/nevaarize
	@echo "✓ Installed to /usr/local/bin/nevaarize"

# Uninstall
.PHONY: uninstall
uninstall:
	rm -f /usr/local/bin/nevaarize
	@echo "✓ Uninstalled nevaarize"

# Listing Target
list:
	@mkdir -p z_listing
	@echo "================================================================================" > z_listing/listing.txt
	@echo "NEVAARIZE PROJECT CODEBASE LISTING" >> z_listing/listing.txt
	@echo "Generated on: $$(date)" >> z_listing/listing.txt
	@echo "================================================================================" >> z_listing/listing.txt
	@echo "" >> z_listing/listing.txt
	@echo "This document contains a comprehensive listing of all source code files within" >> z_listing/listing.txt
	@echo "the Nevaarize project, including core, stdlib, examples, and languagebench." >> z_listing/listing.txt
	@echo "Nevaarize is a high-performance, native JIT compiler language built with C++23." >> z_listing/listing.txt
	@echo "" >> z_listing/listing.txt
	@echo "Directory Structure Overview:" >> z_listing/listing.txt
	@echo "1. core/include: Header files for the JIT compiler, Lexer, Parser, and IR." >> z_listing/listing.txt
	@echo "2. core/src: Implementation of the native JIT engine and x86-64 code generator." >> z_listing/listing.txt
	@echo "3. stdlib: Standard library implementations (Math, IO, Time)." >> z_listing/listing.txt
	@echo "4. examples: Collection of example scripts demonstrating language features." >> z_listing/listing.txt
	@echo "5. languagebench: Performance benchmarks across various languages." >> z_listing/listing.txt
	@echo "" >> z_listing/listing.txt
	@echo "================================================================================" >> z_listing/listing.txt
	@echo "" >> z_listing/listing.txt
	@for f in $$(find core stdlib examples languagebench -type f | sort) Makefile; do \
		echo "FILE: $$f" >> z_listing/listing.txt; \
		echo "--------------------------------------------------------------------------------" >> z_listing/listing.txt; \
		cat "$$f" >> z_listing/listing.txt; \
		echo -e "\n\n" >> z_listing/listing.txt; \
	done
	@echo "Codebase listing generated at z_listing/listing.txt"

# Help
.PHONY: all release debug clean rebuild run run-script run-examples benchmark install uninstall help list
help:
	@echo "Nevaarize Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build release version (default)"
	@echo "  release      Build optimized JIT compiler"
	@echo "  debug        Build debug version with symbols"
	@echo "  clean        Remove build artifacts"
	@echo "  rebuild      Clean and rebuild"
	@echo "  run          Run REPL"
	@echo "  run-script   Run script: make run-script SCRIPT=file.nva"
	@echo "  run-examples Run all example scripts"
	@echo "  benchmark    Run performance benchmarks"
	@echo "  list         Generate codebase listing"
	@echo "  install      Install to /usr/local/bin"
	@echo "  uninstall    Remove from /usr/local/bin"
	@echo "  help         Show this help"
