# Nevaarize Standard Library Reference

## Overview

The Nevaarize standard library provides built-in modules implemented natively in optimized C++23. Standard library modules require zero external package installations and are imported using the `stdlib` keyword:

```nva
import stdlib <moduleName> as <alias>
```

## Module Directory

| Module | Description | Reference Guide |
|:---|:---|:---|
| `ai` | Deep learning models, layers, activations, loss functions, optimizers, and tensor operations | [ai.md](./ai.md) |
| `simd` | Hardware-accelerated AVX2 and AVX-512 vector mathematical reductions and linear algebra | [simd.md](./simd.md) |
| `math` | Elementary, trigonometric, logarithmic, rounding, and pseudo-random mathematical functions | [math.md](./math.md) |
| `time` | Nanosecond-precision timing, benchmarking, clock queries, and execution sleep primitives | [time.md](./time.md) |
| `io` | Buffered and unbuffered console and file input/output routines | [io.md](./io.md) |
| `http` | Lightweight HTTP web server, route dispatcher, and network request utilities | [http.md](./http.md) |
| `json` | RFC 8259 JSON parser, serializer, and dynamic key-value tree extractor | [json.md](./json.md) |
| `csv` | High-throughput delimiter-separated tabular data parser and formatter | [csv.md](./csv.md) |
| `claw` | Web data crawling, HTML tag extraction, CSS selection, and export pipelines | [claw.md](./claw.md) |
| `string` | String manipulation, splitting, trimming, and character encoding operations | [string.md](./string.md) |

## Global Built-in Functions

The following functions are globally available in the root namespace without requiring explicit imports:

- `print(arg1, arg2, ...)`: Prints string representations of arguments separated by spaces, terminated by a newline.
- `write(arg1, arg2, ...)`: Prints string representations of arguments without a trailing newline.
- `Range(start, end)`: Generates an integer sequence `[start, end)` for use in `for` loops.
