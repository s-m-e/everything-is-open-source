/*
 * fnv1a.c (obfuscated) — the same family of hash as ../../c-hash, but built to
 * resist quick bisection of a disassembly. Three layers are stacked on top of
 * the plain FNV-1a core:
 *
 *   1. Runtime-assembled constants. The 64-bit multiplier and offset basis
 *      never appear in the binary as a literal / `movabs` immediate. Each is
 *      rebuilt from four 16-bit pieces that are stored XOR-masked, and the mask
 *      is read through a `volatile`, so the optimiser cannot fold the assembly
 *      back into a constant — it must compute the value at run time.
 *
 *   2. A runtime-derived salt mixed into the initial state and the finaliser,
 *      so the output for a known test string does NOT match textbook FNV-1a and
 *      the algorithm cannot be identified just by hashing "the quick brown fox".
 *
 *   3. A tiny bytecode VM. The actual hashing steps are expressed as opcodes
 *      interpreted at run time, with a couple of trap instructions (dead ends
 *      on scratch registers) and decoy opcodes that never execute. The opcode
 *      stream is read through a `volatile` view, which stops the compiler from
 *      learning it and specialising the VM (or deleting the trap cases) away.
 *
 * It is still a deterministic hash: see ../README.md for the known-answer test.
 * This is a teaching artefact, not strong protection — every layer is only
 * meant to *raise the cost* of reconstruction, which is the whole experiment.
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Runtime-assembled 64-bit constants.
 * Real values (recover by XOR-ing the source pieces below with 0x5a5a):
 *   multiplier = 0x00000100000001b3
 *   basis      = 0xcbf29ce484222325
 * -------------------------------------------------------------------------- */
static uint64_t assemble(const uint16_t masked[4])
{
    volatile uint16_t mask = 0x5a5a; /* volatile: unknown to the optimiser */
    uint64_t acc = 0;
    for (int i = 0; i < 4; i++)
        acc = (acc << 16) | (uint16_t)(masked[i] ^ mask);
    return acc;
}

static uint64_t rotl64(uint64_t x, unsigned r) { r &= 63; return r ? (x << r) | (x >> (64 - r)) : x; }
static uint64_t rotr64(uint64_t x, unsigned r) { r &= 63; return r ? (x >> r) | (x << (64 - r)) : x; }

/* --------------------------------------------------------------------------
 * The VM.
 * -------------------------------------------------------------------------- */
enum {
    OP_HALT = 0, OP_LDP, OP_LDB, OP_LDS, OP_XOR, OP_MUL, OP_SHRX,
    OP_FETCH, OP_JZ, OP_JMP,
    OP_TRAPMUL0, OP_TRAPSPIN, /* executed, but write only scratch registers */
    OP_POISON, OP_TRAPJAM      /* decoys: in the dispatch table, never reached */
};

#define NREG 8

/* Read a program byte through a volatile view so the optimiser cannot learn
 * the opcode stream and specialise (or prune trap cases from) the VM. */
static uint8_t rdcode(const uint8_t *prog, size_t i)
{
    return *(const volatile uint8_t *)(prog + i);
}

static uint64_t vm_run(const uint8_t *prog, size_t ninstr,
                       const unsigned char *in, size_t len,
                       uint64_t P, uint64_t B, uint64_t S)
{
    uint64_t reg[NREG] = {0};
    uint64_t flag = 0;
    size_t pc = 0, ip = 0;

    for (;;) {
        if (pc >= ninstr) break;
        uint8_t op = rdcode(prog, pc * 3 + 0);
        uint8_t o1 = rdcode(prog, pc * 3 + 1);
        uint8_t o2 = rdcode(prog, pc * 3 + 2);

        switch (op) {
        case OP_HALT:     return reg[0];
        case OP_LDP:      reg[o1 & 7] = P; pc++; break;
        case OP_LDB:      reg[o1 & 7] = B; pc++; break;
        case OP_LDS:      reg[o1 & 7] = S; pc++; break;
        case OP_XOR:      reg[o1 & 7] ^= reg[o2 & 7]; pc++; break;
        case OP_MUL:      reg[o1 & 7] *= reg[o2 & 7]; pc++; break;
        case OP_SHRX:     reg[o1 & 7] ^= reg[o1 & 7] >> (o2 & 63); pc++; break;
        case OP_FETCH:
            if (ip < len) { reg[o1 & 7] = in[ip++]; flag = 1; }
            else            flag = 0;
            pc++; break;
        case OP_JZ:       pc = (flag == 0) ? o1 : pc + 1; break;
        case OP_JMP:      pc = o1; break;

        /* Traps: genuine instructions in the program, but they only touch
         * scratch registers that never reach the result. The volatile opcode
         * fetch stops the compiler proving these writes are dead. */
        case OP_TRAPMUL0: reg[o1 & 7] = reg[o1 & 7] * 0u; pc++; break;
        case OP_TRAPSPIN: reg[o1 & 7] = rotr64(reg[o1 & 7] ^ 0xA5A5A5A5A5A5A5A5ULL, 7) + 1u; pc++; break;

        /* Decoys: the real program never dispatches these, but the compiler
         * cannot know that, so they stay in the table as dead ends. */
        case OP_POISON:   reg[o1 & 7] = 0xDEADBEEFDEADBEEFULL; pc++; break;
        case OP_TRAPJAM:  break; /* no pc advance: would spin forever if hit */

        default:          return reg[0];
        }
    }
    return reg[0];
}

static uint64_t obf_hash(const unsigned char *data, size_t len)
{
    /* 16-bit pieces, each = real ^ 0x5a5a (see assemble()). */
    static const uint16_t mul_parts[4]   = { 0x5a5a, 0x5b5a, 0x5a5a, 0x5be9 };
    static const uint16_t basis_parts[4] = { 0x91a8, 0xc6be, 0xde78, 0x797f };

    uint64_t P = assemble(mul_parts);
    uint64_t B = assemble(basis_parts);
    uint64_t S = rotl64(B, 17) ^ (P * 3u); /* salt, derived from P and B at run time */

    /* Bytecode: 3 bytes per instruction {op, o1, o2}.
     *   r0=hash  r1=P  r2=S  r3=byte ;  r6,r7 scratch (traps)
     *  0 LDB  r0       ; r0 = B
     *  1 LDS  r2       ; r2 = S
     *  2 XOR  r0,r2    ; r0 = B ^ S
     *  3 LDP  r1       ; r1 = P
     *  4 TRAPSPIN r7   ; (loop top) scratch dead-end
     *  5 FETCH r3      ; r3 = next byte; flag = had-byte
     *  6 JZ   11       ; no byte -> finalise
     *  7 XOR  r0,r3    ; hash ^= byte
     *  8 MUL  r0,r1    ; hash *= P
     *  9 TRAPMUL0 r6   ; scratch dead-end
     * 10 JMP  4        ; loop
     * 11 SHRX r0,33    ; (finalise) hash ^= hash >> 33
     * 12 MUL  r0,r1    ; hash *= P
     * 13 SHRX r0,29    ; hash ^= hash >> 29
     * 14 XOR  r0,r2    ; hash ^= S
     * 15 HALT
     */
    static const uint8_t prog[] = {
        OP_LDB, 0, 0,   OP_LDS, 2, 0,   OP_XOR, 0, 2,   OP_LDP, 1, 0,
        OP_TRAPSPIN, 7, 0,   OP_FETCH, 3, 0,   OP_JZ, 11, 0,   OP_XOR, 0, 3,
        OP_MUL, 0, 1,   OP_TRAPMUL0, 6, 0,   OP_JMP, 4, 0,   OP_SHRX, 0, 33,
        OP_MUL, 0, 1,   OP_SHRX, 0, 29,   OP_XOR, 0, 2,   OP_HALT, 0, 0,
    };

    return vm_run(prog, sizeof prog / 3, data, len, P, B, S);
}

int main(int argc, char **argv)
{
    const char *input = (argc > 1) ? argv[1] : "the quick brown fox";
    uint64_t h = obf_hash((const unsigned char *)input, strlen(input));
    printf("%016" PRIx64 "  %s\n", h, input);
    return 0;
}
