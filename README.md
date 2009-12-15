
Daydreamer 1.6
==============

Daydreamer is a chess-playing program I've been writing in my spare time. I'm
hoping it will one day be a test bed for some ideas I have on parallel tree
search with distributed memory, but first the serial code needs more work.
I named it Daydreamer after a bug in an early version caused it to occasionally
follow up very strong play with bizarre blunders, as though it had lost its
focus on the game and its mind was wandering aimlessly.

Changes from 1.5 to 1.6
-----------------------

* Chess960 support
* MultiPV support
* Ponder support
* Scorpio bitbase support
* Improved futility pruning
* Razoring
* Aspiration in root search
* Weak square identification
* Open and half-open file identification
* Pawn storm bonuses
* Improved fidelity to uci specifications
* Improved time management, and improved behavior at very fast time controls
* Improved search behavior in positions with short mates or obvious moves
* Fixed a bug that caused losses on time in some sudden death situations
* Fixed a bug that prevented a hash size over 2GB
* Fixed a bug that caused crashes in very long games (over 250 moves)
* Fixed a bug that generated incorrect en passant moves while in check


Using Daydreamer
----------------

Daydreamer uses the universal chess interface (UCI) to communicate with
graphical front-end programs. You can use it directly on the command-line, but
that's not something most people will probably want to do. Daydreamer supports
the UCI specification pretty faithfully, and supports ponder and MultiPV modes.
At startup, it looks for a file named daydreamer.rc, and automatically executes
any UCI commands that it finds inside. You can specifiy a different file to
read from by passing a file name as an argument to the Daydreamer executable.
You can use the rc file to automatically set options every time Daydreamer
starts. For example, in my daydreamer.rc I have the following lines, which
automatically register my Scorpio bitbases:

    setoption name Endgame bitbase path value /Users/abecker/src/chess_engines/egbb/
    setoption name Use endgame bitbases value true


Compiling
---------

If you have a C compiler that can handle C99, this should be easy. Just edit
the CC variable in the Makefile to point at your compiler, set the compile
flags you want in CFLAGS, and you should be good to go. If you compile with
-DCOMPILE_COMMAND, you can pass a string that will be reported when you start
the engine up. I find this pretty helpful for remembering what version I'm
working with. I've tested with gcc on Mac and Linux, clang on Mac, and the
MinGW cross-compiler for Windows. 

Installing
----------

The whole thing is a single executable, so there's nothing to install, really.
Just put it wherever you want. I've included a polyglot.ini file for
compatibility with Winboard interfaces, but aside from that there aren't any
configuration files.

Thanks
------

I'm the only person who actually writes the code of Daydreamer, but the ideas
and structure of the code owe a lot to the chess programming community. I read
through the source of a lot of open source engines before starting this
project, and they've all influenced my design for Daydreamer. In particular,
Fruit by Fabian Letouzey and Glaurung and Viper by Tord Romstad have strongly
influenced the way I approached this project, and Daydreamer would be much
worse without them.

I also had access to a lot of good writing about the design, implementation,
and testing of chess engines. Bruce Moreland's site and the blogs of [Jonatan
Pettersson (Mediocre)](http://mediocrechess.blogspot.com/) and [Matt Gingell
(Chesley)](http://sourceforge.net/apps/wordpress/chesley/) have been
particularly interesting. Thanks also to the authors of the many engines I've
tested against, and to composers of EPD testing suites.

Changes from 1.0 to 1.5
-----------------------

Replaced the standard simplified evaluation with a custom job. The simple
version is still available for testing purposes. In addition to re-doing the
material valuation and piece-square tables, I added pawn evaluation (passed
pawn identification is probably the single largest improvement over version 1
in terms of playing strength) and simple measures of mobility and king safety.

The board representation has been modified for efficiency, and the move
generator is more sophisticated now. I've also completely revamped the move
ordering (probably the second largest improvement) and added futility pruning
to the search. History counters are now used both for futility and move
ordering.

The result is a pretty significant jump in strength. Early on I underestimated
the importance of good move ordering relative to raw nodes per second. For this
release I focused much more energy on reducing the size of the search tree,
with good results. Here are results from some testing matches I ran at 10s per
side to give a rough idea of how the new release of Daydreamer stacks up:

    Diablo 0.5.1    +30-20=0    30.0/50
    Dirty ACCA 3    +32-13=5    34.5/50
    Greko 6.5       +29-10=11   34.5/50
    Sungorus 1.2    +24-13=13   30.5/50
    Romichess P3K   +17-26=7    20.5/50
    Bison 9.6       +5-34=11    10.5/50

and here are scores on two common (relatively easy) suites of problems, solved
at 10s per problem:

    WAC     293/300
    ECMGMP  120/182

Design (1.0)
------------

To over-simplify, all chess engines have two parts: search and evaluation.
Evaluation is the part that looks at the board and figures out who's winning
and by how much.  Search is the part that looks ahead in the game guess how our
opponent would repond to each of our moves, how we'd respond to each of their
responses, and so on.

Daydreamer is almost all search and no evaluation. To evalate a position it
just adds up a value for each piece and a bonus based on which square each
piece is on. The score is the difference between the scores for each player. In
fact, I'm using [an evaluation function]
(http://chessprogramming.wikispaces.com/Simplified+evaluation+function)
specially designed to be as simple and straight-forward as possible. Daydreamer
has no idea about keeping its king safe, or mobilizing its pieces, or keeping a
good pawn structure. Maybe someday I'll teach it some of these things, but
Daydreamer can beat me at chess, so what do I know.

Daydreamer's search is more sophisticated. It uses [principal variation search]
(http://en.wikipedia.org/wiki/Negascout), a variant of classical alpha-beta
search.  A transposition table stores information about search nodes we've
encountered previously. There are a number of extension, reduction, and pruning
heuristics, and a quiescence search to avoid terminating the search in a
position that has easy tactics on the board. It gets a lot of strength from the
[null move heuristic] (http://en.wikipedia.org/wiki/Null-move_heuristic) and
[late move reductions] (http://www.glaurungchess.com/lmr.html). I've also
implemented razoring and futility pruning, although I haven't quite figured out
how to make the most of them yet.

Strength (1.0)
--------------

So, how strong is Daydreamer? Strong enough to beat me, certainly. It's not
terribly strong yet for a chess engine, but it's progressing pretty quickly. I
run tests against a bunch of other chess engines whenever I make big changes to
make sure that I'm not doing anything catastrophic, so I've found quite a few
engines that are close to Daydreamer in strength. Here are the results from a
series of 10 game matches with 30s per side that I did for the 1.0 release:

    BigLion 2.23x   +8-1=1  8.5/10
    BikJump 2.01    +3-6=1  3.5/10
    Clarabit 1.00   +3-4=3  4.5/10
    Greko 6.5       +1-5=4  3.0/10
    Mediocre 0.34   +4-6=0  4.0/10
    Plisk 0.0.9     +8-0=2  9.0/10
    Sungorus 1.2    +2-7=1  2.5/10

I'm pretty pleased with these results so far. Daydreamer plays pretty terribly
in complicated endgames (and sometimes painfully badly even in simple endgames)
and in imbalanced positions, so there's lots of room for improvement. I also
like to test against suites of test positions with known solutions to see how
many Daydreamer can find in a given amount of time. Here are the results from
two popular tests: selected positions from Win at Chess (WAC), a pretty
tactical suite that's generally easy for computers, and the Encyclopedia of
Chess Middlegames (ECMGCP), a harder suite of middlegame problems. I ran
Daydreamer through both sets of tests using
[Polyglot's](http://wbec-ridderkerk.nl/html/details1/PolyGlot.html) testing
function with a maximum time of 10s.

    WAC    291/300
    ECMGMP  93/173
