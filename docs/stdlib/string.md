# Standard Library: `string`

The `string` module and built-in string methods provide operations for searching, transforming, slicing, and formatting text strings.

```nva
import stdlib string as str
```

Strings support method-call syntax on instances directly (`strVal.method()`) as well as namespace functions (`str.method(strVal, ...)`).

## Function Reference

- `toUpperCase(str)` / `str.toUpperCase()`: Converts ASCII characters to uppercase.
- `toLowerCase(str)` / `str.toLowerCase()`: Converts ASCII characters to lowercase.
- `trim(str)` / `str.trim()`: Strips leading and trailing whitespace characters.
- `split(str, delimiter)` / `str.split(delim)`: Splits the string into an array of substrings separated by the specified delimiter.
- `replace(str, oldSub, newSub)` / `str.replace(old, new)`: Replaces occurrences of `oldSub` with `newSub`.
- `substring(str, start, length)` / `str.substring(start, length)`: Returns a substring of specified `length` starting at offset `start`.
- `contains(str, substr)` / `str.contains(substr)`: Returns `1` (true) if `substr` exists within `str`, otherwise `0` (false).
- `indexOf(str, substr)` / `str.indexOf(substr)`: Returns the 0-based index of the first occurrence of `substr`, or `-1` if not found.
- `startsWith(str, prefix)` / `str.startsWith(prefix)`: Returns `1` if string begins with `prefix`, else `0`.
- `endsWith(str, suffix)` / `str.endsWith(suffix)`: Returns `1` if string ends with `suffix`, else `0`.
- `length(str)` / `str.length()`: Returns the number of bytes or characters in the string.

### String Manipulation Example

```nva
import stdlib string as str

greeting = "  Hello, Nevaarize World!  "
cleaned = greeting.trim()
upper = cleaned.toUpperCase()
words = cleaned.split(" ")

print("Length:", cleaned.length())
print("Uppercase:", upper)
print("First word:", words[0])
```
