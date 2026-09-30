# Standard Library: `math`

The `math` module provides standard mathematical, trigonometric, exponential, rounding, and pseudo-random utilities.

```nva
import stdlib math as m
```

## Function Reference

### Elementary & Exponential Functions
- `m.Abs(x)`: Absolute value of `x`.
- `m.Sqrt(x)`: Square root of non-negative number `x`.
- `m.Pow(base, exponent)`: Computes `base` raised to the power of `exponent`.
- `m.Exp(x)`: Natural exponential function `e^x`.
- `m.Log(x)`: Natural logarithm (base `e`) of `x`.
- `m.Log10(x)`: Common logarithm (base 10) of `x`.

### Rounding & Truncation
- `m.Floor(x)`: Largest integer value less than or equal to `x`.
- `m.Ceil(x)`: Smallest integer value greater than or equal to `x`.
- `m.Round(x)`: Nearest integer to `x` using round-half-away-from-zero rules.

### Trigonometric Functions
- `m.Sin(radians)`: Sine of angle in radians.
- `m.Cos(radians)`: Cosine of angle in radians.
- `m.Tan(radians)`: Tangent of angle in radians.
- `m.Asin(x)`: Arc sine of `x` in radians.
- `m.Acos(x)`: Arc cosine of `x` in radians.
- `m.Atan(x)`: Arc tangent of `x` in radians.
- `m.Atan2(y, x)`: Two-argument arc tangent computing angle of vector `(x, y)`.

### Extremum & Randomization
- `m.Min(a, b)`: Returns smaller value between `a` and `b`.
- `m.Max(a, b)`: Returns larger value between `a` and `b`.
- `m.Random()`: Generates uniform floating-point random number in range `[0.0, 1.0)`.
- `m.RandomInt(min, max)`: Generates uniform integer in inclusive range `[min, max]`.
