#!/bin/bash

# Nevaarize Comprehensive Test Runner & Report Generator
# This script runs all .nva examples, shows progress, and captures detailed output.

REPORT_FILE="examples/verification_report.txt"
BIN="./bin/nevaarize"

# Ensure binary exists
if [ ! -f "$BIN" ]; then
    echo "Error: Binary not found at $BIN. Please run 'make' first."
    exit 1
fi

echo "=========================================================="
echo "   Nevaarize Comprehensive Runner (Examples & Benchmarks)"
echo "=========================================================="
echo "Starting test suite at $(date)"
echo ""

# Detect System Specs
CPU_INFO=$(grep "model name" /proc/cpuinfo | head -n 1 | cut -d ':' -f 2 | xargs)
MEM_TOTAL=$(grep MemTotal /proc/meminfo | awk '{print $2/1024 " MB"}')
OS_INFO=$(cat /etc/os-release | grep PRETTY_NAME | cut -d '"' -f 2)

# Initialize Report
echo "Nevaarize Execution & Benchmark Report" > "$REPORT_FILE"
echo "Generated on: $(date)" >> "$REPORT_FILE"
echo "--------------------------------------" >> "$REPORT_FILE"
echo "SYSTEM SPECIFICATIONS:" >> "$REPORT_FILE"
echo "  OS:  $OS_INFO" >> "$REPORT_FILE"
echo "  CPU: $CPU_INFO" >> "$REPORT_FILE"
echo "  RAM: $MEM_TOTAL" >> "$REPORT_FILE"
echo "--------------------------------------" >> "$REPORT_FILE"

# Find and sort all .nva files
FILES=$(find examples -name "*.nva" | sort)
TOTAL=$(echo "$FILES" | wc -l)
COUNT=0
PASSED=0

for f in $FILES; do
    COUNT=$((COUNT + 1))
    echo -n "[$COUNT/$TOTAL] Running $f..."
    
    # Run with 10s timeout and capture stdout/stderr
    temp_out=$(timeout 10s "$BIN" "$f" 2>&1)
    exit_code=$?
    
    echo -n " [ " >> "$REPORT_FILE"
    
    if [ $exit_code -eq 0 ]; then
        echo -e "\033[0;32m PASS \033[0m"
        PASSED=$((PASSED + 1))
        echo "x] $f: PASS" >> "$REPORT_FILE"
        echo "    Output Details:" >> "$REPORT_FILE"
        echo "$temp_out" | sed 's/^/      /' >> "$REPORT_FILE"
    elif [ $exit_code -eq 124 ]; then
        echo -e "\033[0;31m TIMEOUT \033[0m"
        echo " ] $f: FAIL (Timeout)" >> "$REPORT_FILE"
        echo "    Error Details: Execution timed out after 10s" >> "$REPORT_FILE"
    else
        echo -e "\033[0;31m FAIL \033[0m"
        echo " ] $f: FAIL (Exit Code: $exit_code)" >> "$REPORT_FILE"
        echo "    Error Details:" >> "$REPORT_FILE"
        echo "$temp_out" | sed 's/^/      /' >> "$REPORT_FILE"
    fi
    
    echo "--------------------------------------" >> "$REPORT_FILE"
done

echo ""
echo "=========================================================="
echo "RESULTS SUMMARY:"
echo "Total Tests: $TOTAL"
echo "Passed:      $PASSED"
echo "Failed:      $((TOTAL - PASSED))"
echo ""
echo "Detailed report saved to: $REPORT_FILE"
echo "=========================================================="
