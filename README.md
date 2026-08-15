*This project has been created as part of the 42 curriculum by sfurst.*

## Description

`get_next_line` returns one line at a time from a file descriptor. A returned
line includes its trailing newline when one exists. End of file and errors return
`NULL`.

The mandatory implementation keeps one buffered reader in a static variable.
The bonus implementation keeps one static array of readers so calls can be
interleaved across several file descriptors.

This version experiments with AVX2 SIMD. It searches 32 bytes for a newline at
once and copies complete 32-byte blocks at once. The rest of the program still
compiles for the normal target, without a global `-mavx2` flag.

## Instructions

The machine running this implementation must support AVX2. On Linux, check with:

```sh
grep -w avx2 /proc/cpuinfo
```

Build the mandatory version:

```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 \
  main.c get_next_line.c get_next_line_utils.c -o gnl
```

Run it with:

```sh
./gnl input.txt
```

The three submitted bonus files compile with the same flags:

```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 \
  -c get_next_line_bonus.c get_next_line_utils_bonus.c
```

`BUFFER_SIZE` may be changed or omitted. It defaults to 1 when omitted.

## Algorithm

The static `t_gnl` value is zero-initialized by C before its first use. It stores
the current read buffer, the unread position, the number of valid bytes, and the
line being assembled. Explicit `line_len` and `line_cap` fields mean the code
never has to call `strlen` to rediscover information it already knows.

Each call follows this loop:

1. Refill `read_buf` only when its previous contents have been consumed.
2. Search the unread bytes up to and including the first newline.
3. Grow the line allocation only when the new chunk does not fit.
4. Copy the chunk, terminate the line with `\0`, and advance `pos`.
5. Return immediately if the last consumed source byte was `\n`.

Bytes after a newline remain in `read_buf` for the next call. There is no
remainder allocation and no `memmove`. A line starts with at least
`max(BUFFER_SIZE + 1, 64)` bytes of capacity and doubles when necessary. This
turns repeated exact-size reallocations into geometric growth.

The bonus version uses the same algorithm. Its read buffer is allocated lazily
for each descriptor, avoiding a static `MAX_FD * BUFFER_SIZE` byte array.

## SIMD newline search, line by line

The declaration:

```c
static ssize_t chunk_len(t_gnl *gnl) __attribute__((target("avx2")));
```

asks GCC to compile only `chunk_len` with AVX2 enabled. This is why the normal
compile command does not need `-mavx2`. It does not perform runtime CPU dispatch,
so calling the function on a CPU without AVX2 can raise an illegal-instruction
fault.

Inside the function:

```c
__m256i nl;
```

declares one 256-bit vector. A vector of this size holds 32 eight-bit bytes.

```c
nl = _mm256_set1_epi8('\n');
```

fills all 32 lanes with the newline byte.

```c
gnl->scan_i = 0;
gnl->scan_n = gnl->read_len - gnl->pos;
```

starts at offset zero and records how many unread bytes are safe to inspect.
The scratch fields live in the struct to stay within the 42 Norm's local-variable
limit. They are reset before use.

```c
while (gnl->scan_n - gnl->scan_i >= 32)
```

enters the vector loop only when a complete 32-byte block remains. This prevents
an out-of-bounds vector load.

```c
gnl->scan_v = _mm256_loadu_si256((const __m256i *)(gnl->read_buf
            + gnl->pos + gnl->scan_i));
```

loads those 32 bytes. The `u` means unaligned, so `read_buf + pos + scan_i` does
not need a 32-byte-aligned address.

```c
gnl->scan_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(gnl->scan_v,
            nl));
```

compares each input byte with newline. Equal lanes become all-one bytes. The
movemask then collects their top bits into a 32-bit integer, where bit 0
represents the first byte and bit 31 represents the last.

```c
if (gnl->scan_mask)
    return (gnl->scan_i + __builtin_ctz(gnl->scan_mask) + 1);
```

a nonzero mask means at least one newline was found. `__builtin_ctz` counts the
zero bits below the first set bit, giving the first newline's lane index. It is
called only for a nonzero mask because `ctz(0)` is undefined. The extra one
includes the newline in the returned chunk length.

```c
gnl->scan_i += 32;
```

moves to the next vector when the block contains no newline. After the vector
loop, the scalar loop checks the final 0 to 31 bytes. These are the bytes that do
not fill a complete AVX2 register.

## SIMD copy, line by line

`copy_bytes` uses the same function-level `target("avx2")` attribute.

```c
while (len >= 32)
```

copies vectors only while a complete 32-byte block remains.

```c
_mm256_storeu_si256((__m256i *)dst,
    _mm256_loadu_si256((const __m256i *)src));
```

loads 32 unaligned source bytes and stores them at an unaligned destination.
The source and destination never overlap in this implementation.

```c
dst += 32;
src += 32;
len -= 32;
```

advances both pointers and reduces the remaining length. A scalar loop copies
the final 0 to 31 bytes. Explicit lengths also allow internal NUL bytes to be
copied, although the subject declares binary-file behavior undefined.

## Performance

The compared variants contain these optimizations:

| Variant | Newline search | Copy | Length | Allocation |
| ------- | -------------- | ---- | ------ | ---------- |
| Parent scalar | byte loop | byte loop | tracked | exact first size, then doubling |
| Repeated naive `strlen` | AVX2 + scalar tail | AVX2 + scalar tail | rescanned before every append | minimum 64 bytes, then doubling |
| Completed SIMD | AVX2 + scalar tail | AVX2 + scalar tail | tracked | minimum 64 bytes, then doubling |

Lower is better. These are end-to-end wall-clock measurements on an Intel
Core i5-11500H with GCC 16.2.1. They use the grader-style flags
`-Wall -Wextra -Werror` with no `-O` optimization flag. The input was one
256 KiB line ending in newline, already in the OS page cache, read three times
per measurement.

The "parent" column is revision `749afaeb`, before SIMD and the new initial
capacity. The "repeated strlen" column uses the completed implementation but
deliberately rescans the accumulated line with a simple byte-at-a-time `strlen`
before every append. It is a comparison variant, not submitted source.

| `BUFFER_SIZE` | Parent scalar | Repeated naive `strlen` | SIMD + tracked length |
| ------------: | ------------: | ----------------------: | --------------------: |
|            42 |       10.5 ms |               5264.0 ms |                7.6 ms |
|           128 |        7.3 ms |               1900.8 ms |                3.9 ms |
|          1024 |        6.3 ms |                240.5 ms |                2.8 ms |
|          4096 |        6.3 ms |                 61.2 ms |                2.6 ms |
|         65536 |        5.6 ms |                  6.8 ms |                2.5 ms |

The naive comparison inserted this operation before each append:

```c
line_len = 0;
while (line && line[line_len])
    line_len++;
```

For a long line split into many chunks, earlier bytes are scanned repeatedly.
That approaches quadratic work as the number of chunks grows. Tracking
`line_len` makes the work linear. For tiny lines or `BUFFER_SIZE < 32`, syscall
and allocation costs dominate and SIMD should not be expected to help much.
These numbers describe this machine and workload, not a portable guarantee.

## Source overview

- `get_next_line.c`: mandatory read loop and AVX2 newline search
- `get_next_line_utils.c`: allocation growth and AVX2 copy
- `get_next_line_bonus.c`: multi-descriptor read loop and the same search
- `get_next_line_utils_bonus.c`: bonus allocation growth and copy
- `main.c`: local manual test program, ignored by the submission repository

## Resources

- The 42 Get Next Line subject, version 14.3, defines the required API, allowed
  functions, README content, and bonus behavior.
- [Linux `read(2)` manual](https://man7.org/linux/man-pages/man2/read.2.html)
  documents short reads, EOF, errors, and file-descriptor behavior.
- [GCC x86 function attributes](https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html)
  documents per-function `target("avx2")` compilation.
- [GCC bit-operation builtins](https://gcc.gnu.org/onlinedocs/gcc/Bit-Operation-Builtins.html)
  documents `__builtin_ctz` and its undefined zero-input case.
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
  documents the AVX2 load, compare, movemask, and store intrinsics.
- [C static storage duration](https://en.cppreference.com/w/c/language/static_storage_duration.html)
  explains the lifetime and initialization of the static reader state.
- [Dynamic arrays](https://en.wikipedia.org/wiki/Dynamic_array) explains capacity
  and geometric growth.
- [winstonallo/libft](https://github.com/winstonallo/libft) inspired the SIMD
  experiment, especially its `ft_strlen` implementation and its excellent
  `SIMD go brrr` comment. This project adapts the general compare-and-movemask
  idea to bounded newline search rather than copying that function.

AI was used to review the existing optimization, identify the missed SIMD copy
tail, mirror the changes into the bonus files, design boundary and memory tests,
create the temporary naive-`strlen` comparison, run the benchmark, and help draft
this explanation. The implementation and benchmark results were checked locally
with the real compiler and CPU rather than accepted from generated estimates.
