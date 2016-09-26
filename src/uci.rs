use std::fs::File;
use std::io::{stdin, BufRead, BufReader};
use std::sync::mpsc;
use std::thread;
use std::time;

use movement;
use movement::Move;
use perft;
use position;
use search;
use search::SearchData;

// in_millis converts a duration to integer milliseconds. It's always at least
// 1, to avoid divide-by-zero errors.
pub fn in_millis(d: &time::Duration) -> u64 {
    1 + d.as_secs() * 1000 + d.subsec_nanos() as u64 / 1_000_000
}

pub fn read_stream(search_data: &mut SearchData, script: Option<String>) {
    let (tx, rx) = mpsc::channel();
    search_data.uci_channel = rx;
    let engine_state = search_data.state.clone();

    thread::spawn(move || { read_file_or_stdin(script, tx, engine_state) });
    loop {
        match search_data.uci_channel.try_recv() {
            Ok(command) => match handle_command(search_data, command.as_str()) {
                Ok(_) => (),
                Err(err) => {
                    println!("info string ignoring input '{}': {}", command, err);
                }
            },
            Err(mpsc::TryRecvError::Empty) => thread::sleep(time::Duration::from_millis(1)),
            Err(mpsc::TryRecvError::Disconnected) => return,
        }
    }
}

fn read_file_or_stdin(input: Option<String>, chan: mpsc::Sender<String>, state: search::EngineState) {
    match input {
        Some(filename) => {
            let filename_copy = filename.clone();
            match File::open(filename) {
                Ok(f) => consume_stream(BufReader::new(f), chan, state),
                Err(e) => {
                    println!("couldn't open file '{}': {}", filename_copy, e);
                    println!("reading from stdin");
                    let stdin = stdin();
                    consume_stream(stdin.lock(), chan, state);
                },
            }
        },
        None => {
            let stdin = stdin();
            consume_stream(stdin.lock(), chan, state);
        }
    }
}

fn consume_stream<T: BufRead>(stream: T, chan: mpsc::Sender<String>, state: search::EngineState) {
    for line in stream.lines() {
        match line {
            Ok(s) => {
                match s.split_whitespace().next() {
                    Some("isready") => println!("readyok"),
                    Some("wait") => {
                        // Note: we sleep at least once to make sure that the
                        // previous command has a chance to start executing.
                        thread::sleep(time::Duration::from_millis(5));
                        while state.load() != search::WAITING_STATE {
                            thread::sleep(time::Duration::from_millis(5));
                        }
                    },
                    Some("stop") => state.enter(search::STOPPING_STATE),
                    Some("ponderhit") => {
                        if state.load() == search::PONDERING_STATE {
                            state.enter(search::SEARCHING_STATE)
                        }
                    },
                    Some("quit") => ::std::process::exit(0),
                    Some(_) => {
                        match state.load() {
                            // If the engine is searching, we handle commands
                            // ourselves. Only a few commands are valid during a
                            // search. This lets us signal the searching thread
                            // to stop without doing command processing in the
                            // search thread.
                            search::SEARCHING_STATE | search::PONDERING_STATE => {
                                println!("info string busy searching, ignoring command '{}'", s);
                            },
                            // If we're not actively searching, the main thread
                            // will handle the command.
                            search::WAITING_STATE | search::STOPPING_STATE => chan.send(s).unwrap(),
                            _ => panic!("unrecognized engine state: {}", state.load()),
                        }
                    }
                    None => (),
                }
            }
            _ => return,
        }
    }
}

fn handle_command(search_data: &mut SearchData, line: &str) -> Result<(), String> {
    let mut tokens = line.split_whitespace();
    loop {
        match tokens.next() {
            Some("uci") => {
                println!("id name Daydreamer {} ({})",
                         env!("CARGO_PKG_VERSION"),
                         include_str!(concat!(env!("OUT_DIR"), "/version.rs")));
                println!("id author Aaron Becker");
                println!("option name Hash type spin default 64 min 1 max 65536");
                println!("option name Ponder type check default false");
                println!("option name UCI_Chess960 type check default false");
                println!("option name Arena 960 castling type check default false");
                println!("option name MultiPV type spin default 1 min 1 max 255");
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
            Some("go") => return handle_go(search_data, &mut tokens),
            Some("stop") => return Ok(()),
            Some("ponderhit") => return Ok(()),
            Some("quit") => ::std::process::exit(0),
            // Custom extensions
            Some("perft") => return handle_perft(search_data, &mut tokens),
            Some("divide") => return handle_divide(search_data, &mut tokens),
            Some("print") => return handle_print(search_data, &mut tokens),
            Some("setoption") => return handle_option(search_data, &mut tokens),
            // Try to interpret unrecognized tokens as moves to make in the
            // current position. This tends to be handy for debugging.
            Some(unknown) => try!(make_move(search_data, unknown)),
            None => return Ok(()),
        }
    }
}

fn handle_position<'a, I>(search_data: &mut SearchData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    match tokens.next() {
        Some("startpos") => {
            try!(search_data.pos.load_fen(position::START_FEN));
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
                try!(search_data.pos.load_fen(fen.as_str()));
            }
        },
        Some(x) => return Err(format!("unrecognized token '{}'", x)),
        None => return Err("input ended unexpectedly".to_string()),
    }

    // Remaining tokens should be moves.
    while let Some(tok) = tokens.next() {
        if tok == "moves" { continue }
        // handle moves
        let ad = position::AttackData::new(&search_data.pos);
        let m = Move::from_uci(&search_data.pos, &ad, tok);
        if m == movement::NO_MOVE {
            return Err(format!("invalid move {}", tok));
        }
        search_data.pos.do_move(m, &ad);
    }

    Ok(())
}

fn parse_u64_or_0(token: &str) -> u64 {
    match token.parse::<u64>() {
        Ok(x) => x,
        Err(_) => {
            println!("info string unrecognized token {}, expected int", token);
            0
        }
    }
}

fn parse_u32_or_0(token: &str) -> u32 {
    parse_u64_or_0(token) as u32
}

fn handle_go<'a, I>(search_data: &mut SearchData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    let (mut wtime, mut winc, mut btime, mut binc) = (0, 0, 0, 0);
    let (mut movestogo, mut movetime) = (0, 0);
    search_data.constraints.clear();
    let mut ptokens = tokens.peekable();
    while let Some(tok) = ptokens.next() {
        match tok {
            "wtime" => if let Some(x) = ptokens.next() { wtime = parse_u32_or_0(x) },
            "winc" => if let Some(x) = ptokens.next() { winc = parse_u32_or_0(x) },
            "btime" => if let Some(x) = ptokens.next() { btime = parse_u32_or_0(x) },
            "binc" => if let Some(x) = ptokens.next() { binc = parse_u32_or_0(x) },
            "movestogo" => if let Some(x) = ptokens.next() { movestogo = parse_u32_or_0(x) },
            "movetime" => if let Some(x) = ptokens.next() { movetime= parse_u32_or_0(x) },
            "depth" => if let Some(x) = ptokens.next() {
                search_data.constraints.depth_limit = parse_u64_or_0(x) as usize;
            },
            "nodes" => if let Some(x) = ptokens.next() {
                search_data.constraints.node_limit = parse_u64_or_0(x);
            },
            "infinite" => search_data.constraints.infinite = true,
            "mate" => if let Some(_) = ptokens.next() {
                println!("info string mate search not supported, ignoring");
            },
            "ponder" => search_data.constraints.ponder = true,
            "searchmoves" => {
                let ad = position::AttackData::new(&search_data.pos);
                loop {
                    // Keep reading the next token as long as it's a valid move.
                    if let Some(tok) = ptokens.peek() {
                        let m = Move::from_uci(&search_data.pos, &ad, tok);
                        if m == movement::NO_MOVE {
                            break
                        }
                        search_data.constraints.searchmoves.push(m);
                    } else {
                        break
                    }
                    ptokens.next();
                }
            },
            _ => println!("info string unrecognized token {}", tok),
        }
    }
    search_data.constraints.set_timer(search_data.pos.us(),
                                      wtime, btime, winc, binc, movetime, movestogo);
    search::go(search_data);
    Ok(())
}

fn make_move(search_data: &mut SearchData, mv: &str) -> Result<(), String> {
    let ad = position::AttackData::new(&search_data.pos);
    let m = Move::from_uci(&search_data.pos, &ad, mv);
    if m == movement::NO_MOVE {
        return Err(format!("unrecognized token {}", mv));
    }
    search_data.pos.do_move(m, &ad);
    Ok(())
}

fn handle_perft<'a, I>(search_data: &mut SearchData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    match tokens.next() {
        Some(depth) => {
            let d = try!(depth.parse::<u32>().map_err(|e| e.to_string()));
            let t1 = time::Instant::now();
            let count = perft::perft(&mut search_data.pos, d);
            let elapsed_ms = in_millis(&t1.elapsed());
            println!("{} ({} ms, {} nodes/s)", count, elapsed_ms, count * 1000 / elapsed_ms);
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
            println!("{}", perft::divide(&mut search_data.pos, d));
        },
        None => return Err("input ended with no depth".to_string()),
    }
    Ok(())
}

fn handle_print<'a, I>(search_data: &mut SearchData, _tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    println!("{}", search_data.pos.debug_string());
    Ok(())
}

fn handle_option<'a, I>(search_data: &mut SearchData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    let mut name = String::new();
    while let Some(tok) = tokens.next() {
        match tok {
            "name" => {},
            "value" => { break },
            t => { name.push_str(t) },
        }
    }
    match name.to_lowercase().as_ref() {
        "hash" => {
            if let Some(t) = tokens.next() {
                let mb = try!(t.parse::<usize>().map_err(|e| e.to_string()));
                if mb > 0 && mb <= 65536 {
                    search_data.tt = ::transposition::Table::new(mb << 20);
                }
            }
        },
        "uci_chess960" => {
            if let Some(t) = tokens.next() {
                let c960 = try!(t.parse::<bool>().map_err(|e| e.to_string()));
                ::options::set_c960(c960);
            }
        },
        "arena960castling" => {
            if let Some(t) = tokens.next() {
                let arena = try!(t.parse::<bool>().map_err(|e| e.to_string()));
                ::options::set_arena_960_castling(arena);
            }
        },
        "multipv" => {
            if let Some(t) = tokens.next() {
                let mpv = try!(t.parse::<usize>().map_err(|e| e.to_string()));
                if mpv > 0 && mpv < 256 {
                    ::options::set_multi_pv(mpv);
                } else {
                    ::options::set_multi_pv(1);
                }
            }
        }
        _ => {
            println!("info string unknown option {}, ignoring", name);
        },
    }
    Ok(())
}

// Options to support:
// maybe a debugging log file and verbosity control
