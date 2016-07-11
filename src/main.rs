#![allow(dead_code)]

extern crate rand;
extern crate time;

#[macro_use] pub mod macros;
pub mod board;
pub mod bitboard;
pub mod movement;
pub mod options;
pub mod position;

fn main() {
    bitboard::initialize();
}
