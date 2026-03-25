# Getting Started with Nevaarize

> **Note:** Nevaarize is under active development. Only scripts inside the `examples/` directory have been verified to run correctly. Scripts outside the provided examples may encounter unexpected behavior.

## Installation

### Prerequisites

- **C++23 Compiler**: GCC 13+ or Clang 16+
- **Linux x86-64 CPU**: SSE4.2+ required, AVX2 recommended
- **Operating System**: Linux or macOS

### Building from Source

```bash
# Clone the repository
git clone https://github.com/gtkrshnaaa/nevaarize.git
cd nevaarize

# Build release version
make
```

### Verify Installation

```bash
# Check version
./bin/nevaarize --version
```

---

## Your First Program

Create a file `hello.nva`:

```nva
print("Hello, Nevaarize!")
```

Run it:

```bash
nevaarize hello.nva
```

---

## Language Basics

### Variables

```nva
x = 10              // Integer
y = 3.14            // Float
name = "Nevaarize"  // String
flag = true         // Boolean
```

### Functions

```nva
func greet(name) {
    return "Hello, " + name
}

message = greet("World")
print(message)
```

### Control Flow

```nva
if (x > 10) {
    print("Large")
} else {
    print("Small")
}

for (i in Range(1, 6)) {
    print(i)
}
```

### Data Structures

```nva
// Arrays
arr = [1, 2, 3, 4, 5]
arr.push(6)
print(arr[0])

// Structs
struct Point {
    x,
    y
}

p = Point(10, 20)
print(p.x)  // 10
print(p.y)  // 20
```

---

## Standard Library

### Built-in Functions

```nva
print("Hello")      // Output
len([1, 2, 3])      // Length: 3
type(42)            // Type: "int"
Range(1, 10)        // Generate sequence
```

### Math Module

```nva
import stdlib math as m

result = m.Sqrt(16)     // 4.0
angle = m.Sin(3.14159)  // ~0
power = m.Pow(2, 10)    // 1024
```

### Time Module

```nva
import stdlib time as t

start = t.clock()
// ... computation ...
elapsed = t.clock() - start
print("Elapsed:", elapsed, "seconds")
```

---

## Next Steps

- Read the [Language Reference](LANGUAGEREFERENCE.md)
- Explore [Example Programs](../../examples/)
