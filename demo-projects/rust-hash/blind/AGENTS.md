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
   their output is allowed and encouraged.
3. **Separate proof from inference.** Say which claims the bytes *prove* and
   which you are guessing from convention.

## What is in here

`bin/` holds the same tiny program compiled many ways. The filename encodes the
build (e.g. `fnv1a-rust-o0`, `fnv1a-rust-o2`, `fnv1a-rust-arm-o2`,
`*-stripped`). `asm/` holds a matching `<name>.s` disassembly for each.

The variants exist so you can see how the same logic looks under different
optimisation levels, architectures, and with or without a symbol table.
Recommended order:

1. `fnv1a-rust-o2` — optimised, symbols present (clearest core loop).
2. `fnv1a-rust-o0` — unoptimised; the logic is buried under runtime/iterator
   calls, which is itself instructive.
3. `fnv1a-rust-arm-o2` — different architecture (AArch64).
4. `fnv1a-rust-o2-stripped` — symbols removed; hardest. Note how much the
   stripping took away compared to the C demo's stripped variant.

## Tools (run these yourself)

```sh
file bin/<name>
readelf -h bin/<name>
nm bin/<name>                      # mangled symbols (empty if stripped)
nm bin/<name> | rustfilt           # demangled, if rustfilt is installed
strings bin/<name> | less
objdump -d -M intel bin/<name>     # or read asm/<name>.s
llvm-objdump -d bin/<name>

# Run it — your ground-truth oracle:
./bin/fnv1a-rust-o2 "some input"
qemu-aarch64 -L /usr/aarch64-linux-gnu ./bin/fnv1a-rust-arm-o2 "some input"
```

## Procedure

Narrate your reasoning at each step.

**Step 1 — Triage.** Architecture, 64-bit, PIE/dynamic, stripped? Note that the
binary is large compared to a minimal C program — ask yourself why.

**Step 2 — Identify the source language.** Decide what language and toolchain
produced this and cite the evidence (symbol-mangling style, embedded runtime,
panic/formatting machinery, source-path fragments in `strings`, binary size).
State your confidence. Then identify the optimisation level if you can. On the
stripped variant, note which of those tells survive and which vanish.

**Step 3 — Locate the interesting code.** Separate the program's own logic from
the (substantial) runtime, iterator, and formatting boilerplate. Find the
function(s) that do the work. Watch for inlining at higher optimisation levels.

**Step 4 — Read the algorithm instruction by instruction.** Determine exactly
what it computes: initial value(s), per-element operation(s), any
multiply/xor/shift/add constants, the loop bound, how input is obtained, and
how output is produced. Note every magic constant and whether it identifies a
known algorithm. Watch in particular for the presence or absence of an
overflow/panic check around arithmetic — it tells you something about how the
arithmetic was written.

**Step 5 — Reconstruct source.** Write Rust (or C — your choice, but Rust lets
you match more closely) that you believe is equivalent. It must compile and
reproduce the binary's behaviour.

**Step 6 — Verify against the oracle.** Build your reconstruction and compare
output across several inputs (including the no-argument default). Match must be
exact.

```sh
rustc -O -o /tmp/recon recon.rs
for s in "" "a" "the quick brown fox" "hello world"; do
    printf '%-20s recon=%s  real=%s\n' "$s" "$(/tmp/recon "$s")" "$(./bin/fnv1a-rust-o2 "$s")"
done
```

**Step 7 — Report, then ask for the answer key.** Summarise: language +
toolchain verdict and evidence; the algorithm and its constants; your
reconstructed source; verification results. Rate, per dimension, how much each
made analysis harder (`-O0` vs `-O2`, x86_64 vs AArch64, symbols vs stripped),
and compare the experience to analysing the C binary if you have also done
that one. What was unrecoverable?

Only **after** committing your reconstruction, ask the operator to drop the
original source into this folder (e.g. as `solution/`) so you can compare.
