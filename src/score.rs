use board;
use board::{Color, Piece, PieceType};
use position::Position;

// Note: actual scores fit in an i16 and can be safely stored in 16 bits in
// transposition tables etc, but we use i32 because intermediate calculations
// can over- or underflow and casting everywhere is tedious.
// TODO: test for nps effects if we switch to i16
pub type Score = i32;
pub type Phase = i32;

pub const MATE_SCORE: Score = 30000;
pub const MAX_SCORE: Score = MATE_SCORE + 1;
pub const MIN_SCORE: Score = -MAX_SCORE;
pub const DRAW_SCORE: Score = 0;

pub fn score_is_valid(s: Score) -> bool {
    s < MAX_SCORE && s > MIN_SCORE
}

pub fn mate_in(ply: usize) -> Score {
    MATE_SCORE - ply as Score
}

pub fn mated_in(ply: usize) -> Score {
    ply as Score - MATE_SCORE
}

pub fn is_mate_score(s: Score) -> bool {
    use search::MAX_PLY;
    s < mated_in(MAX_PLY) || s > mate_in(MAX_PLY)
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct PhaseScore {
    pub mg: Score,
    pub eg: Score,
}

pub const NONE: PhaseScore = PhaseScore{ mg: 0, eg: 0 };
pub const PAWN: PhaseScore = PhaseScore{ mg: 85, eg: 115 };
pub const KNIGHT: PhaseScore = PhaseScore{ mg: 350, eg: 400 };
pub const BISHOP: PhaseScore = PhaseScore{ mg: 350, eg: 400 };
pub const ROOK: PhaseScore = PhaseScore{ mg: 500, eg: 650 };
pub const QUEEN: PhaseScore = PhaseScore{ mg: 1000, eg: 1200 };

// This is incredibly gross, but I don't want the piece score array to be
// mutable and the compiler won't evaluate PieceScore negation at compile
// time so this is what we're stuck with.
const BPAWN: PhaseScore = PhaseScore{ mg: -85, eg: -115 };
const BKNIGHT: PhaseScore = PhaseScore{ mg: -350, eg: -400 };
const BBISHOP: PhaseScore = PhaseScore{ mg: -350, eg: -400 };
const BROOK: PhaseScore = PhaseScore{ mg: -500, eg: -650 };
const BQUEEN: PhaseScore = PhaseScore{ mg: -1000, eg: -1200 };

const PIECE_SCORE: [PhaseScore; 16] = [NONE,  PAWN,  KNIGHT,  BISHOP,  ROOK,  QUEEN, NONE, NONE,
                                       NONE, BPAWN, BKNIGHT, BBISHOP, BROOK, BQUEEN, NONE, NONE];
const PT_PHASE: [Phase; 8] = [
    NONE.mg as Phase, NONE.mg as Phase, KNIGHT.mg as Phase,
    BISHOP.mg as Phase, ROOK.mg as Phase, QUEEN.mg as Phase,
    NONE.mg as Phase, NONE.mg as Phase];
const MAX_PHASE: Phase = 2 * (2 * (KNIGHT.mg + BISHOP.mg + ROOK.mg) + QUEEN.mg) as Phase;

const MG_MATERIAL: [Score; 8] = [NONE.mg, PAWN.mg, KNIGHT.mg, BISHOP.mg, ROOK.mg, QUEEN.mg, NONE.mg, NONE.mg];
pub fn mg_material(pt: PieceType) -> Score {
    MG_MATERIAL[pt.index()]
}

// FIXME: improve the names so that it's not confusing whether or not the
// returned values are from white's perspective or from the player to move's
// perspective.
impl PhaseScore {
    pub fn new(mg: Score, eg: Score) -> PhaseScore {
        PhaseScore {
            mg: mg,
            eg: eg,
        }
    }
    pub fn interpolate(self, pos: &Position) -> Score {
        let phase = clamp!(pos.phase(), 0, MAX_PHASE);
        let s = (self.eg * (MAX_PHASE - phase) + self.mg * phase) / MAX_PHASE;
        if pos.us() == Color::White {
            s
        } else {
            -s
        }
    }

    pub fn value(p: Piece) -> PhaseScore {
        PIECE_SCORE[p.index()]
    }

    pub fn phase(pt: PieceType) -> Phase {
        PT_PHASE[pt.index()]
    }
}

impl ::std::ops::Add for PhaseScore {
    type Output = PhaseScore;

    fn add(self, rhs: PhaseScore) -> PhaseScore {
        PhaseScore {
            mg: self.mg + rhs.mg,
            eg: self.eg + rhs.eg,
        }
    }
}

impl ::std::ops::AddAssign for PhaseScore {
    fn add_assign(&mut self, rhs: PhaseScore) {
        self.mg += rhs.mg;
        self.eg += rhs.eg;
    }
}

impl ::std::ops::Sub for PhaseScore {
    type Output = PhaseScore;

    fn sub(self, rhs: PhaseScore) -> PhaseScore {
        PhaseScore {
            mg: self.mg - rhs.mg,
            eg: self.eg - rhs.eg,
        }
    }
}

impl ::std::ops::SubAssign for PhaseScore {
    fn sub_assign(&mut self, rhs: PhaseScore) {
        self.mg -= rhs.mg;
        self.eg -= rhs.eg;
    }
}

impl ::std::ops::Neg for PhaseScore {
    type Output = PhaseScore;

    fn neg(self) -> PhaseScore {
        PhaseScore {
            mg: -self.mg,
            eg: -self.eg,
        }
    }
}

impl ::std::fmt::Display for PhaseScore {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "({}, {})", self.mg, self.eg)
    }
}

pub type ScoreType = u8;
pub const AT_LEAST: ScoreType = 0x01;
pub const AT_MOST: ScoreType = 0x02;
pub const EXACT: ScoreType = 0x03;
pub const BOUNDS_MASK: ScoreType = 0x03;

macro_rules! psqt {
    ( $( {$x:expr, $y:expr} ),+ ) => {
        [ $( PhaseScore{ mg:$x, eg:$y} ),+ ]
    };
}

lazy_static! {
    pub static ref PSQT: [[PhaseScore; 64]; 15] = init_psqt();
}

fn init_psqt() -> [[PhaseScore; 64]; 15] {
    let psqt_base = [
        [PhaseScore{ mg: 0, eg: 0 }; 64],
        // Pawn
        psqt!({  0, 0}, { 0,  0}, {0,  0}, { 0, 0}, { 0, 0}, {0,  0}, { 0,  0}, {  0, 0},
              {-10, 6}, { 2,  4}, {8,  2}, {15, 0}, {15, 0}, {8,  2}, { 2,  4}, {-10, 6},
              {-11, 4}, { 1,  2}, {7,  0}, {13,-2}, {13,-2}, {7,  0}, { 1,  2}, {-11, 4},
              {-12, 3}, { 0,  1}, {5, -1}, {12,-3}, {12,-3}, {5, -1}, { 0,  1}, {-12, 3},
              {-14, 2}, {-2,  0}, {3, -2}, {10,-4}, {10,-4}, {3, -2}, {-2,  0}, {-14, 2},
              {-15, 1}, {-4, -1}, {2, -3}, {9, -5}, {9, -5}, {2, -3}, {-4, -1}, {-15, 1},
              {-16, 1}, {-5, -1}, {1, -3}, {6, -5}, {6, -5}, {1, -3}, {-5, -1}, {-16, 1},
              { 0,  0}, { 0,  0}, {0,  0}, {0,  0}, {0,  0}, {0,  0}, { 0,  0}, {  0, 0}),
        // Knight
        psqt!({-82,-12}, {-16, -7}, { -5,-2}, {  0, 1}, {  0, 1}, { -5,-2}, {-16, -7}, {-82,-12},
              {-11, -5}, {  6,  2}, { 17, 6}, { 21, 8}, { 21, 8}, { 17, 6}, {  6,  2}, {-11, -5},
              { -1,  0}, { 15,  6}, { 26,11}, { 30,13}, { 30,13}, { 26,11}, { 15,  6}, { -1,  0},
              {  0, -1}, { 17,  4}, { 28, 9}, { 32,13}, { 32,13}, { 28, 9}, { 17,  4}, {  0, -1},
              { -6, -3}, { 11,  2}, { 22, 7}, { 26,11}, { 26,11}, { 22, 7}, { 11,  2}, { -6, -3},
              {-15, -7}, {  1, -1}, { 13, 4}, { 17, 6}, { 17, 6}, { 13, 4}, {  1, -1}, {-15, -7},
              {-31,-12}, {-15, -5}, { -4,-1}, {  1, 1}, {  1, 1}, { -4,-1}, {-15, -5}, {-31,-12},
              {-53,-19}, {-37,-14}, {-26,-9}, {-22,-6}, {-22,-6}, {-26,-9}, {-37,-14}, {-53,-19}),
        // Bishop
        psqt!({-1, 0}, {-3,-1}, { -6,-2}, { -8,-2}, { -8,-2}, { -6,-2}, { -3,-1}, { -1, 0},
              {-3,-1}, { 4, 1}, {  1, 0}, { -1, 0}, { -1, 0}, {  1, 0}, {  4, 1}, { -3,-1},
              {-6,-2}, { 1, 0}, {  8, 3}, {  7, 2}, {  7, 2}, {  8, 3}, {  1, 0}, { -6,-2},
              {-8,-2}, {-1, 0}, {  7, 2}, { 16, 5}, { 16, 5}, {  7, 2}, { -1, 0}, { -8,-2},
              {-8,-2}, {-1, 0}, {  7, 2}, { 16, 5}, { 16, 5}, {  7, 2}, { -1, 0}, { -8,-2},
              {-6,-2}, { 1, 0}, {  8, 3}, {  7, 2}, {  7, 2}, {  8, 3}, {  1, 0}, { -6,-2},
              {-3,-1}, { 4, 1}, {  1, 0}, { -1, 0}, { -1, 0}, {  1, 0}, {  4, 1}, { -3,-1},
              {-6, 0}, {-8,-1}, {-11,-2}, {-13,-2}, {-13,-2}, {-11,-2}, { -8,-1}, { -6, 0}),
        // Rook
        psqt!({-7,-2}, {-3,-2}, {1,-2}, {5,-2}, {5,-2}, {1,-2}, {-3,-2}, {-7,-2},
              {-6, 1}, {-2, 1}, {2, 1}, {6, 1}, {6, 1}, {2, 1}, {-2, 1}, {-6, 1},
              {-6, 1}, {-2, 1}, {2, 1}, {6, 1}, {6, 1}, {2, 1}, {-2, 1}, {-6, 1},
              {-6, 1}, {-2, 1}, {2, 1}, {6, 1}, {6, 1}, {2, 1}, {-2, 1}, {-6, 1},
              {-6, 0}, {-2, 0}, {2, 0}, {6, 0}, {6, 0}, {2, 0}, {-2, 0}, {-6, 0},
              {-6, 0}, {-2, 0}, {2, 0}, {6, 0}, {6, 0}, {2, 0}, {-2, 0}, {-6, 0},
              {-6, 0}, {-2, 0}, {2, 0}, {6, 0}, {6, 0}, {2, 0}, {-2, 0}, {-6, 0},
              {-6, 0}, {-2, 0}, {2, 0}, {6, 0}, {6, 0}, {2, 0}, {-2, 0}, {-6, 0}),
        // Queen
        psqt!({-11,-11}, { -7,-6}, {-4,-4}, {-2,-3}, {-2,-3}, {-4,-4}, { -7,-6},{-11,-11},
              { -7, -6}, { -1,-1}, { 2, 1}, { 4, 2}, { 4, 2}, { 2, 1}, { -1,-1},{ -7, -6},
              { -4, -4}, {  2, 1}, { 6, 4}, { 7, 6}, { 7, 6}, { 6, 4}, {  2, 1},{ -4, -4},
              { -2, -3}, {  4, 2}, { 7, 6}, {10, 9}, {10, 9}, { 7, 6}, {  4, 2},{ -2, -3},
              { -2, -3}, {  4, 2}, { 7, 6}, {10, 9}, {10, 9}, { 7, 6}, {  4, 2},{ -2, -3},
              { -4, -4}, {  2, 1}, { 6, 4}, { 7, 6}, { 7, 6}, { 6, 4}, {  2, 1},{ -4, -4},
              { -7, -6}, { -1,-1}, { 2, 1}, { 4, 2}, { 4, 2}, { 2, 1}, { -1,-1},{ -7, -6},
              {-16,-11}, {-12,-6}, {-9,-4}, {-7,-3}, {-7,-3}, {-9,-4}, {-12,-6},{-16,-11}),
        // King
        psqt!({-8,-42}, {-3,-19},{-33, -3},{-52,  3},{-52,  3},{-33, -3},{ -3,-19},{ -8,-42},
              { 2,-24}, { 8,  1},{-22, 13},{-42, 19},{-42, 19},{-22, 13},{  8,  1},{  2,-24},
              {12,-13}, {18,  8},{-12, 23},{-32, 29},{-32, 29},{-12, 23},{ 18,  8},{ 12,-13},
              {17, -7}, {23, 14},{ -7, 29},{-27, 38},{-27, 38},{ -7, 29},{ 23, 14},{ 17, -7},
              {22,-12}, {28,  9},{ -2, 24},{-22, 33},{-22, 33},{ -2, 24},{ 28,  9},{ 22,-12},
              {25,-18}, {31,  3},{  1, 18},{-19, 24},{-19, 24},{  1, 18},{ 31,  3},{ 25,-18},
              {28,-29}, {33, -4},{  4,  8},{-16, 14},{-16, 14},{  4,  8},{ 33, -4},{ 28,-29},
              {31,-62}, {36,-39},{  6,-23},{-14,-17},{-14,-17},{  6,-23},{ 36,-39},{ 31,-62})
    ];
    let mut psqt = [[PhaseScore{ mg: 0, eg: 0}; 64]; 15];
    for pt in board::each_piece_type() {
        let bp = Piece::new(Color::Black, pt);
        let wp = Piece::new(Color::White, pt);
        for sq in board::each_square() {
            psqt[wp.index()][sq.index()] = PIECE_SCORE[wp.index()] + psqt_base[pt.index()][sq.flip().index()];
            psqt[bp.index()][sq.flip().index()] = -psqt[wp.index()][sq.index()]
        }
    }
    psqt
}
