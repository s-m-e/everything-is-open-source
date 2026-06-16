# demo-projects — how private is a "private" feature in a shipped binary?

These small projects support a single, deliberately narrow experiment:

> A modern LLM can read disassembled machine code and reason about what it
> does with surprising ease. So **what can a developer still do to keep a
> proprietary feature genuinely private once the binary is in a customer's
> hands?**

To study that, we need a baseline: how much of the *original source* can be
recovered from a *compiled binary* under realistic conditions? Each demo is a
tiny program containing one piece of "secret" logic — a hand-written hashing
algorithm, no library primitives — that we compile many different ways and
then try to reverse-engineer.

The whole exercise is **academic**. The point is to understand the capability
well enough that developers can make informed decisions about their own code.

## The projects

| Project | Language | Secret logic |
|---|---|---|
| [`c-hash/`](c-hash/) | C99 | FNV-1a 64-bit hash |
| [`rust-hash/`](rust-hash/) | Rust (no crates) | the *same* FNV-1a 64-bit hash |

Both implement the identical algorithm and print the identical result, so the
two binaries are directly comparable. Known-answer test (default input
`the quick brown fox`):

```
59aeb7b40bd8c122  the quick brown fox
```

This holds for **every** variant — gcc, clang, rustc, `-O0`, `-O2`, x86_64 and
AArch64 — which makes "did I reconstruct it correctly?" trivial to check.

## The dimensions we vary

Each variation changes what the disassembly looks like, and therefore how hard
(or easy) reconstruction is:

1. **Source language** — C vs Rust. Different idioms, runtimes, and symbol
   conventions leave different fingerprints.
2. **Compiler** — GCC vs LLVM/Clang (C); rustc is LLVM-based (Rust).
3. **Optimisation level** — `-O0` (literal, every variable spilled to the
   stack) vs `-O2` (inlined, vectorised, unrecognisable control flow).
4. **Architecture** — native x86_64 vs ARMv8/AArch64, the latter executed
   under `qemu-aarch64` user-mode emulation.
5. **Symbols** — full symbol table vs **stripped**, the realistic state of a
   shipped binary. Stripping hides function names (and, for Rust, the very loud
   mangled symbols) without touching `.rodata` strings or the code itself.

A good first observation: at `-O2` the tiny `fnv1a_hash` function is **inlined
into `main`** and stops existing as a separate symbol — in both languages.

## Workflow

Everything is driven by `just`. From inside either project:

```sh
just                 # list recipes
just build-all       # native x86_64 matrix
just build-arm-all   # AArch64 matrix
just strip-all       # stripped copies of the -O2 "shipping" variants
just check           # build outputs + verify the known-answer hash
just disasm <bin>    # -> build/<bin>.att.s and .intel.s
just disasm-arm <bin># -> build/<bin>.arm.s
just inspect <bin>   # file type, symbols, tell-tale strings
just run <bin> [arg] # run native
just run-arm <bin>   # run AArch64 under qemu
just stage           # build everything + publish into blind/ (no source!)
just clean
```

### Running the analysis blind

The analyst (an LLM agent) must work **without** seeing the source, or the
experiment is meaningless. The split:

- **`<project>/src/`** + `justfile` + `README.md` — developer side, owns the
  source (the answer key).
- **`<project>/blind/`** — analyst side: only `bin/` (binaries), `asm/`
  (disassembly), and `blind/AGENTS.md` (spoiler-free instructions). `just stage`
  populates it; **no source is ever copied there**.

Point the analysis agent at the `blind/` folder *only*; with that as its root it
cannot reach `../src`. When you want it to grade itself, manually copy the
original source into the blind folder (e.g. `cp src/fnv1a.c c-hash/blind/solution/`)
*after* it has committed its reconstruction.

See [`AGENTS.md`](AGENTS.md) for the operator-facing rationale, each project's
`README.md` for exact recipes, and each `blind/AGENTS.md` for the step-by-step
exercise.

## Required tools (Debian / Ubuntu)

```sh
# C: native compilers, cross compiler, binutils
sudo apt install build-essential clang lld binutils \
                 gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu

# Run AArch64 binaries on an x86_64 host
sudo apt install qemu-user qemu-user-static

# Rust (via rustup) + the AArch64 target
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
rustup target add aarch64-unknown-linux-gnu

# just (task runner)
cargo install just      # or: download from https://github.com/casey/just/releases

# Optional niceties
sudo apt install llvm   # llvm-objdump, an alternative disassembler
cargo install rustfilt  # demangle Rust symbols in `just inspect`
```

Notes:

- **clang** is only needed for the `*-clang-*` C variants; gcc-only use needs
  none of it. **lld** is used by clang's AArch64 cross build (`-fuse-ld=lld`).
- The cross compiler ships `aarch64-linux-gnu-objdump`, which the `disasm-arm`
  recipes use; the system `objdump` may be x86-only.
- `qemu-aarch64` needs the cross **sysroot** at `/usr/aarch64-linux-gnu`
  (installed by `gcc-aarch64-linux-gnu`) to find the dynamic loader and libc;
  the recipes pass it via `-L`.
