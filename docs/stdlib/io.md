# Standard Library: `io`

The `io` module provides system input and output operations across standard terminal streams and filesystem descriptors.

```nva
import stdlib io as io
```

## Function Reference

- `io.Print(arg1, arg2, ...)`: Formats and writes values to standard output separated by single spaces, followed by a trailing newline. Flushes buffer automatically.
- `io.Write(arg1, arg2, ...)`: Writes values to standard output without appending a newline.
- `io.Input(prompt)`: Displays prompt string and blocks waiting for user text entry on standard input. Returns line entered as a string.

### Terminal Interaction Example

```nva
import stdlib io as io

io.Write("Enter your name: ")
name = io.Input("")
io.Print("Welcome to Nevaarize,", name)
```
