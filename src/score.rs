use board::PieceType;
use position::Position;

pub type Score = i16;
pub type Phase = i16;

pub const MATE_SCORE: Score = 30000;
pub const MAX_SCORE: Score = MATE_SCORE + 1;
pub const MIN_SCORE: Score = -MAX_SCORE;
pub const DRAW_SCORE: Score = 0;

pub fn mate_in(ply: u8) -> Score {
    MATE_SCORE - ply as i16
}

pub fn mated_in(ply: u8) -> Score {
    ply as i16 - MATE_SCORE
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

const PT_SCORE: [PhaseScore; 8] = [NONE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, NONE, NONE];
const PT_PHASE: [Phase; 8] = [NONE.mg, NONE.mg, KNIGHT.mg, BISHOP.mg, ROOK.mg, QUEEN.mg, NONE.mg, NONE.mg];
const MAX_PHASE: Score = 2 * (2 * (KNIGHT.mg + BISHOP.mg + ROOK.mg) + QUEEN.mg);

impl PhaseScore {
    pub fn interpolate(self, pos: &Position) -> Score {
        let phase = clamp!(pos.phase(), 0, MAX_PHASE);
        (self.eg * (MAX_PHASE - phase) + self.mg * phase) / MAX_PHASE
    }

    pub fn value(pt: PieceType) -> PhaseScore {
        PT_SCORE[pt.index()]
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
