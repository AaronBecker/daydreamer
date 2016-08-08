use board::PieceType;
use position::Position;

pub type RawScore = i16;

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Score {
    pub mg: RawScore,
    pub eg: RawScore,
}

impl Score {
    pub fn interpolate(self, _: &Position) {
        unimplemented!()
    }

    pub fn value(pt: PieceType) -> Score {
        unimplemented!()
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
