use board::{Color, PieceType};
use position::Position;
use score;
use score::Score;

pub fn full(pos: &Position) -> Score {
    let ps = pos.psqt_score();

    let mut s = ps.interpolate(pos);
    if !can_win(pos.us(), pos) {
        s = min!(s, score::DRAW_SCORE);
    }
    if !can_win(pos.them(), pos) {
        s = max!(s, score::DRAW_SCORE);
    }
    s
}

fn can_win(side: Color, pos: &Position) -> bool {
    !(pos.pieces_of_color_and_type(side, PieceType::Pawn) == 0 &&
      pos.non_pawn_material(side) < score::mg_material(PieceType::Rook))
}
