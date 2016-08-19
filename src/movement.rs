use board::*;
use options;
use position::Position;
use position::AttackData;
use movegen::MoveSelector;

// Move is a 4-byte quantity that encodes source and destination square, the
// moved piece, any captured piece, the promotion value (if any), and flags to
// indicate en-passant capture and castling. The bit layout of a move is as
// follows:
//
// 12345678  12345678  1234     5678     1234       5    6       78
// FROM      TO        PIECE    CAPTURE  PROMOTE    EP   CASTLE  UNUSED
// Square    Square    Piece    Piece    PieceType  bit  bit
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Move(u32);

const EN_PASSANT_FLAG: u32 = 1 << 29;
const CASTLE_FLAG: u32 = 1 << 30;

pub const NO_MOVE: Move = Move(0);
pub const NULL_MOVE: Move = Move(0xffff);

impl Move {
    // The source square. For castling moves, this is the King's square.
    pub fn from(self) -> Square {
        Square::from_u32(self.0 & 0xff)
    }

    // The destination square. For castling moves, this is square of the rook
    // being castled towards.
    pub fn to(self) -> Square {
        Square::from_u32((self.0 >> 8) & 0xff)
    }

    // The piece being moved.
    pub fn piece(self) -> Piece {
        Piece::from_u32((self.0 >> 16) & 0x0f)
    }

    // The piece being captured, if any.
    pub fn capture(self) -> Piece {
        Piece::from_u32((self.0 >> 20) & 0x0f)
    }

    // If this moves promotes, the piece type promoted to.
    pub fn promote(self) -> PieceType {
        PieceType::from_u32((self.0 >> 24) & 0x0f)
    }

    pub fn is_promote(self) -> bool {
        self.0 >> 24 & 0x0f != 0
    }

    pub fn is_en_passant(self) -> bool {
        self.0 & EN_PASSANT_FLAG != 0
    }

    pub fn is_castle(self) -> bool {
        self.0 & CASTLE_FLAG != 0
    }

    pub fn is_long_castle(self) -> bool {
        self.is_castle() && self.from().index() > self.to().index()
    }

    pub fn is_short_castle(self) -> bool {
        self.is_castle() && self.from().index() < self.to().index()
    }

    pub fn new(from: Square, to: Square, p: Piece, capture: Piece) -> Move {
        Move((from.index() as u32) |
             ((to.index() as u32) << 8) |
             ((p.index() as u32) << 16) |
             ((capture.index() as u32) << 20))
    }

    pub fn new_promotion(from: Square, to: Square, p: Piece, capture: Piece, promote: PieceType) -> Move {
        Move((from.index() as u32) |
             ((to.index() as u32) << 8) |
             ((p.index() as u32) << 16) |
             ((capture.index() as u32) << 20) |
             ((promote.index() as u32) << 24))
    }

    pub fn new_castle(from: Square, to: Square, p: Piece) -> Move {
        Move((from.index() as u32) |
             ((to.index() as u32) << 8) |
             ((p.index() as u32) << 16) |
             CASTLE_FLAG)
    }

    pub fn new_en_passant(from: Square, to: Square, p: Piece, capture: Piece) -> Move {
        Move((from.index() as u32) |
             ((to.index() as u32) << 8) |
             ((p.index() as u32) << 16) |
             ((capture.index() as u32) << 20) |
             EN_PASSANT_FLAG)
    }

    pub fn as_u32(self) -> u32 {
        self.0
    }

    pub fn from_uci(pos: &Position, ad: &AttackData, uci: &str) -> Move {
        let uci_lower = uci.to_lowercase();
        let mut ms = MoveSelector::legal();
        while let Some(m) = ms.next(pos, ad) {
            if m.to_string() == uci_lower {
                return m;
            }
        }
        NO_MOVE
    }
}

impl ::std::fmt::Display for Move {
    // Produce a UCI-compatible text representation of this move.
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        if *self == NO_MOVE {
            return write!(f, "(none)");
        } else if *self == NULL_MOVE {
            return write!(f, "0000");
        }
        let mut to = self.to();
        if self.is_short_castle() {
            if options::c960() && options::arena_960_castling() {
                return write!(f, "O-O");
            }
            if !options::c960() {
                to = sq(File::G, to.rank());
            }
        } else if self.is_long_castle() {
            if options::c960() && options::arena_960_castling() {
                return write!(f, "O-O-O");
            }
            if !options::c960() {
                to = sq(File::C, to.rank());
            }
        }
        let mut res = write!(f, "{}{}", self.from(), to);
        if self.is_promote() {
            res = write!(f, "{}", self.promote().glyph());
        }
        res
    }
}
