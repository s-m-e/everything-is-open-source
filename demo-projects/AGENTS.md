# AGENTS.md — orientation (operator-facing)

This folder supports an academic experiment: **how much of a shipped binary's
logic can an analyst (human or LLM) recover from the machine code alone?** That
baseline is what tells a developer how "private" a proprietary feature really
is once the binary leaves their hands.

## Two roles, kept apart

- **Developer side** — the project roots (`c-hash/`, `rust-hash/`). Source in
  `src/`, build/run/disassemble logic in `justfile`, docs in `README.md`. This
  side is *not* blind; it owns the answer key (the real source).
- **Analyst side** — each project's `blind/` folder. It contains only compiled
  binaries (`bin/`) and disassembly (`asm/`) plus its own `blind/AGENTS.md`
  with the exercise. **No source lives there.**

To run the experiment honestly, populate `blind/` and then point the analysis
agent at **that folder only**:

```sh
cd c-hash && just stage     # builds the matrix, strips, disassembles -> blind/
# then, in a fresh session, give the agent ONLY:  c-hash/blind
```

Because the agent's root is `blind/`, it cannot reach `../src`. When you want it
to grade itself, manually copy or symlink the original source into the blind
folder (e.g. `cp src/fnv1a.c c-hash/blind/solution/`) *after* it has committed
its reconstruction.

> If you instead point an agent at a whole project root, it can trivially read
> the source and the experiment is meaningless. The blind/ split exists to
> prevent that by accident.

## The dimensions under study

The same tiny program (a hand-written hash, no library primitives) is compiled
many ways so you can see how each choice changes the disassembly:

1. **Source language** — C vs Rust.
2. **Compiler** — GCC vs LLVM/Clang (C); rustc is LLVM-based (Rust).
3. **Optimisation** — `-O0` vs `-O2`.
4. **Architecture** — x86_64 vs ARMv8/AArch64 (run under `qemu-aarch64`).
5. **Symbols** — full symbol table vs **stripped** (the realistic shipping case).

Both languages produce the identical output, so any reconstruction is checkable
against a known answer without even revealing the source.

## Language-detection markers (general, not project-specific)

| Marker | Suggests |
|---|---|
| Mangled symbols `_ZN…E`, `core::`, `alloc::`, "panicked at" | Rust |
| `__libc_start_main`, `.comment` = `GCC: (Ubuntu) …`, small binary | C (GCC) |
| `.comment` mentions `clang`/`LLVM`, no Rust symbols | C (Clang) |
| Large `.rodata` with paths like `library/std/src/...` | Rust (static std) |
| C++ mangling `_Z…` but no `core::`/Rust panics | C++ |
| No symbols at all | stripped — fall back to entry point + strings |

The detailed, spoiler-free, step-by-step exercise lives in each
`blind/AGENTS.md`. This file deliberately names no algorithm and no constants.

## End-to-end flow of one experiment

1. **Author** the program once, in `src/`, containing a small, self-contained
   piece of "secret" logic and nothing else of interest.
2. **`just stage`** — build the whole matrix (compilers × opt levels ×
   architectures), strip the shipping variants, disassemble everything, and
   publish binaries + disassembly into `blind/{bin,asm}/`. The source is *not*
   copied.
3. **Analyse blind** — in a fresh session, point an agent at `<project>/blind/`
   only. It follows `blind/AGENTS.md`: triage → detect language → locate the
   logic → read it instruction by instruction → reconstruct compilable source →
   verify against the binary's own output. It never sees `src/`.
4. **`just grade <recon>`** — reveal the original into `blind/solution/` and
   score the agent's reconstruction: behavioural comparison (compile both, run
   on several inputs) plus a textual diff. Run this only after the agent has
   committed its work.
5. **Read the result** as the baseline: what survived compilation (algorithm,
   distinctive constants) versus what was lost (names, comments, idioms), and
   how much each dimension raised the cost of recovery.

## Adding a new test project

New projects extend the experiment along the **language** axis (Go, Zig, C++,
Swift, …) and/or the **logic** axis (a different self-contained algorithm). Keep
every project interchangeable so the same flow and the same agent instructions
apply unchanged.

### Conventions

- **Folder name:** `<language>-<logic>`, e.g. `go-hash`, `zig-hash`, `c-crc`.
- **Self-contained logic only.** Implement the algorithm by hand — no library
  primitives, no crates/modules that do the real work. It must be small,
  deterministic, take input from `argv[1]` (with a fixed default), and print a
  deterministic, easily-compared result. Good candidates beyond FNV-1a: CRC-32,
  a tiny xorshift PRNG, a toy stream cipher, a Base32 encoder.
- **Known-answer test.** Either reuse the existing hashing contract (hash
  `the quick brown fox` → `59aeb7b40bd8c122`) so the new binary is directly
  comparable to the C and Rust demos, or, for different logic, define and
  document the project's own known-answer so a reconstruction is checkable.

### Required files

```
<language>-<logic>/
  src/<entrypoint>           # the program (one file, the secret logic + plumbing)
  justfile                   # the build/run/analyse contract (see below)
  README.md                  # developer-facing: recipes + what to look at first
  blind/AGENTS.md            # spoiler-free analyst exercise (no algorithm name!)
```

Everything else under `blind/` (`bin/`, `asm/`, `solution/`, dropped
reconstructions) is generated and already git-ignored by the repo-wide
`demo-projects/*/blind/*` rule; `build/` is ignored too. No `.gitignore` changes
are needed for a new project.

### The justfile contract

A new project's `justfile` must expose the same verbs so the flow above works
unchanged. Match the existing two projects:

| Recipe | Must do |
|---|---|
| `build …` / `build-all` | native x86_64 variants (vary compiler/opt where the toolchain allows) |
| `build-arm… ` / `build-arm-all` | AArch64 variants (cross-compile) |
| `strip` / `strip-arm` / `strip-all` | symbol-stripped copies of the `-O2` variants |
| `run` / `run-arm` | run native / run under `qemu-aarch64 -L /usr/aarch64-linux-gnu` |
| `check` | build all native variants and confirm the known-answer holds |
| `disasm` / `disasm-arm` / `inspect` | dump disassembly; triage file/symbols/strings |
| `stage` | rebuild + strip + disassemble, copy into `blind/{bin,asm}/`, **never** copy `src/` |
| `grade <recon>` | copy `src/` into `blind/solution/`, compile+run both, diff |
| `clean` | remove `build/` and `blind/{bin,asm}` — **keep** `blind/AGENTS.md` and `blind/solution/` |

Copy `c-hash/justfile` (single-file C, GCC+Clang) or `rust-hash/justfile`
(single-file, one compiler) as the starting template and adapt the toolchain.
For other languages: e.g. Go cross-compiles with `GOARCH=arm64` and needs no
external linker; Zig has built-in cross targets; C++ mirrors the C justfile with
`g++`/`clang++`. The AArch64 sysroot and `qemu-aarch64` are shared by all.

### The blind/AGENTS.md

Copy an existing `blind/AGENTS.md` and keep it **spoiler-free**: state the hard
rules, list the tools, and walk the same triage → language-detection →
locate-logic → read → reconstruct → verify → report procedure — but never name
the algorithm, its constants, or give reference code. The original source is the
only answer key, and it arrives only via `just grade`.

### Finishing up

Run `just stage` once to confirm the contract works end-to-end, then (optional,
human-facing) add a row to the projects table in `README.md`.

This file deliberately names no algorithm and no constants for the existing
projects, and new `blind/AGENTS.md` files must do the same.
