use std::io::{stdin, BufRead};

use movement;
use movement::Move;
use perft;
use position;
use position::Position;

const START_FEN: &'static str = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// note: this should probably move to the search eventually
pub struct RootData {
    pos: Position,
}

impl RootData {
    pub fn new() -> RootData {
        RootData {
            pos: Position::from_fen(START_FEN),
        }
    }
}

pub fn input_loop() {
    // note: we'll eventually need some root data that's more than just a position here
    let mut root_data = RootData::new();
    let stdin = stdin();
    for line in stdin.lock().lines() {
        match line {
            Ok(s) => {
                match handle_line(&mut root_data, s.as_str()) {
                    Ok(_) => (),
                    Err(err) => {
                        println!("info string ignoring input '{}': {}", s, err);
                    }
                }
            }
            _ => (),
        }
    }
}

fn handle_line(root_data: &mut RootData, line: &str) -> Result<(), String> {
    let mut tokens = line.split_whitespace();
    loop {
        match tokens.next() {
            Some("isready") => {
                println!("readyok");
                return Ok(());
            },
            Some("perft") => return handle_perft(root_data, &mut tokens),
            Some("divide") => return handle_divide(root_data, &mut tokens),
            Some("position") => return handle_position(root_data, &mut tokens),
            Some("print") => return handle_print(root_data, &mut tokens),
            Some("quit") => ::std::process::exit(0),
            Some(unknown) => try!(make_move(root_data, unknown)),
            None => return Ok(()),
        }
    }
}

fn handle_position<'a, I>(root_data: &mut RootData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    match tokens.next() {
        Some("startpos") => {
            try!(root_data.pos.load_fen(START_FEN));
        },
        Some("fen") => {
            let mut fen = String::new();
            while let Some(tok) = tokens.next() {
                if tok == "moves" {
                    break;
                }
                fen.push_str(tok);
                fen.push_str(" ");
            }
            if fen.len() > 0 {
                try!(root_data.pos.load_fen(fen.as_str()));
            }
        },
        Some(x) => return Err(format!("unrecognized token '{}'", x)),
        None => return Err("input ended unexpectedly".to_string()),
    }

    // Remaining tokens should be moves.
    let ad = position::AttackData::new(&root_data.pos);
    while let Some(tok) = tokens.next() {
        // handle moves
        let m = Move::from_uci(&root_data.pos, &ad, tok);
        if m == movement::NO_MOVE {
            return Err(format!("invalid move {}", tok));
        }
        root_data.pos.do_move(m, &ad);
    }

    Ok(())
}

fn make_move(root_data: &mut RootData, mv: &str) -> Result<(), String> {
    let ad = position::AttackData::new(&root_data.pos);
    let m = Move::from_uci(&root_data.pos, &ad, mv);
    if m == movement::NO_MOVE {
        return Err(format!("unrecognized token {}", mv));
    }
    root_data.pos.do_move(m, &ad);
    Ok(())
}

fn handle_perft<'a, I>(root_data: &mut RootData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    match tokens.next() {
        Some(depth) => {
            let d = try!(depth.parse::<u32>().map_err(|e| e.to_string()));
            let t1 = ::time::precise_time_ns();
            let count = perft::perft(&mut root_data.pos, d);
            let t2 = ::time::precise_time_ns();
            println!("{} ({} ms, {} nodes/s", count, (t2 - t1) / 1_000_000, count * 1_000_000_000 / (t2 - t1));
        },
        None => return Err("input ended with no depth".to_string()),
    }
    Ok(())
}

fn handle_divide<'a, I>(root_data: &mut RootData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    match tokens.next() {
        Some(depth) => {
            let d = try!(depth.parse::<u32>().map_err(|e| e.to_string()));
            println!("{}", perft::divide(&mut root_data.pos, d));
        },
        None => return Err("input ended with no depth".to_string()),
    }
    Ok(())
}


fn handle_print<'a, I>(root_data: &mut RootData, _tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    println!("{}", root_data.pos.debug_string());
    Ok(())
}
