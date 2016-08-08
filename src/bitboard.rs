use board::*;

pub fn initialize() {
    static INIT: ::std::sync::Once = ::std::sync::ONCE_INIT;
    INIT.call_once(|| {
        let t1 = ::time::precise_time_ns();
        init_simple_bitboards();
        init_mundane_attacks();
        init_magic();
        init_pseudo_attacks();
        init_post_attack_bitboards();
        let t2 = ::time::precise_time_ns();
        println!("initialized in {} ms", (t2 -t1) / 1_000_000);
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
        unsafe { rank_bb[self.index()] }
    }
}

impl IntoBitboard for File {
    fn into_bitboard(self) -> Bitboard {
        debug_assert!(self != File::NoFile);
        unsafe { file_bb[self.index()] }
    }
}

impl IntoBitboard for Square {
    fn into_bitboard(self) -> Bitboard {
        unsafe { square_bb[self.index()] }
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
    if c == Color::White {
        unsafe { rank_bb[r.index()] }
    } else {
        unsafe { rank_bb[(Rank::_8 as u8 - r as u8) as usize] }
    }
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


static mut square_bb: [Bitboard; 64] = [0; 64];
static mut rank_bb: [Bitboard; 8] = [0; 8];
static mut file_bb: [Bitboard; 8] = [0; 8];
static mut distance: [[u8; 64]; 64] = [[0; 64]; 64];

fn init_simple_bitboards() {
    for i in 0..8 {
        unsafe {
            rank_bb[i] = 0xff << (8 * i);
            file_bb[i] = 0x0101010101010101 << i;
        }
    }
    for sq1 in each_square() {
        let i = sq1.index();
        unsafe {
            square_bb[i] = 1 << i;
        }
        for sq2 in each_square() {
            let j = sq2.index();
            let rd = sq1.rank() as i8 - sq2.rank() as i8;
            let fd = sq1.file() as i8 - sq2.file() as i8;
            unsafe {
                distance[i][j] = ::std::cmp::max(rd.abs(), fd.abs()) as u8;
            }
        }
    }
}

fn dist(sq1: Square, sq2: Square) -> u8 {
    debug_assert!(sq1 != Square::NoSquare && sq2 != Square::NoSquare);
    unsafe { distance[sq1.index()][sq2.index()] }
}

static mut white_pawn_attacks_bb: [Bitboard; 64] = [0; 64];
static mut black_pawn_attacks_bb: [Bitboard; 64] = [0; 64];
static mut knight_attacks_bb: [Bitboard; 64] = [0; 64];
static mut king_attacks_bb: [Bitboard; 64] = [0; 64];

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
        init_mundane(&mut white_pawn_attacks_bb, &[NORTHWEST, NORTHEAST]);
        init_mundane(&mut black_pawn_attacks_bb, &[SOUTHWEST, SOUTHEAST]);
        init_mundane(&mut knight_attacks_bb,
                     &[NORTH + NORTHWEST,
                       NORTH + NORTHEAST,
                       WEST + NORTHWEST,
                       WEST + SOUTHWEST,
                       EAST + NORTHEAST,
                       EAST + SOUTHEAST,
                       SOUTH + SOUTHEAST,
                       SOUTH + SOUTHWEST]);
        init_mundane(&mut king_attacks_bb,
                     &[NORTHWEST, NORTH, NORTHEAST, WEST, EAST, SOUTHWEST, SOUTH, SOUTHEAST]);
    }
}

static mut bishop_masks: [Bitboard; 64] = [0; 64];
static mut bishop_magic: [Bitboard; 64] = [0; 64];
static mut bishop_attacks_bb: [[Bitboard; 512]; 64] = [[0; 512]; 64];

static mut rook_masks: [Bitboard; 64] = [0; 64];
static mut rook_magic: [Bitboard; 64] = [0; 64];
static mut rook_attacks_bb: [[Bitboard; 4096]; 64] = [[0; 4096]; 64];

fn magic_bishop_index(sq: Square, mut occ: Bitboard) -> usize {
    debug_assert!(sq != Square::NoSquare);
    unsafe {
        occ &= bishop_masks[sq.index()];
        occ = occ.wrapping_mul(bishop_magic[sq.index()]);
    }
    (occ >> 55) as usize
}

fn magic_rook_index(sq: Square, mut occ: Bitboard) -> usize {
    debug_assert!(sq != Square::NoSquare);
    unsafe {
        occ &= rook_masks[sq.index()];
        occ = occ.wrapping_mul(rook_magic[sq.index()]);
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

unsafe fn init_bishop_attacks(sq: Square,
                              size: usize,
                              occ: &[Bitboard; 4096],
                              gold: &[Bitboard; 4096])
                              -> bool {
    ::std::intrinsics::write_bytes(&mut bishop_attacks_bb[sq.index()][0], 0, 512);
    for i in 0..size {
        let att: *mut Bitboard =
            &mut bishop_attacks_bb[sq.index()][magic_bishop_index(sq, occ[i as usize])];
        if *att != 0 && *att != gold[i as usize] {
            return false;
        }
        *att = gold[i as usize]
    }
    true
}

unsafe fn init_rook_attacks(sq: Square,
                            size: usize,
                            occ: &[Bitboard; 4096],
                            gold: &[Bitboard; 4096])
                            -> bool {
    ::std::intrinsics::write_bytes(&mut rook_attacks_bb[sq.index()][0], 0, 4096);
    for i in 0..size {
        let att: *mut Bitboard =
            &mut rook_attacks_bb[sq.index()][magic_rook_index(sq, occ[i as usize])];
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
    unsafe { init_magic_opt(PieceType::Bishop, 17337, u64::max_value()); }
    unsafe { init_magic_opt(PieceType::Rook, 8452, u64::max_value()); }
}

unsafe fn init_magic_opt(pt: PieceType, xseed: usize, best_time: u64) -> u64{
    let t1 = ::time::precise_time_ns();
    let mut occ: [Bitboard; 4096] = [0; 4096];
    let mut gold: [Bitboard; 4096] = [0; 4096];
    let mut masks = if pt == PieceType::Bishop { &mut bishop_masks } else { &mut rook_masks };
    let mut magic = if pt == PieceType::Bishop { &mut bishop_magic } else { &mut rook_magic };
    let mask_fn = if pt == PieceType::Bishop { bishop_slide_mask } else { rook_slide_mask };
    let attack_fn = if pt == PieceType::Bishop { init_bishop_attacks } else { init_rook_attacks };

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
            let t2 = ::time::precise_time_ns();
            if t2 - t1 > best_time {
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
    ::time::precise_time_ns() - t1
}

static mut bishop_pseudo_attacks_bb: [Bitboard; 64] = [0; 64];
static mut rook_pseudo_attacks_bb: [Bitboard; 64] = [0; 64];
static mut queen_pseudo_attacks_bb: [Bitboard; 64] = [0; 64];

fn init_pseudo_attacks() {
    for sq in each_square() {
        unsafe {
            bishop_pseudo_attacks_bb[sq.index()] = bishop_attacks(sq, 0);
            rook_pseudo_attacks_bb[sq.index()] = rook_attacks(sq, 0);
            queen_pseudo_attacks_bb[sq.index()] = queen_attacks(sq, 0);
        }
    }
}

pub fn bishop_pseudo_attacks(sq: Square) -> Bitboard {
    unsafe { bishop_pseudo_attacks_bb[sq.index()] }
}

pub fn rook_pseudo_attacks(sq: Square) -> Bitboard {
    unsafe { rook_pseudo_attacks_bb[sq.index()] }
}

pub fn queen_pseudo_attacks(sq: Square) -> Bitboard {
    unsafe { queen_pseudo_attacks_bb[sq.index()] }
}

static mut rays_bb: [[Bitboard; 64]; 64] = [[0; 64]; 64];
static mut between_bb: [[Bitboard; 64]; 64] = [[0; 64]; 64];

fn init_post_attack_bitboards() {
    for sq1 in each_square() {
        for sq2 in each_square() {
            if queen_pseudo_attacks(sq1) & bb(sq2) == 0 {
                continue;
            }
            if bishop_pseudo_attacks(sq1) & bb(sq2) != 0 {
                unsafe {
                    rays_bb[sq2.index()][sq1.index()] =
                        bishop_pseudo_attacks(sq1) & bishop_pseudo_attacks(sq2) | bb(sq1) | bb(sq2);
                }
            } else {
                unsafe {
                    rays_bb[sq2.index()][sq1.index()] =
                        rook_pseudo_attacks(sq1) & rook_pseudo_attacks(sq2) | bb(sq1) | bb(sq2);
                }
            }
            let d = direction(sq1, sq2);
            let mut sq3 = shift_sq(sq1, d);
            while sq3 != sq2 {
                unsafe { between_bb[sq1.index()][sq2.index()] |= bb(sq3); }
                sq3 = shift_sq(sq3, d);
            }
        }
    }
}

pub fn between(sq1: Square, sq2: Square) -> Bitboard {
    debug_assert!(sq1 != Square::NoSquare && sq2 != Square::NoSquare);
    unsafe { between_bb[sq1.index()][sq2.index()] }
}

pub fn ray(sq1: Square, sq2: Square) -> Bitboard {
    debug_assert!(sq1 != Square::NoSquare && sq2 != Square::NoSquare);
    unsafe { rays_bb[sq1.index()][sq2.index()] }
}

pub fn king_attacks(sq: Square) -> Bitboard {
    unsafe { king_attacks_bb[sq.index()] }
}

pub fn knight_attacks(sq: Square) -> Bitboard {
    unsafe { knight_attacks_bb[sq.index()] }
}

pub fn white_pawn_attacks(sq: Square) -> Bitboard {
    unsafe { white_pawn_attacks_bb[sq.index()] }
}

pub fn black_pawn_attacks(sq: Square) -> Bitboard {
    unsafe { black_pawn_attacks_bb[sq.index()] }
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
    unsafe { bishop_attacks_bb[sq.index()][magic_bishop_index(sq, occ)] }
}

pub fn rook_attacks(sq: Square, occ: Bitboard) -> Bitboard {
    unsafe { rook_attacks_bb[sq.index()][magic_rook_index(sq, occ)] }
}

pub fn queen_attacks(sq: Square, occ: Bitboard) -> Bitboard {
    rook_attacks(sq, occ) | bishop_attacks(sq, occ)
}

// FIXME: switch to chess_test!
#[cfg(test)]
mod tests {
    use super::*;
    use board::*;
    use board::Square::*;

    struct BBStrTest {
        x: Bitboard,
        s: &'static str,
    }

    fn test_one_bb_str(t: BBStrTest) {
        initialize();
        assert_eq!(t.x, bb_from_str(t.s));
        assert_eq!(t.s, bb_to_str(t.x));
    }

    #[test]
    fn test_bb_str_tests() {
        test_one_bb_str(BBStrTest {
            x: 0,
            s: "\n........\n........\n........\n........\n........\n........\n........\n........\n",
        });
        test_one_bb_str(BBStrTest {
            x: bb!(E4, E5, D4, D5),
            s: "\n........\n........\n........\n...xx...\n...xx...\n........\n........\n........\n",
        });
        test_one_bb_str(BBStrTest {
            x: bb!(A1, B2, A8, B7),
            s: "\nx.......\n.x......\n........\n........\n........\n........\n.x......\nx.......\n",
        });

    }

    struct PopSquareTest {
        x: Bitboard,
        want: Square,
        popped: Bitboard,
    }

    fn test_one_pop_square(t: PopSquareTest) {
        assert_eq!(t.want, lsb(t.x));
        let mut x = t.x;
        let got = pop_square(&mut x);
        assert_eq!(t.want, got);
        assert_eq!(t.popped, x);
    }

    #[test]
    fn test_pop_square() {
        initialize();
        test_one_pop_square(PopSquareTest {
            x: 1,
            want: A1,
            popped: 0,
        });
        test_one_pop_square(PopSquareTest {
            x: 2,
            want: B1,
            popped: 0,
        });
        test_one_pop_square(PopSquareTest {
            x: 0xff,
            want: A1,
            popped: 0xfe,
        });
        test_one_pop_square(PopSquareTest {
            x: bb(H8),
            want: H8,
            popped: 0,
        });
        test_one_pop_square(PopSquareTest {
            x: bb(A1),
            want: A1,
            popped: 0,
        });
        test_one_pop_square(PopSquareTest {
            x: bb!(E4, E5, D4, D5),
            want: D4,
            popped: bb!(E4, E5, D5),
        });
    }

    #[test]
    fn test_king_attacks() {
        initialize();
        assert_eq!(king_attacks(A1), bb!(B1, A2, B2));
        assert_eq!(king_attacks(F6), bb!(E5, E6, E7, F5, F7, G5, G6, G7));
    }

    #[test]
    fn test_knight_attacks() {
        initialize();
        assert_eq!(knight_attacks(A1), bb!(B3, C2));
        assert_eq!(knight_attacks(F6), bb!(D5, D7, E4, E8, G4, G8, H5, H7));
    }

    #[test]
    fn test_pawn_attacks() {
        initialize();
        assert_eq!(white_pawn_attacks(B2), bb!(A3, C3));
        assert_eq!(black_pawn_attacks(B2), bb!(A1, C1));
        assert_eq!(white_pawn_attacks(H4), bb(G5));
        assert_eq!(black_pawn_attacks(H4), bb(G3));
    }

    #[test]
    fn test_bishop_attacks() {
        initialize();
        assert_eq!(bishop_attacks(A1, bb!(A1, A8, B3, B6, C6, G3, H1)),
                   bb!(B2, C3, D4, E5, F6, G7, H8));
        assert_eq!(bishop_attacks(F6, bb!(B3, C2, C3, C6, D5, D7, F5)),
                   bb!(C3, D4, D8, E5, E7, G5, G7, H4, H8));
    }

    #[test]
    fn test_rook_attacks() {
        initialize();
        assert_eq!(rook_attacks(A1, bb!(A1, A6, A8, B3, B6, C6, G3, H1)),
                   bb!(A2, A3, A4, A5, A6, B1, C1, D1, E1, F1, G1, H1));
        assert_eq!(rook_attacks(F6, bb!(B3, C2, C3, C6, D5, D7, F6)),
                   bb!(C6, D6, E6, G6, H6, F1, F2, F3, F4, F5, F7, F8));
    }
    
    #[test]
    fn test_queen_attacks() {
        initialize();
        assert_eq!(queen_attacks(A1, bb_from_str("x......x\n........\nxxx.....\n........\n........\n.x....x.\n........\nx......x\n")),
                   bb_from_str(".......x\n......x.\nx....x..\nx...x...\nx..x....\nx.x.....\nxx......\n.xxxxxxx\n"));
        assert_eq!(queen_attacks(F6, bb_from_str("........\n...x....\n..x..x..\n...x....\n........\n.xx.....\n..x.....\n........\n")),
                   bb_from_str("...x.x.x\n....xxx.\n..xxx.xx\n....xxx.\n...x.x.x\n..x..x..\n.....x..\n.....x..\n"));
    }

    #[test]
    fn test_directional_bitboards() {
        initialize();
        assert_eq!(between(C3, E3), bb(D3));
        assert_eq!(between(E3, C3), bb(D3));
        assert_eq!(ray(C3, E3), bb(Rank::_3));
        assert_eq!(ray(E3, C3), bb(Rank::_3));
        assert_eq!(between(D2, G5), bb!(E3, F4));
        assert_eq!(between(G5, D2), bb!(E3, F4));
        assert_eq!(ray(D2, G5), bb!(C1, D2, E3, F4, G5, H6));
        assert_eq!(ray(G5, D2), bb!(C1, D2, E3, F4, G5, H6));
        assert_eq!(between(A1, B3), 0);
        assert_eq!(between(B3, A1), 0);
        assert_eq!(ray(A1, B3), 0);
        assert_eq!(ray(B3, A1), 0);
    }
}
