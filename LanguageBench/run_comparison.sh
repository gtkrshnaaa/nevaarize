#!/bin/bash

# Build Nevaarize first
make -s

REPORT_FILE="LanguageBench/battle_report.txt"

# Detect System Specs
CPU_INFO=$(grep "model name" /proc/cpuinfo | head -n 1 | cut -d ':' -f 2 | xargs)
MEM_TOTAL=$(grep MemTotal /proc/meminfo | awk '{print $2/1024 " MB"}')
OS_INFO=$(cat /etc/os-release | grep PRETTY_NAME | cut -d '"' -f 2)

# Resource Monitor Wrapper function
function run_bench() {
    # -f writes to stderr by default. Redirect stderr to stdout to capture it in the pipe/file.
    /usr/bin/time -f "\n[System Resources]\n  Peak RAM : %M KB\n  CPU Load : %P\n  Total Time: %e s" "$@" 2>&1
}

{
echo "================================================================================"
echo "                   ULTIMATE LANGUAGE BENCHMARK BATTLE"
echo "================================================================================"
echo "Execution Date: $(date)"
echo ""
echo "SYSTEM SPECIFICATIONS:"
echo "  OS:  $OS_INFO"
echo "  CPU: $CPU_INFO"
echo "  RAM: $MEM_TOTAL"
echo "================================================================================"

# Helper for clearer separation
function print_sep() {
    echo ""
    echo "================================================================================"
}

print_sep
echo "--- Nevaarize ---"
echo "Arch: JIT Compiled (x86-64 Native) - High Performance"
run_bench ./bin/nevaarize LanguageBench/bench_nevaarize.nva

# C++
print_sep
echo "--- C++ (GCC -O3) ---"
echo "Arch: Native Machine Code (Compiled)"
g++ -O3 -std=c++20 -o LanguageBench/bench_cpp LanguageBench/bench_cpp.cpp
run_bench ./LanguageBench/bench_cpp
rm LanguageBench/bench_cpp

# C
print_sep
echo "--- C (GCC -O3) ---"
echo "Arch: Native Machine Code (Compiled)"
gcc -O3 -o LanguageBench/bench_c LanguageBench/bench_c.c
run_bench ./LanguageBench/bench_c
rm LanguageBench/bench_c

# Go
print_sep
echo "--- Go 1.21 ---"
echo "Arch: Native Machine Code (Compiled + GC)"
if command -v go &> /dev/null; then
    run_bench go run LanguageBench/bench_go.go
else
    echo "Go not found. Please install: sudo apt install golang-go"
fi

# Java
print_sep
echo "--- Java 21 ---"
echo "Arch: JIT Compiled - HotSpot VM (Machine Code)"
if command -v java &> /dev/null; then
    javac LanguageBench/BenchJava.java
    run_bench java -cp LanguageBench BenchJava
    rm LanguageBench/BenchJava.class LanguageBench/BenchJava\$Obj.class
else
    echo "Java not found. Please install: sudo apt install default-jdk"
fi

# Node.js
print_sep
echo "--- Node.js ---"
echo "Arch: JIT Compiled - V8 Engine (Machine Code)"
run_bench node LanguageBench/bench_node.js

# PHP (Standard)
print_sep
echo "--- PHP 8 (Standard) ---"
echo "Arch: Bytecode (Stack-based) - Zend (OpCache Only)"
run_bench php -d opcache.jit_buffer_size=0 LanguageBench/bench_php.php

# PHP (JIT)
print_sep
echo "--- PHP 8 (JIT) ---"
echo "Arch: Zend Engine + JIT Enabled (Max Power)"
run_bench php -d opcache.enable_cli=1 -d opcache.jit=1255 -d opcache.jit_buffer_size=128M LanguageBench/bench_php.php

# Lua (Standard)
print_sep
echo "--- Lua 5.4 ---"
echo "Arch: Bytecode (Register-based) - Standard Interpreter"
if command -v lua &> /dev/null; then
    run_bench lua LanguageBench/bench_lua.lua
else
    echo "Lua not found. Please install: sudo apt install lua5.4"
fi

# LuaJIT
print_sep
echo "--- LuaJIT (2.1-dev) ---"
echo "Arch: JIT Compiled - Trace-based Compiler"
if command -v luajit &> /dev/null; then
    run_bench luajit LanguageBench/bench_lua.lua
else
    echo "LuaJIT not found. Please install: sudo apt install luajit"
fi

# Python
print_sep
echo "--- Python 3 ---"
echo "Arch: Bytecode (Stack-based) - CPython Interpreter"
run_bench python3 LanguageBench/bench_python.py

print_sep
echo "Benchmark Complete."

} | tee "$REPORT_FILE"

echo ""
echo "Detailed report saved to: $REPORT_FILE"
