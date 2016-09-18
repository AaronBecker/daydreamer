use std::sync::atomic;

static C960: atomic::AtomicBool = atomic::ATOMIC_BOOL_INIT;

// Are we playing Chess960?
pub fn c960() -> bool {
    C960.load(atomic::Ordering::SeqCst)
}

pub fn set_c960(x: bool) {
    C960.store(x, atomic::Ordering::SeqCst);
}

static ARENA_CASTLE: atomic::AtomicBool = atomic::ATOMIC_BOOL_INIT;

// The Arena frontend has its own idea about how to handle castling in
// Chess960.
pub fn arena_960_castling() -> bool {
    ARENA_CASTLE.load(atomic::Ordering::SeqCst)
}

pub fn set_arena_960_castling(x: bool) {
    ARENA_CASTLE.store(x, atomic::Ordering::SeqCst);
}

static MULTI_PV: atomic::AtomicUsize = atomic::ATOMIC_USIZE_INIT;

pub fn multi_pv() -> usize {
    MULTI_PV.load(atomic::Ordering::SeqCst)
}

pub fn set_multi_pv(x: usize) {
    MULTI_PV.store(x, atomic::Ordering::SeqCst);
}

pub fn time_buffer() -> u32 {
    10
}

