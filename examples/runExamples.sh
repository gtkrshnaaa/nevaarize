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

PEAK_RAM=0
PEAK_CPU=0
PEAK_RAM_TEST=""
PEAK_CPU_TEST=""

for f in $FILES; do
    COUNT=$((COUNT + 1))
    echo -n "[$COUNT/$TOTAL] Running $f..."
    
    # Run with 10s timeout, use /usr/bin/time to capture metrics
    TIME_LOG="/tmp/nv_time_${COUNT}.log"
    temp_out=$(/usr/bin/time -v -o "$TIME_LOG" timeout 10s "$BIN" "$f" 2>&1)
    exit_code=$?
    
    RAM_KB=$(grep "Maximum resident set size" "$TIME_LOG" | awk '{print $6}')
    CPU_PCT=$(grep "Percent of CPU this job got" "$TIME_LOG" | awk '{print $7}' | tr -d '%')
    
    if [ ! -z "$RAM_KB" ] && [ "$RAM_KB" -gt "$PEAK_RAM" ]; then
        PEAK_RAM=$RAM_KB
        PEAK_RAM_TEST=$f
    fi
    if [ ! -z "$CPU_PCT" ] && [ "$CPU_PCT" -gt "$PEAK_CPU" ]; then
        PEAK_CPU=$CPU_PCT
        PEAK_CPU_TEST=$f
    fi
    
    echo -n " [ " >> "$REPORT_FILE"
    
    if [ $exit_code -eq 0 ]; then
        echo -e "\033[0;32m PASS \033[0m (RAM: ${RAM_KB}KB, CPU: ${CPU_PCT}%)"
        PASSED=$((PASSED + 1))
        echo "x] $f: PASS (RAM: ${RAM_KB}KB, CPU: ${CPU_PCT}%)" >> "$REPORT_FILE"
        echo "    Output Details:" >> "$REPORT_FILE"
        echo "$temp_out" | sed 's/^/      /' >> "$REPORT_FILE"
    elif [ $exit_code -eq 124 ]; then
        echo -e "\033[0;31m TIMEOUT \033[0m (RAM: ${RAM_KB}KB, CPU: ${CPU_PCT}%)"
        echo " ] $f: FAIL (Timeout) (RAM: ${RAM_KB}KB, CPU: ${CPU_PCT}%)" >> "$REPORT_FILE"
        echo "    Error Details: Execution timed out after 10s" >> "$REPORT_FILE"
    else
        echo -e "\033[0;31m FAIL \033[0m (RAM: ${RAM_KB}KB, CPU: ${CPU_PCT}%)"
        echo " ] $f: FAIL (Exit Code: $exit_code) (RAM: ${RAM_KB}KB, CPU: ${CPU_PCT}%)" >> "$REPORT_FILE"
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
echo "--------------------------------------"
echo "PERFORMANCE METRICS:"
echo "Peak RAM:    ${PEAK_RAM} KB (by $PEAK_RAM_TEST)"
echo "Peak CPU:    ${PEAK_CPU}% (by $PEAK_CPU_TEST)"
echo ""
echo "Detailed report saved to: $REPORT_FILE"
echo "=========================================================="

# Append summary to report file
echo "" >> "$REPORT_FILE"
echo "==========================================================" >> "$REPORT_FILE"
echo "RESULTS SUMMARY:" >> "$REPORT_FILE"
echo "Total Tests: $TOTAL" >> "$REPORT_FILE"
echo "Passed:      $PASSED" >> "$REPORT_FILE"
echo "Failed:      $((TOTAL - PASSED))" >> "$REPORT_FILE"
echo "--------------------------------------" >> "$REPORT_FILE"
echo "PERFORMANCE METRICS:" >> "$REPORT_FILE"
echo "Peak RAM:    ${PEAK_RAM} KB (by $PEAK_RAM_TEST)" >> "$REPORT_FILE"
echo "Peak CPU:    ${PEAK_CPU}% (by $PEAK_CPU_TEST)" >> "$REPORT_FILE"
echo "==========================================================" >> "$REPORT_FILE"
