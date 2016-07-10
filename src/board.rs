use std::mem;

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

impl Rank {
    pub fn index(self) -> usize {
        self as usize
    }

    pub fn from_index(x: u8) -> Rank {
        debug_assert!(x <= Rank::NoRank as u8);
        unsafe { mem::transmute(x) }
    }
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

impl File {
    pub fn index(self) -> usize {
        self as usize
    }

    pub fn from_index(x: u8) -> File {
        debug_assert!(x <= File::NoFile as u8);
        unsafe { mem::transmute(x) }
    }
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

impl Square {
    pub fn rank(self) -> Rank {
        debug_assert!(self != Square::NoSquare);
        unsafe { mem::transmute(self as u8 >> 3) }
    }

    pub fn file(self) -> File {
        debug_assert!(self != Square::NoSquare);
        unsafe { mem::transmute(self as u8 & 7) }
    }

    pub fn index(self) -> usize {
        self as usize
    }

    pub fn from_index(x: u8) -> Square {
        debug_assert!(x <= Square::NoSquare as u8);
        unsafe { mem::transmute(x) }
    }
}

impl ::std::fmt::Display for Square {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f,
               "{}{}",
               (self.file() as u8 + 97) as char,
               self.rank() as u8 + 1)
    }
}

impl ::std::str::FromStr for Square {
    type Err = String;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if s == "-" {
            return Ok(Square::NoSquare);
        }
        let s = s.to_lowercase();
        if s.as_bytes().len() != 2 {
            return Err(format!("couldn't parse string as square: {}", s));
        }
        let fb: u8 = s.as_bytes()[0] - 97;
        let rb: u8 = s.as_bytes()[1] - 49;
        if fb >= File::NoFile as u8 || rb >= Rank::NoRank as u8 {
            return Err(format!("couldn't parse string as square: {}", s));
        }
        unsafe{ Ok(sq(mem::transmute(fb), mem::transmute(rb))) }
    }
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
    Square::from_index(((r as u8) << 3) | (f as u8))
}

pub type Delta = i8;
pub const NORTH: Delta = 8;
pub const SOUTH: Delta = -8;
pub const EAST: Delta = 1;
pub const WEST: Delta = -1;
pub const NORTHWEST: Delta = NORTH + WEST;
pub const NORTHEAST: Delta = NORTH + EAST;
pub const SOUTHWEST: Delta = SOUTH + WEST;
pub const SOUTHEAST: Delta = SOUTH + EAST;

pub fn pawn_push(c: Color) -> Delta {
    debug_assert!(c != Color::NoColor);
    if c == Color::White {
        NORTH
    } else {
        SOUTH
    }
}

pub fn shift_sq(sq: Square, d: Delta) -> Square {
    let sq2: u8 = (sq as i8 + d) as u8;
    if sq2 >= Square::NoSquare as u8 {
        Square::NoSquare
    } else {
        Square::from_index(sq2)
    }
}
