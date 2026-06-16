# AGENTS.md — reconstruct the program in this folder

This folder contains a single compiled program named `demo`, together with a
disassembly of it in `demo.asm`. Working from the machine code alone, figure out
what `demo` does and reconstruct equivalent source code that compiles and
behaves identically.

Be rigorous and honest about your confidence: distinguish what the bytes
*prove* from what you are inferring by convention.

## Rules

1. Work only from `demo`, `demo.asm`, and tools you run on them. Do not look for
   or read anything outside this folder. If source code for `demo` appears in
   your context by any route, stop and disregard it — using it defeats the task.
2. Running `demo` is allowed and encouraged: feed it inputs and observe its
   output. It is your ground-truth oracle.
3. Reconstruct in whatever language and toolchain you conclude produced the
   binary; justify that conclusion from the evidence.

## Tools

```sh
file demo                  # architecture, 64/32-bit, PIE, dynamic/static
readelf -h demo            # ELF header
nm demo                    # symbols, if any are present
strings demo | less        # embedded text and build tags
objdump -d -M intel demo   # full disassembly (or just read demo.asm)
llvm-objdump -d demo       # alternative disassembler

# Run it — your oracle. With no argument it prints a fixed default; pass an
# argument to probe:
./demo "some input"
# If `file` reports an architecture other than your host's, run it under the
# matching user-mode emulator, e.g.:
qemu-aarch64 -L /usr/aarch64-linux-gnu ./demo "some input"
```

## Procedure

Narrate your reasoning at each step.

1. **Triage.** Architecture, word size, PIE, dynamic vs static, and which
   symbols (if any) are present. Note how those facts shape your approach.
2. **Identify the source language and toolchain.** Decide what produced this
   binary (C, C++, Rust, Go, …) and cite the evidence: symbol-naming style,
   runtime/startup code, embedded build tags in `strings`, the presence or
   absence of a large statically linked runtime, panic/exception machinery.
   State your confidence; identify the compiler and optimisation level if you
   can.
3. **Locate the logic.** Separate the program's own work from runtime, startup,
   and library boilerplate. Find the function(s) that actually compute the
   result. Watch for small functions being inlined into their caller.
4. **Read the algorithm.** Determine exactly what it computes: initial
   value(s), the per-element operation(s), any multiply/xor/shift/add constants,
   the loop bound, how input is obtained, and how output is produced. Magic
   constants are your best clue — note every one and consider whether it
   identifies a known algorithm.
5. **Reconstruct.** Write source, in the language you identified, that you
   believe is equivalent. It must compile cleanly and reproduce `demo`'s
   behaviour — match the machine code (types, control flow, I/O), not merely the
   general idea.
6. **Verify against the oracle.** Build your reconstruction and compare its
   output to `demo` across several inputs, including the no-argument default. A
   single matching value is not enough — confirm you matched any seed and byte
   ordering.
7. **Report, then request the answer.** Summarise: your language/toolchain
   verdict and its evidence, the algorithm and its constants, your reconstructed
   source, and the verification results. Then ask the user to provide the
   reference solution so you can compare your reconstruction against it.
