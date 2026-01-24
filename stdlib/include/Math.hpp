/**
 * Math.hpp - Nevaarize Math Standard Library
 *
 * Mathematical functions module.
 */

#ifndef NEVAARIZE_STDLIB_MATH_HPP
#define NEVAARIZE_STDLIB_MATH_HPP

#include "../../core/include/Value.hpp"
#include <unordered_map>

namespace nevaarize {
namespace stdlib {

/**
 * Get all math library functions.
 */
std::unordered_map<std::string, NativeFunction> getMathLibrary();

} // namespace stdlib
} // namespace nevaarize

#endif // NEVAARIZE_STDLIB_MATH_HPP
