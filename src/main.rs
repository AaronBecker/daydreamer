#![allow(dead_code)]

extern crate rand;
extern crate time;

#[macro_use] pub mod macros;
pub mod board;
pub mod bitboard;
pub mod movement;
pub mod movegen;
pub mod options;
pub mod perft;
pub mod position;
pub mod uci;

fn main() {
    bitboard::initialize();
    uci::input_loop();
}
