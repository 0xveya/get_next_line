*This project has been created as part of the 42 curriculum by sfurst.*

## Description

`get_next_line` implements a function that returns one line at a time from a
file descriptor. Each call to `get_next_line()` returns the next available line,
including the trailing newline character (`\n`) when present, and returns `NULL`
at end of file or on error.

The bonus version extends the same behavior to multiple file descriptors at the
same time by keeping separate state for each descriptor.

## Instructions

### Build

Mandatory version:
```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 main.c get_next_line.c get_next_line_utils.c -o gnl
````

Bonus version:

```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 main_bonus.c get_next_line_bonus.c get_next_line_utils_bonus.c -o gnl_bonus
```

### Run

Mandatory:

```sh
./gnl input.txt
```

Bonus (multiple files):

```sh
./gnl_bonus file1.txt file2.txt
```

### Manual Evaluation Helper

Both `get_next_line.c` and `get_next_line_bonus.c` include a commented eval
`main` at the end of the file. Uncomment the block you want to show during
manual evaluation, then comment it again after.

## Source Overview

* `get_next_line.c`: mandatory `get_next_line()` flow
* `get_next_line_utils.c`: memory helpers and dynamic buffer growth
* `get_next_line_bonus.c`: bonus version with one static state per fd
* `get_next_line_utils_bonus.c`: helper functions for the bonus version
* `main.c`: small standalone test program for the mandatory version
* `main_bonus.c`: small standalone test program for the bonus version

## Implementation Strategy

Instead of using repeated `strjoin` operations to grow the internal buffer, this implementation uses a preallocation and exponential growth strategy.

The core idea is to maintain a dynamic buffer with:

* a pointer to allocated memory (`data`)
* current length (`len`)
* total capacity (`cap`)

When new data is read:

* If there is enough capacity, it is appended directly using `memcpy`
* If not, the buffer is resized by growing its capacity (typically ×2) until it can fit the new data

This strategy is used to efficiently accumulate data read from the file descriptor
until a newline is found, minimizing reallocations and copies compared to naive
string concatenation approaches.

### Why this approach

* Better performance: avoids repeatedly copying the entire buffer every time data is added
* Fewer allocations: the buffer size is increased by doubling it when needed instead of growing little by little
* More control: explicit tracking of buffer size and usage

This pattern is commonly used in systems programming and is similar to how dynamic arrays (e.g., vectors) are implemented.

The idea was inspired in part by optimization practices such as Go’s preallocation recommendations (e.g., avoiding repeated slice reallocations by reserving capacity up front or growing geometrically).

## Notes

This implementation stores unread remainder data between calls, keeps reading
until a newline or EOF is found, extracts one line, and then shifts the leftover
bytes for the next call.

## Resources

* Go optimization discussion (preallocation and capacity growth patterns):
  [https://go.dev/wiki/Performance#memory-profiler](https://go.dev/wiki/Performance#memory-profiler)
* Dynamic arrays wikipedia page:
  [https://en.wikipedia.org/wiki/Dynamic_array](https://en.wikipedia.org/wiki/Dynamic_array)
* `read(2)` manual:
  [https://man7.org/linux/man-pages/man2/read.2.html](https://man7.org/linux/man-pages/man2/read.2.html)
* File descriptor explanation:
  [https://man7.org/linux/man-pages/man2/open.2.html](https://man7.org/linux/man-pages/man2/open.2.html)
* 42 subject (Get Next Line):
  Included in project materials

AI was used to help draft this README.
