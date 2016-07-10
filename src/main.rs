#![allow(dead_code)]

extern crate rand;
extern crate time;

pub mod board;
pub mod bitboard;

fn main() {
    bitboard::initialize();
}
