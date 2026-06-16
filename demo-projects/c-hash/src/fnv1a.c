/*
 * fnv1a.c — a tiny, self-contained FNV-1a (64-bit) hash.
 *
 * Deliberately written with NO library hashing primitives: the whole
 * algorithm is the handful of arithmetic operations in fnv1a_hash().
 * This is the "secret" logic a developer might want to keep private in a
 * shipped binary. The rest of the program is just plumbing so the binary
 * is runnable and produces a deterministic, comparable result.
 *
 * Reference algorithm (FNV-1a, 64-bit):
 *   hash = 0xcbf29ce484222325        // FNV offset basis
 *   for each byte b in input:
 *       hash = hash XOR b
 *       hash = hash * 0x100000001b3  // FNV prime, wraps mod 2^64
 *
 * Build/run/disassemble: see ../justfile and ../README.md.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The interesting part. Kept as a separate function so it shows up as a
 * distinct symbol in the disassembly and is easy to point a reader at. */
static uint64_t fnv1a_hash(const unsigned char *data, size_t len)
{
    uint64_t hash = 0xcbf29ce484222325ULL; /* FNV offset basis */
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)data[i];
        hash *= 0x100000001b3ULL; /* FNV prime; multiplication wraps mod 2^64 */
    }
    return hash;
}

int main(int argc, char **argv)
{
    /* Hash argv[1] if given, otherwise a fixed known-answer string so the
     * binary is a deterministic reproducer. */
    const char *input = (argc > 1) ? argv[1] : "the quick brown fox";

    uint64_t h = fnv1a_hash((const unsigned char *)input, strlen(input));

    printf("%016" PRIx64 "  %s\n", h, input);
    return 0;
}
