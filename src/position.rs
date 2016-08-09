use board::*;
use bitboard;
use bitboard::Bitboard;
use movegen;
use movement;
use movement::Move;
use options;
use score;
use score::{Phase, Score};

pub const START_FEN: &'static str = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// CastleRights represents the available castling rights for both sides (black
// and white) and both directions (long and short). This doesn't indicate the
// castling is actually possible at the moment, only that it could become
// possible eventually. Each color/direction combination corresponds to one bit
// of a CastleRights.
pub type CastleRights = u8;
pub const WHITE_OO: CastleRights = 0x01;
pub const BLACK_OO: CastleRights = 0x01 << 1;
pub const WHITE_OOO: CastleRights = 0x01 << 2;
pub const BLACK_OOO: CastleRights = 0x01 << 3;
pub const CASTLE_ALL: CastleRights = WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO;
pub const CASTLE_NONE: CastleRights = 0;

// CastleInfo stores useful information for generating castling moves
// efficiently. It supports both traditional chess and Chess960, which
// is why it seems unreasonably complicated.
pub struct CastleInfo {
    // The set of squares that must be unoccupied.
    pub path: Bitboard,
    // The king's initial square.
    pub king: Square,
    // The rook's initial square.
    pub rook: Square,
    // The king's final square.
    pub kdest: Square,
    // The direction from the king's initial square to its final square.
    pub d: Delta,
    // In some Chess960 positions, the rook may be pinned to the king,
    // making an otherwise legal castle illegal due to discovered check.
    // This flags such positions for extra checking during move generation.
    pub may_discover_check: bool,
}

const EMPTY_CASTLE_INFO: CastleInfo = CastleInfo {
        path: 0,
        king: Square::NoSquare,
        rook: Square::NoSquare,
        kdest: Square::NoSquare,
        d: 0,
        may_discover_check: false,
    };

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
    phase: Phase,
    material_score: Score,
    // TODO:
    //      position hash
    //      material score
    //      phase info?
}

impl State {
    pub fn new() -> State {
        State {
            checkers: 0,
            last_move: movement::NO_MOVE,
            ply: 0,
            fifty_move_counter: 0,
            ep_square: Square::NoSquare,
            castle_rights: CASTLE_NONE,
            us: Color::NoColor,
            phase: 0,
            material_score: score::NONE,
        }
    }

    pub fn clear(&mut self) {
        // It's not clear that this is guaranteed to work correctly, but looking at the
        // implementation of std::cell::Cell makes me think it should be no problem.
        unsafe { ::std::intrinsics::write_bytes(self, 0, 1); }
        self.ep_square = Square::NoSquare;
    }

    pub fn undo_state(p: &Position) -> State {
        unsafe {
            let mut undo: State = ::std::mem::uninitialized();
            ::std::ptr::copy_nonoverlapping(&p.state, &mut undo, 1);
            undo
        }
    }
}

pub type UndoState = State;

pub struct Position {
    state: State,
    board: [Piece; 64],
    pieces_of_type: [Bitboard; 8],
    pieces_of_color: [Bitboard; 2],

    // We store information about possible castles in Position, even though
    // they don't change over the course of a game so they could be global
    // in principle. This lets us run tests over lots of positions concurrently
    // and should have minimal overhead since we rarely copy positions.
    //
    // If you're wondering why the castle info is so complicated, it's to keep
    // move generation simple and fast in the presence of weird Chess960
    // setups.
    possible_castles: [[CastleInfo; 2]; 2],
    // cr &= castle_mask[to] & castle_mask[from] will remove the appropriate rights
    // from cr for a move between from and to.
    castle_mask: [CastleRights; 64],
}

impl Position {
    pub fn new() -> Position {
        Position {
            state: State::new(),
            board: [Piece::NoPiece; 64],
            pieces_of_type: [0; 8],
            pieces_of_color: [0; 2],
            castle_mask: [CASTLE_NONE; 64],
            possible_castles: [[EMPTY_CASTLE_INFO, EMPTY_CASTLE_INFO], [EMPTY_CASTLE_INFO, EMPTY_CASTLE_INFO]],
        }
    }

    pub fn from_fen(fen: &str) -> Position {
        let mut p = Position::new();
        p.load_fen(fen).unwrap();
        p
    }

    pub fn copy_state(&mut self, state: &State) {
        unsafe { ::std::ptr::copy_nonoverlapping(state, &mut self.state, 1); }
    }

    // clear resets the position and removes all pieces from the board.
    pub fn clear(&mut self) {
        unsafe { ::std::intrinsics::write_bytes(self, 0, 1) }
        self.state.ep_square = Square::NoSquare;
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

    pub fn our_pieces(&self) -> Bitboard {
        self.pieces_of_color[self.state.us.index()]
    }

    pub fn their_pieces(&self) -> Bitboard {
        self.pieces_of_color[self.state.us.flip().index()]
    }
    
    pub fn king_sq(&self, c: Color) -> Square {
        bitboard::lsb(self.pieces_of_color_and_type(c, PieceType::King))
    }

    pub fn piece_at(&self, sq: Square) -> Piece {
        self.board[sq.index()]
    }

    pub fn phase(&self) -> Phase {
        self.state.phase
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

    pub fn possible_castles(&self, c: Color, side: usize) -> &CastleInfo {
        &self.possible_castles[c.index()][side]
    }

    // attackers gives the set of pieces attacking sq, regardless of color.
    pub fn attackers(&self, sq: Square) -> Bitboard {
        let mut b: Bitboard = 0;
        let occ = self.all_pieces();
        b |= bitboard::white_pawn_attacks(sq) & self.pieces(Piece::BP);
        b |= bitboard::black_pawn_attacks(sq) & self.pieces(Piece::WP);
        b |= bitboard::knight_attacks(sq) & self.pieces_of_type(PieceType::Knight);
        let queens = self.pieces_of_type(PieceType::Queen);
        b |= bitboard::bishop_attacks(sq, occ) & (self.pieces_of_type(PieceType::Bishop) | queens);
        b |= bitboard::rook_attacks(sq, occ) & (self.pieces_of_type(PieceType::Rook) | queens);
        b |= bitboard::king_attacks(sq) & self.pieces_of_type(PieceType::King);
        b
    }

    fn find_checkers(&self) -> Bitboard {
        self.attackers(self.king_sq(self.us())) & self.their_pieces()
    }

    pub fn checkers(&self) -> Bitboard {
        self.state.checkers
    }

    pub fn last_move(&self) -> Move {
        self.state.last_move
    }

    // attack_occluders returns the set of pieces of color c that are blocking
    // an attack to sq from a piece of color attacker. That is, if an attack
    // occluder were removed from the board, a piece of color attacker would
    // attack sq. This can find pinned pieces or pieces that could discover check
    // depending on the arguments passed.
    fn attack_occluders(&self, sq: Square, c: Color, attacker: Color) -> Bitboard {
        let rpinners = (self.pieces_of_type(PieceType::Rook) |
                        self.pieces_of_type(PieceType::Queen)) & bitboard::rook_pseudo_attacks(sq);
        let bpinners = (self.pieces_of_type(PieceType::Bishop) |
                        self.pieces_of_type(PieceType::Queen)) & bitboard::bishop_pseudo_attacks(sq);

        let mut potential_pinners = (rpinners | bpinners) & self.pieces_of_color(attacker);
        let mut occluders: Bitboard = 0;
        while potential_pinners != 0 {
            let between = bitboard::between(sq, bitboard::pop_square(&mut potential_pinners)) & self.all_pieces();
            if (between & (between.wrapping_sub(1))) == 0 {  // exactly one piece in between
                occluders |= between & self.pieces_of_color(c);
            }
        }
        occluders
    }

    // pinned returns all pinned pieces of color c.
    pub fn pinned(&self, c: Color) -> Bitboard {
        self.attack_occluders(self.king_sq(c), c, c.flip())
    }

    // check_discoverers returns all pieces of color c that could create a
    // discovered check if moved.
    pub fn check_discoverers(&self, c: Color) -> Bitboard {
        self.attack_occluders(self.king_sq(c.flip()), c, c)
    }

    // obvious_check determines whether or not a move is 'obviously' a check.
    // Non-obvious checks are those caused by castling, en passant captures,
    // and promoting moves. Obvious checks are everything else. This is a
    // useful distinction because non-obvious checks are both rare and
    // expensive to check for, so we can handle them separately when testing
    // move legality without sacrificing efficiency.
    pub fn obvious_check(&self, m: Move, ad: &AttackData) -> bool {
        let to = m.to();
        // If we're moving to a checking square for our piece type, it's a
        // check.
        if ad.potential_checks[m.piece().piece_type().index()] & bitboard::bb(to) != 0 {
            return true;
        }
        let from = m.from();
        // If this piece can discover check and we're not moving along the path
        // to the enemy king, this move discovers check.
        ad.check_discoverers & bitboard::bb(from) != 0 &&
            (bitboard::ray(from, to) & bitboard::bb(ad.their_king)) == 0
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
        s.push('\n');

        let ad = AttackData::new(self);
        let moves = &mut Vec::new();
        movegen::gen_legal(self, &ad, moves);

        for m in moves.iter() {
            s.push_str(m.to_string().as_str());
            s.push(' ');
        }
        s.push('\n');
        s
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
        self.state.checkers = self.find_checkers();

        self.state.castle_rights = self.read_castle_rights(pieces[2]);
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

    // read_castle_rights takes a castling rights string in FEN, X-FEN or
    // Shredder-FEN format, returns the associated castling rights, and
    // adjusts the castling data structures appropriately. It's slightly
    // overcomplicated due to the desire to support all three formats.
    fn read_castle_rights(&mut self, s: &str) -> CastleRights {
        let mut c: CastleRights = 0;
        for sq in each_square() {
            self.castle_mask[sq.index()] = !0;
        }
        for ch in s.chars() {
            let mut ksq = Square::NoSquare;
            let mut rsq = Square::NoSquare;
            if ch == '-' {
                return c;
            }
            if ch >= 'a' && ch <= 'z' {
                ksq = self.king_sq(Color::Black);
                if ch == 'k' {
                    rsq = ksq.next();
                    while self.piece_at(rsq).piece_type() != PieceType::Rook {
                        rsq = rsq.next()
                    }
                } else if ch == 'q' {
                    rsq = ksq.prev();
                    while self.piece_at(rsq).piece_type() != PieceType::Rook {
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
                ksq = self.king_sq(Color::White);
                if ch == 'K' {
                    rsq = ksq.next();
                    while self.piece_at(rsq).piece_type() != PieceType::Rook {
                        rsq = rsq.next();
                    }
                } else if ch == 'Q' {
                    rsq = ksq.prev();
                    while self.piece_at(rsq).piece_type() != PieceType::Rook {
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
            self.add_castle(ksq, rsq);
        }
        c
    }

    fn place_piece(&mut self, p: Piece, sq: Square) {
        debug_assert!(self.board[sq.index()] == Piece::NoPiece);
        self.board[sq.index()] = p;
        let b = bitboard::bb(sq);
        self.pieces_of_color[p.color().index()] |= b;
        self.pieces_of_type[p.piece_type().index()] |= b;
        self.pieces_of_type[PieceType::AllPieces.index()] |= b;
        self.state.phase += Score::phase(p.piece_type());
        self.state.material_score += Score::value(p.piece_type());
    }

    fn remove_piece(&mut self, sq: Square) {
        let p = self.board[sq.index()];
        debug_assert!(p != Piece::NoPiece);
        self.board[sq.index()] = Piece::NoPiece;
        let b = bitboard::bb(sq);
        self.pieces_of_color[p.color().index()] ^= b;
        self.pieces_of_type[p.piece_type().index()] ^= b;
        self.pieces_of_type[PieceType::AllPieces.index()] ^= b;
        self.state.phase -= Score::phase(p.piece_type());
        self.state.material_score -= Score::value(p.piece_type());
    }

    fn transfer_piece(&mut self, from: Square, to: Square) {
        let p = self.board[from.index()];
        if self.board[to.index()] != Piece::NoPiece {
            self.remove_piece(to);
        }
        self.board[from.index()] = Piece::NoPiece;
        self.board[to.index()] = p;
        let b = bitboard::bb(from) | bitboard::bb(to);
        self.pieces_of_color[p.color().index()] ^= b;
        self.pieces_of_type[p.piece_type().index()] ^= b;
        self.pieces_of_type[PieceType::AllPieces.index()] ^= b;
    }

    pub fn do_move(&mut self, m: Move, ad: &AttackData) {
        let (us, them) = (self.us(), self.them());
        let (from, mut to) = (m.from(), m.to()); // note: we update 'to' for castling moves
        let (capture, piece_type) = (m.capture(), m.piece().piece_type());

        self.state.ep_square = Square::NoSquare;
        self.state.ply += 1;
        self.state.checkers = 0;

        if piece_type == PieceType::Pawn {
            self.state.fifty_move_counter = 0;
            if from.index() ^ to.index() == 16 && // double pawn push
                (bitboard::pawn_attacks(us, from.pawn_push(us)) &
                 self.pieces_of_color_and_type(them, PieceType::Pawn) != 0) {
                self.state.ep_square = from.pawn_push(us);
            }
        } else {
            self.state.fifty_move_counter += 1;
        }

        if capture != Piece::NoPiece {
            self.state.fifty_move_counter = 0;
        }
        self.state.castle_rights = self.remove_rights(self.state.castle_rights, from, to);

        if m.is_castle() {
            let (rdest, kdest) = if from.index() < to.index() {
                (Square::F1.relative_to(us), Square::G1.relative_to(us))
            } else {
                (Square::D1.relative_to(us), Square::C1.relative_to(us))
            };
            self.remove_piece(from);
            self.transfer_piece(to, rdest);
            self.place_piece(Piece::new(us, PieceType::King), kdest);
            to = kdest;
            self.state.checkers = self.attackers(ad.their_king) & self.our_pieces();
        } else {
            self.transfer_piece(from, to);
            let promote = m.promote();
            if m.is_en_passant() {
                self.remove_piece(to.pawn_push(them));
                self.state.checkers = self.attackers(ad.their_king) & self.our_pieces();
            } else if promote != PieceType::NoPieceType {
                self.remove_piece(to);
                self.place_piece(Piece::new(us, promote), to);
                self.state.checkers = self.attackers(ad.their_king) & self.our_pieces()
            }
        }

        if self.obvious_check(m, ad) {
            let (bb_from, bb_to) = (bitboard::bb(from), bitboard::bb(to));
            if ad.potential_checks[piece_type.index()] & bb_to != 0 {
                self.state.checkers |= bb_to;
            }
            if ad.check_discoverers & bb_from != 0 {
                if piece_type != PieceType::Bishop {
                    self.state.checkers |=
                        bitboard::bishop_attacks(ad.their_king, self.all_pieces()) &
                        (self.pieces_of_type(PieceType::Bishop) | self.pieces_of_type(PieceType::Queen)) &
                        self.our_pieces();
                }
                if piece_type != PieceType::Rook {
                    self.state.checkers |=
                        bitboard::rook_attacks(ad.their_king, self.all_pieces()) &
                        (self.pieces_of_type(PieceType::Rook) | self.pieces_of_type(PieceType::Queen)) &
                        self.our_pieces();
                }
            }
        }
        self.state.us = self.state.us.flip();
    }

    pub fn undo_move(&mut self, mv: Move, undo: &UndoState) {
        self.copy_state(undo);
        let (us, them) = (self.us(), self.them());
        let (from, to) = (mv.from(), mv.to());
        if mv.is_castle() {
            let (rdest, kdest) = if from.index() < to.index() {
                (Square::F1.relative_to(us), Square::G1.relative_to(us))
            } else {
                (Square::D1.relative_to(us), Square::C1.relative_to(us))
            };
            self.remove_piece(kdest);
            self.transfer_piece(rdest, to);
            self.place_piece(Piece::new(us, PieceType::King), from);
        } else {
            self.transfer_piece(to, from);
            let cap = mv.capture();
            if cap != Piece::NoPiece {
                if mv.is_en_passant() {
                    self.place_piece(cap, to.pawn_push(them));
                } else {
                    self.place_piece(cap, to);
                }
            }
        }
        if mv.is_promote() {
            self.remove_piece(from);
            self.place_piece(Piece::new(us, PieceType::Pawn), from);
        }
        self.state.last_move = mv;
    }

    pub fn pseudo_move_is_legal(&self, mv: Move, ad: &AttackData) -> bool {
        let (us, them) = (self.us(), self.them());
        // Discovered check from en passant captures is gross and incredibly
        // infrequent; just handle it inefficiently.
        if mv.is_en_passant() {
            let mut after = self.all_pieces();
            after |= bitboard::bb(mv.to());
            after ^= bb!(mv.from(), mv.to().pawn_push(them));
            let ksq = self.king_sq(us);
            if bitboard::bishop_attacks(ksq, after) &
                (self.pieces_of_type(PieceType::Bishop) |
                 self.pieces_of_type(PieceType::Queen)) &
                    self.their_pieces() != 0 {
                return false;
            }
            if bitboard::rook_attacks(ksq, after) &
                (self.pieces_of_type(PieceType::Rook) |
                 self.pieces_of_type(PieceType::Queen)) &
                    self.their_pieces() != 0 {
                return false;
            }
        }

        if mv.piece().piece_type() == PieceType::King {
            if mv.is_castle() {
                return true;
            }
            // Note: the king can't screen attacks to its destination,
            // because that would mean we're currently in check, and
            // we don't generate those moves in GenEvasions.
            return self.attackers(mv.to()) & self.their_pieces() == 0;
        }

        // If the piece that moved wasn't pinned, we're fine.
        if ad.pinned == 0 || ad.pinned & bb!(mv.from()) == 0 {
            return true;
        }

        // Otherwise, it's legal iff the move travels along the path of the pin.
        bitboard::ray(mv.from(), mv.to()) &
            self.pieces_of_color_and_type(us, PieceType::King) != 0
    }

    // TODO: nullmove handling

    // remove_rights takes a CastleRights and the source and destination of a move,
    // and returns an updated CastleRights with the appropriate rights removed.
    pub fn remove_rights(&self, c: CastleRights, from: Square, to: Square) -> CastleRights {
        c & self.castle_mask[from.index()] & self.castle_mask[to.index()]
    }

    // add_castle adjusts castle_mask and possible_castles to allow castling between
    // a king and rook on the given squares. Valid for both traditional chess and
    // Chess960. This affects castle legality globally, but we only handle one game
    // at a time and castling legality isn't updated during search, so the data is
    // effectively const over the lifetime of any search.
    fn add_castle(&mut self, k: Square, r: Square) {
        let color = if k.index() > Square::H1.index() { Color::Black } else { Color::White };
        let kside = k.index() < r.index();

        let rights = (if kside { WHITE_OO } else { WHITE_OOO }) << color.index();
        self.castle_mask[k.index()] &= !rights;
        self.castle_mask[r.index()] &= !rights;

        let kdest = if kside { Square::G1.relative_to(color) } else { Square::C1.relative_to(color) };
        let rdest = if kside { Square::F1.relative_to(color) } else { Square::D1.relative_to(color) };
        let mut path: Bitboard = 0;
        let min_sq = min!(k, r, kdest, rdest);
        let max_sq = max!(k, r, kdest, rdest);
        for sq in Square::inclusive_range(min_sq, max_sq) {
            if sq != k && sq != r {
                path |= bitboard::bb(sq)
            }
        }
        self.possible_castles[color.index()][if kside { 0 } else { 1 }] =
            CastleInfo {
                path: path,
                king: k,
                rook: r,
                kdest: kdest,
                d: if kdest > k { WEST } else { EAST },
                may_discover_check: r.file() != File::A && r.file() != File::H,
            };
    }

    // rights_string returns a string representation of the given castling rights,
    // compatible with FEN representation (or Shredder-FEN when in Chess960 mode).
    pub fn rights_string(&self) -> String {
        let c = self.state.castle_rights;
        if c == CASTLE_NONE {
            return String::from("-");
        }
        let mut castle = String::new();
        let c960 = options::c960();
        {
            let file_idx = |x: usize, y: usize| -> u8 {
                self.possible_castles[x][y].rook.file().index() as u8
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
               self.rights_string(),
               self.ep_square(),
               self.state.fifty_move_counter,
               (self.state.ply - self.us().index() as u16) / 2 + 1)
    }
}

// AttackData holds information about king attacks, used during move
// generation. We store it in a separate struct to ensure that we only
// calculate it once per position during search. I borrowed the idea for
// this clever setup from Stockfish.
pub struct AttackData {
    potential_checks: [Bitboard; 8], // per piece type
    check_discoverers: Bitboard,
    pub pinned: Bitboard,
    their_king: Square,
}

impl AttackData {
    pub fn new(pos: &Position) -> AttackData {
        let their_king = pos.king_sq(pos.them());
        let occ = pos.all_pieces();
        let ba = bitboard::bishop_attacks(their_king, occ);
        let ra = bitboard::rook_attacks(their_king, occ);
        AttackData {
            potential_checks: [
                0, // PieceType::NoPieceType
                bitboard::pawn_attacks(pos.them(), their_king),
                bitboard::knight_attacks(their_king),
                ba,
                ra,
                ba | ra,
                0,
                0,
            ],
            check_discoverers: pos.check_discoverers(pos.us()),
            pinned: pos.pinned(pos.us()),
            their_king: their_king,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use board::*;
    use board::Square::*;
    use movement::*;

    chess_test!(test_fen, {
        let load_store = |fen| {
            let pos = Position::from_fen(fen);
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

    chess_test!(test_attackers, {
        let test_case = |fen, sq, want| {
            let pos = Position::from_fen(fen);
            assert_eq!(pos.attackers(sq), want);
        };
        test_case("q2rk3/8/3np3/2KPp3/4p3/2N1P3/8/7Q w - - 0 1", D5, bb!(A8, C3, C5, E6));
        test_case("k7/8/8/8/8/B7/2PP4/q1r3RK w - - 0 1", C1, bb!(A1, A3, G1));
    });

    chess_test!(test_obvious_check, {
        let test_case = |fen, m, want| {
            let pos = Position::from_fen(fen);
            let ad = AttackData::new(&pos);
            assert_eq!(pos.obvious_check(m, &ad), want);
        };
        test_case("8/4k3/4r3/4n3/1N6/8/4K3/8 w - - 0 1",
                  Move::new(B4, C6, Piece::WN, Piece::NoPiece),
                  true);
        test_case("8/4k3/4r3/4n3/1N6/8/4K3/8 w - - 0 1",
                  Move::new(B4, A6, Piece::WN, Piece::NoPiece),
                  false);
        test_case("8/4k3/4r3/4n3/1N6/8/4K3/8 b - - 0 1",
                  Move::new(E5, C4, Piece::BN, Piece::NoPiece),
                  true);
    });

    chess_test!(checkers_test, {
        let test_case = |fen, want| {
            let pos = Position::from_fen(fen);
            assert_eq!(pos.checkers(), want);
        };
        test_case("8/4k3/2N1r3/4n3/8/8/4K3/8 b - - 0 1", bb!(C6));
        test_case("8/4k3/2N1r3/4n3/8/B7/4K3/8 b - - 0 1", bb!(A3, C6));
    });

    chess_test!(occlusion_test, {
        let test_case = |fen, c, pin, discover| {
            let pos = Position::from_fen(fen);
            assert_eq!(pos.pinned(c), pin);
            assert_eq!(pos.check_discoverers(c), discover);
        };
        test_case("8/8/RB2kqPR/4N3/1b2b3/4R3/3PP3/4K3 w - - 0 1",
                  Color::White,
                  bb!(D2),
                  bb!(B6));
        test_case("8/8/RB2kqPR/4N3/1b2b3/4R3/3PP3/4K3 w - - 0 1",
                  Color::Black,
                  0,
                  0);
        test_case("kN5Q/NN6/Q1Q5/8/7b/2r1r1P1/3QBP2/bq1NKN1n w - - 0 1",
                  Color::White,
                  bb!(D1, E2),
                  bb!(A7, B7, B8));
        test_case("kN5Q/NN6/Q1Q5/8/7b/2r1r1P1/3QBP2/bq1NKN1n w - - 0 1",
                  Color::Black,
                  0,
                  0);
        test_case("3k4/K2p3r/8/2P5/8/8/8/8 b - - 0 1",
                  Color::Black,
                  0,
                  bb!(D7));
    });

    chess_test!(test_do_move, {
        let test_case = |before, after, mv| {
            let mut pos = Position::from_fen(before);
            let ad = AttackData::new(&pos);
            let undo = UndoState::undo_state(&pos);
            pos.do_move(mv, &ad);
            assert_eq!(pos.to_string(), after);
            pos.undo_move(mv, &undo);
            assert_eq!(pos.to_string(), before);
        };
        test_case("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                  "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
                  Move::new(E2, E4, Piece::WP, Piece::NoPiece));
        test_case("3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1",
                  "3k4/8/8/K1Pp3r/8/8/8/8 w - d6 0 2",
                  Move::new(D7, D5, Piece::BP, Piece::NoPiece));
        test_case("3k4/8/8/8/8/8/8/R3K3 w Q - 0 1",
                  "3k4/8/8/8/8/8/8/2KR4 b - - 1 1",
                  Move::new_castle(E1, A1, Piece::WK));
        test_case("r3k2r/8/8/8/4b3/8/8/R3K2R w KQkq - 0 1",
                  "r3k2r/8/8/8/8/8/8/R3K2b b Qkq - 0 1",
                  Move::new(E4, H1, Piece::BB, Piece::WR));
        test_case("r3k3/2p5/8/8/8/8/8/4K2R w Kq - 0 1",
                  "r3k3/2p5/8/8/8/8/3K4/7R b q - 1 1",
                  Move::new(E1, D2, Piece::WK, Piece::NoPiece));
        test_case("4k3/8/8/1Pp5/8/8/8/4K3 w - c6 0 2",
                  "4k3/8/2P5/8/8/8/8/4K3 b - - 0 2",
                  Move::new_en_passant(B5, C6, Piece::WP, Piece::BP));
        test_case("n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
                  "n1n5/PPPk4/8/8/8/8/4Kp1p/5NqN w - - 0 2",
                  Move::new_promotion(G2, G1, Piece::BP, Piece::NoPiece, PieceType::Queen));
    });

    chess_test!(test_pseudo_move_is_legal, {
        let test_case = |fen, mv, want| {
            let pos = Position::from_fen(fen);
            let ad = AttackData::new(&pos);
            assert_eq!(want, pos.pseudo_move_is_legal(mv, &ad));
        };
        test_case("k7/8/8/5n2/8/4K3/8/8 w - - 0 1",
                  Move::new(E3, D4, Piece::WK, Piece::NoPiece), false);
        test_case("k7/8/8/5n2/8/4K3/8/8 w - - 0 1",
                  Move::new(E3, E2, Piece::WK, Piece::NoPiece), true);
        test_case("k3r3/8/8/8/4Q3/4K3/8/8 w - - 0 1",
                  Move::new(E4, E7, Piece::WQ, Piece::NoPiece), true);
        test_case("k3r3/8/8/8/4Q3/4K3/8/8 w - - 0 1",
                  Move::new(E4, E8, Piece::WQ, Piece::BR), true);
        test_case("1k2r3/8/8/8/4Q3/4K3/8/8 w - - 0 1",
                  Move::new(E4, D4, Piece::WQ, Piece::NoPiece), false);
        test_case("k7/8/8/2Pp4/4K3/8/8/8 w - d6 0 2",
                  Move::new_en_passant(C5, D6, Piece::WP, Piece::BP), true);
        test_case("k7/8/8/r1Pp2K1/8/8/8/8 w - d6 0 2",
                  Move::new_en_passant(C5, D6, Piece::WP, Piece::BP), false);
        test_case("n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
                  Move::new(D7, D8, Piece::BK, Piece::NoPiece), false);
        test_case("n1n5/PPPk4/8/8/8/8/4Kp1p/5b1N w - - 0 1",
                  Move::new(E2, E1, Piece::WK, Piece::NoPiece), false);
        test_case("r2q3r/p1ppkpb1/bn2pnp1/3PN3/NB2P3/5Q1p/PPP1BPPP/1R2K2R b K - 0 3",
                  Move::new(C7, C5, Piece::BP, Piece::NoPiece), true);
    });
}
