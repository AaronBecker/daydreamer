use movement::{Move, NO_MOVE};
use position::HashKey;
use score::{Score, ScoreType};
use search::SearchDepth;


// TODO: experiment with Entry/Bucket size for the purposes of cache tuning.
// It may be beneficial to add padding so that buckets are cache-aligned.
// TODO: experiment with collision detection. I think that very weak detection
// is plenty in practice and we don't need elaborate tests to verify that
// moves returned by the table are pseudo-legal. Experimental validiation
// would be nice though. We can also add a supplementary verification hash
// if needed or as part of testing. Note that issues are more likely with
// large tables and very large searches, so they are less likely to arise in
// fast time control strenth tests.
#[derive(Clone, Copy)]
pub struct Entry {
    key: u32,
    m: Move,
    score: i16,
    generation: u8,
    depth: u8,
    score_type: ScoreType,
    // TODO: when we have a proper eval, add static eval.
}

impl Entry {
    pub fn new() -> Entry {
        Entry {
            key: 0,
            m: NO_MOVE,
            score: 0,
            generation: 0,
            depth: 0,
            score_type: 0,
        }
    }
}

const BUCKET_SIZE: usize = 4;

#[derive(Clone, Copy)]
pub struct Bucket {
    entries: [Entry; BUCKET_SIZE],
}

impl Bucket {
    pub fn new() -> Bucket {
        Bucket {
            entries: [Entry::new(); BUCKET_SIZE],
        }
    }
}

pub struct Table {
    table: Vec<Bucket>,
    buckets: usize,
    generation: u8,
    // TODO: add stats
}

// Note: right now we're using one big table and returning references to
// entries. To support parallel search we'll likely need to move to locked
// shards and return copies of entries to make the lifetime management
// feasible while controlling lock contention. Implementing the table this
// way up front will give a more fair comparison between serial and parallel
// implementations, assuming I do end up implementing parallel search.
impl<'a> Table {
    pub fn new(bytes: usize) -> Table {
        let mut buckets = 1;
        while (buckets << 1) * ::std::mem::size_of::<Bucket>() < bytes {
            buckets = buckets << 1;
        }
        Table {
            table: vec![Bucket::new(); buckets],
            buckets: buckets,
            generation: 0,
        }
    }

    pub fn new_generation(&mut self) {
        self.generation = self.generation.wrapping_add(1);
    }

    pub fn get(&'a mut self, key: HashKey) -> Option<&'a Entry> {
        let bucket: &mut Bucket = &mut self.table[key as usize & (self.buckets - 1)];
        let short_key = (key >> 32) as u32;
        for i in 0..BUCKET_SIZE {
            if short_key == bucket.entries[i].key {
                return Some(&mut bucket.entries[i]);
            }
        }
        None
    }

    pub fn put(&mut self, key: HashKey, m: Move, d: SearchDepth, s: Score, st: ScoreType) {
        let bucket: &mut Bucket = &mut self.table[key as usize & (self.buckets - 1)];
        let short_key = (key >> 32) as u32;
        let mut index = BUCKET_SIZE;
        for i in 0..BUCKET_SIZE {
            if short_key == bucket.entries[i].key || bucket.entries[i].key == 0 {
                index = i;
                break;
            }
        }
        if index == BUCKET_SIZE {
            let mut min_score = i16::max_value();
            for i in 0..BUCKET_SIZE {
                // Note the wrapping behavior here: we allow generations to wrap
                // so that age doesn't break when our generation number wraps, but
                // the total score should never over- or under-flow an 16.
                let entry_score = bucket.entries[i].depth as i16 -
                    self.generation.wrapping_sub(bucket.entries[i].generation) as i16;
                if entry_score < min_score {
                    min_score = entry_score;
                    index = i;
                }
            }
        }

        // TODO: test ignoring the write if the depth is shallower (maybe by some
        // margin) than the existing entry and we're updating an existing entry
        debug_assert!(index < BUCKET_SIZE);
        debug_assert!(s >= i16::min_value() as i32 && s <= i16::max_value() as i32);
        bucket.entries[index].key = short_key;
        bucket.entries[index].m = m;
        bucket.entries[index].score = s as i16;
        bucket.entries[index].generation = self.generation;
        bucket.entries[index].depth = d as u8;
        bucket.entries[index].score_type = st;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use board::Piece;
    use movement::Move;
    use score;

    chess_test!(test_tt_replacement, {
        let table_size = 4 << 20;
        let mut tt = Table::new(table_size);
        let key1 = 0x1c0debaddeadbeef;
        let (score1, depth1, st1) = (1, 10., score::EXACT);
        use board::Square::*;
        let move1 = Move::new(E2, E4, Piece::WP, Piece::NoPiece);
        tt.put(key1, move1, depth1, score1, st1);

        if let Some(entry) = tt.get(key1) {
            assert_eq!(entry.m, move1);
            assert_eq!(entry.score as i32, score1);
            assert_eq!(entry.score_type, st1);
            assert_eq!(entry.depth, depth1 as u8);
        } else {
            assert!(false);
        }

        // Create a colliding key.
        let key2 = key1 + (1 << 60);
        let (score2, depth2, st2) = (2, 20., score::AT_MOST);
        let move2 = Move::new(D2, D4, Piece::WP, Piece::NoPiece);
        tt.put(key2, move2, depth2, score2, st2);

        if let Some(entry) = tt.get(key1) {
            assert_eq!(entry.m, move1);
            assert_eq!(entry.score as i32, score1);
            assert_eq!(entry.score_type, st1);
            assert_eq!(entry.depth, depth1 as u8);
        } else {
            assert!(false);
        }

        if let Some(entry) = tt.get(key2) {
            assert_eq!(entry.m, move2);
            assert_eq!(entry.score as i32, score2);
            assert_eq!(entry.score_type, st2);
            assert_eq!(entry.depth, depth2 as u8);
        } else {
            assert!(false);
        }

        tt.new_generation();

        // Fill the bucket.
        tt.put(key2 + (1 << 60), move2, depth2, score2, st2);
        tt.put(key2 + (2 << 60), move2, depth2, score2, st2);

        // Update to existing entry.
        tt.put(key1, move1, depth1 + 1., score1 + 1, st1);
        if let Some(entry) = tt.get(key1) {
            assert_eq!(entry.m, move1);
            assert_eq!(entry.score as i32, score1 + 1);
            assert_eq!(entry.score_type, st1);
            assert_eq!(entry.depth, depth1 as u8 + 1);
        } else {
            assert!(false);
        }

        // Collide, replacement based on depth.
        tt.put(key2 + (3 << 60), move1, depth2, score1 + 2, st1);
        if let Some(_) = tt.get(key1) {
            assert!(false);
        }

        // Collide, replacement based on age.
        tt.put(key2 + (4 << 60), move1, depth2, score1 + 3, st1);
        if let Some(_) = tt.get(key2) {
            assert!(false);
        }
        if let Some(entry) = tt.get(key2 + (4 << 60)) {
            assert_eq!(entry.m, move1);
            assert_eq!(entry.score as i32, score1 + 3);
            assert_eq!(entry.score_type, st1);
            assert_eq!(entry.depth, depth2 as u8);
        } else {
            assert!(false);
        }

    });
}
