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

## What SIMD means

SIMD means Single Instruction, Multiple Data. A scalar byte loop handles one
byte per iteration. An AVX2 instruction operates on a 256-bit register, which
can hold 32 independent bytes. Here, one vector comparison asks the same
question of all 32 bytes: "is this byte a newline?"

The copy loop gets a similar benefit. A scalar loop needs a load, a store,
pointer updates, a length update, and a branch for every byte. The AVX2 loop
loads and stores 32 bytes before doing its pointer updates and branch. Memory
bandwidth, cache state, and small scalar tails still matter, so this is not a
guaranteed 32-times speedup. It simply removes much of the per-byte loop work.

This distinction matters for the evaluator compile command. With no `-O` flag,
the compiler is not being asked to discover and auto-vectorize a scalar loop.
The intrinsics explicitly request vector operations, so the AVX2 instructions
are still emitted.

## Why capacity growth is faster than `strjoin`

Suppose a long line arrives in `n` chunks. An exact-size `strjoin` approach
allocates a new string for every chunk, measures the old string, copies the old
contents again, copies the new chunk, and frees the previous string. Early bytes
are recopied for every later chunk. The approximate historical copy work is:

```text
BUFFER_SIZE * (1 + 2 + 3 + ... + n)
```

That triangular sum grows quadratically with the number of chunks. It also
causes one allocation and free cycle per chunk.

This implementation keeps `line_len` and a separate `line_cap`. Appending does
not scan old bytes. When capacity is exhausted, it doubles, so reallocations
happen around 64, 128, 256, 512 bytes, and so on instead of after every read.
Each chunk is copied into the line once, and existing bytes move only during the
much less frequent growth steps. The total work stays linear in the line length.

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

The labels below describe long-line performance only. They are not judgments
about whether an implementation is readable, correct, or appropriate for a
learning project.

| Long-line rating | Version | What happens for every chunk |
| ---------------- | ------- | ---------------------------- |
| Bad | Friend's `ft_strjoin` version | Measure both strings, allocate their exact combined size, copy both, then free the old stash |
| Bad | Synthetic repeated `strlen` | Rescan the whole accumulated line, then append into a geometrically grown buffer |
| Good | Parent scalar version | Track length, grow geometrically, scan and copy one byte at a time |
| Better | Completed SIMD version | Track length, grow geometrically, scan and copy 32 bytes at a time |

The friend comparison is the real mandatory implementation from
[`Ketaminepunch/getnextline`](https://github.com/Ketaminepunch/getnextline) at
commit `62ece7f`. Its direct `ft_strjoin` design is simple and conventional, but
repeated exact-size allocation and copying become expensive for one long line.

Lower is better. These are end-to-end wall-clock measurements on an Intel
Core i5-11500H with GCC 16.2.1. They use the grader-style flags
`-Wall -Wextra -Werror` with no `-O` optimization flag. The input was one
256 KiB line ending in newline, already in the OS page cache, read three times
per measurement.

The parent version is revision `749afaeb`, before SIMD and the new initial
capacity. The repeated-`strlen` version is synthetic: it uses the completed
implementation but deliberately rescans the accumulated line before every
append. It is not submitted source.

| `BUFFER_SIZE` | `ft_strjoin` | Repeated `strlen` | Tracked scalar | Tracked SIMD |
| ------------: | -----------: | ----------------: | -------------: | -----------: |
|            42 |    8946.6 ms |         5264.0 ms |        10.5 ms |       7.6 ms |
|           128 |    2882.7 ms |         1900.8 ms |         7.3 ms |       3.9 ms |
|          1024 |     382.6 ms |          240.5 ms |         6.3 ms |       2.8 ms |
|          4096 |     103.8 ms |           61.2 ms |         6.3 ms |       2.6 ms |
|         65536 |      18.6 ms |            6.8 ms |         5.6 ms |       2.5 ms |

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

### Generate evaluator inputs

Raw `/dev/urandom` can contain both `\n` and `\0`, so it does not reliably make
one long text line. Encoding it with `base64 -w 0` produces printable bytes
without inserted newlines. These commands create useful stress cases outside
the submitted source files.

One 50 MiB line with a final newline:

```sh
head -c 50M /dev/urandom | base64 -w 0 | head -c 50M > /tmp/gnl-one-50m.txt
printf '\n' >> /tmp/gnl-one-50m.txt
```

One 50 MiB line without a final newline:

```sh
head -c 50M /dev/urandom | base64 -w 0 | head -c 50M > /tmp/gnl-no-final-nl.txt
```

Twenty separate 1 MiB lines:

```sh
for i in $(seq 1 20); do
  head -c 1M /dev/urandom | base64 -w 0 | head -c 1M
  printf '\n'
done > /tmp/gnl-many-long-lines.txt
```

Boundary lines around one 32-byte AVX2 block:

```sh
for n in 31 32 33 63 64 65; do
  head -c "$n" /dev/zero | tr '\0' x
  printf '\n'
done > /tmp/gnl-avx-boundaries.txt
```

Build and time several buffer sizes and long-line shapes with the subject's
flags:

```sh
for size in 42 128 1024 4096 65536; do
  cc -Wall -Wextra -Werror -D BUFFER_SIZE="$size" \
    main.c get_next_line.c get_next_line_utils.c -o "/tmp/gnl-$size"
  for input in /tmp/gnl-one-50m.txt /tmp/gnl-no-final-nl.txt \
    /tmp/gnl-many-long-lines.txt; do
    /usr/bin/time -f "BUFFER_SIZE=$size  %e s  %M KiB  $input" \
      "/tmp/gnl-$size" "$input" > /dev/null
  done
done
```

`BUFFER_SIZE=1` causes one `read` syscall per byte, so using it on 50 MiB mostly
benchmarks millions of syscalls and wastes evaluator time. Test it on the small
boundary file instead:

```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=1 \
  main.c get_next_line.c get_next_line_utils.c -o /tmp/gnl-1
/usr/bin/time -f 'BUFFER_SIZE=1  %e s  %M KiB' \
  /tmp/gnl-1 /tmp/gnl-avx-boundaries.txt > /dev/null
```

Run each command more than once. The first run may include filesystem I/O while
later runs may read from the OS page cache. Redirecting output to `/dev/null`
keeps terminal rendering out of the measurement, although the local `main.c`
still calls `printf` for every returned line. For careful comparisons, use the
same compiler, flags, input, machine, power mode, and number of repetitions.

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
- [Ketaminepunch/getnextline](https://github.com/Ketaminepunch/getnextline), by
  a friend, provides the real `ft_strjoin` implementation used in the benchmark
  comparison.

AI was used to review the existing optimization, identify the missed SIMD copy
tail, mirror the changes into the bonus files, design boundary and memory tests,
create the temporary naive-`strlen` comparison, benchmark the friend's
`ft_strjoin` version, and help draft this explanation. The implementation and
benchmark results were checked locally with the real compiler and CPU rather
than accepted from generated estimates.
