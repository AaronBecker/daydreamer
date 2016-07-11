#[macro_use]

// The bitboard you get from OR'ing together any number of elements tat are
// convertable to a bitboard.
macro_rules! all_bb {
    ( $( $x:expr ),* ) => {
        {
            let mut ret: Bitboard = 0;
            $(
                ret |= $x.into_bitboard();
            )*
            ret
        }
    };
}

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
