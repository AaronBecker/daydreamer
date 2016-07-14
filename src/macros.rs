#[macro_use]

// The max of any number of comparable elements.
macro_rules! max {
    ($x:expr) => ( $x );
    ($x:expr, $($xs:expr),+) => {
        {
            use std::cmp::max;
            max($x, max!( $($xs),+ ))
        }
    };
}

// The min of any number of comparable elements.
macro_rules! min {
    ($x:expr) => ( $x );
    ($x:expr, $($xs:expr),+) => {
        {
            use std::cmp::min;
            min($x, min!( $($xs),+ ))
        }
    };
}

// The bitboard you get from OR'ing together any number of elements tat are
// convertable to a bitboard.
macro_rules! bb {
    ( $( $x:expr ),* ) => {
        {
            use bitboard::{IntoBitboard};
            let mut ret = 0;
            $(
                ret |= $x.into_bitboard();
            )*
            ret
        }
    };
}


macro_rules! chess_test {
    ($id:ident, $b:block) => {
        #[test]
        fn $id() {
            bitboard::initialize();
            $b
        }
    };
}
