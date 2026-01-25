# Nevaarize Examples Runner
# Usage: make run-all OR make run FILE=path/to/file.nva

NEVAARIZE := ../bin/nevaarize

# Find all .nva files
NVA_FILES := $(shell find . -name "*.nva" -type f)

.PHONY: run-all run list help

help:
	@echo "Nevaarize Examples Runner"
	@echo "========================="
	@echo "  make list      - List all .nva files"
	@echo "  make run-all   - Run all .nva files (shows pass/fail)"
	@echo "  make run FILE=path/to/file.nva - Run specific file"
	@echo ""
	@echo "Example: make run FILE=float_test.nva"

list:
	@echo "Available .nva files:"
	@for f in $(NVA_FILES); do echo "  $$f"; done

run:
ifndef FILE
	@echo "Error: FILE not specified. Usage: make run FILE=path/to/file.nva"
else
	@echo "=== Running $(FILE) ==="
	@$(NEVAARIZE) $(FILE) && echo "✓ PASSED" || echo "✗ FAILED"
endif

run-all:
	@echo "Running all Nevaarize examples..."
	@echo "================================="
	@passed=0; failed=0; \
	for f in $(NVA_FILES); do \
		echo ""; \
		echo ">>> $$f"; \
		if timeout 5 $(NEVAARIZE) "$$f" 2>&1; then \
			echo "✓ PASSED"; \
			passed=$$((passed + 1)); \
		else \
			echo "✗ FAILED (exit code: $$?)"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "================================"; \
	echo "Results: $$passed passed, $$failed failed"

# Run basic tests only (known working files)
run-basic:
	@echo "Running basic tests..."
	@$(NEVAARIZE) float_test.nva && echo "✓ float_test.nva"
	@$(NEVAARIZE) printTest.nva && echo "✓ printTest.nva"
	@$(NEVAARIZE) benchmarks/benchmark.nva && echo "✓ benchmark.nva"
