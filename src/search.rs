use std::sync::mpsc;
use std::time;
use std::time::{Duration, SystemTime};

use board;
use movement::Move;
use options;
use position;
use position::Position;


#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum EngineState {
    Waiting,
    Stopping,
    Searching,
}

pub struct SearchConstraints {
    pub infinite: bool,
    pub searchmoves: Vec<Move>, // TODO this doesn't seem quite right here, maybe move out
    pub node_limit: u64,
    pub depth_limit: u8,

    use_timer: bool,
    hard_limit: Duration,
    soft_limit: Duration,
    start_time: SystemTime,
}

impl SearchConstraints {
    pub fn new() -> SearchConstraints {
        SearchConstraints {
            infinite: false,
            searchmoves: Vec::new(),
            node_limit: u64::max_value(),
            depth_limit: u8::max_value(),
            use_timer: false,
            hard_limit: Duration::new(0, 0),
            soft_limit: Duration::new(0, 0),
            start_time: time::SystemTime::now(),
        }
    }

    pub fn set_timer(&mut self, us: board::Color, wtime: u32, btime: u32,
                     winc: u32, binc: u32, movetime: u32, movestogo: u32) {
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
    pub pos: Position,
}

impl RootData {
    pub fn new() -> RootData {
        RootData {
            pos: Position::from_fen(position::START_FEN),
        }
    }
}

pub struct SearchStats {
   nodes: u64,
   qnodes: u64,
   pvnodes: u64,
}

impl SearchStats {
    pub fn new() -> SearchStats {
        SearchStats {
            nodes: 0,
            qnodes: 0,
            pvnodes: 0,
        }
    }
}

pub struct SearchData {
    pub root_data: RootData,
    pub constraints: SearchConstraints,
    stats: SearchStats,
    state: EngineState,
    pub uci_channel: mpsc::Receiver<String>,
}

impl SearchData {
    pub fn new(uci_channel: mpsc::Receiver<String>) -> SearchData {
        SearchData {
            root_data: RootData::new(),
            constraints: SearchConstraints::new(),
            stats: SearchStats::new(),
            state: EngineState::Waiting,
            uci_channel: uci_channel,
        }
    }

    pub fn should_stop(&self, depth: u8, nodes: u64) -> bool {
        if self.constraints.infinite {
            return true;
        }
        if depth > self.constraints.depth_limit || nodes > self.constraints.node_limit {
            return true
        }
        if self.constraints.use_timer {
            // TODO: actually use the soft limit
            match self.constraints.start_time.elapsed() {
                Ok(d) => return d > self.constraints.hard_limit,
                Err(_) => return false,
            }
        }
        false
    }
}

pub fn go(search_data: &mut SearchData) {
}
