# c-hash-obf — obfuscated FNV-1a (64-bit) in C99

An **obfuscated** sibling of [`../c-hash`](../c-hash). Same single source file
([`src/fnv1a.c`](src/fnv1a.c)), same build/run/disassemble/stage machinery, but
the hashing logic is deliberately hardened against quick bisection of a
disassembly. It hashes its first command-line argument (or `the quick brown
fox` by default) and prints a 16-hex-digit result:

```
$ just run fnv1a-gcc-o2
403d1a231678d168  the quick brown fox
```

Note the digest **differs** from the plain `c-hash` (`59aeb7b40bd8c122`): a
runtime-derived salt is mixed in, so the algorithm can't be identified just by
hashing the test string. The obfuscation has three layers (see the source
header for detail):

1. **Runtime-assembled constants** — the FNV multiplier and basis never appear
   as a 64-bit immediate; each is rebuilt from XOR-masked 16-bit pieces, with
   the mask read through a `volatile` so the optimiser can't fold it back.
2. **A runtime-derived salt** mixed into the initial state and finaliser.
3. **A tiny bytecode VM** carrying the hashing steps, with trap instructions
   (dead ends on scratch registers) and decoy opcodes that never run; the
   opcode stream is read through a `volatile` view so the compiler can't
   specialise the VM or prune the traps.

This is the obfuscated arm of the experiment in [`../README.md`](../README.md):
measure how much that raises the cost of recovering the source from the binary.

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

Real shipped binaries usually have their symbol table removed, which hides the
`assemble`/`vm_run`/`obf_hash` names and makes analysis harder. Build the
symbol-bearing variant first, then strip a copy:

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
just disasm-fn fnv1a-gcc-o0  # the vm_run function (Intel syntax), if not inlined
just disasm-arm fnv1a-arm-gcc-o2  # -> build/fnv1a-arm-gcc-o2.arm.s
just inspect fnv1a-gcc-o2    # file type, symbols, tell-tale strings
```

`disasm` writes both AT&T and Intel syntax. `disasm-fn` greps for the hash
function by name (`fnv1a_hash` in the plain demo; here the symbols are
`assemble`/`vm_run`/`obf_hash`), falling back to `main` when it has been
inlined.

## What to look at first (vs the plain c-hash)

- **The constants are gone.** `objdump -d build/fnv1a-gcc-o2 | grep -i
  '100000001b3\|cbf29ce484222325'` returns nothing at any optimisation level —
  the FNV multiplier and basis are assembled at run time from XOR-masked
  16-bit pieces, never materialising as a `movabs` immediate.
- **The control flow is a VM.** Instead of a tight xor/multiply loop you'll find
  a dispatch loop (an indirect `jmp`/jump table) over an opcode stream, plus the
  trap/decoy constants `0xa5a5a5a5a5a5a5a5` and `0xdeadbeefdeadbeef` — dead-end
  instructions that survive `-O2` because the opcode bytes are read through a
  `volatile`.
- **The test-string trick fails.** The default digest is `403d1a231678d168`, not
  textbook FNV-1a's `59aeb7b40bd8c122`, because of the salt — so you can't name
  the algorithm just from one known input/output pair.
- **GCC vs Clang** still differ in how they lower the interpreter and the
  constant assembly; compare the two `-O2` disassemblies.

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
