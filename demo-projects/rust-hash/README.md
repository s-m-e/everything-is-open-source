# rust-hash — FNV-1a (64-bit) in Rust

A single source file, [`src/main.rs`](src/main.rs), implementing the *same*
FNV-1a 64-bit hash as [`../c-hash`](../c-hash) — no external crates, no library
hashing primitives. Built with `rustc` directly (no Cargo) to keep it as
minimal as the C demo.

```
$ just run fnv1a-rust-o2
59aeb7b40bd8c122  the quick brown fox
```

The output is byte-for-byte identical to the C demo, which is the point: it
lets you compare how the *same* logic looks when lowered from two very
different languages. This is the Rust half of the experiment in
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
just disasm-fn fnv1a-rust-o0 # just fnv1a_hash (it's inlined at -O2)
just disasm-arm fnv1a-rust-arm-o2
just inspect fnv1a-rust-o2   # file type, (demangled) symbols, strings
```

## What to look at first

- **Symbol names scream "Rust".** `just inspect` (or `nm`) shows mangled names
  like `_ZN4main10fnv1a_hash17h…E` and references to `core::slice::iter`,
  `core::panicking`, `core::fmt`. Install `rustfilt` to demangle them.
- **`-O0` is verbose.** Rust at `-O0` keeps iterator machinery as real calls
  (`into_iter`, `Iterator::next`), so the hash loop is buried in call/branch
  noise. At `-O2` it collapses to a tight loop with the same two FNV constants
  as the C build — and, like the C build, **inlined into `main`**.
- **Bigger binary, more strings.** The Rust binary statically links `std`, so
  panic messages, formatting code, and source-path fragments (e.g.
  `library/std/src/...`) leak in. These are strong language markers — and a good
  reason to compare the **stripped** variant, which removes the symbol table but
  *not* those `.rodata` strings.

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
