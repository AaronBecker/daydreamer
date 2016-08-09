use board::PieceType;
use position::Position;

pub type RawScore = i16;
pub type Phase = i16;

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Score {
    pub mg: RawScore,
    pub eg: RawScore,
}

pub const NONE: Score = Score{ mg: 0, eg: 0 };
pub const PAWN: Score = Score{ mg: 85, eg: 115 };
pub const KNIGHT: Score = Score{ mg: 350, eg: 400 };
pub const BISHOP: Score = Score{ mg: 350, eg: 400 };
pub const ROOK: Score = Score{ mg: 500, eg: 650 };
pub const QUEEN: Score = Score{ mg: 1000, eg: 1200 };

const PT_SCORE: [Score; 8] = [NONE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, NONE, NONE];
const PT_PHASE: [Phase; 8] = [NONE.mg, NONE.mg, KNIGHT.mg, BISHOP.mg, ROOK.mg, QUEEN.mg, NONE.mg, NONE.mg];
const MAX_PHASE: RawScore = 2 * (2 * (KNIGHT.mg + BISHOP.mg + ROOK.mg) + QUEEN.mg);

impl Score {
    pub fn interpolate(self, pos: &Position) -> RawScore {
        let phase = clamp!(pos.phase(), 0, MAX_PHASE);
        (self.eg * (MAX_PHASE - phase) + self.mg * phase) / MAX_PHASE
    }

    pub fn value(pt: PieceType) -> Score {
        PT_SCORE[pt.index()]
    }

    pub fn phase(pt: PieceType) -> Phase {
        PT_PHASE[pt.index()]
    }
}

impl ::std::ops::Add for Score {
    type Output = Score;

    fn add(self, rhs: Score) -> Score {
        Score {
            mg: self.mg + rhs.mg,
            eg: self.eg + rhs.eg,
        }
    }
}

impl ::std::ops::AddAssign for Score {
    fn add_assign(&mut self, rhs: Score) {
        self.mg += rhs.mg;
        self.eg += rhs.eg;
    }
}

impl ::std::ops::Sub for Score {
    type Output = Score;

    fn sub(self, rhs: Score) -> Score {
        Score {
            mg: self.mg - rhs.mg,
            eg: self.eg - rhs.mg,
        }
    }
}

impl ::std::ops::SubAssign for Score {
    fn sub_assign(&mut self, rhs: Score) {
        self.mg -= rhs.mg;
        self.eg -= rhs.eg;
    }
}

