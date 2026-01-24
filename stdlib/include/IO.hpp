/**
 * IO.hpp - Nevaarize IO Standard Library
 *
 * Input/Output functions module.
 */

#ifndef NEVAARIZE_STDLIB_IO_HPP
#define NEVAARIZE_STDLIB_IO_HPP

#include "../../core/include/Value.hpp"
#include <unordered_map>

namespace nevaarize {
namespace stdlib {

/**
 * Get all IO library functions.
 */
std::unordered_map<std::string, NativeFunction> getIOLibrary();

} // namespace stdlib
} // namespace nevaarize

#endif // NEVAARIZE_STDLIB_IO_HPP
