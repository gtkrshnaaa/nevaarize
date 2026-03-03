/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * string.hpp - Nevaarize Standard Library
 *
 * Provides string manipulation functions for the Nevaarize interpreter fallback.
 */

#ifndef NEVAARIZE_STDLIB_STRING_HPP
#define NEVAARIZE_STDLIB_STRING_HPP

#include "value.hpp"

namespace nevaarize {
namespace stdlib {

// Registrasi namespace std.string
std::unordered_map<std::string, NativeFunction> getStringLibrary();

} // namespace stdlib
} // namespace nevaarize

#endif // NEVAARIZE_STDLIB_STRING_HPP
