# AGENTS.md — blind binary-analysis exercise

You have been pointed at **this folder only**. It contains compiled binaries
(`bin/`) and their disassembly (`asm/`). It does **not** contain the original
source code — that is deliberate. Your job is to reconstruct the program from
the machine code alone, then have a human compare your result against the real
source.

This is an academic exercise in how much of a shipped binary's logic an
analyst (human or LLM) can recover. Be rigorous and honest about your
confidence.

## Hard rules

1. **Do not go looking for the source.** Do not read files outside this folder,
   do not `cat ../src`, do not search the wider repository. If you find source
   code in your context, stop and disregard it — using it invalidates the
   experiment. Work only from `bin/` and `asm/` (and tools you run on them).
2. **The binaries themselves are a fair oracle.** Running them and observing
   their output is allowed and encouraged — that is what a real analyst does.
3. **Separate proof from inference.** Say which claims the bytes *prove* and
   which you are guessing from convention.

## What is in here

`bin/` holds the same tiny program compiled many ways. The filename encodes the
build (e.g. `fnv1a-gcc-o2`, `fnv1a-clang-o0`, `fnv1a-arm-gcc-o2`,
`*-stripped`). `asm/` holds a matching `<name>.s` disassembly for each.

The variants exist so you can see how the same logic looks under different
compilers, optimisation levels, architectures, and with or without a symbol
table. Recommended order, easiest first:

1. `fnv1a-gcc-o0` — unoptimised, symbols present.
2. `fnv1a-gcc-o2` / `fnv1a-clang-o2` — optimised, symbols present.
3. `fnv1a-arm-gcc-o2` — different architecture (AArch64).
4. `fnv1a-gcc-o2-stripped` — symbols removed; hardest.

## Tools (run these yourself)

```sh
file bin/<name>                 # arch, PIE, dynamic/static, stripped?
readelf -h bin/<name>           # ELF header
nm bin/<name>                   # symbols (empty if stripped)
strings bin/<name> | less       # embedded text, build tags
objdump -d -M intel bin/<name>  # full disassembly (or read asm/<name>.s)
llvm-objdump -d bin/<name>      # alternative disassembler

# Run it — your ground-truth oracle:
./bin/fnv1a-gcc-o2 "some input"                         # native x86_64
qemu-aarch64 -L /usr/aarch64-linux-gnu ./bin/fnv1a-arm-gcc-o2 "some input"  # AArch64
```

## Procedure

Narrate your reasoning at each step.

**Step 1 — Triage.** For your chosen binary, record architecture, 64-bit,
PIE/dynamic, and whether it is stripped. Note how those facts would change your
approach.

**Step 2 — Identify the source language.** Decide whether this came from C,
C++, Rust, Go, etc., and cite the evidence (symbol naming style, runtime
startup, embedded build tags in `strings`, presence/absence of a large
statically linked runtime, panic/exception machinery, …). State your
confidence. Then identify the compiler and optimisation level if you can.

**Step 3 — Locate the interesting code.** Separate the program's own logic from
runtime/startup/library boilerplate. Find the function(s) that actually do the
work. If symbols are present this is easy; if stripped, reason from the entry
point and string/`printf`-style references. Watch for the function being
**inlined into another** at higher optimisation levels.

**Step 4 — Read the algorithm instruction by instruction.** Determine exactly
what it computes: the initial value(s), the per-element operation(s), any
multiply/xor/shift/add constants, the loop bound, how input is obtained, and
how output is produced. Magic constants are your best friends — note every one
and consider whether it identifies a known algorithm.

**Step 5 — Reconstruct compilable C.** Write C source you believe is equivalent.
It must compile cleanly and reproduce the binary's behaviour. Keep it faithful
to what the machine code shows (types, control flow, I/O), not just
"something that hashes."

**Step 6 — Verify against the oracle.** Compile your reconstruction and compare
its output to the real binary across several inputs (including the no-argument
default). They must match exactly — a single matching value is not enough to
prove you got the seed and byte order right.

```sh
gcc -O2 -o /tmp/recon recon.c
for s in "" "a" "the quick brown fox" "hello world"; do
    printf '%-20s recon=%s  real=%s\n' "$s" "$(/tmp/recon "$s")" "$(./bin/fnv1a-gcc-o2 "$s")"
done
```

**Step 7 — Report, then ask for the answer key.** Summarise: language +
compiler verdict and evidence; the algorithm and its constants; your
reconstructed source; and the verification results. Explicitly rate, per
dimension, how much each made analysis harder (`-O0` vs `-O2`, gcc vs clang,
x86_64 vs AArch64, symbols vs stripped) and what (if anything) was
unrecoverable (names, comments, exact source idioms).

Only **after** you have committed your reconstruction, ask the operator to drop
the original source into this folder (e.g. as `solution/`) so you can compare.
Until they do, treat the source as unavailable.
