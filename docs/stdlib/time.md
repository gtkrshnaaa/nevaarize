# Standard Library: `time`

The `time` module provides high-resolution clock timing, execution profiling, duration conversion, and thread sleeping primitives.

```nva
import stdlib time as t
```

## Function Reference

- `t.nanos()`: Returns monotonic timestamp in nanoseconds since steady epoch. Recommended for high-precision benchmarking and performance profiling.
- `t.millis()`: Returns current Unix epoch time in milliseconds.
- `t.clock()`: Returns high-resolution CPU process execution time in nanoseconds.
- `t.sleep(milliseconds)`: Suspends execution of current thread for specified duration.
- `t.format(timestampMs, formatString)`: Formats epoch timestamp into date-time string according to standard POSIX strftime conventions.

### Benchmarking Example

```nva
import stdlib time as t

start = t.nanos()

total = 0
for (i in Range(0, 1000000)) {
    total = total + i
}

elapsedNs = t.nanos() - start
elapsedMs = elapsedNs / 1000000.0
print("Completed in:", elapsedMs, "ms")
```
