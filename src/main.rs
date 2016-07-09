#![allow(dead_code)]
pub mod board;
pub mod bitboard;

use board::*;
use bitboard::{bb, bb_to_str, bb_from_str};

fn main() {
    bitboard::initialize();
    println!("Hello, world!");
    println!("{}", bb_to_str(bb(Rank::_1)));
    println!("{}", bb(File::A));
    println!("{}", bb_to_str(bb(File::A)));
    println!("{}", bb_from_str(bb_to_str(bb(File::A)).as_str()));
    for sq in each_square() {
        print!("{}", sq);
    }
    println!("");
    for c in each_color() {
        println!("{:?}", c)
    }
    println!("{}", "e5".parse::<Square>().unwrap())
}
