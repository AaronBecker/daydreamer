use board::{Piece, PieceType};
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
    s <= MAX_SCORE && s >= MIN_SCORE
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

const PIECE_SCORE: [PhaseScore; 16] = [NONE, BPAWN, BKNIGHT, BBISHOP, BROOK, BQUEEN, NONE, NONE,
                                       NONE,  PAWN,  KNIGHT,  BISHOP,  ROOK,  QUEEN, NONE, NONE];
const PT_PHASE: [Phase; 8] = [
    NONE.mg as Phase, NONE.mg as Phase, KNIGHT.mg as Phase,
    BISHOP.mg as Phase, ROOK.mg as Phase, QUEEN.mg as Phase,
    NONE.mg as Phase, NONE.mg as Phase];
const MAX_PHASE: Phase = 2 * (2 * (KNIGHT.mg + BISHOP.mg + ROOK.mg) + QUEEN.mg) as Phase;

pub fn mg_material(pt: PieceType) -> Score {
    PIECE_SCORE[pt.index()].mg
}

impl PhaseScore {
    pub fn interpolate(self, pos: &Position) -> Score {
        let phase = clamp!(pos.phase(), 0, MAX_PHASE);
        (self.eg * (MAX_PHASE - phase) + self.mg * phase) / MAX_PHASE
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
            eg: self.eg - rhs.mg,
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
