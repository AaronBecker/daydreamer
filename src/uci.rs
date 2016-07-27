use std::io::{stdin, BufRead};
use std::sync::mpsc;
use std::thread;
use std::time;

use movement;
use movement::Move;
use perft;
use position;
use search::SearchData;


pub fn input_loop() {
    let (tx, rx) = mpsc::channel();
    let mut search_data = SearchData::new(rx);

    thread::spawn(move || { read_input_forever(tx) });
    loop {
        match search_data.uci_channel.try_recv() {
            Ok(command) => match handle_command(&mut search_data, command.as_str()) {
                Ok(_) => (),
                Err(err) => {
                    println!("info string ignoring input '{}': {}", command, err);
                }
            },
            Err(mpsc::TryRecvError::Empty) => thread::sleep(time::Duration::from_millis(10)),
            Err(mpsc::TryRecvError::Disconnected) => panic!("Broken connection to stdin"),
        }
    }
}

fn read_input_forever(chan: mpsc::Sender<String>) {
    let stdin = stdin();
    for line in stdin.lock().lines() {
        match line {
            Ok(s) => chan.send(s).unwrap(),
            _ => (),
        }
    }
}

fn handle_command(search_data: &mut SearchData, line: &str) -> Result<(), String> {
    let mut tokens = line.split_whitespace();
    loop {
        match tokens.next() {
            Some("uci") => {
                println!("id name baru 0");
                println!("id author Aaron Becker");
                println!("uciok");
            }
            Some("isready") => {
                println!("readyok");
                return Ok(());
            },
            Some("debug") => return Ok(()),
            Some("register") => return Ok(()),
            Some("ucinewgame") => return Ok(()),
            Some("position") => return handle_position(search_data, &mut tokens),
            Some("go") => return Ok(()),
            Some("stop") => return Ok(()),
            Some("ponderhit") => return Ok(()),
            Some("quit") => ::std::process::exit(0),
            // Custom extensions
            Some("perft") => return handle_perft(search_data, &mut tokens),
            Some("divide") => return handle_divide(search_data, &mut tokens),
            Some("print") => return handle_print(search_data, &mut tokens),
            Some(unknown) => try!(make_move(search_data, unknown)),
            None => return Ok(()),
        }
    }
}

fn handle_position<'a, I>(search_data: &mut SearchData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    match tokens.next() {
        Some("startpos") => {
            try!(search_data.root_data.pos.load_fen(position::START_FEN));
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
                try!(search_data.root_data.pos.load_fen(fen.as_str()));
            }
        },
        Some(x) => return Err(format!("unrecognized token '{}'", x)),
        None => return Err("input ended unexpectedly".to_string()),
    }

    // Remaining tokens should be moves.
    let ad = position::AttackData::new(&search_data.root_data.pos);
    while let Some(tok) = tokens.next() {
        // handle moves
        let m = Move::from_uci(&search_data.root_data.pos, &ad, tok);
        if m == movement::NO_MOVE {
            return Err(format!("invalid move {}", tok));
        }
        search_data.root_data.pos.do_move(m, &ad);
    }

    Ok(())
}

fn handle_go<'a, I>(search_data: &mut SearchData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    Ok(())
}

fn make_move(search_data: &mut SearchData, mv: &str) -> Result<(), String> {
    let ad = position::AttackData::new(&search_data.root_data.pos);
    let m = Move::from_uci(&search_data.root_data.pos, &ad, mv);
    if m == movement::NO_MOVE {
        return Err(format!("unrecognized token {}", mv));
    }
    search_data.root_data.pos.do_move(m, &ad);
    Ok(())
}

fn handle_perft<'a, I>(search_data: &mut SearchData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    match tokens.next() {
        Some(depth) => {
            let d = try!(depth.parse::<u32>().map_err(|e| e.to_string()));
            let t1 = ::time::precise_time_ns();
            let count = perft::perft(&mut search_data.root_data.pos, d);
            let t2 = ::time::precise_time_ns();
            println!("{} ({} ms, {} nodes/s", count, (t2 - t1) / 1_000_000, count * 1_000_000_000 / (t2 - t1));
        },
        None => return Err("input ended with no depth".to_string()),
    }
    Ok(())
}

fn handle_divide<'a, I>(search_data: &mut SearchData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    match tokens.next() {
        Some(depth) => {
            let d = try!(depth.parse::<u32>().map_err(|e| e.to_string()));
            println!("{}", perft::divide(&mut search_data.root_data.pos, d));
        },
        None => return Err("input ended with no depth".to_string()),
    }
    Ok(())
}

fn handle_print<'a, I>(search_data: &mut SearchData, _tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    println!("{}", search_data.root_data.pos.debug_string());
    Ok(())
}
