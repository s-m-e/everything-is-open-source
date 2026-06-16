// main.rs — a tiny, self-contained FNV-1a (64-bit) hash.
//
// The Rust counterpart of ../../c-hash/src/fnv1a.c. It implements the exact
// same algorithm so the two binaries produce identical output for identical
// input, letting us compare how the same logic looks when lowered from two
// very different source languages.
//
// No external crates, no library hashing primitives — the algorithm is just
// the arithmetic in `fnv1a_hash`.
//
// Reference algorithm (FNV-1a, 64-bit):
//   hash = 0xcbf29ce484222325        // FNV offset basis
//   for each byte b in input:
//       hash = hash XOR b
//       hash = hash * 0x100000001b3  // FNV prime, wraps mod 2^64
//
// Build/run/disassemble: see ../justfile and ../README.md.

/// The interesting part. `wrapping_mul` is required because plain `*` panics
/// on overflow in debug builds; FNV relies on wrap-around mod 2^64.
fn fnv1a_hash(data: &[u8]) -> u64 {
    let mut hash: u64 = 0xcbf2_9ce4_8422_2325; // FNV offset basis
    for &b in data {
        hash ^= b as u64;
        hash = hash.wrapping_mul(0x100_0000_01b3); // FNV prime
    }
    hash
}

fn main() {
    // Hash argv[1] if given, otherwise a fixed known-answer string so the
    // binary is a deterministic reproducer (matches the C demo).
    let args: Vec<String> = std::env::args().collect();
    let input: String = if args.len() > 1 {
        args[1].clone()
    } else {
        "the quick brown fox".to_string()
    };

    let h = fnv1a_hash(input.as_bytes());

    println!("{:016x}  {}", h, input);
}
