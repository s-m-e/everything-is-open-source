// main.rs (obfuscated) — the Rust counterpart of ../../c-hash-obf/src/fnv1a.c.
//
// It implements the exact same obfuscated hash, so the two binaries print
// identical output for identical input. Three layers sit on top of a plain
// FNV-1a core:
//
//   1. Runtime-assembled constants. The 64-bit multiplier and offset basis
//      never appear in the binary as a literal. Each is rebuilt from four
//      16-bit pieces stored XOR-masked, with the mask read through a volatile
//      load, so the optimiser cannot fold the assembly back into a constant.
//
//   2. A runtime-derived salt mixed into the initial state and the finaliser,
//      so the output does NOT match textbook FNV-1a and the algorithm cannot be
//      identified just by hashing a known test string.
//
//   3. A tiny bytecode VM carrying the hashing steps, with trap instructions
//      (dead ends on scratch registers) and decoy opcodes that never execute.
//      The opcode stream is read via volatile loads so the compiler cannot
//      specialise the VM (or delete the trap cases) away.
//
// A teaching artefact, not strong protection — each layer only raises the cost
// of reconstruction. See ../README.md for the known-answer test.

// Real values (recover by XOR-ing the pieces below with 0x5a5a):
//   multiplier = 0x0000_0100_0000_01b3
//   basis      = 0xcbf2_9ce4_8422_2325
fn assemble(masked: &[u16; 4]) -> u64 {
    let m: u16 = 0x5a5a; // volatile load below: value unknown to the optimiser
    let mask = unsafe { core::ptr::read_volatile(&m) };
    let mut acc: u64 = 0;
    for &x in masked {
        acc = (acc << 16) | ((x ^ mask) as u64);
    }
    acc
}

// VM opcodes.
const OP_HALT: u8 = 0;
const OP_LDP: u8 = 1;
const OP_LDB: u8 = 2;
const OP_LDS: u8 = 3;
const OP_XOR: u8 = 4;
const OP_MUL: u8 = 5;
const OP_SHRX: u8 = 6;
const OP_FETCH: u8 = 7;
const OP_JZ: u8 = 8;
const OP_JMP: u8 = 9;
const OP_TRAPMUL0: u8 = 10; // executed, but writes only scratch registers
const OP_TRAPSPIN: u8 = 11;
const OP_POISON: u8 = 12; // decoys: in the dispatch, never reached
const OP_TRAPJAM: u8 = 13;

// Read a program byte through a volatile load so the optimiser cannot learn the
// opcode stream and specialise (or prune trap cases from) the VM.
fn rdcode(prog: &[u8], i: usize) -> u8 {
    unsafe { core::ptr::read_volatile(prog.as_ptr().add(i)) }
}

fn vm_run(prog: &[u8], ninstr: usize, input: &[u8], p: u64, b: u64, s: u64) -> u64 {
    let mut reg = [0u64; 8];
    let mut flag: u64 = 0;
    let mut pc: usize = 0;
    let mut ip: usize = 0;

    loop {
        if pc >= ninstr {
            break;
        }
        let op = rdcode(prog, pc * 3);
        let o1 = rdcode(prog, pc * 3 + 1);
        let o2 = rdcode(prog, pc * 3 + 2);
        let d = (o1 & 7) as usize;
        let sreg = (o2 & 7) as usize;

        match op {
            OP_HALT => return reg[0],
            OP_LDP => { reg[d] = p; pc += 1; }
            OP_LDB => { reg[d] = b; pc += 1; }
            OP_LDS => { reg[d] = s; pc += 1; }
            OP_XOR => { reg[d] ^= reg[sreg]; pc += 1; }
            OP_MUL => { reg[d] = reg[d].wrapping_mul(reg[sreg]); pc += 1; }
            OP_SHRX => { reg[d] ^= reg[d] >> (o2 & 63); pc += 1; }
            OP_FETCH => {
                if ip < input.len() {
                    reg[d] = input[ip] as u64;
                    ip += 1;
                    flag = 1;
                } else {
                    flag = 0;
                }
                pc += 1;
            }
            OP_JZ => { pc = if flag == 0 { o1 as usize } else { pc + 1 }; }
            OP_JMP => { pc = o1 as usize; }

            // Traps: genuine instructions, but they only touch scratch registers
            // that never reach the result. The volatile opcode fetch stops the
            // compiler proving these writes are dead.
            OP_TRAPMUL0 => { reg[d] = reg[d].wrapping_mul(0); pc += 1; }
            OP_TRAPSPIN => {
                reg[d] = (reg[d] ^ 0xA5A5_A5A5_A5A5_A5A5).rotate_right(7).wrapping_add(1);
                pc += 1;
            }

            // Decoys: never dispatched by the real program, but the compiler
            // cannot know that, so they stay in the table as dead ends.
            OP_POISON => { reg[d] = 0xDEAD_BEEF_DEAD_BEEF; pc += 1; }
            OP_TRAPJAM => { /* no pc advance: would spin forever if hit */ }

            _ => return reg[0],
        }
    }
    reg[0]
}

fn obf_hash(data: &[u8]) -> u64 {
    // 16-bit pieces, each = real ^ 0x5a5a (see assemble()).
    let mul_parts: [u16; 4] = [0x5a5a, 0x5b5a, 0x5a5a, 0x5be9];
    let basis_parts: [u16; 4] = [0x91a8, 0xc6be, 0xde78, 0x797f];

    let p = assemble(&mul_parts);
    let b = assemble(&basis_parts);
    let s = b.rotate_left(17) ^ p.wrapping_mul(3); // salt, derived at run time

    // Bytecode: 3 bytes per instruction {op, o1, o2}.
    //   r0=hash  r1=P  r2=S  r3=byte ;  r6,r7 scratch (traps)
    //  0 LDB r0 ; 1 LDS r2 ; 2 XOR r0,r2 ; 3 LDP r1
    //  4 TRAPSPIN r7 (loop top) ; 5 FETCH r3 ; 6 JZ 11 ; 7 XOR r0,r3
    //  8 MUL r0,r1 ; 9 TRAPMUL0 r6 ; 10 JMP 4 ; 11 SHRX r0,33 (finalise)
    // 12 MUL r0,r1 ; 13 SHRX r0,29 ; 14 XOR r0,r2 ; 15 HALT
    let prog: [u8; 48] = [
        OP_LDB, 0, 0, OP_LDS, 2, 0, OP_XOR, 0, 2, OP_LDP, 1, 0,
        OP_TRAPSPIN, 7, 0, OP_FETCH, 3, 0, OP_JZ, 11, 0, OP_XOR, 0, 3,
        OP_MUL, 0, 1, OP_TRAPMUL0, 6, 0, OP_JMP, 4, 0, OP_SHRX, 0, 33,
        OP_MUL, 0, 1, OP_SHRX, 0, 29, OP_XOR, 0, 2, OP_HALT, 0, 0,
    ];

    vm_run(&prog, prog.len() / 3, data, p, b, s)
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let input = if args.len() > 1 {
        args[1].clone()
    } else {
        "the quick brown fox".to_string()
    };
    let h = obf_hash(input.as_bytes());
    println!("{:016x}  {}", h, input);
}
