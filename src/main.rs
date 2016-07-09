#![allow(dead_code)]
pub mod board;

fn main() {
    println!("Hello, world!");
    println!("{}", board::bb(board::Rank::_1));
    println!("{}", board::bb(board::File::A));
    for sq in board::each_square() {
        println!("{}", sq);
    }
    for c in board::each_color() {
        println!("{:?}", c)
    }
}
