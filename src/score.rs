use board::PieceType;
use position::Position;

pub const NO_VAL: Score = Score{ mg: 0, eg: 0 };
pub const PAWN_VAL: Score = Score{ mg: 85, eg: 115 };
pub const KNIGHT_VAL: Score = Score{ mg: 350, eg: 400 };
pub const BISHOP_VAL: Score = Score{ mg: 350, eg: 400 };
pub const ROOK_VAL: Score = Score{ mg: 500, eg: 650 };
pub const QUEEN_VAL: Score = Score{ mg: 1000, eg: 1200 };

pub const MAX_PHASE: RawScore = 2 * (2 * (KNIGHT_VAL.mg + BISHOP_VAL.mg + ROOK_VAL.mg) + QUEEN_VAL.mg);

pub const PT_VALS: [Score; 8] = [NO_VAL, PAWN_VAL, KNIGHT_VAL, BISHOP_VAL,ROOK_VAL, QUEEN_VAL, NO_VAL, NO_VAL];

pub type RawScore = i16;

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Score {
    pub mg: RawScore,
    pub eg: RawScore,
}

impl Score {
    pub fn interpolate(self, pos: &Position) -> RawScore {
        let phase = clamp!(pos.phase(), 0, MAX_PHASE);
        (self.eg * (MAX_PHASE - phase) + self.mg * phase) / MAX_PHASE
    }

    pub fn value(pt: PieceType) -> Score {
        PT_VALS[pt.index()]
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

impl ::std::ops::Sub for Score {
    type Output = Score;

    fn sub(self, rhs: Score) -> Score {
        Score {
            mg: self.mg - rhs.mg,
            eg: self.eg - rhs.mg,
        }
    }
}
