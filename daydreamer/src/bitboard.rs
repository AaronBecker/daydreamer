use board::*;
use uci::in_millis;

pub fn initialize() {
    static INIT: ::std::sync::Once = ::std::sync::Once::new();
    INIT.call_once(|| {
        let t1 = ::std::time::Instant::now();
        init_simple_bitboards();
        init_mundane_attacks();
        init_magic();
        init_pseudo_attacks();
        init_post_attack_bitboards();
        init_king_safety();
        println!("initialized in {} ms", in_millis(&t1.elapsed()));
    })
}

// TODO: look into performance implications of unchecked indexing
pub type Bitboard = u64;

pub trait IntoBitboard {
    fn into_bitboard(self) -> Bitboard;
}

impl IntoBitboard for Rank {
    fn into_bitboard(self) -> Bitboard {
        debug_assert!(self != Rank::NoRank);
        unsafe { RANK_BB[self.index()] }
    }
}

impl IntoBitboard for File {
    fn into_bitboard(self) -> Bitboard {
        debug_assert!(self != File::NoFile);
        unsafe { FILE_BB[self.index()] }
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

pub fn bb_to_str(b: Bitboard) -> String {
    let mut s = String::from("\n");
    for r in each_rank().rev() {
        for f in each_file() {
            if b & bb(sq(f, r)) != 0 {
                s.push('x');
            } else {
                s.push('.');
            }
        }
        s.push('\n');
    }
    s
}

pub fn bb_from_str(s: &str) -> Bitboard {
    let mut b: Bitboard = 0;
    let lines: Vec<&str> = s.split_whitespace().collect();
    for sq in each_square() {
        if lines[7 - sq.rank().index()].as_bytes()[sq.file().index()] == 'x' as u8 {
            b |= bb(sq);
        }
    }
    b
}

pub fn relative_rank_bb(c: Color, r: Rank) -> Bitboard {
    debug_assert!(r != Rank::NoRank);
    debug_assert!(c != Color::NoColor);
    r.relative_to(c).into_bitboard()
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

pub fn lsb(b: Bitboard) -> Square {
    debug_assert!(b != 0);
    Square::from_u8(b.trailing_zeros() as u8)
}

pub fn pop_square(b: &mut Bitboard) -> Square {
    let sq = lsb(*b);
    *b &= *b - 1;
    sq
}

static mut RANK_BB: [Bitboard; 8] = [0; 8];
static mut FILE_BB: [Bitboard; 8] = [0; 8];
static mut DISTANCE: [[u8; 64]; 64] = [[0; 64]; 64];

static mut NEIGHBOR_FILES_BB: [Bitboard; 8] = [0; 8];
static mut IN_FRONT_BB: [[Bitboard; 64]; 2] = [[0; 64]; 2];
static mut PASSER_BB: [[Bitboard; 64]; 2] = [[0; 64]; 2];
static mut OUTPOST_BB: [[Bitboard; 64]; 2] = [[0; 64]; 2];

static mut SQUARES_OF_COLOR_BB: [Bitboard; 2] = [0; 2];

fn init_simple_bitboards() {
    for i in 0..8 {
        unsafe {
            RANK_BB[i] = 0xff << (8 * i);
            FILE_BB[i] = 0x0101010101010101 << i;
        }
    }
    for i in 0..8 {
        unsafe {
            if i > 0 {
                NEIGHBOR_FILES_BB[i] |= FILE_BB[i - 1];
            }
            if i < 7 {
                NEIGHBOR_FILES_BB[i] |= FILE_BB[i + 1];
            }
        }
    }

    for sq1 in each_square() {
        let i = sq1.index();
        unsafe {
            SQUARES_OF_COLOR_BB[(sq1.file().index() + sq1.rank().index() + 1) & 1] |= bb!(sq1);
            let this_file = bb!(sq1.file());
            let neighbor_files = NEIGHBOR_FILES_BB[sq1.file().index()];
            for r in 0..sq1.rank().index() {
                PASSER_BB[1][i] |= RANK_BB[r];
            }
            for r in (sq1.rank().index() + 1)..8 {
                PASSER_BB[0][i] |= RANK_BB[r];
            }
            let near_files = this_file | neighbor_files;

            PASSER_BB[0][i] &= near_files;
            IN_FRONT_BB[0][i] = PASSER_BB[0][i] & this_file;
            OUTPOST_BB[0][i] = PASSER_BB[0][i] & neighbor_files;

            PASSER_BB[1][i] &= near_files;
            IN_FRONT_BB[1][i] = PASSER_BB[1][i] & this_file;
            OUTPOST_BB[1][i] = PASSER_BB[1][i] & neighbor_files;
        }
        for sq2 in each_square() {
            let j = sq2.index();
            let rd = sq1.rank() as i8 - sq2.rank() as i8;
            let fd = sq1.file() as i8 - sq2.file() as i8;
            unsafe {
                DISTANCE[i][j] = ::std::cmp::max(rd.abs(), fd.abs()) as u8;
            }
        }
    }
}

pub fn squares_of_color(sq: Square) -> Bitboard {
    unsafe { SQUARES_OF_COLOR_BB[(sq.file().index() + sq.rank().index() + 1) & 1] }
}

pub fn passer_mask(side: Color, sq: Square) -> Bitboard {
    debug_assert!(sq != Square::NoSquare);
    unsafe { PASSER_BB[side.index()][sq.index()] }
}

pub fn outpost_mask(side: Color, sq: Square) -> Bitboard {
    debug_assert!(sq != Square::NoSquare);
    unsafe { OUTPOST_BB[side.index()][sq.index()] }
}

pub fn in_front_mask(side: Color, sq: Square) -> Bitboard {
    debug_assert!(sq != Square::NoSquare);
    unsafe { IN_FRONT_BB[side.index()][sq.index()] }
}

pub fn neighbor_mask(f: File) -> Bitboard {
    unsafe { NEIGHBOR_FILES_BB[f.index()] }
}

pub fn dist(sq1: Square, sq2: Square) -> u8 {
    debug_assert!(sq1 != Square::NoSquare && sq2 != Square::NoSquare);
    unsafe { DISTANCE[sq1.index()][sq2.index()] }
}

static mut WHITE_PAWN_ATTACKS_BB: [Bitboard; 64] = [0; 64];
static mut BLACK_PAWN_ATTACKS_BB: [Bitboard; 64] = [0; 64];
static mut KNIGHT_ATTACKS_BB: [Bitboard; 64] = [0; 64];
static mut KING_ATTACKS_BB: [Bitboard; 64] = [0; 64];

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

fn init_mundane_attacks() {
    unsafe {
        init_mundane(&mut WHITE_PAWN_ATTACKS_BB, &[NORTHWEST, NORTHEAST]);
        init_mundane(&mut BLACK_PAWN_ATTACKS_BB, &[SOUTHWEST, SOUTHEAST]);
        init_mundane(
            &mut KNIGHT_ATTACKS_BB,
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
            &mut KING_ATTACKS_BB,
            &[
                NORTHWEST, NORTH, NORTHEAST, WEST, EAST, SOUTHWEST, SOUTH, SOUTHEAST,
            ],
        );
    }
}

static mut KING_NEAR_SHIELD_BB: [[Bitboard; 64]; 2] = [[0; 64]; 2];
static mut KING_SHIELD_BB: [[Bitboard; 64]; 2] = [[0; 64]; 2];
static mut KING_HALO_BB: [Bitboard; 64] = [0; 64];

fn init_king_safety() {
    unsafe {
        for sq in each_square() {
            for c in each_color() {
                let mut shield = king_attacks(sq);
                if (sq.rank() != Rank::_1 || c == Color::White)
                    && (sq.rank() != Rank::_8 || c == Color::Black)
                {
                    shield |= king_attacks(sq.pawn_push(c));
                }
                let far_shield = shield ^ king_attacks(sq) ^ bb!(sq);
                let near_shield = shift(far_shield, pawn_push(c.flip()));
                KING_SHIELD_BB[c.index()][sq.index()] = shield;
                KING_NEAR_SHIELD_BB[c.index()][sq.index()] = near_shield;
                KING_HALO_BB[sq.index()] = king_attacks(sq);
                if sq.rank() != Rank::_1 {
                    KING_HALO_BB[sq.index()] |= king_attacks(shift_sq(sq, SOUTH));
                }
                if sq.rank() != Rank::_8 {
                    KING_HALO_BB[sq.index()] |= king_attacks(shift_sq(sq, NORTH));
                }
                if sq.file() != File::A {
                    KING_HALO_BB[sq.index()] |= king_attacks(shift_sq(sq, WEST));
                }
                if sq.file() != File::H {
                    KING_HALO_BB[sq.index()] |= king_attacks(shift_sq(sq, EAST));
                }
            }
        }
    }
}

pub fn king_shield(us: Color, sq: Square) -> Bitboard {
    unsafe { KING_SHIELD_BB[us.index()][sq.index()] }
}

pub fn king_near_shield(us: Color, sq: Square) -> Bitboard {
    unsafe { KING_NEAR_SHIELD_BB[us.index()][sq.index()] }
}

pub fn king_halo(sq: Square) -> Bitboard {
    unsafe { KING_HALO_BB[sq.index()] }
}

static mut BISHOP_MASKS: [Bitboard; 64] = [0; 64];
static mut BISHOP_MAGIC: [Bitboard; 64] = [0; 64];
static mut BISHOP_ATTACKS_BB: [[Bitboard; 512]; 64] = [[0; 512]; 64];

static mut ROOK_MASKS: [Bitboard; 64] = [0; 64];
static mut ROOK_MAGIC: [Bitboard; 64] = [0; 64];
static mut ROOK_ATTACKS_BB: [[Bitboard; 4096]; 64] = [[0; 4096]; 64];

fn magic_bishop_index(sq: Square, mut occ: Bitboard) -> usize {
    debug_assert!(sq != Square::NoSquare);
    unsafe {
        occ &= BISHOP_MASKS[sq.index()];
        occ = occ.wrapping_mul(BISHOP_MAGIC[sq.index()]);
    }
    (occ >> 55) as usize
}

fn magic_rook_index(sq: Square, mut occ: Bitboard) -> usize {
    debug_assert!(sq != Square::NoSquare);
    unsafe {
        occ &= ROOK_MASKS[sq.index()];
        occ = occ.wrapping_mul(ROOK_MAGIC[sq.index()]);
    }
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
) -> bool {
    ::std::intrinsics::write_bytes(&mut BISHOP_ATTACKS_BB[sq.index()][0], 0, 512);
    for i in 0..size {
        let att: *mut Bitboard =
            &mut BISHOP_ATTACKS_BB[sq.index()][magic_bishop_index(sq, occ[i as usize])];
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
) -> bool {
    ::std::intrinsics::write_bytes(&mut ROOK_ATTACKS_BB[sq.index()][0], 0, 4096);
    for i in 0..size {
        let att: *mut Bitboard =
            &mut ROOK_ATTACKS_BB[sq.index()][magic_rook_index(sq, occ[i as usize])];
        if *att != 0 && *att != gold[i as usize] {
            return false;
        }
        *att = gold[i as usize]
    }
    true
}

pub fn optimize_rook_seed() {
    init_simple_bitboards();
    init_mundane_attacks();
    let mut seed = 35000;
    let mut best_time: u64;
    unsafe {
        best_time = init_magic_opt(PieceType::Rook, 8452, u64::max_value());
    }
    println!("starting optimization...");
    loop {
        unsafe {
            let t = init_magic_opt(PieceType::Rook, seed, best_time);
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

pub fn optimize_bishop_seed() {
    init_simple_bitboards();
    init_mundane_attacks();
    let mut seed = 0;
    let mut best_time: u64 = u64::max_value();
    println!("starting optimization...");
    loop {
        unsafe {
            let t = init_magic_opt(PieceType::Bishop, seed, best_time);
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

fn init_magic() {
    // We cheat on initialization time by choosing rng seeds that are known to
    // find conforming magic numbers quickly. This doesn't matter much for real
    // applications, but it makes the edit/compile/test cycle much faster--it's
    // mostly a feature for my own convenience in development, so the fact that
    // the benefits don't necessarily translate across systems doesn't matter.
    // I tested Seed values up to 100k.
    unsafe {
        init_magic_opt(PieceType::Bishop, 17337, u64::max_value());
    }
    unsafe {
        init_magic_opt(PieceType::Rook, 8452, u64::max_value());
    }
}

unsafe fn init_magic_opt(pt: PieceType, xseed: usize, best_time: u64) -> u64 {
    let t1 = ::std::time::Instant::now();
    let mut occ: [Bitboard; 4096] = [0; 4096];
    let mut gold: [Bitboard; 4096] = [0; 4096];
    let masks = if pt == PieceType::Bishop {
        &mut BISHOP_MASKS
    } else {
        &mut ROOK_MASKS
    };
    let magic = if pt == PieceType::Bishop {
        &mut BISHOP_MAGIC
    } else {
        &mut ROOK_MAGIC
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
        // See http://chessprogramming.wikispaces.com/Traversing+Subsets+of+a+Set
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
            if attack_fn(sq, size, &occ, &gold) {
                break;
            }
        }
    }
    in_millis(&t1.elapsed())
}

static mut BISHOP_PSEUDO_ATTACKS_BB: [Bitboard; 64] = [0; 64];
static mut ROOK_PSEUDO_ATTACKS_BB: [Bitboard; 64] = [0; 64];
static mut QUEEN_PSEUDO_ATTACKS_BB: [Bitboard; 64] = [0; 64];

fn init_pseudo_attacks() {
    for sq in each_square() {
        unsafe {
            BISHOP_PSEUDO_ATTACKS_BB[sq.index()] = bishop_attacks(sq, 0);
            ROOK_PSEUDO_ATTACKS_BB[sq.index()] = rook_attacks(sq, 0);
            QUEEN_PSEUDO_ATTACKS_BB[sq.index()] = queen_attacks(sq, 0);
        }
    }
}

pub fn bishop_pseudo_attacks(sq: Square) -> Bitboard {
    unsafe { BISHOP_PSEUDO_ATTACKS_BB[sq.index()] }
}

pub fn rook_pseudo_attacks(sq: Square) -> Bitboard {
    unsafe { ROOK_PSEUDO_ATTACKS_BB[sq.index()] }
}

pub fn queen_pseudo_attacks(sq: Square) -> Bitboard {
    unsafe { QUEEN_PSEUDO_ATTACKS_BB[sq.index()] }
}

static mut RAYS_BB: [[Bitboard; 64]; 64] = [[0; 64]; 64];
static mut BETWEEN_BB: [[Bitboard; 64]; 64] = [[0; 64]; 64];

fn init_post_attack_bitboards() {
    for sq1 in each_square() {
        for sq2 in each_square() {
            if queen_pseudo_attacks(sq1) & bb(sq2) == 0 {
                continue;
            }
            if bishop_pseudo_attacks(sq1) & bb(sq2) != 0 {
                unsafe {
                    RAYS_BB[sq2.index()][sq1.index()] =
                        bishop_pseudo_attacks(sq1) & bishop_pseudo_attacks(sq2) | bb(sq1) | bb(sq2);
                }
            } else {
                unsafe {
                    RAYS_BB[sq2.index()][sq1.index()] =
                        rook_pseudo_attacks(sq1) & rook_pseudo_attacks(sq2) | bb(sq1) | bb(sq2);
                }
            }
            let d = direction(sq1, sq2);
            let mut sq3 = shift_sq(sq1, d);
            while sq3 != sq2 {
                unsafe {
                    BETWEEN_BB[sq1.index()][sq2.index()] |= bb(sq3);
                }
                sq3 = shift_sq(sq3, d);
            }
        }
    }
}

pub fn between(sq1: Square, sq2: Square) -> Bitboard {
    debug_assert!(sq1 != Square::NoSquare && sq2 != Square::NoSquare);
    unsafe { BETWEEN_BB[sq1.index()][sq2.index()] }
}

pub fn ray(sq1: Square, sq2: Square) -> Bitboard {
    debug_assert!(sq1 != Square::NoSquare && sq2 != Square::NoSquare);
    unsafe { RAYS_BB[sq1.index()][sq2.index()] }
}

pub fn king_attacks(sq: Square) -> Bitboard {
    unsafe { KING_ATTACKS_BB[sq.index()] }
}

pub fn knight_attacks(sq: Square) -> Bitboard {
    unsafe { KNIGHT_ATTACKS_BB[sq.index()] }
}

pub fn white_pawn_attacks(sq: Square) -> Bitboard {
    unsafe { WHITE_PAWN_ATTACKS_BB[sq.index()] }
}

pub fn black_pawn_attacks(sq: Square) -> Bitboard {
    unsafe { BLACK_PAWN_ATTACKS_BB[sq.index()] }
}

pub fn pawn_attacks(c: Color, sq: Square) -> Bitboard {
    debug_assert!(c != Color::NoColor);
    if c == Color::White {
        white_pawn_attacks(sq)
    } else {
        black_pawn_attacks(sq)
    }
}

pub fn bishop_attacks(sq: Square, occ: Bitboard) -> Bitboard {
    unsafe { BISHOP_ATTACKS_BB[sq.index()][magic_bishop_index(sq, occ)] }
}

pub fn rook_attacks(sq: Square, occ: Bitboard) -> Bitboard {
    unsafe { ROOK_ATTACKS_BB[sq.index()][magic_rook_index(sq, occ)] }
}

pub fn queen_attacks(sq: Square, occ: Bitboard) -> Bitboard {
    rook_attacks(sq, occ) | bishop_attacks(sq, occ)
}

