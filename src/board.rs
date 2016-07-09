use std::cmp;
use std::mem;
use std::fmt;
use std::str;

// Iterator implementation for board enums. This allows us to iterator over
// files, ranks, squares, etc easily while keeping the definitions clean and
// type-safe.
pub struct EachElement<T> {
    front: u8,
    back: u8,
    phantom: ::std::marker::PhantomData<T>,
}

impl<T> Iterator for EachElement<T> {
    type Item = T;

    fn next(&mut self) -> Option<T> {
        if self.front == self.back {
            None
        } else {
            let t: T = unsafe { mem::transmute_copy(&self.front) };
            self.front += 1;
            Some(t)
        }
    }
}

impl<T> DoubleEndedIterator for EachElement<T> {
    fn next_back(&mut self) -> Option<T> {
        if self.front == self.back {
            None
        } else {
            self.back -= 1;
            let t: T = unsafe { mem::transmute_copy(&self.back) };
            Some(t)
        }
    }
}
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum Color {
    White,
    Black,
    NoColor,
}

pub fn each_color() -> EachElement<Color> {
    EachElement::<Color> {
        front: 0,
        back: Color::NoColor as u8,
        phantom: ::std::marker::PhantomData,
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum PieceType {
    NoPieceType,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
    AllPieces,
}

pub fn each_piece_type() -> EachElement<Color> {
    EachElement::<Color> {
        front: PieceType::Pawn as u8,
        back: PieceType::AllPieces as u8,
        phantom: ::std::marker::PhantomData,
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum Piece {
    NoPiece,
    WP,
    WN,
    WB,
    WR,
    WQ,
    WK,
    BP = 9,
    BN,
    BB,
    BR,
    BQ,
    BK,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum Rank {
    _1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
    _8,
    NoRank,
}

pub fn each_rank() -> EachElement<Rank> {
    EachElement::<Rank> {
        front: 0,
        back: Rank::NoRank as u8,
        phantom: ::std::marker::PhantomData,
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum File {
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    NoFile,
}

pub fn each_file() -> EachElement<File> {
    EachElement::<File> {
        front: 0,
        back: File::NoFile as u8,
        phantom: ::std::marker::PhantomData,
    }
}

#[cfg_attr(rustfmt, rustfmt_skip)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NoSquare,
}

pub fn each_square() -> EachElement<Square> {
    EachElement::<Square> {
        front: 0,
        back: Square::NoSquare as u8,
        phantom: ::std::marker::PhantomData,
    }
}

pub fn sq(f: File, r: Rank) -> Square {
    debug_assert!(f != File::NoFile);
    debug_assert!(r != Rank::NoRank);
    unsafe { mem::transmute(((r as u8) << 3) | (f as u8)) }
}

pub fn rank(sq: Square) -> Rank {
    debug_assert!(sq != Square::NoSquare);
    unsafe { mem::transmute(sq as u8 >> 3) }
}

pub fn file(sq: Square) -> File {
    unsafe { mem::transmute(sq as u8 & 7) }
}

impl fmt::Display for Square {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f,
               "{}{}",
               (file(*self) as u8 + 97) as char,
               rank(*self) as u8 + 1)
    }
}

// TODO: look into performance implications of unchecked indexing
pub type Bitboard = u64;

static mut square_bb: [Bitboard; 64] = [0; 64];
static mut rank_bb: [Bitboard; 8] = [0; 8];
static mut file_bb: [Bitboard; 8] = [0; 8];
static mut distance: [[u8; 64]; 2] = [[0; 64]; 2];
// inline string String(Bitboard bb) {
// string str = "\n";
// for (Rank r = Rank8; r >= Rank1; r = Prev(r)) {
// for (File f = FileA; f <= FileH; f = Next(f)) {
// if (bb & BB(NewSquare(f, r))) {
// str += "x";
// } else {
// str += ".";
// }
// }
// str += "\n";
// }
// return str;
// }

pub trait IntoBitboard {
    fn into_bitboard(self) -> Bitboard;
}

impl IntoBitboard for Rank {
    fn into_bitboard(self) -> Bitboard {
        debug_assert!(self != Rank::NoRank);
        unsafe { rank_bb[self as usize] }
    }
}

impl IntoBitboard for File {
    fn into_bitboard(self) -> Bitboard {
        debug_assert!(self != File::NoFile);
        unsafe { file_bb[self as usize] }
    }
}

impl IntoBitboard for Square {
    fn into_bitboard(self) -> Bitboard {
        unsafe { square_bb[self as usize] }
    }
}

// inline Bitboard BitboardFromString(string str) {
// Bitboard bb = 0;
// vector<string> lines = strings::Split(str, "\n", strings::SkipEmpty());
// for (Square sq = A1; sq <= H8; sq = Next(sq)) {
// if (lines[7 - RankOf(sq)][FileOf(sq)] == 'x') {
// bb |= BB(sq);
// }
// }
// return bb;
// }
pub fn bb<T: IntoBitboard>(x: T) -> Bitboard {
    x.into_bitboard()
}

pub fn relative_rank_bb(c: Color, r: Rank) -> Bitboard {
    debug_assert!(r != Rank::NoRank);
    debug_assert!(c != Color::NoColor);
    if c == Color::White {
        unsafe { rank_bb[r as usize] }
    } else {
        unsafe { rank_bb[(Rank::_8 as u8 - r as u8) as usize] }
    }
}

fn fill_simple_bitboards() {
    for i in 0..9 {
        unsafe {
            rank_bb[i] = 0xff << (8 * i);
            file_bb[i] = 0x0101010101010101 << i;
        }
    }
    for sq1 in each_square() {
        let i = sq1 as usize;
        unsafe {
            square_bb[i] = 1 << i;
        }
        for sq2 in each_square() {
            let j = sq2 as usize;
            let rd = rank(sq1) as i8 - rank(sq2) as i8;
            let fd = file(sq1) as i8 - file(sq2) as i8;
            unsafe {
                distance[i][j] = cmp::max(rd.abs(), fd.abs()) as u8;
            }
        }
    }
}

// typedef uint64_t Bitboard;
//
// extern int distance[64][64];
// extern Bitboard betweenBB[64][64];
// extern Bitboard raysBB[64][64];
// extern Bitboard KingAttacks[64];
// extern Bitboard WhitePawnAttacks[64];
// extern Bitboard BlackPawnAttacks[64];
// extern Bitboard KnightAttacks[64];
// extern Bitboard RookPseudoAttacks[64];
// extern Bitboard BishopPseudoAttacks[64];
// extern Bitboard QueenPseudoAttacks[64];
// extern Bitboard RookAttacks[64][4096];
// extern Bitboard BishopAttacks[64][512];
// extern Bitboard RookMasks[64];
// extern Bitboard BishopMasks[64];
// extern Bitboard RookMagic[64];
// extern Bitboard BishopMagic[64];
//

// inline Bitboard Shift(Bitboard bb, Delta d) {
// switch (d) {
// case North:
// return bb << 8;
// case North + North:
// return bb << 16;
// case South:
// return bb >> 8;
// case South + South:
// return bb >> 16;
// case Northeast:
// return (bb & ~BB(FileH)) << 9;
// case Southeast:
// return (bb & ~BB(FileH)) >> 7;
// case Northwest:
// return (bb & ~BB(FileA)) << 7;
// case Southwest:
// return (bb & ~BB(FileA)) >> 9;
// }
// assert(false);
// return 0;
// }
//
// LSB returns the square corresponding to the least significant bit of bb.
// inline Square LSB(Bitboard bb) {
// assert(bb);
// return Square(__builtin_ffsll(bb) - 1);
// }
//
// MSB returns the square corresponding to the most significant bit of bb.
// inline Square MSB(Bitboard bb) {
// assert(bb);
// return Square(63 - __builtin_clzll(bb));
// }
//
// PopSquare clears the least significant bit of bb and returns the
// corresponding square.
// inline Square PopSquare(Bitboard* bb) {
// Square sq = LSB(*bb);
// bb &= (*bb - 1);
// return sq;
// }
//
// Popcount returns the number of 1 bits in x.
// inline int Popcount(Bitboard x) {
// return __builtin_popcountll(x);
// }
//
// inline int MagicBishopIndex(Square sq, Bitboard occ) {
// assert(Valid(sq));
// occ &= BishopMasks[sq];
// occ *= BishopMagic[sq];
// return int(occ >> 55);
// }
//
// inline int MagicRookIndex(Square sq, Bitboard occ) {
// assert(Valid(sq));
// occ &= RookMasks[sq];
// occ *= RookMagic[sq];
// return int(occ >> 52);
// }
//
// template <PieceType pt>
// inline Bitboard Attacks(Square sq) {
// assert(Valid(sq));
// switch (pt) {
// case Bishop:
// return BishopPseudoAttacks[sq];
// case Rook:
// return RookPseudoAttacks[sq];
// case Queen:
// return QueenPseudoAttacks[sq];
// case Knight:
// return KnightAttacks[sq];
// case King:
// return KingAttacks[sq];
// default:
// assert(false);
// }
// return 0;
// }
//
// template <PieceType pt>
// inline Bitboard Attacks(Square sq, Bitboard occ) {
// assert(Valid(sq));
// switch (pt) {
// case Bishop:
// return BishopAttacks[sq][MagicBishopIndex(sq, occ)];
// case Rook:
// return RookAttacks[sq][MagicRookIndex(sq, occ)];
// case Queen:
// return BishopAttacks[sq][MagicBishopIndex(sq, occ)] |
// RookAttacks[sq][MagicRookIndex(sq, occ)];
// case Knight:
// return KnightAttacks[sq];
// case King:
// return KingAttacks[sq];
// default:
// assert(false);
// }
// return 0;
// }
//
// template <PieceType pt>
// inline Bitboard Attacks(Color c, Square sq);
//
// template <>
// inline Bitboard Attacks<Pawn>(Color c, Square sq) {
// assert(Valid(sq));
// if (c == Color::White) {
// return WhitePawnAttacks[sq];
// } else {
// return BlackPawnAttacks[sq];
// }
// }
//
// template <Piece p>
// inline Bitboard Attacks(Square sq) {
// assert(Valid(sq));
// switch (p) {
// case Piece::WP:
// return WhitePawnAttacks[sq];
// case Piece::BP:
// return BlackPawnAttacks[sq];
// case Piece::WB:
// case Piece::BB:
// return BishopPseudoAttacks[sq];
// case Piece::WR:
// case Piece::BR:
// return RookPseudoAttacks[sq];
// case Piece::WQ:
// case Piece::BQ:
// return QueenPseudoAttacks[sq];
// case Piece::WN:
// case Piece::BN:
// return KnightAttacks[sq];
// case Piece::WK:
// case Piece::BK:
// return KingAttacks[sq];
// default:
// assert(false);
// }
// return 0;
// }
//
// template <Piece p>
// inline Bitboard Attacks(Square sq, Bitboard occ) {
// assert(Valid(sq));
// switch (p) {
// case Piece::WP:
// return WhitePawnAttacks[sq];
// case Piece::BP:
// return BlackPawnAttacks[sq];
// default:
// return Attacks<TypeOf(p)>(sq, occ);
// }
// }
//
// inline Bitboard Attacks(PieceType pt, Square sq, Bitboard occ) {
// assert(Valid(sq));
// switch (pt) {
// case Bishop:
// return BishopAttacks[sq][MagicBishopIndex(sq, occ)];
// case Rook:
// return RookAttacks[sq][MagicRookIndex(sq, occ)];
// case Queen:
// return BishopAttacks[sq][MagicBishopIndex(sq, occ)] |
// RookAttacks[sq][MagicRookIndex(sq, occ)];
// case Knight:
// return KnightAttacks[sq];
// case King:
// return KingAttacks[sq];
// default:
// assert(false);
// }
// return 0;
// }
//
