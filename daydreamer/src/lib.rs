extern crate rand;
#[macro_use]
extern crate lazy_static;
extern crate board;

#[macro_use]
pub mod macros;
pub mod bitboard;
pub mod eval;
pub mod movegen;
pub mod movement;
pub mod options;
pub mod perft;
pub mod position;
pub mod score;
pub mod search;
pub mod transposition;
pub mod uci;
