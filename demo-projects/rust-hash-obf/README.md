# rust-hash-obf — obfuscated FNV-1a (64-bit) in Rust

An **obfuscated** sibling of [`../rust-hash`](../rust-hash), and the Rust twin
of [`../c-hash-obf`](../c-hash-obf): same single source file
([`src/main.rs`](src/main.rs)), same `rustc`-only build, but the hashing logic
is hardened against quick bisection of a disassembly.

```
$ just run fnv1a-rust-o2
403d1a231678d168  the quick brown fox
```

The output is byte-for-byte identical to `c-hash-obf` (and **differs** from the
plain demos' `59aeb7b40bd8c122` because of a runtime-derived salt). Three
obfuscation layers sit on top of the FNV-1a core (see the source header):

1. **Runtime-assembled constants** — the multiplier and basis never appear as a
   64-bit literal; each is rebuilt from XOR-masked 16-bit pieces with the mask
   read via a `volatile` load.
2. **A runtime-derived salt** mixed into the initial state and finaliser.
3. **A tiny bytecode VM** carrying the hashing steps, with trap instructions
   and decoy opcodes that never run; the opcode stream is read with
   `read_volatile` so the compiler can't specialise it away.

This is the obfuscated Rust arm of the experiment in
[`../README.md`](../README.md).

## Build matrix

| Recipe | Produces |
|---|---|
| `just build 0`      | `build/fnv1a-rust-o0` (`-C opt-level=0`) |
| `just build 2`      | `build/fnv1a-rust-o2` (`-C opt-level=2`) |
| `just build-all`    | both native variants |
| `just build-arm 0` / `... 2` | `build/fnv1a-rust-arm-o{0,2}` (AArch64) |
| `just build-arm-all`| both AArch64 variants |
| `just all`          | native + AArch64 |

AArch64 builds use the `aarch64-unknown-linux-gnu` target with
`aarch64-linux-gnu-gcc` as the linker.

### Third dimension: stripped binaries

Stripping is especially impactful for Rust — it removes the loud mangled
symbols that otherwise scream "Rust". Build the symbol-bearing variant first,
then strip a copy:

| Recipe | Produces |
|---|---|
| `just strip fnv1a-rust-o2`         | `build/fnv1a-rust-o2-stripped` |
| `just strip-arm fnv1a-rust-arm-o2` | `build/fnv1a-rust-arm-o2-stripped` |
| `just strip-all`                   | strips the `-O2` native + arm variants |

## Run

```sh
just run fnv1a-rust-o2                # native
just run fnv1a-rust-o2 "some input"   # hash your own string
just run-arm fnv1a-rust-arm-o2        # AArch64 under qemu-aarch64
just check                            # all native variants, same hash?
```

## Disassemble

```sh
just disasm fnv1a-rust-o2     # -> build/fnv1a-rust-o2.att.s and .intel.s
just disasm-fn fnv1a-rust-o0 # the hash function by name, if not inlined
just disasm-arm fnv1a-rust-arm-o2
just inspect fnv1a-rust-o2   # file type, (demangled) symbols, strings
```

## What to look at first (vs the plain rust-hash)

- **Still obviously Rust.** Mangled symbols (`_ZN…E`), `core::`, panic/format
  machinery and `library/std/src/...` paths remain — obfuscating the *logic*
  does nothing to hide the *language*. Strip the binary to take the symbols
  away; the `.rodata` strings still give it away.
- **The constants are gone.** `objdump -d build/fnv1a-rust-o2 | grep -i
  '100000001b3\|cbf29ce484222325'` returns nothing — the multiplier and basis
  are assembled at run time from XOR-masked pieces.
- **The logic is a VM.** Look for the dispatch loop over an opcode stream and the
  trap/decoy constants `0xa5a5a5a5a5a5a5a5` and `0xdeadbeefdeadbeef`; they
  survive `-O2` because the opcodes are read with `read_volatile`.
- **The test-string trick fails.** The salted digest `403d1a231678d168` doesn't
  match textbook FNV-1a, so one input/output pair won't name the algorithm.

## Staging the blind analysis exercise

The reverse-engineering exercise must run **without** sight of `src/`:

```sh
just stage   # rebuild + strip, then publish one opaque folder per binary
```

`blind/` then holds one `case-NN/` folder per binary, each containing just the
binary renamed `demo`, its disassembly `demo.asm`, and a copy of the
spoiler-free `blind/AGENTS.md`; the variant map lives only in the operator-only
`blind/_manifest.txt`. Point an analysis agent at **a single `case-NN/` folder**
— one opaque binary, no siblings, no path to `src/`. After it commits a
reconstruction (e.g. `case-05/recon.rs`), grade it:

```sh
just grade blind/case-05/recon.rs   # drops the reference beside it, compiles+diffs both
```

See [`../AGENTS.md`](../AGENTS.md) for the rationale and
[`blind/AGENTS.md`](blind/AGENTS.md) for the exercise.

## Required tools

See [`../README.md`](../README.md#required-tools-debian--ubuntu). For this
project specifically: a Rust toolchain via `rustup`, the
`aarch64-unknown-linux-gnu` target (`rustup target add ...`),
`gcc-aarch64-linux-gnu` (used as the cross linker and for `disasm-arm`), and
`qemu-user`. `rustfilt` (`cargo install rustfilt`) is optional but makes
`inspect` far more readable.
