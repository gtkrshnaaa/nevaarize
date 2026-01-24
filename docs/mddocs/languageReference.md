# Nevaarize Language Reference

## Table of Contents
1. [Path Resolution Rules](#path-resolution-rules)
2. [Basic Syntax](#basic-syntax)
3. [Data Types](#data-types)
4. [Control Flow](#control-flow)
5. [Functions](#functions)
6. [Structs](#structs)
7. [Module System](#module-system)
8. [Async/Await](#asyncawait)

---

## Path Resolution Rules

> [!IMPORTANT]
> **CRITICAL: All file paths in Nevaarize are resolved RELATIVE to the source file being executed, NOT relative to the current working directory.**

This behavior is identical to:
- `#include` in C/C++
- `<a href>` in HTML
- `import` in Python (when using relative imports)

### Path Resolution Behavior

```nva
// File: /project/src/main.nva
import "utils.nva" as utils           // Resolves to: /project/src/utils.nva
import "lib/helper.nva" as helper     // Resolves to: /project/src/lib/helper.nva
import "../config.nva" as config      // Resolves to: /project/config.nva
```

### Examples

#### Example 1: Same Directory
```
project/
├── main.nva
└── utils.nva
```

```nva
// File: project/main.nva
import "utils.nva" as utils
```
✅ Resolves to: `project/utils.nva`

#### Example 2: Subdirectory
```
project/
├── main.nva
└── lib/
    └── helper.nva
```

```nva
// File: project/main.nva
import "lib/helper.nva" as helper
```
✅ Resolves to: `project/lib/helper.nva`

#### Example 3: Parent Directory
```
project/
├── config.nva
└── src/
    └── main.nva
```

```nva
// File: project/src/main.nva
import "../config.nva" as config
```
✅ Resolves to: `project/config.nva`

#### Example 4: Nested Imports
```
project/
├── main.nva
└── modules/
    ├── importer.nva
    └── utils/
        └── helper.nva
```

```nva
// File: project/modules/importer.nva
import "utils/helper.nva" as helper
```
✅ Resolves to: `project/modules/utils/helper.nva`

### Working Directory Independence

> [!WARNING]
> Path resolution is **INDEPENDENT** of where you run the `nevaarize` command.

```bash
# All of these produce the SAME result:
cd /project && nevaarize src/main.nva
cd /project/src && nevaarize main.nva
cd / && nevaarize /project/src/main.nva
```

In all cases, `import "utils.nva"` inside `main.nva` resolves to `/project/src/utils.nva`.

### Absolute Paths

Absolute paths are **NOT recommended** but supported:

```nva
import "/absolute/path/to/module.nva" as mod
```

> [!CAUTION]
> Absolute paths break portability. Always use relative paths.

---

## Basic Syntax

### Comments

```nva
// Single-line comment

// Multi-line comments are multiple single-line comments
// like this
```

### Variables

```nva
x = 42              // Integer
y = 3.14            // Float
name = "Nevaarize"  // String
flag = true         // Boolean
nothing = nil       // Nil
```

### Operators

**Arithmetic:**
```nva
a + b    // Addition
a - b    // Subtraction
a * b    // Multiplication
a / b    // Division
a % b    // Modulo
```

**Comparison:**
```nva
a == b   // Equal
a != b   // Not equal
a < b    // Less than
a <= b   // Less than or equal
a > b    // Greater than
a >= b   // Greater than or equal
```

**Logical:**
```nva
a and b  // Logical AND
a or b   // Logical OR
not a    // Logical NOT
```

---

## Data Types

### Primitives

```nva
nil      // Null/undefined value
true     // Boolean true
false    // Boolean false
42       // Integer (int64)
3.14     // Float (double)
"text"   // String
```

### Arrays

```nva
arr = [1, 2, 3, 4, 5]
arr[0]              // Access: 1
arr[0] = 10         // Modify
arr.push(6)         // Add element
arr.pop()           // Remove last
arr.length          // Get length
```

### Structs

```nva
struct Point {
    x
    y
}

p = Point()
p.x = 10
p.y = 20
```

---

## Control Flow

### If/Elif/Else

```nva
if (x > 10) {
    print("Large")
} elif (x > 5) {
    print("Medium")
} else {
    print("Small")
}
```

### For Loop

```nva
for (i in Range(1, 11)) {
    print(i)
}

for (item in array) {
    print(item)
}
```

### While Loop

```nva
while (condition) {
    // loop body
}
```

---

## Functions

### Basic Functions

```nva
func add(a, b) {
    return a + b
}

result = add(10, 20)
```

### Closures

```nva
func makeCounter() {
    count = 0
    
    func increment() {
        count = count + 1
        return count
    }
    
    return increment
}

counter = makeCounter()
print(counter())  // 1
print(counter())  // 2
```

---

## Structs

```nva
struct Vector3 {
    x
    y
    z
}

func magnitude(v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
}

v = Vector3()
v.x = 3
v.y = 4
v.z = 0

mag = magnitude(v)  // 5.0
```

---

## Module System

### Importing Files

> [!IMPORTANT]
> Paths are **relative to the importing file**, not the working directory.

```nva
import "utils.nva" as utils
import "lib/helper.nva" as helper
import "../config.nva" as config
```

### Importing Standard Library

```nva
import stdlib math as m
import stdlib io as io
import stdlib time as t
```

### Using Imported Modules

```nva
import "utils.nva" as utils

result = utils.square(5)
```

---

## Async/Await

### Basic Async

```nva
async func fetchData(id) {
    // Async operation
    return "Data" + str(id)
}

data = await fetchData(1)
```

### Sequential Async

```nva
async func loadUser(id) {
    user = await fetchUser(id)
    posts = await fetchPosts(user)
    return posts
}
```

### Parallel Async

```nva
async func parallelFetch() {
    task1 = fetchData(1)
    task2 = fetchData(2)
    
    result1 = await task1
    result2 = await task2
    
    return [result1, result2]
}
```

---

## No-Underscore Policy

> [!CAUTION]
> Underscores `_` are **FORBIDDEN** in all identifiers.

```nva
// ❌ REJECTED
my_variable = 10
func calculate_sum(a, b) { }

// ✅ ACCEPTED
myVariable = 10
func calculateSum(a, b) { }
```

This policy ensures:
- Visual consistency
- Clean, modern code aesthetic
- No mixing of naming conventions

---

## Built-in Functions

| Function | Description | Example |
|----------|-------------|---------|
| `print(...)` | Output to stdout | `print("Hello", 42)` |
| `Range(start, end)` | Generate sequence | `Range(1, 10)` |
| `len(obj)` | Get length | `len([1,2,3])` → 3 |
| `type(val)` | Get type name | `type(42)` → "int" |
| `str(val)` | Convert to string | `str(123)` → "123" |
| `int(val)` | Convert to integer | `int("42")` → 42 |
| `float(val)` | Convert to float | `float(10)` → 10.0 |

---

## Standard Library

### Math Module

```nva
import stdlib math as m

m.Abs(x)      // Absolute value
m.Sqrt(x)     // Square root
m.Pow(x, y)   // x^y
m.Floor(x)    // Round down
m.Ceil(x)     // Round up
m.Sin(x)      // Sine
m.Cos(x)      // Cosine
m.Tan(x)      // Tangent
```

### IO Module

```nva
import stdlib io as io

io.Print(...)         // Print with newline
io.Input(prompt)      // Read line
io.Write(...)         // Print without newline
```

### Time Module

```nva
import stdlib time as t

t.clock()      // Current time (seconds)
t.sleep(ms)    // Sleep milliseconds
```
