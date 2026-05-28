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

The static `t_gnl` state is a small buffered reader. It keeps the unread position
inside the current `BUFFER_SIZE` chunk, plus the line currently being built.

Each call consumes only as much of the read chunk as the next line needs. This
means bytes after a newline stay in place for the following call: there is no
remainder allocation and no `memmove`. The line allocation grows geometrically
only when a line spans multiple chunks, then ownership of that allocation is
passed directly to the caller.

The bonus version uses one static array of reader states. Its read buffers are
allocated only for file descriptors that are actually used, avoiding a
`MAX_FD * BUFFER_SIZE` static array.

### Why this approach

* No repeated `strjoin` calls or shifting of unread bytes
* Usually one required allocation per returned line
* No reads past the chunk that completes the current line

This pattern is commonly used in systems programming and is similar to how dynamic arrays (e.g., vectors) are implemented.

The idea was inspired in part by optimization practices such as Go’s preallocation recommendations (e.g., avoiding repeated slice reallocations by reserving capacity up front or growing geometrically).

## Notes

This implementation keeps unread bytes in its read chunk between calls and
returns lines including their trailing newline, as required by the subject.

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
