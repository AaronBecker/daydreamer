use board::*;
use bitboard;
use bitboard::{bb, Bitboard};
use movement;
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

const MISSING: Bitboard = 0xffffffffffffffff;

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
    pub fn new() -> State {
        State {
            checkers: MISSING,
            last_move: movement::NO_MOVE,
            ply: 0,
            fifty_move_counter: 0,
            ep_square: Square::NoSquare,
            castle_rights: CASTLE_NONE,
            us: Color::NoColor,
        }
    }

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
    pub fn new() -> Position {
        Position {
            state: State::new(),
            board: [Piece::NoPiece; 64],
            pieces_of_type: [0; 8],
            pieces_of_color: [0; 2],
        }
    }

    // clear resets the position and removes all pieces from the board.
    pub fn clear(&mut self) {
        unsafe { ::std::intrinsics::write_bytes(self, 0, 1) }
    }

    pub fn debug_string(&self) -> String {
        let mut s = String::new();

        for rank in each_rank().rev() {
            for file in each_file() {
                s.push(self.piece_at(Square::new(file, rank)).glyph());
            }
            s.push('\n');
        }
        s.push_str(self.to_string().as_str());
        s
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
        self.pieces_of_color[c.index()] & self.pieces_of_type[pt.index()]
    }

    pub fn pieces(&self, p: Piece) -> Bitboard {
        self.pieces_of_color[p.color().index()] & self.pieces_of_type[p.piece_type().index()]
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
                    sq = Square::from_u8(sq as u8 - 16);
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
        self.pieces_of_color[p.color().index()] |= b;
        self.pieces_of_type[p.piece_type().index()] |= b;
        self.pieces_of_type[PieceType::AllPieces.index()] |= b;
    }

    fn remove_piece(&mut self, sq: Square) {
        let p = self.board[sq.index()];
        self.board[sq.index()] = Piece::NoPiece;
        let b = !bb(sq);
        self.pieces_of_color[p.color().index()] ^= b;
        self.pieces_of_type[p.piece_type().index()] ^= b;
        self.pieces_of_type[PieceType::AllPieces.index()] ^= b;
    }

    fn transfer_piece(&mut self, from: Square, to: Square) {
        let p = self.board[from.index()];
        if self.board[to.index()] != Piece::NoPiece {
            self.remove_piece(to);
        }
        self.board[from.index()] = Piece::NoPiece;
        self.board[to.index()] = p;
        let b = bb(from) | bb(to);
        self.pieces_of_color[p.color().index()] ^= b;
        self.pieces_of_type[p.piece_type().index()] ^= b;
        self.pieces_of_type[PieceType::AllPieces.index()] ^= b;
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
    use super::*;
    use bitboard;

    chess_test!(test_fen, {
        let load_store = |fen| {
            let mut pos = Position::new();
            pos.load_fen(fen).unwrap();
            assert_eq!(pos.to_string(), fen);
        };
        load_store("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        load_store("3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1");
        load_store("8/8/8/8/k1p4R/8/3P4/3K4 w - - 0 2");
        load_store("8/5bk1/8/2Pp4/8/1K6/8/8 w - d6 0 3");
        load_store("8/8/1k6/8/2pP4/8/5BK1/8 b - d3 0 4");
        load_store("8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 5");
        load_store("8/5k2/8/2Pp4/2B5/1K6/8/8 w - d6 0 6");
        load_store("5k2/8/8/8/8/8/8/4K2R w K - 0 7");
        load_store("4k2r/8/8/8/8/8/8/5K2 b k - 0 8");
        load_store("3k4/8/8/8/8/8/8/R3K3 w Q - 0 9");
        load_store("r3k3/8/8/8/8/8/8/3K4 b q - 0 10");
        load_store("r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 1 1");
        load_store("r3k2r/7b/8/8/8/8/1B4BQ/R3K2R b KQkq - 2 1");
        load_store("r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 3 1");
        load_store("r3k2r/8/5Q2/8/8/3q4/8/R3K2R w KQkq - 4 1");
        load_store("2K2r2/4P3/8/8/8/8/8/3k4 w - - 5 1");
        load_store("3K4/8/8/8/8/8/4p3/2k2R2 b - - 6 1");
        load_store("8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 7 1");
        load_store("5K2/8/1Q6/2N5/8/1p2k3/8/8 w - - 8 1");
        load_store("4k3/1P6/8/8/8/8/K7/8 w - - 9 1");
        load_store("8/k7/8/8/8/8/1p6/4K3 b - - 10 1");
        load_store("8/P1k5/K7/8/8/8/8/8 w - - 0 1");
        load_store("8/8/8/8/8/k7/p1K5/8 b - - 0 1");
        load_store("K1k5/8/P7/8/8/8/8/8 w - - 0 1");
        load_store("8/8/8/8/8/p7/8/k1K5 b - - 0 1");
        load_store("8/k1P5/8/1K6/8/8/8/8 w - - 0 1");
        load_store("8/8/8/8/1k6/8/K1p5/8 b - - 0 1");
        load_store("8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1");
        load_store("8/5k2/8/5N2/5Q2/2K5/8/8 w - - 0 1");
        load_store("1k6/1b6/8/8/7R/8/8/4K2R b K - 0 1");
        load_store("4k2r/8/8/7r/8/8/1B6/1K6 w k - 0 1");
        load_store("1k6/8/8/8/R7/1n6/8/R3K3 b Q - 0 1");
        load_store("r3k3/8/1N6/r7/8/8/8/1K6 w q - 0 1");
    });
}
