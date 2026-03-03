#!/bin/bash
# Nevaarize Benchmark Environment Setup Script
# Installs compilers and runtimes for all 13 languages in the benchmark suite.

set -e

echo "Updating package lists..."
sudo apt update

echo "Installing Nevaarize build dependencies (C++23)..."
sudo apt install -y g++-13 make time git curl wget

# Set g++-13 as default if available (needs to be >= 13 for C++23)
if command -v g++-13 &> /dev/null; then
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100
fi

echo "Installing language runtimes and compilers..."

# C/C++ (already handled by g++-13 above)

# Rust
if ! command -v rustc &> /dev/null; then
    echo "Installing Rust..."
    sudo apt install -y rustc
fi

# Go
if ! command -v go &> /dev/null; then
    echo "Installing Go..."
    sudo apt install -y golang-go
fi

# Java (OpenJDK 21 recommended)
if ! command -v java &> /dev/null; then
    echo "Installing Java 21..."
    sudo apt install -y openjdk-21-jdk
fi

# Node.js
if ! command -v node &> /dev/null; then
    echo "Installing Node.js..."
    sudo apt install -y nodejs
fi

# PHP
if ! command -v php &> /dev/null; then
    echo "Installing PHP 8..."
    sudo apt install -y php-cli
fi

# Lua & LuaJIT
echo "Installing Lua and LuaJIT..."
sudo apt install -y lua5.4 luajit

# Python 3 (usually present, but ensuring)
sudo apt install -y python3

# Zig (Often requires snap or PPA on Ubuntu, or direct download)
if ! command -v zig &> /dev/null; then
    echo "Installing Zig via Snap..."
    if command -v snap &> /dev/null; then
        sudo snap install zig --classic --beta || echo "Warning: Snap install failed, please install Zig manually from ziglang.org"
    else
        echo "Snap not found. Please install Zig manually from https://ziglang.org/download/"
    fi
fi

echo ""
echo "================================================================================"
echo "  Setup Complete! All dependencies for Nevaarize and LanguageBench are ready."
echo "================================================================================"
echo "  To run benchmarks:"
echo "    cd languagebench"
echo "    ./runComparison.sh"
echo "================================================================================"
