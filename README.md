_This project has been created as part of the 42 curriculum by sfurst._

## Description

`get_next_line` returns one line at a time from a file descriptor. A returned
line includes its trailing newline when one exists. End of file and errors return
`NULL`.

The mandatory implementation keeps one buffered reader in a static variable.
The bonus implementation keeps one static array of readers so calls can be
interleaved across several file descriptors.

This version experiments with AVX2 SIMD and a direct Linux x86-64 `read` syscall.
It searches 32 bytes for a delimiter at once, copies large chunks with an unrolled
AVX2 loop, and enters the kernel directly from `refill()` instead of calling the
libc `read()` wrapper. Function attributes enable these optimizations only where
they are requested, so the normal compile command still does not need a global
`-mavx2` flag.

The direct syscall makes this implementation intentionally platform-specific: the
inline assembly shown below is for the Linux x86-64 syscall ABI.

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

The line terminator is also named in one place:

```c
#ifndef GNL_DELIMITER
# define GNL_DELIMITER '\n'
#endif
```

The subject behavior remains unchanged because the default is newline. For the
common evaluation exercise of returning through another character, either edit
that macro or override it while compiling. For example, this makes `a` terminate
a returned segment:

```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 -D "GNL_DELIMITER='a'" \
  main.c get_next_line.c get_next_line_utils.c -o /tmp/gnl-delimiter-a
```

The delimiter is used by both the AVX2 broadcast and scalar tail, and the
terminating delimiter remains included in the returned string.

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

AVX2 means Advanced Vector Extensions 2. It is a hardware instruction-set
extension implemented by many x86-64 CPUs, not a C library or an operating
system feature. It gives the CPU 256-bit `YMM` vector registers and instructions
for operations such as loading, storing, and comparing packed integer bytes.
The C intrinsics in `<immintrin.h>` are compiler-provided names that map onto
those CPU instructions.

SIMD is the general idea; AVX2 is the particular CPU technology used by this
implementation. Other architectures provide different SIMD instruction sets,
such as NEON on ARM. A compiler accepting the intrinsics only proves it can
create the instructions. The CPU running the program must also support AVX2.
Because this project has no runtime dispatch or scalar fallback, an unsupported
CPU may stop with an illegal-instruction fault.

The copy loop gets a similar benefit. A scalar loop needs a load, a store,
pointer updates, a length update, and a branch for every byte. The AVX2 loop
loads and stores 32 bytes before doing its pointer updates and branch. Memory
bandwidth, cache state, and small scalar tails still matter, so this is not a
guaranteed 32-times speedup. It simply removes much of the per-byte loop work.

This distinction matters for the evaluator compile command. With no `-O` flag,
the compiler is not being asked to discover and auto-vectorize a scalar loop.
The intrinsics explicitly request vector operations, so the AVX2 instructions
are still emitted.

## Compiler attributes used

Several GNU-style function attributes tell GCC or Clang how individual hot-path
functions should be compiled. They affect compiler code generation; they do not
change the C API of `get_next_line`.

```c
static ssize_t chunk_len(t_gnl *gnl)
    __attribute__((target("avx2"), hot));

static inline int refill(int fd, t_gnl *gnl)
    __attribute__((always_inline, hot));

static inline char *finish_line(t_gnl *gnl)
    __attribute__((always_inline));
```

`target("avx2")` enables AVX2 instructions for that function only. `chunk_len`
and `copy_bytes` therefore may use `_mm256_*` intrinsics even though the complete
program is compiled without `-mavx2`. This is a compile-time promise, not a CPU
feature check: executing those functions on a CPU without AVX2 can still fault.

`hot` tells the compiler that a function is expected to be on an important,
frequently executed path. GCC may optimize a hot function more aggressively and
may place hot functions together to improve instruction-cache locality. It is a
hint to the optimizer, not a correctness requirement and not a guarantee of a
particular instruction sequence.

`always_inline` strengthens the request made by the `inline` keyword. It is used
for `refill` and `finish_line` because both are tiny helpers in the central read
loop. Inlining removes an ordinary C function call boundary when the compiler can
honor the attribute. The attribute should still be viewed as a code-generation
choice rather than part of the algorithm's correctness.

The code also uses `__builtin_expect(expression, expected)` around branches. That
is a branch-probability hint rather than a function attribute. The second argument
says which result the programmer expects most often: `1` means the expression is
usually true and `0` means it is usually false. For example, `ret > 0` is marked
likely because normal reads usually return data, while allocation failure and a
nonzero newline mask are marked unlikely. The compiler can use this information
to arrange the likely path as the straight-through path and move uncommon paths
out of the way, reducing unnecessary jumps on the hot path and potentially
improving instruction-cache behavior. It does not force the CPU's branch predictor
to make a particular prediction, and it never changes the value or correctness of
the condition.

These hints matter most here because `get_next_line` contains a small, repeatedly
executed loop. The intended fast path is: buffered data exists (or `refill`
succeeds), `append_chunk` succeeds, and scanning continues without finding the
delimiter until the final chunk. Error handling, allocation failure, and delimiter
hits are deliberately treated as side paths. Modern CPUs already predict branches
dynamically, so `__builtin_expect` should be understood as a code-layout/compiler
hint rather than a guaranteed speedup.

## Direct Linux x86-64 `read` syscall

`refill` now invokes the kernel directly with GNU extended inline assembly:

```c
static inline int refill(int fd, t_gnl *gnl)
{
    long ret;

    __asm__("syscall" : "=a"(ret) : "a"(0L), "D"((long)fd),
        "S"(gnl->read_buf), "d"((size_t)BUFFER_SIZE)
        : "rcx", "r11", "memory");
    gnl->pos = 0;
    gnl->read_len = ret;
    return (__builtin_expect(ret > 0, 1));
}
```

On the Linux x86-64 syscall ABI, the syscall number is passed in `RAX`; arguments
1 through 6 use `RDI`, `RSI`, `RDX`, `R10`, `R8`, and `R9`; and the return value
comes back in `RAX`. `read` needs only three arguments, so this function uses:

| Inline-asm operand | Register | Meaning for `read` |
| ------------------ | -------- | ------------------ |
| `"a"(0L)` | `RAX` | syscall number `0`, which is `read` on Linux x86-64 |
| `"D"((long)fd)` | `RDI` | argument 1: file descriptor |
| `"S"(gnl->read_buf)` | `RSI` | argument 2: destination buffer |
| `"d"((size_t)BUFFER_SIZE)` | `RDX` | argument 3: maximum byte count |
| `"=a"(ret)` | `RAX` | return value after the syscall |

The constraint letters are GCC's x86 register constraints: `a` selects the
accumulator register, while `D`, `S`, and `d` select the registers required here
for the first three syscall arguments. The same `RAX` register is an input before
`syscall` and an output afterwards.

The clobber list is equally important:

```c
: "rcx", "r11", "memory"
```

The x86-64 `syscall` instruction itself overwrites `RCX` and `R11`, so the compiler
must be told that their previous values do not survive the assembly statement.
The `"memory"` clobber tells the compiler that memory can be changed by the
operation. That matters because the kernel writes bytes into `gnl->read_buf`; the
compiler must not move surrounding memory accesses across the assembly as if the
buffer were untouched. The used `ret` output makes the assembly observable, so
the compiler cannot delete it as dead computation.

A successful `read` returns a positive byte count. EOF returns zero. A raw Linux
syscall reports failure as a negative error value in `RAX`; unlike the libc
`read()` wrapper, this inline syscall does not translate that value to `-1` and
set `errno`. `get_next_line` only needs to distinguish positive data from EOF or
failure, so `refill` returns true only when `ret > 0`, and the outer function later
clears its state and returns `NULL` when `read_len < 0`.

### Why bypass libc here

The direct syscall removes the userspace `read()` wrapper from this hot path. In
particular, this implementation does not need libc to inspect a kernel error,
convert Linux's negative error return into `-1`, and publish the error number
through `errno`: `get_next_line` never uses `errno` and only cares whether the raw
return value is positive, zero, or negative. That makes the path from this helper
to the kernel deliberately minimal.

This can remove a small amount of wrapper/error-handling overhead, so the direct
syscall can be faster at the call boundary. It should not be described as a large
or guaranteed speedup: the kernel still performs exactly the expensive part of
`read`, and for normal successful reads the libc wrapper is already very thin.
For this project the more important performance wins are avoiding repeated
allocation/copying, tracking lengths directly, and processing bytes in AVX2-sized
blocks. The raw syscall is a smaller hot-path optimization on top of those changes.

This is deliberately less portable than calling `read(fd, buf, count)`. Syscall
numbers, registers, instructions, and error conventions vary by architecture and
ABI. The assembly above should therefore be read specifically as Linux x86-64
code, not as a generic C implementation.

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
static ssize_t chunk_len(t_gnl *gnl) __attribute__((target("avx2"), hot));
```

asks GCC or Clang to compile only `chunk_len` with AVX2 enabled. This is why the
normal compile command does not need `-mavx2`. The same source was tested with
GCC 16.2.1 and Clang 22.1.8 using `-Wall -Wextra -Werror`. The attribute does not
perform runtime CPU dispatch, so calling the function on a CPU without AVX2 can
raise an illegal-instruction fault.

Inside the function:

```c
__m256i nl;
```

declares one 256-bit vector. A vector of this size holds 32 eight-bit bytes.

```c
nl = _mm256_set1_epi8(GNL_DELIMITER);
```

fills all 32 lanes with the configured delimiter byte. `_epi8` describes 32
packed eight-bit integer lanes. With the default configuration, every lane
contains newline.

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
not need a 32-byte-aligned address. The address expression selects the first
unscanned byte, and the cast presents that byte pointer to the intrinsic as a
pointer to one read-only 256-bit vector.

```c
gnl->scan_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(gnl->scan_v,
            nl));
```

`_mm256_cmpeq_epi8` performs 32 independent byte comparisons. A matching lane
becomes `0xff`, while a different lane becomes `0x00`. The inner comparison
finishes before `_mm256_movemask_epi8` takes the high bit of every result lane
and packs those bits into a 32-bit integer. Bit 0 represents the first loaded
byte and bit 31 represents the last, so each set bit marks a delimiter.

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
loop, the scalar tail checks the final zero to 31 bytes:

```c
while (gnl->scan_i < gnl->scan_n)
{
    if (gnl->read_buf[gnl->pos + gnl->scan_i++] == GNL_DELIMITER)
        return (gnl->scan_i);
}
return (gnl->scan_n);
```

The post-increment advances past every checked byte. When it finds the
delimiter, the returned length therefore includes it. If no delimiter exists,
the function returns the complete unread length.

## SIMD copy, line by line

`copy_bytes` uses the same function-level `target("avx2")` attribute:

```c
static void copy_bytes(char *dst, const char *src,
    ssize_t len) __attribute__((target("avx2"), hot));
```

The attribute affects code generation for this function only and does not alter
the function's arguments or return type.

```c
while (len >= 128)
```

handles four AVX2 vectors per loop iteration. The body performs four independent
32-byte loads and stores at offsets 0, 32, 64, and 96, then advances the pointers
by 128 bytes. This manual unrolling reduces loop-counter updates and branches for
large copies.

```c
_mm256_storeu_si256((__m256i *)dst,
    _mm256_loadu_si256((const __m256i *)src));
_mm256_storeu_si256((__m256i *)(dst + 32),
    _mm256_loadu_si256((const __m256i *)(src + 32)));
_mm256_storeu_si256((__m256i *)(dst + 64),
    _mm256_loadu_si256((const __m256i *)(src + 64)));
_mm256_storeu_si256((__m256i *)(dst + 96),
    _mm256_loadu_si256((const __m256i *)(src + 96)));
```

Each load/store pair moves 32 unaligned bytes, for 128 bytes total. The casts
present the byte pointers as vector pointers, while the `u` suffix means neither
address needs 32-byte alignment. The source and destination never overlap in this
implementation, so `memmove` behavior is unnecessary.

After the 128-byte loop, a second loop handles any remaining complete vectors:

```c
while (len >= 32)
{
    _mm256_storeu_si256((__m256i *)dst,
        _mm256_loadu_si256((const __m256i *)src));
    dst += 32;
    src += 32;
    len -= 32;
}
```

The scalar tail then handles the final zero to 31 bytes:

```c
while (len-- > 0)
    *dst++ = *src++;
```

It copies the final zero to 31 bytes one at a time. The postfix increments move
both pointers after each assignment. Explicit lengths also allow internal NUL
bytes to be copied, although the subject declares binary-file behavior
undefined.

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
256 KiB line ending in newline, already in the OS page cache. The comparison
columns used three reads per measurement. The current Tracked SIMD column is
the average of ten batches of 1,000 complete program executions.

The parent version is revision `749afaeb`, before SIMD and the new initial
capacity. The repeated-`strlen` version is synthetic: it uses the completed
implementation but deliberately rescans the accumulated line before every
append. It is not submitted source.

| `BUFFER_SIZE` | `ft_strjoin` | Repeated `strlen` | Tracked scalar | Tracked SIMD |
| ------------: | -----------: | ----------------: | -------------: | -----------: |
|            42 |    8946.6 ms |         5264.0 ms |        10.5 ms |      4.703 ms |
|           128 |    2882.7 ms |         1900.8 ms |         7.3 ms |      1.780 ms |
|          1024 |     382.6 ms |          240.5 ms |         6.3 ms |      1.136 ms |
|          4096 |     103.8 ms |           61.2 ms |         6.3 ms |      1.106 ms |
|         65536 |      18.6 ms |            6.8 ms |         5.6 ms |      1.098 ms |

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

### Allocation-failure and crash checks

`funcheck` needs the compiled program before its flags are useful. The complete
command for this project is:

```sh
cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 \
  main.c get_next_line.c get_next_line_utils.c -o /tmp/gnl-funcheck
funcheck -abc /tmp/gnl-funcheck /tmp/gnl-many-long-lines.txt
```

Here, `-a` tracks allocations, `-b` keeps complete backtraces, and `-c` treats
`abort()` as a crash. Running only `funcheck -abc` is incomplete and reports
"No program specified." On the current implementation, funcheck 1.1.5 detected
and tested nine functions, with nine passing.

Use Valgrind separately because function-failure injection is not a substitute
for checking ownership and invalid memory access:

```sh
valgrind --leak-check=full --show-leak-kinds=all \
  --errors-for-leak-kinds=all --error-exitcode=99 \
  /tmp/gnl-funcheck /tmp/gnl-many-long-lines.txt > /dev/null
```

## Source overview

- `get_next_line.c`: mandatory read loop, inline Linux x86-64 `read` syscall, and AVX2 newline search
- `get_next_line_utils.c`: allocation growth and AVX2 copy
- `get_next_line_bonus.c`: multi-descriptor read loop and the same search
- `get_next_line_utils_bonus.c`: bonus allocation growth and copy
- `main.c`: local manual test program, ignored by the submission repository

## Resources

- The 42 Get Next Line subject, version 14.3, defines the required API, allowed
  functions, README content, and bonus behavior.
- [Linux `read(2)` manual](https://man7.org/linux/man-pages/man2/read.2.html)
  documents short reads, EOF, errors, and file-descriptor behavior.
- [Linux `syscall(2)` manual](https://man7.org/linux/man-pages/man2/syscall.2.html)
  explains direct system-call invocation and architecture-specific calling
  conventions.
- [Filippo Valsorda's searchable Linux syscall table](https://filippo.io/linux-syscall-table/)
  is a compact x86-64 reference for syscall numbers, argument names, and the
  register convention; it is especially useful for checking which register
  carries each argument when writing inline syscall assembly.
- [Chromium OS Linux syscall table](https://www.chromium.org/chromium-os/developer-library/reference/linux-constants/syscalls/)
  gives a cross-architecture register cheat sheet, including x86-64 `RAX` for
  the syscall number/return value and `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9` for
  arguments.
- [GCC x86 function attributes](https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html)
  documents per-function `target("avx2")` compilation.
- [GCC common function attributes](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html)
  documents `hot` and `always_inline`.
- [Clang attribute reference](https://clang.llvm.org/docs/AttributeReference.html#target)
  documents Clang's compatible GNU-style `target` function attribute.
- [GCC bit-operation builtins](https://gcc.gnu.org/onlinedocs/gcc/Bit-Operation-Builtins.html)
  documents `__builtin_ctz` and its undefined zero-input case.
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
  documents the AVX2 load, compare, movemask, and store intrinsics.
- [Single instruction, multiple data](https://en.wikipedia.org/wiki/Single_instruction%2C_multiple_data)
  gives an overview of SIMD, its history, and data-level parallelism.
- [Everyone Should Know SIMD](https://mitchellh.com/writing/everyone-should-know-simd)
  explains the common vector-loop shape: broadcast, load, operate, reduce, then
  finish with a scalar tail. That is the same shape used by `chunk_len`.
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
`ft_strjoin` version, and help draft this explanation, including the compiler-attribute
and direct-syscall documentation. The implementation and
benchmark results were checked locally with the real compiler and CPU rather
than accepted from generated estimates.
