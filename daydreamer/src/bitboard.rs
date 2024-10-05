use board::*;

// TODO: look into performance implications of unchecked indexing
pub type Bitboard = u64;

pub trait IntoBitboard {
    fn into_bitboard(self) -> Bitboard;
}

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

include!(concat!(env!("OUT_DIR"), "/generated_bitboards.rs"));

pub fn squares_of_color(sq: Square) -> Bitboard {
    SQUARES_OF_COLOR_BB[(sq.file().index() + sq.rank().index() + 1) & 1]
}

pub fn passer_mask(side: Color, sq: Square) -> Bitboard {
    debug_assert!(sq != Square::NoSquare);
    PASSER_BB[side.index()][sq.index()]
}

pub fn outpost_mask(side: Color, sq: Square) -> Bitboard {
    debug_assert!(sq != Square::NoSquare);
    OUTPOST_BB[side.index()][sq.index()]
}

pub fn in_front_mask(side: Color, sq: Square) -> Bitboard {
    debug_assert!(sq != Square::NoSquare);
    IN_FRONT_BB[side.index()][sq.index()]
}

pub fn neighbor_mask(f: File) -> Bitboard {
    NEIGHBOR_FILES_BB[f.index()]
}

pub fn dist(sq1: Square, sq2: Square) -> u8 {
    debug_assert!(sq1 != Square::NoSquare && sq2 != Square::NoSquare);
    DISTANCE[sq1.index()][sq2.index()]
}

pub fn king_shield(us: Color, sq: Square) -> Bitboard {
    KING_SHIELD_BB[us.index()][sq.index()]
}

pub fn king_near_shield(us: Color, sq: Square) -> Bitboard {
    KING_NEAR_SHIELD_BB[us.index()][sq.index()]
}

pub fn king_halo(sq: Square) -> Bitboard {
    KING_HALO_BB[sq.index()]
}

fn magic_bishop_index(sq: Square, mut occ: Bitboard) -> usize {
    debug_assert!(sq != Square::NoSquare);
    occ &= BISHOP_MASKS[sq.index()];
    occ = occ.wrapping_mul(BISHOP_MAGIC[sq.index()]);
    (occ >> 55) as usize
}

fn magic_rook_index(sq: Square, mut occ: Bitboard) -> usize {
    debug_assert!(sq != Square::NoSquare);
    occ &= ROOK_MASKS[sq.index()];
    occ = occ.wrapping_mul(ROOK_MAGIC[sq.index()]);
    (occ >> 52) as usize
}

pub fn bishop_pseudo_attacks(sq: Square) -> Bitboard {
    BISHOP_PSEUDO_ATTACKS_BB[sq.index()]
}

pub fn rook_pseudo_attacks(sq: Square) -> Bitboard {
    ROOK_PSEUDO_ATTACKS_BB[sq.index()]
}

pub fn queen_pseudo_attacks(sq: Square) -> Bitboard {
    QUEEN_PSEUDO_ATTACKS_BB[sq.index()]
}

pub fn between(sq1: Square, sq2: Square) -> Bitboard {
    debug_assert!(sq1 != Square::NoSquare && sq2 != Square::NoSquare);
    BETWEEN_BB[sq1.index()][sq2.index()]
}

pub fn ray(sq1: Square, sq2: Square) -> Bitboard {
    debug_assert!(sq1 != Square::NoSquare && sq2 != Square::NoSquare);
    RAYS_BB[sq1.index()][sq2.index()]
}

pub fn king_attacks(sq: Square) -> Bitboard {
    KING_ATTACKS_BB[sq.index()]
}

pub fn knight_attacks(sq: Square) -> Bitboard {
    KNIGHT_ATTACKS_BB[sq.index()]
}

pub fn white_pawn_attacks(sq: Square) -> Bitboard {
    WHITE_PAWN_ATTACKS_BB[sq.index()]
}

pub fn black_pawn_attacks(sq: Square) -> Bitboard {
    BLACK_PAWN_ATTACKS_BB[sq.index()]
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
    BISHOP_ATTACKS_BB[sq.index()][magic_bishop_index(sq, occ)]
}

pub fn rook_attacks(sq: Square, occ: Bitboard) -> Bitboard {
    ROOK_ATTACKS_BB[sq.index()][magic_rook_index(sq, occ)]
}

pub fn queen_attacks(sq: Square, occ: Bitboard) -> Bitboard {
    rook_attacks(sq, occ) | bishop_attacks(sq, occ)
}
