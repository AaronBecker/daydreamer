use std::fmt;
use std::io::Write;
use std::time;

use board::*;

pub fn in_millis(d: &time::Duration) -> u64 {
    1 + d.as_secs() * 1000 + d.subsec_nanos() as u64 / 1_000_000
}

pub fn write_bitboards(mut f: std::fs::File) {
    let t1 = ::std::time::Instant::now();
    let mut tables = Tables::new();
    init_simple_bitboards(&mut tables);
    init_mundane_attacks(&mut tables);
    init_magic(&mut tables);
    init_pseudo_attacks(&mut tables);
    init_post_attack_bitboards(&mut tables);
    init_king_safety(&mut tables);
    println!("initialized in {} ms", in_millis(&t1.elapsed()));
    f.write_all(format!("{}", tables).as_bytes()).unwrap();
}

// TODO: look into performance implications of unchecked indexing
pub type Bitboard = u64;

pub trait IntoBitboard {
    fn into_bitboard(self) -> Bitboard;
}

static RANK_BB: [Bitboard; 8] = [
    0xff,
    0xff << 8,
    0xff << 16,
    0xff << 24,
    0xff << 32,
    0xff << 40,
    0xff << 48,
    0xff << 56,
];

static FILE_BB: [Bitboard; 8] = [
    0x0101010101010101,
    0x0101010101010101 << 1,
    0x0101010101010101 << 2,
    0x0101010101010101 << 3,
    0x0101010101010101 << 4,
    0x0101010101010101 << 5,
    0x0101010101010101 << 6,
    0x0101010101010101 << 7,
];

impl IntoBitboard for Rank {
    fn into_bitboard(self) -> Bitboard {
        debug_assert!(self != Rank::NoRank);
        RANK_BB[self.index()]
    }
}

impl IntoBitboard for File {
    fn into_bitboard(self) -> Bitboard {
        debug_assert!(self != File::NoFile);
        FILE_BB[self.index()]
    }
}

impl IntoBitboard for Square {
    fn into_bitboard(self) -> Bitboard {
        1 << self.index()
    }
}

pub fn bb<T: IntoBitboard>(x: T) -> Bitboard {
    x.into_bitboard()
}

fn direction(sq1: Square, sq2: Square) -> Delta {
    let mut d: Delta = 0;
    let (r1, r2) = (sq1.rank() as u8, sq2.rank() as u8);
    if r1 < r2 {
        d += NORTH;
    } else if r1 > r2 {
        d += SOUTH;
    }

    let (f1, f2) = (sq1.file() as u8, sq2.file() as u8);
    if f1 < f2 {
        d += EAST;
    } else if f1 > f2 {
        d += WEST;
    }
    d
}

pub fn shift(b: Bitboard, d: Delta) -> Bitboard {
    const NN: Delta = NORTH + NORTH;
    const SS: Delta = SOUTH + SOUTH;
    match d {
        NORTH => b << 8,
        NN => b << 16,
        SOUTH => b >> 8,
        SS => b >> 16,
        NORTHEAST => (b & !bb(File::H)) << 9,
        SOUTHEAST => (b & !bb(File::H)) >> 7,
        NORTHWEST => (b & !bb(File::A)) << 7,
        SOUTHWEST => (b & !bb(File::A)) >> 9,
        _ => panic!("unexpected shift"),
    }
}

struct Tables {
    rank_bb: [Bitboard; 8],
    file_bb: [Bitboard; 8],
    distance: [[u8; 64]; 64],
    neighbor_files_bb: [Bitboard; 8],
    in_front_bb: [[Bitboard; 64]; 2],
    passer_bb: [[Bitboard; 64]; 2],
    outpost_bb: [[Bitboard; 64]; 2],
    squares_of_color_bb: [Bitboard; 2],

    white_pawn_attacks_bb: [Bitboard; 64],
    black_pawn_attacks_bb: [Bitboard; 64],
    knight_attacks_bb: [Bitboard; 64],
    king_attacks_bb: [Bitboard; 64],

    king_near_shield_bb: [[Bitboard; 64]; 2],
    king_shield_bb: [[Bitboard; 64]; 2],
    king_halo_bb: [Bitboard; 64],

    bishop_masks: [Bitboard; 64],
    bishop_magic: [Bitboard; 64],
    bishop_attacks_bb: [[Bitboard; 512]; 64],

    rook_masks: [Bitboard; 64],
    rook_magic: [Bitboard; 64],
    rook_attacks_bb: [[Bitboard; 4096]; 64],

    bishop_pseudo_attacks_bb: [Bitboard; 64],
    rook_pseudo_attacks_bb: [Bitboard; 64],
    queen_pseudo_attacks_bb: [Bitboard; 64],

    rays_bb: [[Bitboard; 64]; 64],
    between_bb: [[Bitboard; 64]; 64],
}

impl Tables {
    pub fn new() -> Tables {
        Tables {
            rank_bb: [0; 8],
            file_bb: [0; 8],
            distance: [[0; 64]; 64],
            neighbor_files_bb: [0; 8],
            in_front_bb: [[0; 64]; 2],
            passer_bb: [[0; 64]; 2],
            outpost_bb: [[0; 64]; 2],
            squares_of_color_bb: [0; 2],
            white_pawn_attacks_bb: [0; 64],
            black_pawn_attacks_bb: [0; 64],
            knight_attacks_bb: [0; 64],
            king_attacks_bb: [0; 64],
            king_near_shield_bb: [[0; 64]; 2],
            king_shield_bb: [[0; 64]; 2],
            king_halo_bb: [0; 64],
            bishop_masks: [0; 64],
            bishop_magic: [0; 64],
            bishop_attacks_bb: [[0; 512]; 64],
            rook_masks: [0; 64],
            rook_magic: [0; 64],
            rook_attacks_bb: [[0; 4096]; 64],
            bishop_pseudo_attacks_bb: [0; 64],
            rook_pseudo_attacks_bb: [0; 64],
            queen_pseudo_attacks_bb: [0; 64],
            rays_bb: [[0; 64]; 64],
            between_bb: [[0; 64]; 64],
        }
    }
}

struct BB8([Bitboard; 8]);
impl fmt::Display for BB8 {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut buf = String::from("[");
        for i in 0..8 {
            buf.push_str(&format!("{},", self.0[i]));
        }
        buf.push_str("]");
        write!(f, "{}", buf)
    }
}

struct BB64([Bitboard; 64]);
impl fmt::Display for BB64 {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut buf = String::from("[");
        for i in 0..64 {
            buf.push_str(&format!("{},", self.0[i]));
        }
        buf.push_str("]");
        write!(f, "{}", buf)
    }
}

struct BB64x2([[Bitboard; 64]; 2]);
impl fmt::Display for BB64x2 {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut buf = String::from("[");
        for i in 0..2 {
            buf.push_str(&format!("{},", BB64(self.0[i])));
        }
        buf.push_str("]");
        write!(f, "{}", buf)
    }
}

struct BB64x64([[Bitboard; 64]; 64]);
impl fmt::Display for BB64x64 {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut buf = String::from("[");
        for i in 0..64 {
            buf.push_str(&format!("{},", BB64(self.0[i])));
        }
        buf.push_str("]");
        write!(f, "{}", buf)
    }
}

impl fmt::Display for Tables {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut buf = format!(
            "static RANK_BB: [Bitboard; 8] = {};\n",
            BB8(self.rank_bb)
        );
        buf.push_str(&format!(
            "static FILE_BB: [Bitboard; 8] = {};\n",
            BB8(self.file_bb)
        ));
        buf.push_str(&format!(
            "static NEIGHBOR_FILES_BB: [Bitboard; 8] = {};\n",
            BB8(self.neighbor_files_bb)
        ));
        // DISTANCE
        {
            let mut dist = String::new();
            for i in 0..64 {
                dist.push_str("[");
                for j in 0..64 {
                    dist.push_str(&format!("{},", self.distance[i][j]));
                }
                dist.push_str("],");
            }
            buf.push_str(&format!("static DISTANCE: [[u8; 64]; 64] = [{}];\n", dist));
        }
        buf.push_str(&format!(
            "static IN_FRONT_BB: [[Bitboard; 64]; 2] = {};\n",
            BB64x2(self.in_front_bb)));
        buf.push_str(&format!(
            "static PASSER_BB: [[Bitboard; 64]; 2] = {};\n",
            BB64x2(self.passer_bb)));
        buf.push_str(&format!(
            "static OUTPOST_BB: [[Bitboard; 64]; 2] = {};\n",
            BB64x2(self.outpost_bb)));
        buf.push_str(&format!(
            "static SQUARES_OF_COLOR_BB: [Bitboard; 2] = [{}, {}];\n",
            self.squares_of_color_bb[0], self.squares_of_color_bb[1]));
        buf.push_str(&format!(
            "static WHITE_PAWN_ATTACKS_BB: [Bitboard; 64] = {};\n",
            BB64(self.white_pawn_attacks_bb)));
        buf.push_str(&format!(
            "static BLACK_PAWN_ATTACKS_BB: [Bitboard; 64] = {};\n",
            BB64(self.black_pawn_attacks_bb)));
        buf.push_str(&format!(
            "static KNIGHT_ATTACKS_BB: [Bitboard; 64] = {};\n",
            BB64(self.knight_attacks_bb)));
        buf.push_str(&format!(
            "static KING_ATTACKS_BB: [Bitboard; 64] = {};\n",
            BB64(self.king_attacks_bb)));
        buf.push_str(&format!(
            "static KING_NEAR_SHIELD_BB: [[Bitboard; 64]; 2] = {};\n",
            BB64x2(self.king_near_shield_bb)));
        buf.push_str(&format!(
            "static KING_SHIELD_BB: [[Bitboard; 64]; 2] = {};\n",
            BB64x2(self.king_shield_bb)));
        buf.push_str(&format!(
            "static KING_HALO_BB: [Bitboard; 64] = {};\n",
            BB64(self.king_halo_bb)));
        buf.push_str(&format!(
            "static BISHOP_MASKS: [Bitboard; 64] = {};\n",
            BB64(self.bishop_masks)));
        buf.push_str(&format!(
            "static BISHOP_MAGIC: [Bitboard; 64] = {};\n",
            BB64(self.bishop_magic)));
        buf.push_str(&format!(
            "static ROOK_MASKS: [Bitboard; 64] = {};\n",
            BB64(self.rook_masks)));
        buf.push_str(&format!(
            "static ROOK_MAGIC: [Bitboard; 64] = {};\n",
            BB64(self.rook_magic)));
        // BISHOP_ATTACKS_BB
        {
            let mut atk = String::new();
            for i in 0..64 {
                atk.push_str("[");
                for j in 0..512 {
                    atk.push_str(&format!("{},", self.bishop_attacks_bb[i][j]));
                }
                atk.push_str("],");
            }
            buf.push_str(&format!("static BISHOP_ATTACKS_BB: [[Bitboard; 512]; 64] = [{}];\n", atk));
        }
        // ROOK_ATTACKS_BB
        {
            let mut atk = String::new();
            for i in 0..64 {
                atk.push_str("[");
                for j in 0..4096 {
                    atk.push_str(&format!("{},", self.rook_attacks_bb[i][j]));
                }
                atk.push_str("],");
            }
            buf.push_str(&format!("static ROOK_ATTACKS_BB: [[Bitboard; 4096]; 64] = [{}];\n", atk));
        }
        buf.push_str(&format!(
            "static BISHOP_PSEUDO_ATTACKS_BB: [Bitboard; 64] = {};\n",
            BB64(self.bishop_pseudo_attacks_bb)));
        buf.push_str(&format!(
            "static ROOK_PSEUDO_ATTACKS_BB: [Bitboard; 64] = {};\n",
            BB64(self.rook_pseudo_attacks_bb)));
        buf.push_str(&format!(
            "static QUEEN_PSEUDO_ATTACKS_BB: [Bitboard; 64] = {};\n",
            BB64(self.queen_pseudo_attacks_bb)));
        buf.push_str(&format!(
            "static RAYS_BB: [[Bitboard; 64]; 64] = {};\n",
            BB64x64(self.rays_bb)));
        buf.push_str(&format!(
            "static BETWEEN_BB: [[Bitboard; 64]; 64] = {};\n",
            BB64x64(self.between_bb)));

        write!(f, "{}", buf)
    }
}

fn init_simple_bitboards(tables: &mut Tables) {
    for i in 0..8 {
        tables.rank_bb[i] = 0xff << (8 * i);
        tables.file_bb[i] = 0x0101010101010101 << i;
    }
    for i in 0..8 {
        if i > 0 {
            tables.neighbor_files_bb[i] |= tables.file_bb[i - 1];
        }
        if i < 7 {
            tables.neighbor_files_bb[i] |= tables.file_bb[i + 1];
        }
    }

    for sq1 in each_square() {
        let i = sq1.index();
        tables.squares_of_color_bb[(sq1.file().index() + sq1.rank().index() + 1) & 1] |=
            sq1.into_bitboard();
        let this_file = sq1.file().into_bitboard();
        let neighbor_files = tables.neighbor_files_bb[sq1.file().index()];
        for r in 0..sq1.rank().index() {
            tables.passer_bb[1][i] |= tables.rank_bb[r];
        }
        for r in (sq1.rank().index() + 1)..8 {
            tables.passer_bb[0][i] |= tables.rank_bb[r];
        }
        let near_files = this_file | neighbor_files;

        tables.passer_bb[0][i] &= near_files;
        tables.in_front_bb[0][i] = tables.passer_bb[0][i] & this_file;
        tables.outpost_bb[0][i] = tables.passer_bb[0][i] & neighbor_files;

        tables.passer_bb[1][i] &= near_files;
        tables.in_front_bb[1][i] = tables.passer_bb[1][i] & this_file;
        tables.outpost_bb[1][i] = tables.passer_bb[1][i] & neighbor_files;
        for sq2 in each_square() {
            let j = sq2.index();
            let rd = sq1.rank() as i8 - sq2.rank() as i8;
            let fd = sq1.file() as i8 - sq2.file() as i8;
            tables.distance[i][j] = ::std::cmp::max(rd.abs(), fd.abs()) as u8;
        }
    }
}

pub fn dist(sq1: Square, sq2: Square) -> u8 {
    let rd = sq1.rank() as i8 - sq2.rank() as i8;
    let fd = sq1.file() as i8 - sq2.file() as i8;
    ::std::cmp::max(rd.abs(), fd.abs()) as u8
}

fn init_mundane(attacks_bb: &mut [Bitboard; 64], deltas: &[Delta]) {
    for sq1 in each_square() {
        for d in deltas.iter() {
            let sq2: Square = shift_sq(sq1, *d);
            // Illegal shifts that wrap around the board imply a distance >= 3.
            // Legal shifts are all distance 2 or less.
            if sq2 != Square::NoSquare && dist(sq1, sq2) < 3 {
                attacks_bb[sq1.index()] |= bb(sq2);
            }
        }
    }
}

fn init_mundane_attacks(tables: &mut Tables) {
    init_mundane(&mut tables.white_pawn_attacks_bb, &[NORTHWEST, NORTHEAST]);
    init_mundane(&mut tables.black_pawn_attacks_bb, &[SOUTHWEST, SOUTHEAST]);
    init_mundane(
        &mut tables.knight_attacks_bb,
        &[
            NORTH + NORTHWEST,
            NORTH + NORTHEAST,
            WEST + NORTHWEST,
            WEST + SOUTHWEST,
            EAST + NORTHEAST,
            EAST + SOUTHEAST,
            SOUTH + SOUTHEAST,
            SOUTH + SOUTHWEST,
        ],
    );
    init_mundane(
        &mut tables.king_attacks_bb,
        &[
            NORTHWEST, NORTH, NORTHEAST, WEST, EAST, SOUTHWEST, SOUTH, SOUTHEAST,
        ],
    );
}

fn init_king_safety(tables: &mut Tables) {
    for sq in each_square() {
        for c in each_color() {
            let mut shield = king_attacks(sq, tables);
            if (sq.rank() != Rank::_1 || c == Color::White)
                && (sq.rank() != Rank::_8 || c == Color::Black)
            {
                shield |= king_attacks(sq.pawn_push(c), tables);
            }
            let far_shield = shield ^ king_attacks(sq, tables) ^ sq.into_bitboard();
            let near_shield = shift(far_shield, pawn_push(c.flip()));
            tables.king_shield_bb[c.index()][sq.index()] = shield;
            tables.king_near_shield_bb[c.index()][sq.index()] = near_shield;
            tables.king_halo_bb[sq.index()] = king_attacks(sq, tables);
            if sq.rank() != Rank::_1 {
                tables.king_halo_bb[sq.index()] |= king_attacks(shift_sq(sq, SOUTH), tables);
            }
            if sq.rank() != Rank::_8 {
                tables.king_halo_bb[sq.index()] |= king_attacks(shift_sq(sq, NORTH), tables);
            }
            if sq.file() != File::A {
                tables.king_halo_bb[sq.index()] |= king_attacks(shift_sq(sq, WEST), tables);
            }
            if sq.file() != File::H {
                tables.king_halo_bb[sq.index()] |= king_attacks(shift_sq(sq, EAST), tables);
            }
        }
    }
}

fn magic_bishop_index(
    sq: Square,
    mut occ: Bitboard,
    bishop_masks: &[Bitboard],
    bishop_magic: &[Bitboard],
) -> usize {
    debug_assert!(sq != Square::NoSquare);
    occ &= bishop_masks[sq.index()];
    occ = occ.wrapping_mul(bishop_magic[sq.index()]);
    (occ >> 55) as usize
}

fn magic_rook_index(
    sq: Square,
    mut occ: Bitboard,
    rook_masks: &[Bitboard],
    rook_magic: &[Bitboard],
) -> usize {
    debug_assert!(sq != Square::NoSquare);
    occ &= rook_masks[sq.index()];
    occ = occ.wrapping_mul(rook_magic[sq.index()]);
    (occ >> 52) as usize
}

fn slide_mask(sq: Square, occ: Bitboard, deltas: &[Delta]) -> Bitboard {
    debug_assert!(sq != Square::NoSquare);
    let mut mask: Bitboard = 0;
    for d in deltas.iter() {
        let (mut sq2, mut old_sq2) = (sq, sq);
        while sq2 != Square::NoSquare && dist(sq2, old_sq2) <= 1 {
            mask |= bb(sq2);
            if bb(sq2) & occ != 0 {
                break;
            }
            old_sq2 = sq2;
            sq2 = shift_sq(sq2, *d);
        }
    }
    mask & !bb(sq)
}

pub fn bishop_slide_mask(sq: Square, occ: Bitboard) -> Bitboard {
    slide_mask(sq, occ, &[NORTHEAST, SOUTHEAST, NORTHWEST, SOUTHWEST])
}

fn rook_slide_mask(sq: Square, occ: Bitboard) -> Bitboard {
    slide_mask(sq, occ, &[NORTH, SOUTH, EAST, WEST])
}

unsafe fn init_bishop_attacks(
    sq: Square,
    size: usize,
    occ: &[Bitboard; 4096],
    gold: &[Bitboard; 4096],
    bishop_masks: &[Bitboard],
    bishop_magic: &[Bitboard],
    bishop_attacks_bb: *mut Bitboard,
) -> bool {
    ::std::intrinsics::write_bytes(bishop_attacks_bb.wrapping_add(sq.index() * 512), 0, 512);
    for i in 0..size {
        let att: *mut Bitboard = bishop_attacks_bb.wrapping_add(
            sq.index() * 512 + magic_bishop_index(sq, occ[i as usize], bishop_masks, bishop_magic),
        );
        if *att != 0 && *att != gold[i as usize] {
            return false;
        }
        *att = gold[i as usize]
    }
    true
}

unsafe fn init_rook_attacks(
    sq: Square,
    size: usize,
    occ: &[Bitboard; 4096],
    gold: &[Bitboard; 4096],
    rook_masks: &[Bitboard],
    rook_magic: &[Bitboard],
    rook_attacks_bb: *mut Bitboard,
) -> bool {
    ::std::intrinsics::write_bytes(rook_attacks_bb.wrapping_add(sq.index() * 4096), 0, 4096);
    for i in 0..size {
        let att: *mut Bitboard = rook_attacks_bb.wrapping_add(
            sq.index() * 4096 + magic_rook_index(sq, occ[i as usize], rook_masks, rook_magic),
        );
        if *att != 0 && *att != gold[i as usize] {
            return false;
        }
        *att = gold[i as usize]
    }
    true
}

#[allow(dead_code)]
pub fn optimize_rook_seed() {
    let mut tables = Tables::new();
    init_simple_bitboards(&mut tables);
    init_mundane_attacks(&mut tables);
    let mut seed = 35000;
    let mut best_time: u64;
    unsafe {
        best_time = init_magic_opt(&mut tables, PieceType::Rook, 8452, u64::max_value());
    }
    println!("starting optimization...");
    loop {
        unsafe {
            let t = init_magic_opt(&mut tables, PieceType::Rook, seed, best_time);
            if t < best_time {
                best_time = t;
                println!("\nnew best seed: {}, {}ms", seed, best_time / 1000 / 1000);
            }
        }
        seed += 1;
        if seed % 500 == 0 {
            println!("{}", seed);
        }
    }
}

#[allow(dead_code)]
pub fn optimize_bishop_seed() {
    let mut tables = Tables::new();
    init_simple_bitboards(&mut tables);
    init_mundane_attacks(&mut tables);
    let mut seed = 0;
    let mut best_time: u64 = u64::max_value();
    println!("starting optimization...");
    loop {
        unsafe {
            let t = init_magic_opt(&mut tables, PieceType::Bishop, seed, best_time);
            if t < best_time {
                best_time = t;
                println!("\nnew best seed: {}, {}ms", seed, best_time / 1000 / 1000);
            }
        }
        seed += 1;
        if seed % 500 == 0 {
            println!("{}", seed);
        }
    }
}

fn init_magic(tables: &mut Tables) {
    // We cheat on initialization time by choosing rng seeds that are known to
    // find conforming magic numbers quickly. This doesn't matter much for real
    // applications, but it makes the edit/compile/test cycle much faster--it's
    // mostly a feature for my own convenience in development, so the fact that
    // the benefits don't necessarily translate across systems doesn't matter.
    // I tested Seed values up to 100k.
    unsafe {
        init_magic_opt(tables, PieceType::Bishop, 17337, u64::max_value());
    }
    unsafe {
        init_magic_opt(tables, PieceType::Rook, 8452, u64::max_value());
    }
}

unsafe fn init_magic_opt(tables: &mut Tables, pt: PieceType, xseed: usize, best_time: u64) -> u64 {
    let t1 = ::std::time::Instant::now();
    let mut occ: [Bitboard; 4096] = [0; 4096];
    let mut gold: [Bitboard; 4096] = [0; 4096];
    let attacks_bb = if pt == PieceType::Bishop {
        tables.bishop_attacks_bb[0].as_mut_ptr()
    } else {
        tables.rook_attacks_bb[0].as_mut_ptr()
    };
    let masks = if pt == PieceType::Bishop {
        &mut tables.bishop_masks
    } else {
        &mut tables.rook_masks
    };
    let magic = if pt == PieceType::Bishop {
        &mut tables.bishop_magic
    } else {
        &mut tables.rook_magic
    };
    let mask_fn = if pt == PieceType::Bishop {
        bishop_slide_mask
    } else {
        rook_slide_mask
    };
    let attack_fn = if pt == PieceType::Bishop {
        init_bishop_attacks
    } else {
        init_rook_attacks
    };

    use rand::{Rng, SeedableRng, StdRng};
    let seed: &[_] = &[xseed];
    let mut prng: StdRng = SeedableRng::from_seed(seed);
    for sq in each_square() {
        let rank_mask = (bb(Rank::_1) | bb(Rank::_8)) & !bb(sq.rank());
        let file_mask = (bb(File::A) | bb(File::H)) & !bb(sq.file());
        masks[sq.index()] = mask_fn(sq, 0) & !(rank_mask | file_mask);

        // Each subset of masks[sq] is a possible occupancy mask that we must
        // handle. Enumerate them and store both the occupancy and the reference
        // attack set that we want to generate for that occupancy.
        // See http://chessprogramming.org/Traversing+Subsets+of+a+Set
        let (mut size, mut subset): (usize, Bitboard) = (0, 0);
        while size == 0 || subset != 0 {
            occ[size] = subset;
            gold[size] = mask_fn(sq, subset);
            subset = subset.wrapping_sub(masks[sq.index()]) & masks[sq.index()];
            size += 1;
        }

        // Find a magic number that works by trial and error.
        loop {
            let elapsed_ms = in_millis(&t1.elapsed());
            if elapsed_ms > best_time {
                return u64::max_value();
            }
            magic[sq.index()] = prng.gen::<u64>() & prng.gen::<u64>() & prng.gen::<u64>();
            if (magic[sq.index()].wrapping_mul(masks[sq.index()]) >> 56).count_ones() < 6 {
                continue;
            }
            if attack_fn(sq, size, &occ, &gold, masks, magic, attacks_bb) {
                break;
            }
        }
    }
    in_millis(&t1.elapsed())
}

fn init_pseudo_attacks(tables: &mut Tables) {
    for sq in each_square() {
        tables.bishop_pseudo_attacks_bb[sq.index()] = bishop_attacks(sq, 0, tables);
        tables.rook_pseudo_attacks_bb[sq.index()] = rook_attacks(sq, 0, tables);
        tables.queen_pseudo_attacks_bb[sq.index()] = queen_attacks(sq, 0, tables);
    }
}

fn bishop_pseudo_attacks(sq: Square, tables: &Tables) -> Bitboard {
    tables.bishop_pseudo_attacks_bb[sq.index()]
}

fn rook_pseudo_attacks(sq: Square, tables: &Tables) -> Bitboard {
    tables.rook_pseudo_attacks_bb[sq.index()]
}

fn queen_pseudo_attacks(sq: Square, tables: &Tables) -> Bitboard {
    tables.queen_pseudo_attacks_bb[sq.index()]
}

fn init_post_attack_bitboards(tables: &mut Tables) {
    for sq1 in each_square() {
        for sq2 in each_square() {
            if queen_pseudo_attacks(sq1, tables) & bb(sq2) == 0 {
                continue;
            }
            if bishop_pseudo_attacks(sq1, tables) & bb(sq2) != 0 {
                tables.rays_bb[sq2.index()][sq1.index()] = bishop_pseudo_attacks(sq1, tables)
                    & bishop_pseudo_attacks(sq2, tables)
                    | bb(sq1)
                    | bb(sq2);
            } else {
                tables.rays_bb[sq2.index()][sq1.index()] = rook_pseudo_attacks(sq1, tables)
                    & rook_pseudo_attacks(sq2, tables)
                    | bb(sq1)
                    | bb(sq2);
            }
            let d = direction(sq1, sq2);
            let mut sq3 = shift_sq(sq1, d);
            while sq3 != sq2 {
                tables.between_bb[sq1.index()][sq2.index()] |= bb(sq3);
                sq3 = shift_sq(sq3, d);
            }
        }
    }
}

fn king_attacks(sq: Square, tables: &Tables) -> Bitboard {
    tables.king_attacks_bb[sq.index()]
}

fn bishop_attacks(sq: Square, occ: Bitboard, tables: &Tables) -> Bitboard {
    tables.bishop_attacks_bb[sq.index()]
        [magic_bishop_index(sq, occ, &tables.bishop_masks, &tables.bishop_magic)]
}

fn rook_attacks(sq: Square, occ: Bitboard, tables: &Tables) -> Bitboard {
    tables.rook_attacks_bb[sq.index()]
        [magic_rook_index(sq, occ, &tables.rook_masks, &tables.rook_magic)]
}

fn queen_attacks(sq: Square, occ: Bitboard, tables: &Tables) -> Bitboard {
    rook_attacks(sq, occ, tables) | bishop_attacks(sq, occ, tables)
}
