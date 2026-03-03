/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Time.hpp - Nevaarize Time Standard Library
 *
 * Time and timing functions module.
 */

#ifndef NEVAARIZE_STDLIB_TIME_HPP
#define NEVAARIZE_STDLIB_TIME_HPP

#include "value.hpp"
#include <unordered_map>

namespace nevaarize {
namespace stdlib {

/**
 * Get all time library functions.
 */
std::unordered_map<std::string, NativeFunction> getTimeLibrary();

} // namespace stdlib
} // namespace nevaarize

#endif // NEVAARIZE_STDLIB_TIME_HPP
