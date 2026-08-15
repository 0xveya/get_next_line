*This project has been created as part of the 42 curriculum by sfurst.*

## Description

`get_next_line` reads one line at a time from a file descriptor. A returned line
includes its terminating newline when one exists. At end of file, or when an
error occurs, the function returns `NULL`.

The main challenge is that `read()` does not understand lines. It returns at
most `BUFFER_SIZE` bytes, so one call may contain part of a line, several lines,
or the end of one line and the beginning of another. The implementation keeps
the unread bytes between calls and grows the current line only when necessary.

The mandatory version stores one reader in a static variable. The bonus version
stores one reader per file descriptor, allowing reads from several descriptors
to be interleaved.

This version also experiments with AVX2 SIMD. Newline search and bulk copying
process 32 bytes at a time, with a scalar loop handling the remaining bytes.

## Instructions

Compile the mandatory files with a test program:

```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 \
  main.c get_next_line.c get_next_line_utils.c -o gnl
```

Compile the bonus files:

```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 \
  main.c get_next_line_bonus.c get_next_line_utils_bonus.c -o gnl_bonus
```

`BUFFER_SIZE` defaults to `1` when it is not defined. It controls how many bytes
each call to `read()` requests, not the maximum line length.

The delimiter defaults to newline and can be overridden at compile time:

```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 -D "GNL_DELIMITER='a'" \
  main.c get_next_line.c get_next_line_utils.c -o gnl
```

This implementation requires an x86 CPU with AVX2 support. On Linux it can be
checked with:

```sh
grep -w avx2 /proc/cpuinfo
```

## Algorithm And Data Structure

Each reader is represented by a `t_gnl` structure. It keeps two distinct pieces
of state:

- `read_buf`, `pos`, and `read_len` describe the current input buffer and which
  bytes have already been consumed.
- `line`, `line_len`, and `line_cap` describe the line being assembled and the
  size of its allocation.

Each call to `get_next_line()` follows the same loop:

1. Refill the input buffer only after its previous contents are consumed.
2. Find the first delimiter in the unread portion of the buffer.
3. Append that chunk to the current line, growing its allocation if required.
4. Advance `pos`, leaving bytes after the delimiter for the next call.
5. Return the line when a delimiter is consumed.
6. At EOF, return the unfinished final line, or `NULL` when no bytes remain.

Keeping `pos` avoids moving leftovers with `memmove()` or allocating a separate
remainder string. Keeping `line_len` avoids repeatedly scanning the accumulated
line with `strlen()`.

### Growing the line buffer

The line begins with at least 64 bytes of capacity and doubles whenever it runs
out of space. A long line therefore grows through capacities such as 64, 128,
256, and 512 bytes instead of allocating an exact-size string after every read.

An exact-size `strjoin` approach recopies all earlier bytes for every new chunk,
which can approach quadratic work for a long line. Geometric growth makes the
number of reallocations small and keeps the total copying proportional to the
line length.

### SIMD newline search and copy

SIMD means Single Instruction, Multiple Data. AVX2 is the x86 instruction set
used here; its 256-bit registers hold 32 bytes. In `chunk_len()`, the delimiter
is broadcast to all 32 lanes and compared with a 32-byte block from the read
buffer. A bit mask records matching lanes, and `__builtin_ctz()` locates the
first match. The final zero to 31 bytes are checked by a normal scalar loop.

`copy_bytes()` uses the same block size to load and store 32 bytes per loop
iteration before copying its scalar tail. The source and destination do not
overlap, so no `memmove()` behavior is required.

Both functions use `__attribute__((target("avx2")))`. This lets GCC and Clang
emit AVX2 instructions for those functions without adding `-mavx2` to the whole
program. It does not check the CPU at runtime or provide a fallback. Running the
program on a CPU without AVX2 may cause an illegal-instruction fault.

SIMD does not make the complete function 32 times faster. Calls to `read()`,
allocation, cache behavior, and scalar tails still contribute to the runtime.
For small buffers or short lines, those costs may dominate. The main benefit is
removing much of the per-byte loop work for large chunks.

### Mandatory and bonus state

The mandatory implementation has one static `t_gnl`, so it tracks one stream at
a time. The bonus implementation has a static array indexed by file descriptor.
Its read buffers are allocated lazily, avoiding a fixed
`MAX_FD * BUFFER_SIZE` byte array. On EOF or error, the state and owned memory
for that descriptor are cleared.

## Source Overview

- `get_next_line.c`: mandatory read loop and delimiter search.
- `get_next_line_utils.c`: line growth, copying, and cleanup.
- `get_next_line_bonus.c`: multi-descriptor read loop and delimiter search.
- `get_next_line_utils_bonus.c`: bonus line growth, copying, and cleanup.
- `get_next_line.h` and `get_next_line_bonus.h`: configuration and reader state.

## Resources

- the 42 Get Next Line subject
- [`read(2)`](https://man7.org/linux/man-pages/man2/read.2.html)
- [GCC x86 function attributes](https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html)
- [Clang `target` attribute](https://clang.llvm.org/docs/AttributeReference.html#target)
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
- [Dynamic arrays](https://en.wikipedia.org/wiki/Dynamic_array)

AI was used to help review and structure this README. The described behavior was
checked against the submitted implementation.
