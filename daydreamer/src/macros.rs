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

macro_rules! clamp {
    ($x:expr, $min:expr, $max:expr) => {{
        if $x < $min {
            $min
        } else if $x > $max {
            $max
        } else {
            $x
        }
    }};
}

// The bitboard you get from OR'ing together any number of elements that are
// convertable to a bitboard.
macro_rules! bb {
    ( $( $x:expr ),* ) => {
        {
            use crate::bitboard::IntoBitboard;
            let mut ret = 0;
            $(
                ret |= $x.into_bitboard();
            )*
            ret
        }
    };
}

// Convenience macro for populating tables of phase scores.
macro_rules! sc {
    ( $mg:expr, $eg:expr ) => {{
        use crate::score::PhaseScore;
        PhaseScore { mg: $mg, eg: $eg }
    }};
}

#[cfg(test)]
macro_rules! chess_test {
    ($id:ident, $b:block) => {
        #[test]
        fn $id() {
            use crate::position;
            position::initialize();
            $b
        }
    };
}
