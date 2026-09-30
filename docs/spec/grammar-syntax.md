# Nevaarize Formal Language Specification: Grammar & Syntax

## 1. Overview

Nevaarize is a statically typed, native JIT-compiled programming language targeting Linux x86-64 architectures. The syntax is designed for high-performance numerical computing, tensor and neural network modeling, concurrent asynchronous workflows, and native system integration without external dependencies.

## 2. Lexical Conventions

### 2.1 Identifiers & The No-Underscore Policy
Nevaarize enforces a strict naming convention at the lexer and parser levels:
- **No-Underscore Policy**: Underscores (`_`) are strictly forbidden in all identifier names (variables, functions, structs, parameters, and aliases). Using an underscore triggers a lexical diagnostic error.
- **Variables & Functions**: Must use `camelCase` (starting with a lowercase ASCII letter).
- **Struct Types**: Must use `PascalCase` (starting with an uppercase ASCII letter).
- **Module Aliases**: Short lowercase alphanumeric identifiers.

### 2.2 Keywords
The following 23 keywords are reserved:

```
async       await       function    func        return
if          elif        else        for         while
in          struct      import      as          stdlib
true        false       and         or          try
catch       throw       finally
```

Both `func` and `function` denote function declarations.

### 2.3 Comments
- Single-line comments begin with `//` and extend to the end of the line.
- Block comments are reserved for future specifications.

### 2.4 Statement Terminators
Statements may be terminated by either:
- A newline character (`\n`)
- A semicolon (`;`)
- A closing brace (`}`) for block structures

Consecutive newlines and semicolons are treated as insignificant whitespace separators.

## 3. Operator Precedence & Associativity

Operators are listed below from lowest to highest precedence:

| Precedence | Operators | Description | Associativity |
|:---|:---|:---|:---|
| 1 | `or` | Logical OR | Left-to-right |
| 2 | `and` | Logical AND | Left-to-right |
| 3 | `==`, `!=` | Equality and Inequality | Left-to-right |
| 4 | `<`, `<=`, `>`, `>=` | Relational Comparisons | Left-to-right |
| 5 | `+`, `-` | Addition and Subtraction | Left-to-right |
| 6 | `*`, `/`, `%` | Multiplication, Division, Modulo | Left-to-right |
| 7 | `-`, `!` | Unary Negation, Logical NOT | Right-to-left |
| 8 | `.`, `()`, `[]` | Member Access, Function Call, Index Access | Left-to-right |

## 4. Statements & Declarations

### 4.1 Variable Assignments
Variables are declared upon their first assignment. The type of a variable is dynamically inferred and tracked:

```nva
x = 10
learningRate = 0.001
name = "nevaarize"
active = true
```

### 4.2 Function Declarations
Functions are first-class constructs declared using `func` or `function`:

```nva
func calculateLoss(prediction, target) {
    diff = prediction - target
    return diff * diff
}
```

Parameters are comma-separated identifiers enclosed in parentheses. A function body is a mandatory braced block `{ ... }`.

### 4.3 Control Flow

#### If / Elif / Else
Conditional branches require parentheses around test expressions:

```nva
if (score >= 90) {
    grade = "A"
} elif (score >= 80) {
    grade = "B"
} else {
    grade = "C"
}
```

#### While Loop
```nva
count = 0
while (count < 10) {
    count = count + 1
}
```

#### For Loop with Range
Bounded numeric iteration is executed via `for (variable in Range(start, end))`:

```nva
for (i in Range(0, 100)) {
    sum = sum + i
}
```

### 4.4 Exception Handling
Exceptions can be thrown with any primitive or object value and handled via `try / catch / finally`:

```nva
try {
    if (divisor == 0) {
        throw "DivisionByZeroError"
    }
    result = value / divisor
} catch (err) {
    print("Caught error:", err)
} finally {
    cleanupResources()
}
```

### 4.5 Module Imports
Standard libraries and local source files are imported via:

```nva
import stdlib math as m
import stdlib ai as nn
import "models/classifier.nva" as cls
```

Local imports are resolved strictly relative to the source directory of the importing file.
