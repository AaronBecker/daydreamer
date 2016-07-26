use std::io::{stdin, BufRead};
use std::sync::mpsc;
use std::thread;
use std::time;
use std::time::Duration;

use board;
use movement;
use movement::Move;
use options;
use perft;
use position;
use position::Position;

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum EngineState {
    Waiting,
    Stopping,
    Searching,
}

pub struct Constraints {
    infinite: bool,
    use_timer: bool,
    hard_limit: Duration,
    soft_limit: Duration,
    node_limit: u64,
    depth_limit: u8,
    searchmoves: Vec<Move>,
}

impl Constraints {
    pub fn new() -> Constraints {
        Constraints {
            infinite: false,
            use_timer: false,
            hard_limit: Duration::new(0, 0),
            soft_limit: Duration::new(0, 0),
            node_limit: u64::max_value(),
            depth_limit: u8::max_value(),
            searchmoves: Vec::new(),
        }
    }

    pub fn set_timer(&mut self, us: board::Color, wtime: u32, btime: u32, winc: u32, binc: u32, movetime: u32, movestogo: u32) {
        if wtime == 0 && btime == 0 && winc == 0 && binc == 0 && movetime == 0 {
            self.use_timer = false;
            return;
        }
        self.use_timer = true;
        
        if movetime != 0 {
            self.hard_limit = Duration::from_millis(max!(0, movetime - options::time_buffer()) as u64);
            self.soft_limit = self.hard_limit;
            return;
        }
        let time = if us == board::Color::White { wtime } else { btime };
        let inc = if us == board::Color::White { winc } else { binc };
        let (mut soft_limit, mut hard_limit);
        if movestogo != 0 {
            // x/y time control
            soft_limit = time / clamp!(movestogo, 2, 20);
            hard_limit = if movestogo == 1 {
                max!(time - 250, time / 2)
            } else {
                min!(time / 4, time * 4 / movestogo)
            };
        } else {
            // x+y time control
            soft_limit = time / 30 + inc;
            hard_limit = max!(time / 5, inc - 250);
        }
        soft_limit = min!(soft_limit, time - options::time_buffer());
        hard_limit = min!(hard_limit, time - options::time_buffer());
        self.soft_limit = Duration::from_millis(soft_limit as u64);
        self.hard_limit = Duration::from_millis(hard_limit as u64);
    }

}

// note: this should probably move to the search eventually
pub struct RootData {
    pos: Position,
    state: EngineState,
    uci_channel: mpsc::Receiver<String>,
}

impl RootData {
    pub fn new(uci_channel: mpsc::Receiver<String>) -> RootData {
        RootData {
            pos: Position::from_fen(position::START_FEN),
            state: EngineState::Waiting,
            uci_channel: uci_channel,
        }
    }
}

pub fn input_loop() {
    let (tx, rx) = mpsc::channel();
    let mut root_data = RootData::new(rx);

    thread::spawn(move || { read_input_forever(tx) });
    loop {
        match root_data.uci_channel.try_recv() {
            Ok(command) => match handle_command(&mut root_data, command.as_str()) {
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

fn handle_command(root_data: &mut RootData, line: &str) -> Result<(), String> {
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
            Some("position") => return handle_position(root_data, &mut tokens),
            Some("go") => return Ok(()),
            Some("stop") => return Ok(()),
            Some("ponderhit") => return Ok(()),
            Some("quit") => ::std::process::exit(0),
            // Custom extensions
            Some("perft") => return handle_perft(root_data, &mut tokens),
            Some("divide") => return handle_divide(root_data, &mut tokens),
            Some("print") => return handle_print(root_data, &mut tokens),
            Some(unknown) => try!(make_move(root_data, unknown)),
            None => return Ok(()),
        }
    }
}

fn handle_position<'a, I>(root_data: &mut RootData, tokens: &mut I) -> Result<(), String>
        where I: Iterator<Item=&'a str> {
    match tokens.next() {
        Some("startpos") => {
            try!(root_data.pos.load_fen(position::START_FEN));
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
