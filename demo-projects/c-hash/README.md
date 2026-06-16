# c-hash — FNV-1a (64-bit) in C99

A single source file, [`src/fnv1a.c`](src/fnv1a.c), implementing the FNV-1a
64-bit hash by hand (no library hashing primitives). It hashes its first
command-line argument, or the fixed string `the quick brown fox` if given none,
and prints the 16-hex-digit result:

```
$ just run fnv1a-gcc-o2
59aeb7b40bd8c122  the quick brown fox
```

This is the C half of the experiment described in
[`../README.md`](../README.md): compile the same tiny "secret" the binary
many ways, then see how recoverable the source is from the machine code.

## Build matrix

| Recipe | Produces |
|---|---|
| `just build gcc 0`   | `build/fnv1a-gcc-o0`     (GCC, `-O0`) |
| `just build gcc 2`   | `build/fnv1a-gcc-o2`     (GCC, `-O2`) |
| `just build clang 0` | `build/fnv1a-clang-o0`   (Clang, `-O0`) |
| `just build clang 2` | `build/fnv1a-clang-o2`   (Clang, `-O2`) |
| `just build-all`     | all four native variants |
| `just build-arm-gcc 0` / `... 2`   | `build/fnv1a-arm-gcc-o{0,2}` (AArch64, GNU) |
| `just build-arm-clang 0` / `... 2` | `build/fnv1a-arm-clang-o{0,2}` (AArch64, Clang) |
| `just build-arm-all` | AArch64: gcc `-O0`/`-O2` + clang `-O2` |
| `just all`           | native + AArch64 |

All builds use `-std=c99 -Wall -Wextra`. The clang AArch64 recipe compiles with
clang (so the *body* code is clang's) but links with the GNU cross driver —
clang's lld plus the multilib sysroot mis-resolve libc's linker script.

### Third dimension: stripped binaries

Real shipped binaries usually have their symbol table removed, which hides
`fnv1a_hash`/`main` names and makes analysis harder. Build the symbol-bearing
variant first, then strip a copy:

| Recipe | Produces |
|---|---|
| `just strip fnv1a-gcc-o2`        | `build/fnv1a-gcc-o2-stripped` |
| `just strip-arm fnv1a-arm-gcc-o2`| `build/fnv1a-arm-gcc-o2-stripped` (cross strip) |
| `just strip-all`                 | strips the `-O2` gcc+clang, native+arm variants |

## Run

```sh
just run fnv1a-gcc-o2                 # native
just run fnv1a-gcc-o2 "some input"    # hash your own string
just run-arm fnv1a-arm-gcc-o2         # AArch64 under qemu-aarch64
just check                            # all native variants, same hash?
```

## Disassemble

```sh
just disasm fnv1a-gcc-o2      # -> build/fnv1a-gcc-o2.att.s and .intel.s
just disasm-fn fnv1a-gcc-o0  # just the fnv1a_hash function (Intel syntax)
just disasm-arm fnv1a-arm-gcc-o2  # -> build/fnv1a-arm-gcc-o2.arm.s
just inspect fnv1a-gcc-o2    # file type, symbols, tell-tale strings
```

`disasm` writes both AT&T and Intel syntax so you can read whichever you
prefer. `disasm-fn` is the quickest way to see *only* the algorithm — but note
that at `-O2` the static `fnv1a_hash` is **inlined into `main`** and no longer
has its own symbol, so the recipe falls back to disassembling `main`. Use `-O0`
to see the function standing on its own.

## What to look at first

- **`-O0` vs `-O2`.** At `-O0` the source maps almost line-for-line: the
  offset basis `0xcbf29ce484222325` loaded with `movabs`, an XOR, a multiply
  by `0x100000001b3`, a loop counter on the stack. At `-O2` the loop is tighter
  and folded into `main`.
- **The two magic constants** `0xcbf29ce484222325` and `0x100000001b3` are a
  dead giveaway for FNV-1a; they survive every optimisation level.
- **GCC vs Clang** differ in register allocation, loop layout, and the `.comment`
  section (`strings build/... | grep GCC` shows the GCC version that built it).

## Staging the blind analysis exercise

The reverse-engineering exercise must run **without** sight of `src/`. To make
that easy:

```sh
just stage   # rebuild matrix + strip, then publish one opaque folder per binary
```

`blind/` then holds one `case-NN/` folder per binary, each containing just the
binary renamed `demo`, its disassembly `demo.asm`, and a copy of the
spoiler-free `blind/AGENTS.md`. The variant each folder came from is recorded
only in the operator-only `blind/_manifest.txt`. Point an analysis agent at **a
single `case-NN/` folder** — it sees one opaque binary, no siblings, and no path
to `src/`. After it commits a reconstruction (e.g. `case-09/recon.c`), grade it:

```sh
just grade blind/case-09/recon.c   # drops the reference beside it, compiles+diffs both
```

See [`../AGENTS.md`](../AGENTS.md) for the rationale and
[`blind/AGENTS.md`](blind/AGENTS.md) for the exercise.

## Required tools

See [`../README.md`](../README.md#required-tools-debian--ubuntu). For this
project specifically: `build-essential` (gcc), `clang` + `lld` (for the
`*-clang-*` variants), `gcc-aarch64-linux-gnu` + `binutils-aarch64-linux-gnu`
(cross build and `disasm-arm`), and `qemu-user` (to run the AArch64 binaries).
