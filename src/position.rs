use board::*;
use bitboard;
use bitboard::{bb, Bitboard};
use movement::Move;
use options;

// CastleRights represents the available castling rights for both sides (black
// and white) and both directions (long and short). This doesn't indicate the
// castling is actually possible at the moment, only that it could become
// possible eventually. Each color/direction combination corresponds to one bit
// of a CastleRights.
pub type CastleRights = u8;
const WHITE_OO: CastleRights = 0x01;
const BLACK_OO: CastleRights = 0x01 << 1;
const WHITE_OOO: CastleRights = 0x01 << 2;
const BLACK_OOO: CastleRights = 0x01 << 3;
const CASTLE_ALL: CastleRights = WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO;
const CASTLE_NONE: CastleRights = 0;

// cr &= castle_mask[to] & castle_mask[from] will remove the appropriate rights
// from cr for a move between from and to.
static mut castle_mask: [CastleRights; 64] = [0; 64];

// remove_rights takes a CastleRights and the source and destination of a move,
// and returns an updated CastleRights with the appropriate rights removed.
pub fn remove_rights(c: CastleRights, from: Square, to: Square) -> CastleRights {
    unsafe { c & castle_mask[from.index()] & castle_mask[to.index()] }
}

// CastleInfo stores useful information for generating castling moves
// efficiently. It supports both traditional chess and Chess960, which
// is why it seems unreasonably complicated.
pub struct CastleInfo {
    // The set of squares that must be unoccupied.
    path: Bitboard,
    // The king's initial square.
    king: Square,
    // The rook's initial square.
    rook: Square,
    // The king's final square.
    kdest: Square,
    // The direction from the king's initial square to its final square.
    d: Delta,
    // In some Chess960 positions, the rook may be pinned to the king,
    // making an otherwise legal castle illegal due to discovered check.
    // This flags such positions for extra checking during move generation.
    may_discover_check: bool,
}

const EMPTY_CASTLE_INFO: CastleInfo = CastleInfo {
        path: 0,
        king: Square::NoSquare,
        rook: Square::NoSquare,
        kdest: Square::NoSquare,
        d: 0,
        may_discover_check: false,
    };
static mut possible_castles: [[CastleInfo; 2]; 2] =
    [[EMPTY_CASTLE_INFO, EMPTY_CASTLE_INFO], [EMPTY_CASTLE_INFO, EMPTY_CASTLE_INFO]];

// add_castle adjusts castle_mask and possible_castles to allow castling between
// a king and rook on the given squares. Valid for both traditional chess and
// Chess960. This affects castle legality globally, but we only handle one game
// at a time and castling legality isn't updated during search, so the data is
// effectively const over the lifetime of any search.
fn add_castle(k: Square, r: Square) {
    let color = if k.index() > Square::H1.index() { Color::Black } else { Color::White };
    let kside = k.index() < r.index();

    let rights = (if kside { WHITE_OO } else { WHITE_OOO }) << color.index();
    unsafe {
        castle_mask[k.index()] &= !rights;
        castle_mask[r.index()] &= !rights;
    }

    let kdest = if kside { Square::G1.relative_to(color) } else { Square::C1.relative_to(color) };
    let rdest = if kside { Square::F1.relative_to(color) } else { Square::D1.relative_to(color) };
    let mut path: Bitboard = 0;
    let min_sq = min!(k, r, kdest, rdest);
    let max_sq = max!(k, r, kdest, rdest);
    for sq in Square::inclusive_range(min_sq, max_sq) {
        if sq != k && sq != r {
            path |= bb(sq)
        }
    }
    unsafe {
        possible_castles[color.index()][if kside { 0 } else { 1 }] =
            CastleInfo {
                path: path,
                king: k,
                rook: r,
                kdest: kdest,
                d: if kdest > k { WEST } else { EAST },
                may_discover_check: r.file() != File::A && r.file() != File::H,
            }
    }
}

// rights_string returns a string representation of the given castling rights,
// compatible with FEN representation (or Shredder-FEN when in Chess960 mode).
pub fn rights_string(c: CastleRights) -> String {
    if c == CASTLE_NONE {
        return String::from("-");
    }
    let mut castle = String::new();
    let c960 = options::c960();
    {
        let file_idx = |x: usize, y: usize| -> u8 {
            unsafe { possible_castles[x][y].rook.file().index() as u8 }
        };
        let mut add_glyph = |x: usize, y: usize, ch: char| {
            let base = if ch.is_uppercase() { 'A' } else { 'a' };
            let glyph = if !c960 { ch } else { (file_idx(x, y) + base as u8) as char };
            castle.push(glyph);
        };
        if c & WHITE_OO != 0 {
            add_glyph(0, 0, 'K'); 
        }
        if c & WHITE_OOO != 0 {
            add_glyph(0, 1, 'Q'); 
        }
        if c & BLACK_OO != 0 {
            add_glyph(1, 0, 'k'); 
        }
        if c & BLACK_OOO != 0 {
            add_glyph(1, 1, 'q'); 
        }
    }
    return castle;
}

// State stores core position state information that would otherwise be lost
// when making a move.
pub struct State {
    checkers: Bitboard,
    last_move: Move,
    ply: u16,
    fifty_move_counter: u8,
    ep_square: Square,
    castle_rights: CastleRights,
    us: Color,
    // TODO:
    // position hash
    // material score
    // phase info?
}

impl State {
    pub fn clear(&mut self) {
        unsafe { ::std::intrinsics::write_bytes(self, 0, 1); }
        self.ep_square = Square::NoSquare;
    }
}

pub type UndoState = State;

pub struct Position {
    state: State,
    board: [Piece; 64],
    pieces_of_type: [Bitboard; 8],
    pieces_of_color: [Bitboard; 2],
}

impl Position {
    // clear resets the position and removes all pieces from the board.
    pub fn clear(&mut self) {
        unsafe { ::std::intrinsics::write_bytes(self, 0, 1) }
    }

    pub fn debug_string(&self) -> String {
        unimplemented!();
    }

    pub fn all_pieces(&self) -> Bitboard {
        self.pieces_of_color[0] | self.pieces_of_color[1]
    }

    pub fn pieces_of_color(&self, c: Color) -> Bitboard {
        self.pieces_of_color[c.index()]
    }

    pub fn pieces_of_type(&self, pt: PieceType) -> Bitboard {
        self.pieces_of_type[pt.index()]
    }

    pub fn pieces_of_color_and_type(&self, c: Color, pt: PieceType) -> Bitboard {
        self.pieces_of_color[c.index()] | self.pieces_of_type[pt.index()]
    }

    pub fn pieces(&self, p: Piece) -> Bitboard {
        self.pieces_of_color[p.color().index()] | self.pieces_of_type[p.piece_type().index()]
    }
    
    pub fn king_sq(&self, c: Color) -> Square {
        bitboard::lsb(self.pieces_of_color_and_type(c, PieceType::King))
    }

    pub fn piece_at(&self, sq: Square) -> Piece {
        self.board[sq.index()]
    }

    pub fn us(&self) -> Color {
        self.state.us
    }

    pub fn them(&self) -> Color {
        self.state.us.flip()
    }

    pub fn ep_square(&self) -> Square {
        self.state.ep_square
    }

    pub fn castle_rights(&self) -> CastleRights {
        self.state.castle_rights
    }

    pub fn checkers(&self) -> Bitboard {
        self.state.checkers
    }

    pub fn last_move(&self) -> Move {
        self.state.last_move
    }

    // TODO: clean up the unwraps in here.
    pub fn load_fen(&mut self, fen: &str) -> Result<(), String> {
        self.clear();
        let pieces: Vec<&str> = fen.split_whitespace().collect();

        if pieces.len() < 4 {
            return Err(format!("error parsing fen position {}: expected at least 4 tokens, got {}",
                               fen, pieces.len()));
        }

        {
            let mut sq = Square::A8;
            for ch in pieces[0].chars() {
                if ch.is_numeric() {
                    sq = Square::from_u8(sq as u8 + ch as u8 - '0' as u8);
                } else if ch == '/' {
                    sq = Square::from_u8(sq as u8 - '0' as u8);
                } else {
                    self.place_piece(Piece::from_glyph(ch), sq);
                    sq = sq.next();
                }
            }
        }

        {
            let ch = pieces[1].chars().next().unwrap();
            if ch == 'w' {
                self.state.us = Color::White;
            } else if ch == 'b' {
                self.state.us = Color::Black;
            } else {
                return Err(format!("error parsing fen position {}: couldn't parse side to move ({})",
                                   fen, pieces[1]));
            }
        }

        self.state.castle_rights = read_castle_rights(pieces[2], self);
        self.state.ep_square = pieces[3].parse().unwrap();
        if self.ep_square() != Square::NoSquare {
            // Don't believe the ep square unless there's a pawn that could
            // actually do the capture.
            if (self.us() == Color::White &&
                bitboard::black_pawn_attacks(self.ep_square()) & self.pieces(Piece::WP) == 0) ||
               (self.us() == Color::Black &&
                bitboard::white_pawn_attacks(self.ep_square()) & self.pieces(Piece::BP) == 0) {
                self.state.ep_square = Square::NoSquare;
            }
        }

        self.state.fifty_move_counter = 0;
        self.state.ply = 0;

        if pieces.len() <= 4 {
            return Ok(());
        }
        self.state.fifty_move_counter = pieces[4].parse().unwrap();
        if pieces.len() <= 5 {
            return Ok(());
        }
        self.state.ply = pieces[5].parse().unwrap();
        self.state.ply = ::std::cmp::max(2 * (self.state.ply - 1), 0);
        if self.us() == Color::Black {
            self.state.ply += 1;
        }
        Ok(())
    }

    fn place_piece(&mut self, p: Piece, sq: Square) {
        self.board[sq.index()] = p;
        let b = bb(sq);
        self.pieces_of_type[p.piece_type().index()] |= b;
        self.pieces_of_color[p.color().index()] |= b;
        self.pieces_of_type[PieceType::AllPieces.index()] |= b;
    }
    //Bitboard Attackers(Square sq) const;
    //Bitboard Attackers(Square sq, Bitboard occ) const;
    //Bitboard Pinned() const;
    //Bitboard Pinned(Color c) const;
    //Bitboard CheckDiscoverers(Color c) const;

    //bool PseudoMoveIsLegal(Move move, const AttackData& ad) const;
    //void DoMove(Move move, UndoState* undo, const AttackData& ad);
    //void DoMove(Move move, UndoState* undo, const AttackData& ad, bool gives_check);
    //void DoNullMove(UndoState* undo);
    //void UndoMove(Move move);
    //void UndoNullMove();

    //  private:
    //    void PlacePiece(Piece p, Square sq);
    //    void RemovePiece(Square sq);
    //    void TransferPiece(Square from, Square to);
}

impl ::std::fmt::Display for Position {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        let mut empty = 0;
        for rank in each_rank().rev() {
            for file in each_file() {
                let sq = Square::new(file, rank);
                let p = self.piece_at(sq);
                if empty != 0 && p != Piece::NoPiece {
                    try!(write!(f, "{}", empty));
                    empty = 0;
                }
                if p == Piece::NoPiece {
                    empty += 1;
                } else {
                    try!(write!(f, "{}", p.glyph()));
                }
                if sq.file() == File::H {
                    if empty != 0 {
                        try!(write!(f, "{}", empty));
                        empty = 0;
                    }
                    if sq.rank() != Rank::_1 {
                        try!(write!(f, "/"));
                    }
                }
            }
        }
                
        write!(f, " {} {} {} {} {}",
               self.us().glyph(),
               rights_string(self.castle_rights()),
               self.ep_square(),
               self.state.fifty_move_counter,
               (self.state.ply - self.us().index() as u16) / 2 + 1)
    }
}

// read_castle_rights takes a castling rights string in FEN, X-FEN or
// Shredder-FEN format, returns the associated castling rights, and
// adjusts the castling data structures appropriately. It's slightly
// overcomplicated due to the desire to support all three formats.
pub fn read_castle_rights(s: &str, pos: &Position) -> CastleRights {
    let mut c: CastleRights = 0;
    for sq in each_square() {
        unsafe { castle_mask[sq.index()] = !0; }
    }
    for ch in s.chars() {
        let mut ksq = Square::NoSquare;
        let mut rsq = Square::NoSquare;
        if ch == '-' {
            return c;
        }
        if ch >= 'a' && ch <= 'z' {
            ksq = pos.king_sq(Color::Black);
            if ch == 'k' {
                rsq = ksq.next();
                while pos.piece_at(rsq).piece_type() != PieceType::Rook {
                    rsq = rsq.next()
                }
            } else if ch == 'q' {
                rsq = ksq.prev();
                while pos.piece_at(rsq).piece_type() != PieceType::Rook {
                    rsq = rsq.prev();
                }
            } else {
                rsq = Square::new(File::from_u8(ch as u8 - 'a' as u8), Rank::_8);
            }
            if ksq < rsq {
                c |= BLACK_OO;
            } else {
                c |= BLACK_OOO;
            }
        } else if ch >= 'A' && ch <= 'Z' {
            ksq = pos.king_sq(Color::White);
            if ch == 'K' {
                rsq = ksq.next();
                while pos.piece_at(rsq).piece_type() != PieceType::Rook {
                    rsq = rsq.next();
                }
            } else if ch == 'Q' {
                rsq = ksq.prev();
                while pos.piece_at(rsq).piece_type() != PieceType::Rook {
                    rsq = rsq.prev();
                }
            } else {
                rsq = Square::new(File::from_u8(ch as u8 - 'A' as u8), Rank::_1);
            }
            if ksq < rsq {
                c |= WHITE_OO;
            } else {
                c |= WHITE_OOO;
            }
        }
        add_castle(ksq, rsq);
    }
    return c;
}



#[cfg(test)]
mod tests {
}
