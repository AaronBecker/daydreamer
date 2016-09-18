use std::sync::atomic;

static C960: atomic::AtomicBool = atomic::ATOMIC_BOOL_INIT;

pub fn set_c960(x: bool) {
    C960.store(x, atomic::Ordering::SeqCst);
}

// Are we playing Chess960?
pub fn c960() -> bool {
    C960.load(atomic::Ordering::SeqCst)
}

// The Arena frontend has its own idea about how to handle castling in
// Chess960.
pub fn arena_960_castling() -> bool {
    false
}

pub fn time_buffer() -> u32 {
    10
}

pub fn multi_pv() -> usize {
    1
}
