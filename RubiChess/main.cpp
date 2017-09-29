
#include "RubiChess.h"


#if 0

ChangeLog

0.1 (release 2017 - 02 - 24) :
    -0x88 board representation with(hopefully) bugfree move generation and play / unplay
    - alpha - beta - search implemented with pv move ordering and some 'prefer captures' heuristic
    - basic implementation of transposition table; not yet very powerfull as it is cannot deal with non - exact values and is reset after every iterative seach
    - getPositionValue: expensive but quite strong position evaluation bases on counting dominated fields
    - basic UCI protocol implementation so that it can play tournaments in Arena
    - perft implemented
    - testengine mode for bulk testing of EPD files

    0.2 (release 2017 - 03 - 19) :
    -improved use of transposition table which is not reset anymore and can deal with alpha and beta bound entries
    - alphabeta now uses a quiscence based on SEE evaluation for captures to go
    - null move pruning added
    - move ordering now uses killer moves
    - simplified unplay to just reset the old values stored by play procedure instead of emulating the reverse play
    - getPositionValue changed to some simple rules
    + prefer central fields for knight and bishop
    + prefer open files for rook and queen
    + prefer border positions for king in the beginning to force castles
    + pawns: prefer central pawns, penalty for double pawns, move forward in endgame

    0.2.1 (release 2017 - 03 - 28) :
    -tuned attack detection, move generation, SEE, play / unplay to get more speed out of it; perft shows ~10 % better performance
    - added history heuristic for move ordering; improvement visible in wacnew / 199 where next depth is reached in half time compared to 0.2
    - implemented 50 - moves rule
    - fixed bug in en passant hashing
    - fixed bug : ENGINESTOPSOON did not take move of last iteration
    - improved testengine mode(compare mode, avoid moves, exit flags)

    0.3 (release 2017 - 06 - 05) :
    -now really fixed ep hashing(code got lost somehow before releasing 0.2.1)
    - new evaluation using phase triggered value tables and some pawn specials
    - getMoves now returns pseudo legal moves to avoid time consuming play / unplay
    - both of these changes leads to speed improvement ~40 % compared to 0.2.1
    - improved quiscense serach although this might not be the end of story
    - implemented simple heuristic for insufficient material
    - improved time management for tournament mode and switched to QueryPerformanceCounter API
    - fixed memory leaks
    - removed undotest flag
    - implemented benchmark mode
    - some fixes in uci option handling

    0.4 (release 2017 - 08 - 04) :
    -first rotating bitboard version (choose by #define BITBOARD); 0x88 board still available
    - use 2^x for transposition size and & instead of % which make accessing the TP a lot faster
    - some fixes in the pawn evaulation (got different scores for white and black before)
    - added test for symmetric position evaluation (new parameter -dotests substitutes -hashtest)
    - fixed timemode "time/inc for whole match"
    - testrepetition() stops searching at halfmovecounter reset now
    - search improvement by adding aspiration windows and principal variation search
	 
	 0.5 (release 201x - xx - xx) :
    -moved undo data from chessmove struct to a static stack which speeds up playing moves
    - More speed by making engine, position, tables global instead of paasing them by parameters
    - Better evaluation: Ballanced pawn/minors, diagonal mobility, progress of passed pawns
    - improved time management, especially sudden death mode
    - avoids wrong best move from last search when under time pressure
    - really disabled all the debug code if DEBUG is not defined

      

/* Der Lasker-Test */
s = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1"

/* Matt in 6 KN 12.01.2017 */
s = "3k4/3P4/3P4/3P4/3P4/1P1P1P2/3P4/1R1K4 w - - 0 1";

/* Matt in 4 KN 26.01.2017 */
s = "1qn5/1rp5/4QKb1/pp1R1p1r/4p3/8/3R4/1k4n1 w - - 8 1";

/* Matt in 4 KN 03.02.2017 */
s = "1B6/1K6/3Np3/3k4/2N5/2P5/8/8 w - - 8 1";

/* Matt in 3 KN 10.02.2017 */
s = "8/2p5/8/2P5/1KB1kp1N/4p3/1B6/4R3 w - - 0 1"

/* Matt in 3 KN 17.02.2017 */
s = "K5b1/p3Q3/1p2PR2/r3k3/6p1/3pP3/8/3n3B w - - 0 1"

/* Matt in 3 KN 24.02.2017 */
s = "8/8/2N5/1k6/1N6/8/2Q5/7K w - - 0 1"

/* Matt in 3 KN 02.03.2017 */
s = "8/8/8/8/8/1pN2p2/1B1k1K2/4N3 w - - 0 1"
s = "8/8/8/8/5p2/2N2p2/1B1k1K2/4N3 w - - 0 1"

/* Matt in 3 KN 09.03.2017 */
s = "3K4/6p1/2p2nQ1/2Pp4/N2k1P2/1p1PR2p/3n2q1/3RbNB1 w - - 0 1"

/* Matt in 3 KN 16.03.2017 */
s = "4r3/1b1PR2N/1n1pp1p1/b3k3/6Q1/1P6/4P1n1/N1B3K1 w - - 0 1"

/* Matt in 5 KN 23.03.2017 */
s = "8/8/6B1/2KNk3/7N/8/5P2/8 w - - 0 1"

/* Matt in 4 KN 30.03.2017 */
s = "8/B1p5/1p2PNKp/4pP2/1p1P1kN1/2p4Q/3q3P/7n w - - 0 1"

/* Matt in 6 KN 06.04.2017 */
s = "8/3K1P1k/6pq/8/8/3B1Q2/8/8 w - - 0 1"

/* Matt in 4 KN 13.04.2017 */
s = "K7/8/k7/8/p7/P7/8/1R6 w - - 0 1"

/* Matt in 3 Rätselheft Ben */
s= "6Q1/ppk5/4p2p/1b1pn3/3pq3/B1n5/P1P1NPPP/R4K1R b - - 0 1"
/* Matt in 3 Rätselheft Ben */
s = "5rk1/n1p1R1bp/pr1p4/1qpP1QB1/7P/6PB/PPP2P2/6K1 w - - 0 1"

/* Matt in 6 KN 27.04.2017 */
s = "5B2/2p2p1p/2P1kP1P/p4R1P/2P3P1/1P6/6p1/6Kb w - - 0 1"

/* Matt in 4 KN 04.05.2017 */
s = "8/3K1p2/1R5p/1N6/k1p5/1bBp3r/2p5/5r2 w - - 0 1"

/* Matt in 3 KN 11.05.2017 */
s = "8/8/4p3/3BK2p/4R3/3p2Nk/3B3P/7R w - - 0 1"

/* Matt in 3 KN 18.05.2017 */
s = "8/8/1p6/2k1P3/8/P2K4/3P4/7Q w - - 0 1"

/* Matt in 4 KN 01.06.2017 */
s = "8/8/3B2Rp/2R5/3kp1p1/pp6/4K3/2b5 w - - 0 1"

/* Matt in 4 KN 08.06.2017 */
s = "2n5/2K4b/2N1p3/2pkr2r/n6p/1P3P2/N2P3Q/8 w - - 0 1"

/* Matt in 6 KN 15.06.2017 */
s = "6k1/5q2/5P2/5Q1p/8/3B4/8/5K2 w - - 0 1"

/* Matt in 3 KN 22.06.2017 */
s = "2BR4/r5q1/5R2/2B1k3/2b1p2Q/p1n5/3p4/Kn3rb1 w - - 0 1"

/* Matt in 3 KN 29.06.2017 */
s = "8/1PP2P2/BPP1k3/8/3K4/8/8/8 w - - 0 1"

/* Matt in 3 KN 06.07.2017 */
s = "2R5/8/8/3K4/p7/Bk6/8/1N6 w - - 0 1"

/* Matt in 4 KN 13.07.2017 */
s = "2bnN1B1/n1p3K1/6p1/3p1kN1/3P1p1P/5P2/8/8 w - - 0 1"

/* Matt in 8 KN 20.07.2017 */
s = "8/8/8/8/5P2/5Kp1/2N3pb/N4kr1 w - - 0 1"

/* Matt in 6 KN 27.07.2017 */
s = "7k/2p4P/6BP/2p5/1pP5/pP4K1/Pn2P3/B7 w - - 0 1"

/* Matt in 4 KN 03.08.2017 */
s = "8/7K/4N1p1/2p2prk/2r3pq/6p1/3Q3p/b7 w - - 0 1"

/* Matt in 4 KN Problem 4407 */
s = "kB6/Pp6/8/2p5/2P2p1p/7K/8/7B w - - 0 1"

/* Matt in 4 KN Problem 4408 */
s = "8/2p5/4K3/2P4p/6kP/7N/5BB1/8 w - - 0 1"

/* Matt in 8 KN Problem 4409 */
s = "k1K5/p5p1/P7/8/8/8/6P1/4B3 w - - 0 1"

/* Matt in 3 KN Problem 4410a+b */
s = "8/8/3n4/3k4/1K1Nn3/5N2/8/3Q4 w - - 0 1"
s = "8/8/3n4/3k4/1K1Nn3/4QN2/8/8 w - - 0 1"

/* Matt in 3 KN Problem 4411 */
s = "6K1/3N1N2/6k1/3P1p1p/P4p1p/4bP1p/4P2P/3Q4 w - - 0 1"

/* Matt in 3 KN Problem 4412 */
s = "8/8/5Q2/4r3/3k4/4p3/1P2K3/7B w - - 0 1"

/* Matt in 8 KN Problem 4414 */
s = "8/8/8/5p2/5P2/4NN1p/5P2/4K2k w - - 0 1"

// ToDo:
TP / Sortierungsproblem:
position startpos moves d2d4 d7d5 c1f4 g8f6 d1d3 e7e6 b1c3 f8e7 e2e4 d5e4 c3e4 f6d5 g1h3 e8g8 f1e2 f7f5 e4c3 e7b4 e1c1 b4c3 b2c3 d8e7 c1d2 d5f4 h3f4 f8d8 h2h4 e7d6 d2e3 b8c6 e2f3 c6e5 d3b5 e5f3 g2f3 d6a3 b5c4 c7c6 d1g1 a3d6 f4h5 g7g6 c4b4 d6c7 h5f6 g8h8 b4c5 c7g7 f6h5 g7c7 h5f4 a7a5 h4h5 b7b6 c5c4 c6c5 f4g6 h7g6 h5g6 h8g8 h1h4 c5d4 h4d4 c7e5 e3d3 c8a6 c4a6 a5a4 a6b6 d8b8 b6c6 a4a3 c6d7 e5b5 d7b5 b8b5 g1e1 a8a6 c3c4 b5b2 e1g1 b2a2 d4d8 g8g7 d8d7 g7f6 g6g7 a6a8 g7g8q a8g8 g1g8 a2a1 d7a7 f6e5 d3e3 a1e1 e3d2 e1f1 d2e3 f1e1 e3d2
position startpos moves d2d4 e7e6 g1f3 d7d5 c1f4 g8f6 b1c3 f8b4 a2a3 b4c3 b2c3 f6e4 d1d3 b8c6 h2h4 e8g8 a1b1 d8e7 c3c4 e4f6 c4d5 f6d5 e2e3 d5f4 e3f4 e7d6 f3e5 c6d4 b1d1 d4f5 d3c3 d6e7 f1d3 f8d8 h4h5 f7f6 e5f3 d8d5 e1e2 c7c6 d1b1 e7d7 b1d1 f5d4 f3d4 e6e5 d4f5 e5f4 e2f3 d5d3 d1d3 d7f5 c3c4 c8e6 c4f4 f5c5 h5h6 c5c2 f4e4 e6d5 d3d5 c2e4 f3e4 a8e8 e4d4 c6d5 h6g7 e8e2 h1b1 b7b6 b1c1 e2f2 d4d5 f2g2 c1c8 g8g7 c8c7 g7g6 c7a7 g2c2 a7b7 c2a2 b7b6 a2a3 b6b2 a3a5 d5d6 h7h5 d6c6 h5h4 b2b4 h4h3 b4h4 a5a3 h4g4 g6h5 g4g8 a3b3 c6d6 b3b2 d6e6 h3h2 g8h8 h5g5 e6d5 f6f5 d5c4 f5f4 c4c3 b2e2 c3d3 f4f3 h8g8 g5f6 g8h8 e2c2 d3c2

Stellungwiederholung nicht erkannt
position startpos moves d2d4 g8f6 b1c3 e7e6 c1f4 f8b4 d1d3 f6d5 g1h3 b4c3 b2c3 e8g8 a1b1 d7d6 e1d2 e6e5 d4e5 d5f4 h3f4 d6e5 f4d5 e5e4 d3e4 c7c6 c3c4 f8e8 e4d4 c6d5 c4d5 d8a5 d2d1 a5a2 b1b3 a2a5 e2e4 f7f5 e4e5 a5d8 f1c4 g8h8 f2f4 b8c6 d4c3 c6a5 b3b5 a5c4 c3c4 a7a6 b5c5 c8e6 h1g1 b7b6 d1e2 b6c5 d5e6 d8b6 g2g4 f5g4 g1g4 g7g6 g4g3 e8e6 c4e4 b6b5 e2e3 a8d8 f4f5 g6f5 e4f5 d8e8 g3g5 b5b2 e3f3 b2e5 f5e5 e6e5 g5e5 e8e5 f3f2 a6a5 f2f3 a5a4 f3f4 e5e1 f4f3 a4a3 f3f2 e1c1 f2f3 c1c2 f3e4 c2h2 e4d5 h2c2 d5e4 c2d2 e4e3 d2d6 e3f4 c5c4 f4e5 d6d3 e5f6 h8g8 f6f5 g8g7 f5g5 a3a2 g5g4 g7g6 g4f4 a2a1q f4e4 g6f6 e4f4 f6g6 f4e4 g6f6 e4f4

Bis depth 5 ganz übler Zug a5a1 mit Damenverlust und viel zu hoher Bewertung
3r2k1/pbr2npp/5p2/q6P/3PP3/3Q2P1/1P1NK1P1/R6R b - -1 30
-> erledigt, Fehler in Zobrist, der Feld 0 nicht berücksichtigt, dadurch falscher TP-Wert

Illegaler Zug:
2017 - 03 - 09 23:38 : 55, 530-->1:position startpos moves d2d4 d7d5 c1f4 g8f6 d1d3 e7e6 b1c3 f8d6 e2e3 d6f4 e3f4 e8g8 g1f3 d8d6 g2g3 d6a6 e1d2 a6d3 f1d3 f6g4 h1f1 b8d7 h2h3 g4f6 a1e1 d7b6 f3e5 f6e8 f1g1 f7f6 e5f3 c8d7 c3b5 c7c6 b5c3 a8c8 g1h1 e8d6 d2e2 c8c7 e2d2 f8c8 h3h4 c6c5 d4c5 c7c5 a2a3 d6c4 d2c1 c4b2 c1b2 c5c3 h4h5 b6a4 b2b1 c3a3 f3d4 a4c3 b1b2 a3a2 b2c1 a2a4 d4f3 d5d4 c1d2 d7c6 f3h4 c6h1 e1h1 f6f5 h4f3 b7b5 h1e1 c8c6 f3e5 c6c5 g3g4 f5g4 e5g4
2017 - 03 - 09 23:38 : 55, 533-->1:go wtime 8562 btime 8662 winc 6000 binc 6000
2017 - 03 - 09 23:38 : 55, 538<--1 : info nodes 1 nps 1 hashfull 613
    2017 - 03 - 09 23:38 : 56, 539<--1 : info depth 1 time 0 score cp 455 pv c5h5 e1e6 h5c5 e6e5 c3d5 f4f5
    2017 - 03 - 09 23:38 : 56, 544<--1 : info depth 2 time 0 score cp 455 pv c5h5 e1e6 h5c5 e6e5 c3d5 f4f5
    2017 - 03 - 09 23:38 : 56, 550<--1 : info depth 3 time 0 score cp 455 pv c5h5 e1e6 h5c5 e6e5 c3d5 f4f5
    2017 - 03 - 09 23:38 : 56, 556<--1 : info depth 4 time 0 score cp 455 pv c5h5 e1e6 h5c5 e6e5 c3d5 f4f5
    2017 - 03 - 09 23:38 : 56, 563<--1 : info depth 5 time 0 score cp 455 pv c5h5 e1e6 h5c5 e6e5 c3d5 f4f5
    2017 - 03 - 09 23:38 : 56, 569<--1 : info depth 6 time 0 score cp 455 pv c5h5 e1e6 h5c5 e6e5 c3d5 f4f5
    2017 - 03 - 09 23:38 : 56, 581<--1 : info nodes 164792 nps 164791 hashfull 613
    2017 - 03 - 09 23:38 : 57, 538<--1 : info nodes 319402 nps 154610 hashfull 614
    2017 - 03 - 09 23:38 : 58, 538<--1 : info nodes 452334 nps 132932 hashfull 614
    2017 - 03 - 09 23:38 : 59, 539<--1 : info nodes 645259 nps 192925 hashfull 614
    2017 - 03 - 09 23:39 : 00, 539<--1 : info nodes 836337 nps 191078 hashfull 615
    2017 - 03 - 09 23:39 : 01, 539<--1 : info nodes 1026066 nps 189729 hashfull 615
    2017 - 03 - 09 23:39 : 01, 544<--1 : Searchorder - Success : 0.823755
    2017 - 03 - 09 23:39 : 01, 548<--1 : bestmove f5g4
    2017 - 03 - 09 23:39 : 01, 551 * 1 * --------->Arena : Illegaler Zug!: "f5g4" (Feinprüfung)
    2017 - 03 - 09 23:39 : 01, 563 * ***Tour**Partie zuende : 1 - 0 {1 - 0 Arena Entscheidung.Illegaler Zug!}

Stellungswiederholung nicht erkannt(?)
2017 - 03 - 10 19 : 50 : 52, 549-->2 : position startpos moves c2c4 g8f6 g2g3 e7e6 f1g2 d7d5 g1f3 b8c6 c4d5 f6d5 e2e4 d5f6 d2d3 f8b4 b1c3 b4c3 b2c3 f6d7 d3d4 e8g8 c1f4 f7f5 d1b3 d7b6 e4f5 f8f5 f3e5 c6e5 f4e5 c7c5 g2e4 c5c4 b3c2 f5h5 a1d1 b6d7 c2e2 d8e8 g3g4 h5h4 e5g3 h4h3 e2f1 h3h6 f1c4 d7b6 c4d3 b6d5 c3c4 d5b6 g3f4 h6h4 h2h3 e8a4 f4g3 h4h6 g3f4 h6h4 f4g3 h4h6 g3f4
2017 - 03 - 10 19 : 50 : 52, 552-->2:go wtime 28285 btime 7572 winc 6000 binc 6000
2017 - 03 - 10 19 : 50 : 52, 558<--2 : info depth 1 time 0 score cp - 90 pv h6h4 d3g3
    2017 - 03 - 10 19 : 50 : 52, 565<--2 : info depth 2 time 0 score cp - 90 pv h6h4 d3g3 a4b4
    2017 - 03 - 10 19 : 50 : 52, 574<--2 : info depth 3 time 0 score cp - 90 pv h6h4 d3g3 a4b4 d1d2
    2017 - 03 - 10 19 : 50 : 52, 581<--2 : info depth 4 time 0 score cp - 90 pv h6h4 d3g3 a4b4 d1d2 b4e7
    2017 - 03 - 10 19 : 50 : 52, 590<--2 : info depth 5 time 0 score cp - 90 pv h6h4 d3g3 a4b4 d1d2 b4e7 g3b3
    2017 - 03 - 10 19 : 50 : 52, 596<--2 : info depth 6 time 0 score cp - 90 pv h6h4 d3g3 a4b4 d1d2 b4e7 g3b3
    2017 - 03 - 10 19 : 50 : 53, 559<--2 : info nodes 98097 nps 98097 hashfull 182
    2017 - 03 - 10 19 : 50 : 54, 558<--2 : info nodes 180742 nps 82645 hashfull 183
    2017 - 03 - 10 19 : 50 : 55, 559<--2 : info nodes 295779 nps 115037 hashfull 184
    2017 - 03 - 10 19 : 50 : 56, 559<--2 : info nodes 396556 nps 100777 hashfull 185
    2017 - 03 - 10 19 : 50 : 57, 559<--2 : info nodes 507724 nps 111168 hashfull 186
    2017 - 03 - 10 19 : 50 : 58, 560<--2 : info nodes 651338 nps 143614 hashfull 186
    2017 - 03 - 10 19 : 50 : 58, 564<--2 : Searchorder - Success : 0.833431
    2017 - 03 - 10 19 : 50 : 58, 568<--2 : bestmove h6h4
    2017 - 03 - 10 19 : 50 : 58, 570 * 2 * Zug gefunden : Th6 - h4
    2017 - 03 - 10 19 : 50 : 58, 725 * ***Tour**Partie zuende : ½ - ½{ Stellungswiederholung }

    Und auch hier:
position startpos moves d2d4 d7d5 e2e3 b8c6 f1b5 d8d6 b5c6 d6c6 b1c3 b7b5 g1f3 c8g4 e1g1 g4f3 d1f3 g8f6 f3f5 b5b4 c3e2 e7e6 f5d3 f8d6 f2f3 e8e7 c1d2 a8b8 d2e1 h7h6 e1g3 h8d8 g3e5 g7g5 e5f6 e7f6 e3e4 d5e4 f3e4 f6g7 f1f2 f7f6 g1h1 g5g4 a1f1 d6e7 f2f4 h6h5 e4e5 f6f5 e2g3 h5h4 g3e2 h4h3 f4f2 c6d5 d3a6 b8b6 a6a7 d8a8 a7c7 b6b7 e2f4 h3g2 f2g2 b7c7 f4d5 e6d5 f1f5 a8a2 g2g4 g7h6 c2c3 b4c3 b2c3 c7c3 h1g1 c3c1 f5f1 c1f1 g1f1 a2h2 g4g8 h2d2 g8g4 e7g5 f1e1 d2a2 e1f1 g5e3 g4g3 e3d4 e5e6 h6h5 e6e7 a2a1 f1e2 a1a2 e2d3 a2a3 d3d4 a3a4 d4d5 a4a8 d5d6 a8a6 d6d5 a6a8 d5d6 a8a6 d6d5
(trotz besserer Stellung lädt d6d5 zu Stellungswiederholung ein)

Matt zu früh:
position startpos moves d2d4 d7d5 e2e3 b8c6 f1b5 d8d6 b5c6 d6c6 b1c3 b7b5 g1f3 c8g4 e1g1 g4f3 d1f3 g8f6 f3f5 b5b4 c3e2 e7e6 f5d3 f8d6 f2f3 e8e7 f3f4 f6e4 c1d2 a7a5 f1f3 h7h5 g1h1 a5a4 d2c1 a4a3 b2a3 b4a3 a1b1 g7g5 f4g5 h5h4 g5g6 f7g6 e2f4 c6e8 c2c4 g6g5 c4d5 e4g3 f3g3 h4g3 f4g6 e7d8 g6h8 e8h8 h2h3 e6d5 d3g6 d8e7 e3e4 h8d4 c1g5 e7d7 g6f5 d7c6 b1c1 c6b6 f5d5 d4d5 e4d5 a8e8 g5f6 e8e4 h3h4 d6f4 c1c6 b6b5 c6e6 e4b4 e6e1 f4d2 e1d1 b4e4 h1g1 d2b4 h4h5 b4d2 f6g7 d2e3 g1f1 b5c4 h5h6 e3c5 h6h7 e4f4 f1e1 c5b4 d1d2 b4d2 e1d2 f4h4 h7h8q h4h8 g7h8 c4d5 d2d3 c7c6 h8c3 d5e6 c3e1 e6d5 e1g3 d5c5 g3e1 c5b5 e1f2 b5b4 g2g4 c6c5 g4g5 c5c4 d3d2 c4c3 d2c2 b4c4 g5g6 c4d5 g6g7 d5e5   c2c3 e5f5
findet Matt in 6 statt in 7
position startpos moves d2d4 d7d5 e2e3 b8c6 f1b5 d8d6 b5c6 d6c6 b1c3 b7b5 g1f3 c8g4 e1g1 g4f3 d1f3 g8f6 f3f5 b5b4 c3e2 e7e6 f5d3 f8d6 f2f3 e8e7 f3f4 f6e4 c1d2 a7a5 f1f3 h7h5 g1h1 a5a4 d2c1 a4a3 b2a3 b4a3 a1b1 g7g5 f4g5 h5h4 g5g6 f7g6 e2f4 c6e8 c2c4 g6g5 c4d5 e4g3 f3g3 h4g3 f4g6 e7d8 g6h8 e8h8 h2h3 e6d5 d3g6 d8e7 e3e4 h8d4 c1g5 e7d7 g6f5 d7c6 b1c1 c6b6 f5d5 d4d5 e4d5 a8e8 g5f6 e8e4 h3h4 d6f4 c1c6 b6b5 c6e6 e4b4 e6e1 f4d2 e1d1 b4e4 h1g1 d2b4 h4h5 b4d2 f6g7 d2e3 g1f1 b5c4 h5h6 e3c5 h6h7 e4f4 f1e1 c5b4 d1d2 b4d2 e1d2 f4h4 h7h8q h4h8 g7h8 c4d5 d2d3 c7c6 h8c3 d5e6 c3e1 e6d5 e1g3 d5c5 g3e1 c5b5 e1f2 b5b4 g2g4 c6c5 g4g5 c5c4 d3d2 c4c3 d2c2 b4c4 g5g6 c4d5 c2c3 d5e5 g6g7 e5e4 g7g8Q e4f4 g8g6 f4f3
f2h4
geht jetzt mit Matt / Ply - Handling von VICE

falsches Matt in 3 statt in 4(?):
    "8/8/8/6Q1/7B/p1K2k2/P7/8 w - - 15 74"   liegt ev. an Stellungswiederholung?? andere Engines vermeiden g5g6 obwohl nicht immer derselbe dran war
    position startpos moves d2d4 d7d5 e2e3 b8c6 f1b5 d8d6 b5c6 d6c6 b1c3 b7b5 g1f3 c8g4 e1g1 g4f3 d1f3 g8f6 f3f5 b5b4 c3e2 e7e6 f5d3 f8d6 f2f3 e8e7 f3f4 f6e4 c1d2 a7a5 f1f3 h7h5 g1h1 a5a4 d2c1 a4a3 b2a3 b4a3 a1b1 g7g5 f4g5 h5h4 g5g6 f7g6 e2f4 c6e8 c2c4 g6g5 c4d5 e4g3 f3g3 h4g3 f4g6 e7d8 g6h8 e8h8 h2h3 e6d5 d3g6 d8e7 e3e4 h8d4 c1g5 e7d7 g6f5 d7c6 b1c1 c6b6 f5d5 d4d5 e4d5 a8e8 g5f6 e8e4 h3h4 d6f4 c1c6 b6b5 c6e6 e4b4 e6e1 f4d2 e1d1 b4e4 h1g1 d2b4 h4h5 b4d2 f6g7 d2e3 g1f1 b5c4 h5h6 e3c5 h6h7 e4f4 f1e1 c5b4 d1d2 b4d2 e1d2 f4h4 h7h8q h4h8 g7h8 c4d5 d2d3 c7c6 h8c3 d5e6 c3e1 e6d5 e1g3 d5c5 g3e1 c5b5 e1f2 b5b4 g2g4 c6c5 g4g5 c5c4 d3d2 c4c3 d2c2 b4c4 g5g6 c4d5 c2c3 d5e5 g6g7 e5e4 g7g8q e4f4 g8g6 f4f3 f2h4 f3e2 g6f6 e2e3 f6g5 e3e2 g5g6 e2e3 g6g7 e3e2 g7g5 e2f3

Er kriegt partout kein Matt hin, zeigt sogar negative Bewertung:
-> position startpos moves d2d4 d7d5 c1f4 g8f6 d1d3 e7e6 b1c3 f8d6 e2e3 d6f4 e3f4 e8g8 g1f3 d8d6 g2g3 d6a6 e1d2 a6d3 f1d3 f6g4 h1f1 b8d7 h2h3 g4f6 a1e1 d7b6 f3e5 f6e8 f1g1 f7f6 e5f3 c8d7 c3b5 c7c6 b5c3 a8c8 g1h1 e8d6 d2e2 c8c7 e2d2 f8c8 h3h4 c6c5 d4c5 c7c5 a2a3 d6c4 d2c1 c4b2 c1b2 c5c3 h4h5 b6a4 b2b1 c3a3 f3d4 a4c3 b1b2 a3a2 b2c1 a2a4 d4f3 d5d4 c1d2 d7c6 f3h4 c6h1 e1h1 f6f5 h4f3 b7b5 h1e1 c8c6 f3e5 c6c5 g3g4 f5g4 e5g4 c5h5 e1e6 a4a1 e6e1 a1e1 d2e1 h5h4 g4e5 h4f4 e5c6 a7a6 e1d2 f4f2 d2e1 f2f4 e1d2 c3b1 d2e1 g7g5 c2c4 b1c3 e1d2 f4f2 d2e1 f2f3 e1d2 f3f2 d2e1 f2f4 c4b5 a6b5 e1d2 f4f2 d2e1 f2f3 d3f5 d4d3 c6d4 f3f4 e1d2 f4d4 d2c3 d3d2 f5e6 g8g7 e6b3 d2d1q b3d1 d4d1 c3c2 d1d5 c2c3 h7h5 c3b4 h5h4 b4a5 h4h3 a5b4 h3h2 b4a5 h2h1q a5b6 d5d8 b6c5 h1d1 c5c6 d1g1 c6c7 g1d4 c7c6 d4c3 c6b6 c3c2 b6a6 c2d1 a6b6 d1d5 b6a6 d5h1 a6b6 h1d5 b6a6 d5d2 a6b6 d2c1 b6a6 c1c7 a6b5 c7c1 b5b6 d8d7 b6b5 d7d8 b5b6 d8d7 b6b5 d7d6 b5b4 d6d7 b4b5
geht jetzt mit Matt/Ply-Handling von VICE

18.03.17 : drohende Zugwiederholung wird nicht erkannt; es wird  e4c3 gespielt, Siegstellung wird daruch zum Remis
position startpos moves d2d4 d7d5 g1f3 b8c6 e2e3 g8f6 f1b5 a7a6 b5c6 b7c6 f3e5 d8d6 e1g1 a8b8 b2b3 c6c5 c1a3 b8b5 b1c3 b5a5 c3a4 f6e4 f2f3 f7f6 f3e4 f6e5 a3c5 d6g6 d1d2 a5b5 a4c3 b5b8 e4d5 e5d4 c5d4 c8f5 a1c1 e7e6 d5e6 f5e6 f1f3 f8d6 d2d3 e8d7 f3f1 g6d3 c2d3 d6a3 c1c2 h8g8 c3e4 e6d5 e4c3 d5e6 c3e4 e6d5
->behoben; erst Moves holen und auf repetition prüfen, dann Nullmove

19.03.17: Benötigt zu lange, um Zug b2e5 als Selbstmord zu erkennen
2r5/8/p2q2k1/R3ppB1/4b3/5pPN/1Q3K2/8 w - - 18 78

25.03.17: Warum wird hier nicht der letzte pv e7e5 genommen:
2017 - 03 - 25 18 : 35 : 10, 846-->1:position startpos moves e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3 f6g4 e3g5 h7h6 g5h4 g7g5 h4g3
2017 - 03 - 25 18 : 35 : 10, 849-->1:go wtime 53989 btime 54000 winc 6000 binc 6000
2017 - 03 - 25 18 : 35 : 10, 855<--1 : info nodes 1 nps 1 hashfull 0
2017 - 03 - 25 18 : 35 : 10, 859<--1 : info depth 1 time 0 score cp - 62 pv d8c7
2017 - 03 - 25 18 : 35 : 10, 864<--1 : info depth 2 time 0 score cp - 71 pv d8c7 f1d3
2017 - 03 - 25 18 : 35 : 10, 880<--1 : info depth 3 time 0 score cp - 48 pv e7e5 d4f3 d8c7
2017 - 03 - 25 18 : 35 : 11, 006<--1 : info depth 4 time 1000 score cp - 57 pv e7e5 d4f3 d8c7 f1d3
2017 - 03 - 25 18 : 35 : 11, 242<--1 : info depth 5 time 1000 score cp - 49 pv e7e5 d4b3 d8c7 f1e2 g4f6
2017 - 03 - 25 18 : 35 : 11, 655<--2 : info nodes 1129022 nps 1576 hashfull 8
2017 - 03 - 25 18 : 35 : 11, 856<--1 : info nodes 118024 nps 118023 hashfull 0
2017 - 03 - 25 18 : 35 : 12, 856<--1 : info nodes 202055 nps 84031 hashfull 1
2017 - 03 - 25 18 : 35 : 13, 856<--1 : info nodes 303949 nps 101894 hashfull 2
2017 - 03 - 25 18 : 35 : 14, 856<--1 : info nodes 401027 nps 97078 hashfull 4
2017 - 03 - 25 18 : 35 : 15, 857<--1 : info nodes 493687 nps 92660 hashfull 5
2017 - 03 - 25 18 : 35 : 16, 857<--1 : info nodes 598434 nps 104747 hashfull 6
2017 - 03 - 25 18 : 35 : 17, 345<--1 : bestmove f8g7
2017 - 03 - 25 18 : 35 : 17, 348 * 1 * Zug gefunden : Lf8 - g7
-> sollte gefixt sein (ENGINESTOPSOON wurde nicht korrekt berücksichtigt)

28.03.17: Pläne für kommende Version
- Verbesserung Evaluierung
+ Bauern auch am Rand etwas mehr nach Vorne (derzeit wird Diagonale bevorzugt, was zu sehr vorsichtigem / sterilem Spiel führt)
+ grundsätzlich etwas mehr Drang gegen den gegnerischen König?
+ häufig rutschen Turm und König nur auf der Grundlinie hin und her
+ Schutz des Königs
+ Wert von vorrückenden Bauern schon im frühen SPiel nicht unterschätzen; ein schönes Beispiel, wie selbst die 0.1 mit einem vorrückenden Bauern noch gewinnen kann:
position startpos moves d2d4 d7d5 c1f4 e7e6 d1d3 b8c6 b1c3 c6b4 d3d1 f8d6 e2e3 d6f4 e3f4 d8d6 g1h3 b4c6 f1b5 d6b4 e1e2 b4b2 d1d3 b2a3 b5c6 b7c6 e2d2 a8b8 a1b1 b8b1 h1b1 g8f6 h3g5 e8g8 h2h4 a3a5 f2f3 c8a6 d3e3 a6c4 a2a4 f6d7 g2g4 d7b6 f4f5 c4a2 b1b6 a7b6 f5e6 f7f6 e6e7 f8e8 e3e6 g8h8 g5f7 h8g8 f7h6 g8h8 e6g8 e8g8 h6f7

9.4.17: Wieder Problem mit Stellungswiederholung (0.2.1 (2) - 0.3beta (1))
2017 - 04 - 09 19:08 : 39, 383-->2:position startpos moves e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6 h2h4 h7h6 g1f3 b8d7 h4h5 g6h7 f1d3 h7d3 d1d3 g8f6 e1g1 d8c7 c1d2 e7e6 a1e1 f8d6 g3f5 f6h5 d3e4 h5f6 e4h4 e8g8 f5g7 g8g7 d2h6 g7h7 h6f8 h7g8 f8d6 c7d6 c2c4 d6b4 h4g3 g8f8 b2b3 a8e8 e1e2 e8e7 f1e1 a7a6 f3e5 f8e8 g3g7 d7e5 e2e5 f6d7 e5e2 e8d8 g7h8 d8c7 e2e4 f7f5 h8h2 c7c8 h2h8 c8c7 e4e2 a6a5 d4d5 e6e5 d5c6 b7c6 h8h5 f5f4 h5h4 e7e6 h4h3 d7c5 h3h7 c5d7 e1d1 b4e7 h7h5 d7c5 d1e1 e5e4 e1d1 e6e5 h5h8 c7b7 d1d8 e4e3 d8b8 b7c7 b8c8 c7b7 c8b8 b7c7 b8a8 c7b6 h8b8 c5b7 b8a7 b6c7 g1f1 e7c5 a7c5 b7c5 a8a5 c7d6 f2e3 f4e3 g2g3 e5e6 f1g2 c5d3 a5h5 d3c1 c4c5 d6e7 e2e1 c1d3 e1e2 d3c1 e2e1 c1d3 e1f1 e3e2 h5h7 e7e8 f1b1 d3c5 b1e1 c5d3 h7h1 e6e3 b3b4 e8d7 a2a4 d7c7 a4a5 c7b7 g3g4 b7a6 g4g5 d3b4 g2f2 e3e5 e1e2 e5g5 e2e8 g5f5 f2e2 a6a5 h1a1 a5b5 e8b8 b5c4 a1c1 c4d5 b8b4 f5f6 c1d1 d5e6
2017 - 04 - 09 19:08 : 39, 386-->2:go wtime 7626 btime 7461 winc 6000 binc 6000
2017 - 04 - 09 19:08 : 39, 392<--2 : info depth 1 time 0 score cp 420 pv d1c1
    2017 - 04 - 09 19:08 : 39, 398<--2 : info depth 2 time 0 score cp 420 pv d1c1 e6d6
    2017 - 04 - 09 19:08 : 39, 405<--2 : info depth 3 time 0 score cp 420 pv d1c1 e6d6 c1d1
    2017 - 04 - 09 19:08 : 39, 413<--2 : info depth 4 time 0 score cp 420 pv d1c1 e6d6 c1d1 d6c7(none)
    2017 - 04 - 09 19:08 : 39, 419<--2 : info depth 5 time 0 score cp 420 pv d1c1 e6d6 c1d1 d6c7 d1c1
    2017 - 04 - 09 19:08 : 39, 486<--2 : info depth 6 time 0 score cp 420 pv d1c1 e6d6 c1d1 d6c7 d1c1 f6f5(none)
    2017 - 04 - 09 19:08 : 39, 823<--2 : info depth 7 time 0 score cp 420 pv d1c1 e6d6 c1d1 d6c7 d1c1 f6f5 c1a1
    2017 - 04 - 09 19:08 : 40, 219<--1 : info nodes 3078814 nps 5 hashfull 988
    2017 - 04 - 09 19:08 : 40, 393<--2 : info nodes 329930 nps 329930 hashfull 988
    2017 - 04 - 09 19:08 : 40, 927<--2 : info depth 8 time 1000 score cp 420 pv d1c1 e6d6 c1d1 d6c7 d1c1 f6f5 b4c4 f5e5
    2017 - 04 - 09 19:08 : 41, 392<--2 : info nodes 643167 nps 313237 hashfull 988
    2017 - 04 - 09 19:08 : 42, 393<--2 : info nodes 1024994 nps 381827 hashfull 988
    2017 - 04 - 09 19:08 : 43, 393<--2 : info nodes 1409258 nps 384264 hashfull 988
    2017 - 04 - 09 19:08 : 44, 394<--2 : info nodes 1764756 nps 355498 hashfull 988
    2017 - 04 - 09 19:08 : 45, 392<--2 : info depth 9 time 6000 score cp 420 pv d1c1 e6d6 c1d1 d6c7 d1c1 f6f5 b4c4 f5e5 e2d2
    2017 - 04 - 09 19:08 : 45, 397<--2 : info nodes 2096611 nps 331855 hashfull 988
    2017 - 04 - 09 19:08 : 45, 402<--2 : bestmove d1c1
    2017 - 04 - 09 19:08 : 45, 405 * 2 * Zug gefunden : Td1 - c1
    2017 - 04 - 09 19:08 : 45, 557 * 1 * Child Process Prio Adj : PID 8028 conhost.exe
    2017 - 04 - 09 19:08 : 45, 564 * 1 * Start calc, move no : 165
    2017 - 04 - 09 19:08 : 45, 568-->1 : position startpos moves e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6 h2h4 h7h6 g1f3 b8d7 h4h5 g6h7 f1d3 h7d3 d1d3 g8f6 e1g1 d8c7 c1d2 e7e6 a1e1 f8d6 g3f5 f6h5 d3e4 h5f6 e4h4 e8g8 f5g7 g8g7 d2h6 g7h7 h6f8 h7g8 f8d6 c7d6 c2c4 d6b4 h4g3 g8f8 b2b3 a8e8 e1e2 e8e7 f1e1 a7a6 f3e5 f8e8 g3g7 d7e5 e2e5 f6d7 e5e2 e8d8 g7h8 d8c7 e2e4 f7f5 h8h2 c7c8 h2h8 c8c7 e4e2 a6a5 d4d5 e6e5 d5c6 b7c6 h8h5 f5f4 h5h4 e7e6 h4h3 d7c5 h3h7 c5d7 e1d1 b4e7 h7h5 d7c5 d1e1 e5e4 e1d1 e6e5 h5h8 c7b7 d1d8 e4e3 d8b8 b7c7 b8c8 c7b7 c8b8 b7c7 b8a8 c7b6 h8b8 c5b7 b8a7 b6c7 g1f1 e7c5 a7c5 b7c5 a8a5 c7d6 f2e3 f4e3 g2g3 e5e6 f1g2 c5d3 a5h5 d3c1 c4c5 d6e7 e2e1 c1d3 e1e2 d3c1 e2e1 c1d3 e1f1 e3e2 h5h7 e7e8 f1b1 d3c5 b1e1 c5d3 h7h1 e6e3 b3b4 e8d7 a2a4 d7c7 a4a5 c7b7 g3g4 b7a6 g4g5 d3b4 g2f2 e3e5 e1e2 e5g5 e2e8 g5f5 f2e2 a6a5 h1a1 a5b5 e8b8 b5c4 a1c1 c4d5 b8b4 f5f6 c1d1 d5e6 d1c1
    2017 - 04 - 09 19:08 : 45, 573-->1:go wtime 7616 btime 7461 winc 6000 binc 6000
    2017 - 04 - 09 19:08 : 45, 576<--1 : info string Alarm!Zug d1c1 nicht anwendbar(oder Enginefehler)
    2017 - 04 - 09 19:08 : 45, 581<--1 : info string Phase is 192
    2017 - 04 - 09 19:08 : 45, 585<--1 : info depth 1 time 0 score cp 0 pv
    2017 - 04 - 09 19:08 : 45, 593<--1 : bestmove(none)
    2017 - 04 - 09 19:08 : 45, 596 * 1 * --------->Arena : Illegaler Zug!: "(none)" (Feinprüfung)
    2017 - 04 - 09 19:08 : 45, 607 * ***Tour**Partie zuende : 1 - 0 {1 - 0 Arena Entscheidung.Illegaler Zug!}

-> verursacht durch Hashtable-Kollision; (hoffentlich) gelöst durch zusätzliche Wiederholungserkennung testRepetition()

10.4.17: Weiß opfert Turm mit c5d5, weil Königszug zu 50_züge-regel führt und als Matt in 1 fehlinterpretiert wird
position startpos moves b1c3 c7c5 g1f3 g8f6 e2e4 b8c6 f1c4 e7e6 d1e2 c6b4 d2d4 c5d4 f3d4 f8c5 d4b3 d7d6 b3c5 d6c5 c1e3 b7b6 e4e5 f6d5 a1d1 e8g8 e1g1 c8b7 a2a3 d5c3 b2c3 b4d5 c4d5 e6d5 f2f4 d8e7 d1d2 a8e8 e2b5 f7f6 f1b1 f8f7 e3f2 f6e5 b5a4 e8a8 f4e5 e7e5 d2d3 e5e2 b1f1 a8f8 d3f3 f7f3 g2f3 e2f3 a4b3 b7a6 c3c4 a6c4 b3f3 f8f3 f1a1 f3c3 a1c1 c3a3 f2g3 g7g5 c1e1 a3f3 g3b8 f3f7 e1e8 g8g7 c2c3 h7h5 e8e3 a7a5 e3e6 f7f1 g1g2 f1b1 b8e5 g7h7 e6f6 b1b2 g2f3 b2e2 f6f7 h7h6 f7f6 h6g7 f6f5 g7g6 f5f6 g6g7 f6f5 g7g6 f5f6 g6h7 f6f7 h7g8 f7g7 g8f8 g7g5 h5h4 e5d6 f8f7 h2h3 d5d4 c3d4 c5d4 g5g4 e2e3 f3f2 e3h3 g4d4 b6b5 f2g2 c4e6 d4f4 f7g6 d6e7 g6h5 f4e4 e6f5 e4e5 h5g4 e5b5 h3e3 e7d6 a5a4 g2f2 e3e4 b5b7 h4h3 b7g7 g4h4 d6g3 h4h5 g7a7 e4d4 a7a5 h5g5 g3e5 d4c4 f2g3 c4e4 g3f2 e4b4 f2g1 b4e4 e5d6 e4e1 g1f2 e1e4 f2f3 e4d4 d6e5 d4d3 f3e2 a4a3 e2f1 d3d2 a5a3 h3h2 e5h2 d2h2 a3a4 h2d2 a4a5 d2c2 a5b5 g5f4 f1e1 f5e4 e1d1 c2a2 b5c5 a2a1 d1d2 a1b1 c5c7 b1a1 c7e7 a1b1 e7f7 f4g4 d2e3 b1e1 e3d4 e1e2 f7f1 g4g3 f1c1 g3g2 c1c3 g2f2 c3b3 e2e1 b3c3 e1e2 c3h3 e2e1 h3c3 e4f3 c3c7 e1d1 d4e5 d1d3 c7g7 f2e2 e5f4 f3d5 g7c7 e2e1 c7c5 e1d2 c5c7 d2d1 c7c8 d1e1 c8c5 e1d2 c5c7 d2d1 f4f5 d1e2 f5f4 e2d2 c7c5 d5f3 c5a5 d2c2 f4e5 c2b3 a5a1 f3d5 a1c1 b3b4 c1c8 d3d2 c8c7 d2d1 c7d7 b4c4 d7c7 c4b4 c7d7 b4c4 d7c7 c4b5 c7c3 d1d2 c3c1 b5b4 c1b1 b4c4 b1c1 c4b3 c1c7 d5h1 c7c5 d2e2 e5d6 e2d2

-> gelöst, indem das 50-Züge-Kriterium in alphabeta geprüft wird und zum SCOREDRAW führt

03.05.17: Nicht erkanntes Remis durch Stellungswiederholung vermutlich wegen Cache-Eintrag
2017-05-03 13:33:52,629-->1:position startpos moves e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d3 a7a6 c4b3 d7d6 b1d2 c5a7 h2h3 e8g8 e1g1 a7c5 b3a4 c8e6 a4c6 b7c6 d1a4 d8d7 d3d4 e5d4 f3d4 c5d4 a4d4 c6c5 d4d3 d6d5 e4e5 e6f5 d3f3 f6e4 f3d1 e4g5 f2f4 g5e6 d2c4 a8e8 c4e3 f5e4 d1d2 d7c6 e3g4 e4f5 g4e3 f5e4 e3g4 d5d4 f4f5 d4c3 b2c3 h7h5 f5e6 h5g4 e6f7 f8f7 f1f7 g8f7 d2f2 f7g8 h3g4 e8e5 c1f4 e5e7 a1d1 e7e8 f2d2 a6a5 d1e1 a5a4 g4g5 a4a3 c3c4 e8f8 g2g3 f8a8 e1e3 a8a7 d2e1 e4f5 e3e8 g8h7 e8e5 c6d7 e5c5 c7c6 c5e5 a7b7 e1d2 b7b1 e5e1 d7d2 f4d2 b1b2 d2c1 b2a2 c1e3 a2c2 c4c5 a3a2 e1a1 f5h3 g1h1 h7g6 h1g1 c2e2 e3c1 h3e6 c1f4 e6c4 g1h1 e2b2 f4e5 c4d5 h1g1 b2b1 g1f2 b1a1 e5a1 g6g5 a1g7 g5f5 f2e3 d5g2 e3d3 f5g4 g7e5 g2d5 d3e3 d5e6 e3e4 e6f5 e4e3 f5b1 e3d2 g4f3 d2c1 f3e4 e5f6 e4d5 g3g4 d5c5 c1b2 c5d5 g4g5 d5e6 f6d4 e6f5 d4f6 f5e6 f6c3 e6f5 c3f6 c6c5 b2b3 f5e6 b3c4 e6d6 c4b3 d6e6 f6c3 e6f5 c3f6 c5c4 b3c4 b1e4 c4b4 e4f3 b4b3 f3d5 b3b2 f5f4 b2c2 d5e6 c2c1 e6d5 c1c2 f4f5 c2c1 d5e4 c1d1 e4d5 d1c1 d5c4 c1c2 c4e6 c2b2 f5f4 b2c2 f4e4 c2b2 e4f4 b2c2 f4e4 c2c1
2017-05-03 13:33:52,629-->1:go wtime 6995 btime 7168 winc 6000 binc 6000
2017-05-03 13:33:52,645<--1:info string Phase is 235
2017-05-03 13:33:52,645<--1:info depth 1 time 0 score cp 471 pv e4f4 c1b2 
2017-05-03 13:33:52,661<--1:info depth 2 time 0 score cp 471 pv e4f4 c1b2 f4f5 
2017-05-03 13:33:52,676<--1:info depth 3 time 0 score cp 471 pv e4f4 c1b2 f4f5 b2c2 
2017 - 05 - 03 13 : 33 : 52, 692<--1 : info depth 4 time 0 score cp 471 pv e4f4 c1b2 f4f5 b2c2
2017 - 05 - 03 13 : 33 : 52, 707<--1 : info depth 5 time 0 score cp 471 pv e4f4 c1b2 f4f5 b2c2
2017 - 05 - 03 13 : 33 : 52, 723<--1 : info depth 6 time 0 score cp 471 pv e4f4 c1b2 f4f5 b2c2
2017 - 05 - 03 13 : 33 : 52, 739<--1 : info depth 7 time 0 score cp 471 pv e4f4 c1b2 f4f5 b2c2
2017 - 05 - 03 13 : 33 : 52, 754<--1 : info depth 8 time 0 score cp 471 pv e4f4 c1b2 f4f5 b2c2
2017 - 05 - 03 13 : 33 : 52, 770<--1 : info depth 9 time 0 score cp 471 pv e4f4 c1b2 f4f5 b2c2
2017 - 05 - 03 13 : 33 : 52, 785<--1 : info depth 10 time 0 score cp 471 pv e4f4 c1b2 f4f5 b2c2
2017 - 05 - 03 13 : 33 : 53, 191<--1 : info depth 11 time 1000 score cp 471 pv e4f4 c1b2 f4f5 b2c2
2017 - 05 - 03 13 : 33 : 53, 456<--2 : info nodes 1525472 nps 13 hashfull 582
2017 - 05 - 03 13 : 33 : 53, 659<--1 : info nodes 229496 nps 229496 hashfull 657
2017 - 05 - 03 13 : 33 : 54, 673<--1 : info nodes 444256 nps 214760 hashfull 658
2017 - 05 - 03 13 : 33 : 55, 687<--1 : info nodes 624819 nps 180563 hashfull 658
2017 - 05 - 03 13 : 33 : 56, 701<--1 : info nodes 785436 nps 160617 hashfull 658
2017 - 05 - 03 13 : 33 : 57, 715<--1 : info nodes 1022119 nps 236683 hashfull 659
2017 - 05 - 03 13 : 33 : 58, 729<--1 : info nodes 1303567 nps 281448 hashfull 659
2017 - 05 - 03 13 : 33 : 58, 745<--1 : bestmove e4f4
2017 - 05 - 03 13 : 33 : 58, 745 * 1 * Zug gefunden : Ke4 - f4
2017 - 05 - 03 13 : 33 : 58, 916 * 2 * Start calc, move no : 200
2017 - 05 - 03 13 : 33 : 58, 916-->2 : position startpos moves e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d3 a7a6 c4b3 d7d6 b1d2 c5a7 h2h3 e8g8 e1g1 a7c5 b3a4 c8e6 a4c6 b7c6 d1a4 d8d7 d3d4 e5d4 f3d4 c5d4 a4d4 c6c5 d4d3 d6d5 e4e5 e6f5 d3f3 f6e4 f3d1 e4g5 f2f4 g5e6 d2c4 a8e8 c4e3 f5e4 d1d2 d7c6 e3g4 e4f5 g4e3 f5e4 e3g4 d5d4 f4f5 d4c3 b2c3 h7h5 f5e6 h5g4 e6f7 f8f7 f1f7 g8f7 d2f2 f7g8 h3g4 e8e5 c1f4 e5e7 a1d1 e7e8 f2d2 a6a5 d1e1 a5a4 g4g5 a4a3 c3c4 e8f8 g2g3 f8a8 e1e3 a8a7 d2e1 e4f5 e3e8 g8h7 e8e5 c6d7 e5c5 c7c6 c5e5 a7b7 e1d2 b7b1 e5e1 d7d2 f4d2 b1b2 d2c1 b2a2 c1e3 a2c2 c4c5 a3a2 e1a1 f5h3 g1h1 h7g6 h1g1 c2e2 e3c1 h3e6 c1f4 e6c4 g1h1 e2b2 f4e5 c4d5 h1g1 b2b1 g1f2 b1a1 e5a1 g6g5 a1g7 g5f5 f2e3 d5g2 e3d3 f5g4 g7e5 g2d5 d3e3 d5e6 e3e4 e6f5 e4e3 f5b1 e3d2 g4f3 d2c1 f3e4 e5f6 e4d5 g3g4 d5c5 c1b2 c5d5 g4g5 d5e6 f6d4 e6f5 d4f6 f5e6 f6c3 e6f5 c3f6 c6c5 b2b3 f5e6 b3c4 e6d6 c4b3 d6e6 f6c3 e6f5 c3f6 c5c4 b3c4 b1e4 c4b4 e4f3 b4b3 f3d5 b3b2 f5f4 b2c2 d5e6 c2c1 e6d5 c1c2 f4f5 c2c1 d5e4 c1d1 e4d5 d1c1 d5c4 c1c2 c4e6 c2b2 f5f4 b2c2 f4e4 c2b2 e4f4 b2c2 f4e4 c2c1 e4f4
2017 - 05 - 03 13 : 33 : 58, 916-->2 : go wtime 6995 btime 7058 winc 6000 binc 6000
2017 - 05 - 03 13 : 33 : 58, 947<--2 : info depth 1 time 0 score cp 0 pv c1c2(none)
2017 - 05 - 03 13 : 33 : 58, 963<--2 : info depth 2 time 0 score cp 0 pv c1c2(none) c2b2
2017 - 05 - 03 13 : 33 : 58, 979<--2 : info depth 3 time 0 score cp 0 pv c1c2(none) c2b2 f4g4
2017 - 05 - 03 13 : 33 : 58, 994<--2 : info depth 4 time 0 score cp 0 pv c1c2(none) c2b2 f4g4 b2c1
2017 - 05 - 03 13 : 33 : 59, 010<--2 : info depth 5 time 0 score cp 0 pv c1c2(none) c2b2 f4g4 b2c1 g4f5
2017 - 05 - 03 13 : 33 : 59, 025<--2 : info depth 6 time 0 score cp 0 pv c1c2(none) c2b2 f4g4 b2c1 g4f5
2017 - 05 - 03 13 : 33 : 59, 041<--2 : info depth 7 time 0 score cp 0 pv c1c2(none) c2b2 f4g4 b2c1 g4f5
2017 - 05 - 03 13 : 33 : 59, 057<--2 : info depth 8 time 0 score cp 0 pv c1c2
2017 - 05 - 03 13 : 33 : 59, 072<--2 : info depth 9 time 0 score cp 0 pv c1c2
2017 - 05 - 03 13 : 33 : 59, 088<--2 : info depth 10 time 0 score cp 0 pv c1c2
2017 - 05 - 03 13 : 33 : 59, 103<--2 : info depth 11 time 1000 score cp 0 pv c1c2
2017 - 05 - 03 13 : 33 : 59, 493<--2 : info depth 12 time 1000 score cp 0 pv c1c2
2017 - 05 - 03 13 : 33 : 59, 743<--1 : info nodes 1303572 nps 5 hashfull 659
2017 - 05 - 03 13 : 33 : 59, 962<--2 : info nodes 218497 nps 218497 hashfull 583
2017 - 05 - 03 13 : 34 : 00, 976<--2 : info nodes 432871 nps 214374 hashfull 584
2017 - 05 - 03 13 : 34 : 01, 818<--2 : info depth 13 time 3000 score cp 0 pv c1c2
2017 - 05 - 03 13 : 34 : 01, 990<--2 : info nodes 645248 nps 212377 hashfull 584
2017 - 05 - 03 13 : 34 : 03, 004<--2 : info nodes 845832 nps 200584 hashfull 586
2017 - 05 - 03 13 : 34 : 03, 160<--2 : info depth 14 time 5000 score cp 0 pv c1c2
2017 - 05 - 03 13 : 34 : 04, 018<--2 : info nodes 1130319 nps 284487 hashfull 586
2017 - 05 - 03 13 : 34 : 04, 033<--2 : bestmove c1c2
2017 - 05 - 03 13 : 34 : 04, 033 * 2 * Zug gefunden : Kc1 - c2
2017 - 05 - 03 13 : 34 : 04, 205 * ***Tour**Partie zuende : ½ - ½{ Stellungswiederholung }


10.05.17: In einem Turnierspiel gegen TSCP wurde ein drohendes Matt viel zu spät erkannt
2017-05-12 12:26:58,668-->1:position startpos moves b1c3 c7c5 e2e4 d7d6 g1f3 g8f6 d2d4 c5d4 f3d4 a7a6 c1e3 e7e5 d4b3 c8e6 f2f3 b8d7 d1d2 f8e7 e1c1 a8c8 c1b1 e8g8 b3c1 c8c6 c1d3 d7b6 d3b4 c6c8 f1e2 a6a5 e3b6 d8b6 b4d5 f6d5 c3d5 e6d5 d2d5 c8c5 d5d3 f8c8 h1f1 c8c6 c2c4 e7g5 g2g3 b6b4 b2b3 a5a4 f3f4 g5f6 f4e5 f6e5 d3f3 f7f6 f3h5 h7h6 h5e8 g8h7 e2h5
2017-05-12 12:26:58,668-->1:go infinite
2017-05-12 12:26:58,668**Dauer Start Analyse:31
2017-05-12 12:26:58,668<--1:info string Phase is 64
2017-05-12 12:26:58,668<--1:info depth 1 time 0 score cp 169 pv c5c4 
2017 - 05 - 12 12 : 26 : 58, 684<--1 : info depth 2 time 0 score cp 159 pv c5c4 h5g6
2017 - 05 - 12 12 : 26 : 58, 715<--1 : info depth 3 time 0 score cp - 39 pv g7g5 e8g6 h7h8
2017 - 05 - 12 12 : 26 : 59, 043<--1 : info depth 4 time 1000 score cp - 39 pv g7g5 e8g6 h7h8 g6h6
2017 - 05 - 12 12 : 26 : 59, 667<--1 : info depth 5 time 1000 score cp - 48 pv g7g5 h5f7 h6h5 f7d5 a4b3
2017 - 05 - 12 12 : 27 : 03, 380<--1 : info depth 6 time 5000 score cp - 49 pv g7g5 e8g6 h7h8 g6h6 h8g8 d1d3
2017 - 05 - 12 12 : 27 : 18, 699<--1 : info depth 7 time 20000 score cp - 850 pv b4b3 a2b3 g7g5 h5f7 h6h5 b3b4 c5c4

-> hoffentlich gelöst durch if (isCheck) depth++; sowie verbesserte getQuiescence, die auch bei vorangegangenem Schach weiterspielt

Ein weiterer Kanididat:
position fen r1bq1rk1 / pp2bpp1 / 2np1n2 / 2p1p3 / 2P1P1Pp / 2NPB2P / PP3PB1 / R2QK1NR w KQ - 0 1 moves g1f3 c8e6 f3h4 f6g4 h3g4 e7h4 g2f3 c6d4 c3d5 h4e7 e1f1 f7f6 f1g1 d8d7 h1h4 f6f5 g4g5 f5f4 e3d4 c5d4 h4h5 e6f7 h5h1 e7g5 f3g4 d7c6 d1f1 f8e8 f1h3 f7d5 c4d5
c6c5 darf nicht gespielt werden; die frühe 0.3 erkennt das sehr spät


27.05.17: In Spiel 54 von Current vs a wurde Zugwiederholung nicht erkannt und dadurch bessere Position verspielt

15.06.17 : In 2n5 / 2K4b / 2N1p3 / 2pkr2r / n6p / 1P3P2 / N2P3Q / 8 w - -0 1 (KN 8.6.17) wurde das Matt in 4 wegen schlechtem Null Move Pruning erst in Tiefe 10 nach über 6 Minuten erkannt


14.07.17 (erste Bitboard Version) : Probleme mit falschem Eintrag in pvline
2017 - 07 - 14 13 : 25 : 46, 803-- > 2:position fen rn1qkb1r / pp3ppp / 2p1pn2 / 3p4 / 4P3 / 2N2B1P / PPPP1PP1 / R1BQK2R w KQkq - 0 1 moves d2d4 d5e4 f3e4 f8b4 c1g5
2017 - 07 - 14 13 : 25 : 46, 803-- > 2:go wtime 3981 btime 3971 winc 3000 binc 3000
2017 - 07 - 14 13 : 25 : 46, 813 < --2 : info string Phase is 21
    2017 - 07 - 14 13 : 25 : 46, 823 < --2 : info depth 1 time 0 score cp 33 pv b8d7 d1e2
    2017 - 07 - 14 13 : 25 : 46, 833 < --2 : info depth 2 time 0 score cp 33 pv b8d7 d1e2 b4c3
    2017 - 07 - 14 13 : 25 : 46, 843 < --2 : info depth 3 time 0 score cp 33 pv b8d7 d1e2 b4c3 b2c3
    2017 - 07 - 14 13 : 25 : 46, 853 < --2 : info depth 4 time 0 score cp 33 pv b8d7 d1e2 b4c3 b2c3 d8b6
    2017 - 07 - 14 13 : 25 : 46, 863 < --2 : info depth 5 time 0 score cp 33 pv b8d7 d1e2 b4c3 b2c3 d8b6 g5f6
    2017 - 07 - 14 13 : 25 : 46, 873 < --2 : info depth 6 time 0 score cp 33 pv b8d7 d1e2 b4c3 b2c3 d8b6 g5f6 d7f6
    2017 - 07 - 14 13 : 25 : 47, 253 < --2 : info depth 7 time 437 score cp 33 pv b8d7 d1f3 b4c3 b2c3 d8a5 g5f6 d7f6 e4d3
    2017 - 07 - 14 13 : 25 : 47, 823 < --2 : info nodes 707058 nps 706652 hashfull 535
    2017 - 07 - 14 13 : 25 : 48, 623 < --2 : Alarm.Ungültiger Zug e8c8 in pvline
    2017 - 07 - 14 13 : 25 : 48, 633 < --2 : info depth 8 time 1804 score cp 55 pv b4c3 b2c3 b8d7 d1f3 d8a5 g5f6 d7f6 e1g1 e8c8
    2017 - 07 - 14 13 : 26 : 30, 794 * ***Tour**LOOPWATCH - MOVE - ABORTED - after very long movetime

    position fen rn1qkb1r / pp3ppp / 2p1pn2 / 3p4 / 4P3 / 2N2B1P / PPPP1PP1 / R1BQK2R w KQkq - 0 1 moves d2d4 d5e4 f3e4 f8b4 c1g5   b4c3 b2c3 b8d7 d1f3 d8a5 g5f6 d7f6 e1g1 e8c8

    Analyse : Da steht ein weißer Bauer auf b7.Wie kommt der da hin ? Vermutlich noch ein Play / Unplay - Fehler.

    ->gelöst am 15.7.war ein "Bauer schlägt eigenes EP nach Nullmove" - Problem

    01.09.17 : Falsche Züge in PV - Line, z.B.
    info depth 8 time 3415 score cp 4 pv d4b2 d8b6 f2f4 g4f3 b2c3 c3e5 ... ergibt
    position fen rnbqkbnr / ppp1pp1p / 3p4 / 6p1 / 8 / BP6 / P1PPPPPP / RN1QKBNR w KQkq - 0 1 moves b1c3 b8c6 e2e3 g8f6 d2d4 g5g4 f1b5 c8d7 g1e2 e7e6 e1g1 h8g8 e2f4 f8e7 d4d5 e6d5 f4d5 c6e5 b5d3 g8g5 d3e4 f6e4 c3e4 g5f5 e4g3 f5g5 g3e4 g5f5 e4g3 f5g5 c2c4 d7e6 g3e4 g5f5 e4g3 f5g5 g3e4 g5f5 a3b2 a8b8 b2d4 c7c5 d5e7 e8e7 d4b2 d8b6 f2f4 g4f3 b2c3 c3e5

    info depth 5 time 61 score cp 15 pv f7f6 e5f6 a7a6 g7f6 ... ergibt
    position fen r1bqkbnr / 1ppppppp / 2n5 / p7 / 8 / 5N1P / PPPPPPP1 / RNBQKB1R w KQkq - 0 1 moves e2e4 d7d5 e4e5 e7e6 d2d4 c8d7 f1d3 c6b4 c1f4 c7c5 b1c3 b4d3 d1d3 d8b6 e1c1 c5c4 d3e2 f8b4 e2d2 g8e7 a2a3 b4c3 d2c3 e8g8 h3h4 e7f5 g2g4 f5e7 g4g5 a8a7 h4h5 f7f6 e5f6 a7a6 g7f6
    Vermutung : s2m ist nicht korrekt im Hash kodiert, da der falsche Zug immer von der falschen Seite gemacht wird.Oder Nullmove.
    Im move steckt keine Info, welche Seite zieht und in playMove wird ebenfalls nicht geprüft, ob die ziehende Seite zur Steinfarbe passt
    Deswegen wird der Zug in getpvline nicht als illegal erkannt.Aber wie kommt er da hin ? Hash - Kollision ? ?
    Wohl kaum, dann könnte dort ja jeder beliebige Zug stehen und nicht immer ein passender der falschen Farbe.
    weiteres Beispiel(b1b7 in depth 10; Turm zieht vorher auf b1 und dann direkt weiter) :
position fen rnbqkbnr/pppp1pp1/4p2p/8/8/3P4/PPPNPPPP/R1BQKBNR w KQkq - 0 1 moves g1f3 b8c6 e2e4 d7d5 e4d5 e6d5 d1e2 d8e7 d2b3 c8g4 e2e7 g8e7 f1e2 e7f5 h2h3 g4f3 e2f3 c6d4 b3d4 f5d4 f3d1 d4e6 c1e3 d5d4 e3d2 f8d6 d1g4 e8g8 e1c1 a8e8 d1e1 e8e7 e1e2 f7f5 g4f3 c7c6 h1e1 f8f6 h3h4 d6c5 h4h5 a7a6 d2f4 c5b4 f4d2 b4d2 c1d2 g8f8 d2d1 f8e8 d1c1 e8d7 e2e5 f6f7 e5a5 e6g5 e1e7 d7e7 f3e2 e7d6 f2f4 g5e6 g2g3 c6c5 e2f3 f7c7 b2b4 c7e7 a2a4 e7d7 b4b5 a6b5 a5b5 e6d8 b5b6 d6c7 b6g6 d7e7 f3d5 e7d7 d5f3 d7e7 a4a5 c7b8 f3d5 b8c7 c1d2 e7d7 d5f3 d7e7 c2c3 c7c8 f3d5 c8d7 c3d4 c5d4 d2c2 d7c8 c2b3 e7c7 b3b4 c8b8 d5c4 b8c8 g6d6 c7e7 b4c5 e7c7 c5b5 c7e7 b5b6 e7e2 d6d4 d8c6 d4d5 c6e7 d5e5 e2b2 c4b5 e7c6 e5f5 c6d8 f5e5 d8c6 a5a6 c6e5 a6b7 c8b8 f4e5 b2e2 d3d4 e2e3 g3g4 e3e4 b5c6 e4d4 e5e6 d4b4 b6c5 b4b1
...
info depth 9 time 162 score cp 415 pv e6e7 b1e1 e7e8Q e1e8 c6e8 b8b7 e8c6 b7c7 c6d5
info depth 10 time 537 score cp 438 pv e6e7 b1e1 c5d6 g7g6 h5g6 e1d1 d6c5 d1b1 b1b7

02.09.17: Hier findet er gar keinen Zug (zählt bis Tiefe 256 und dann bestmove (none)):
position fen rnb1kbnr/pp1pppp/1q6/1Bp5/8/4P3/PPPP1PPP/RNBQK1NR w KQkq - 0 1 moves b1c3 g8f6 g1f3 b8c6 d2d4 c5d4 e3d4 a7a6 d4d5 a6b5 d5c6 b6c6 e1g1 b5b4 c3e2 d7d6 e2d4 c6d5 d1e2 c8g4 f1d1 e7e5 c1g5 f6e4 d4b5 d5c6 b5c7 c6c7 e2e4 g4f3 g2f3 c7c6 e4b4 f7f6 g5e3 c6c2 b4b7 a8a2 b7b5 e8e7 b5b7 e7e8 b7b5 e8e7 b5b7 e7e8 b7b5
... nicht reproduzierbar


http://pwnedthegameofchess.com/engine/
http://www.herderschach.de/index.html


#endif

#ifdef _WIN32

string GetSystemInfo()
{
    // shameless copy from MSDN example explaining __cpuid
    char CPUString[0x20];
    char CPUBrandString[0x40];
    int CPUInfo[4] = { -1 };
    int nSteppingID = 0;
    int nModel = 0;
    int nFamily = 0;
    int nProcessorType = 0;
    int nExtendedmodel = 0;
    int nExtendedfamily = 0;
    int nBrandIndex = 0;
    int nCLFLUSHcachelinesize = 0;
    int nLogicalProcessors = 0;
    int nAPICPhysicalID = 0;
    int nFeatureInfo = 0;
    int nCacheLineSize = 0;
    int nL2Associativity = 0;
    int nCacheSizeK = 0;
    int nPhysicalAddress = 0;
    int nVirtualAddress = 0;
    int nRet = 0;

    int nCores = 0;
    int nCacheType = 0;
    int nCacheLevel = 0;
    int nMaxThread = 0;
    int nSysLineSize = 0;
    int nPhysicalLinePartitions = 0;
    int nWaysAssociativity = 0;
    int nNumberSets = 0;

    unsigned    nIds, nExIds, i;

    bool    bSSE3Instructions = false;
    bool    bMONITOR_MWAIT = false;
    bool    bCPLQualifiedDebugStore = false;
    bool    bVirtualMachineExtensions = false;
    bool    bEnhancedIntelSpeedStepTechnology = false;
    bool    bThermalMonitor2 = false;
    bool    bSupplementalSSE3 = false;
    bool    bL1ContextID = false;
    bool    bCMPXCHG16B = false;
    bool    bxTPRUpdateControl = false;
    bool    bPerfDebugCapabilityMSR = false;
    bool    bSSE41Extensions = false;
    bool    bSSE42Extensions = false;
    bool    bPOPCNT = false;
    bool    bBMI2 = false;

    bool    bMultithreading = false;

    bool    bLAHF_SAHFAvailable = false;
    bool    bCmpLegacy = false;
    bool    bSVM = false;
    bool    bExtApicSpace = false;
    bool    bAltMovCr8 = false;
    bool    bLZCNT = false;
    bool    bSSE4A = false;
    bool    bMisalignedSSE = false;
    bool    bPREFETCH = false;
    bool    bSKINITandDEV = false;
    bool    bSYSCALL_SYSRETAvailable = false;
    bool    bExecuteDisableBitAvailable = false;
    bool    bMMXExtensions = false;
    bool    bFFXSR = false;
    bool    b1GBSupport = false;
    bool    bRDTSCP = false;
    bool    b64Available = false;
    bool    b3DNowExt = false;
    bool    b3DNow = false;
    bool    bNestedPaging = false;
    bool    bLBRVisualization = false;
    bool    bFP128 = false;
    bool    bMOVOptimization = false;

    bool    bSelfInit = false;
    bool    bFullyAssociative = false;

    __cpuid(CPUInfo, 0);
    nIds = CPUInfo[0];
    memset(CPUString, 0, sizeof(CPUString));
    *((int*)CPUString) = CPUInfo[1];
    *((int*)(CPUString + 4)) = CPUInfo[3];
    *((int*)(CPUString + 8)) = CPUInfo[2];

    // Get the information associated with each valid Id
    for (i = 0; i <= nIds; ++i)
    {
        __cpuid(CPUInfo, i);
        // Interpret CPU feature information.
        if (i == 1)
        {
            nSteppingID = CPUInfo[0] & 0xf;
            nModel = (CPUInfo[0] >> 4) & 0xf;
            nFamily = (CPUInfo[0] >> 8) & 0xf;
            nProcessorType = (CPUInfo[0] >> 12) & 0x3;
            nExtendedmodel = (CPUInfo[0] >> 16) & 0xf;
            nExtendedfamily = (CPUInfo[0] >> 20) & 0xff;
            nBrandIndex = CPUInfo[1] & 0xff;
            nCLFLUSHcachelinesize = ((CPUInfo[1] >> 8) & 0xff) * 8;
            nLogicalProcessors = ((CPUInfo[1] >> 16) & 0xff);
            nAPICPhysicalID = (CPUInfo[1] >> 24) & 0xff;
            bSSE3Instructions = (CPUInfo[2] & 0x1) || false;
            bMONITOR_MWAIT = (CPUInfo[2] & 0x8) || false;
            bCPLQualifiedDebugStore = (CPUInfo[2] & 0x10) || false;
            bVirtualMachineExtensions = (CPUInfo[2] & 0x20) || false;
            bEnhancedIntelSpeedStepTechnology = (CPUInfo[2] & 0x80) || false;
            bThermalMonitor2 = (CPUInfo[2] & 0x100) || false;
            bSupplementalSSE3 = (CPUInfo[2] & 0x200) || false;
            bL1ContextID = (CPUInfo[2] & 0x300) || false;
            bCMPXCHG16B = (CPUInfo[2] & 0x2000) || false;
            bxTPRUpdateControl = (CPUInfo[2] & 0x4000) || false;
            bPerfDebugCapabilityMSR = (CPUInfo[2] & 0x8000) || false;
            bSSE41Extensions = (CPUInfo[2] & 0x80000) || false;
            bSSE42Extensions = (CPUInfo[2] & 0x100000) || false;
            bPOPCNT = (CPUInfo[2] & 0x800000) || false;
            nFeatureInfo = CPUInfo[3];
            bMultithreading = (nFeatureInfo & (1 << 28)) || false;
        }

        if (i == 7)
        {
            // this is not in the MSVC2012 example but may be useful later
            bBMI2 = (CPUInfo[1] & 0x100) || false;
        }
    }


    // Calling __cpuid with 0x80000000 as the InfoType argument
    // gets the number of valid extended IDs.
    __cpuid(CPUInfo, 0x80000000);
    nExIds = CPUInfo[0];
    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    // Get the information associated with each extended ID.
    for (i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(CPUInfo, i);
        if (i == 0x80000001)
        {
            bLAHF_SAHFAvailable = (CPUInfo[2] & 0x1) || false;
            bCmpLegacy = (CPUInfo[2] & 0x2) || false;
            bSVM = (CPUInfo[2] & 0x4) || false;
            bExtApicSpace = (CPUInfo[2] & 0x8) || false;
            bAltMovCr8 = (CPUInfo[2] & 0x10) || false;
            bLZCNT = (CPUInfo[2] & 0x20) || false;
            bSSE4A = (CPUInfo[2] & 0x40) || false;
            bMisalignedSSE = (CPUInfo[2] & 0x80) || false;
            bPREFETCH = (CPUInfo[2] & 0x100) || false;
            bSKINITandDEV = (CPUInfo[2] & 0x1000) || false;
            bSYSCALL_SYSRETAvailable = (CPUInfo[3] & 0x800) || false;
            bExecuteDisableBitAvailable = (CPUInfo[3] & 0x10000) || false;
            bMMXExtensions = (CPUInfo[3] & 0x40000) || false;
            bFFXSR = (CPUInfo[3] & 0x200000) || false;
            b1GBSupport = (CPUInfo[3] & 0x400000) || false;
            bRDTSCP = (CPUInfo[3] & 0x8000000) || false;
            b64Available = (CPUInfo[3] & 0x20000000) || false;
            b3DNowExt = (CPUInfo[3] & 0x40000000) || false;
            b3DNow = (CPUInfo[3] & 0x80000000) || false;
        }

        // Interpret CPU brand string and cache information.
        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000006)
        {
            nCacheLineSize = CPUInfo[2] & 0xff;
            nL2Associativity = (CPUInfo[2] >> 12) & 0xf;
            nCacheSizeK = (CPUInfo[2] >> 16) & 0xffff;
        }
        else if (i == 0x80000008)
        {
            nPhysicalAddress = CPUInfo[0] & 0xff;
            nVirtualAddress = (CPUInfo[0] >> 8) & 0xff;
        }
        else if (i == 0x8000000A)
        {
            bNestedPaging = (CPUInfo[3] & 0x1) || false;
            bLBRVisualization = (CPUInfo[3] & 0x2) || false;
        }
        else if (i == 0x8000001A)
        {
            bFP128 = (CPUInfo[0] & 0x1) || false;
            bMOVOptimization = (CPUInfo[0] & 0x2) || false;
        }
    }

    for (i = 0;; i++)
    {
        __cpuidex(CPUInfo, 0x4, i);
        if (!(CPUInfo[0] & 0xf0)) break;

        if (i == 0)
        {
            //FIXME: This is not the real number of cores but the maximum possible - 1
            nCores = CPUInfo[0] >> 26;
        }

        nCacheType = (CPUInfo[0] & 0x1f);
        nCacheLevel = (CPUInfo[0] & 0xe0) >> 5;
        bSelfInit = (((CPUInfo[0] & 0x100) >> 8) > 0);
        bFullyAssociative = (((CPUInfo[0] & 0x200) >> 9) > 0);
        nMaxThread = (CPUInfo[0] & 0x03ffc000) >> 14;
        nSysLineSize = (CPUInfo[1] & 0x0fff);
        nPhysicalLinePartitions = (CPUInfo[1] & 0x03ff000) >> 12;
        nWaysAssociativity = (CPUInfo[1]) >> 22;
        nNumberSets = CPUInfo[2];
    }
    return CPUBrandString;
}

#else

string GetSystemInfo()
{
    return "some Linux box";
}

#endif


long long perft(int depth, bool dotests)
{
    long long retval = 0;

    if (dotests)
    {
        if (pos.hash != zb.getHash())
        {
            printf("Alarm! Wrong Hash! %llu\n", zb.getHash());
            pos.print();
        }
        int val1 = pos.getValue();
        pos.mirror();
        int val2 = pos.getValue();
        pos.mirror();
        int val3 = pos.getValue();
        if (!(val1 == val3 && val1 == -val2))
        {
            printf("Mirrortest  :error  (%d / %d / %d)\n", val1, val2, val3);
            pos.print();
            pos.mirror();
            pos.print();
            pos.mirror();
            pos.print();
		}
    }
    chessmovelist* movelist = pos.getMoves();
    //movelist->sort();
    //printf("Path: %s \nMovelist : %s\n", p->actualpath.toString().c_str(), movelist->toString().c_str());

    if (depth == 0)
        retval = 1;
    else if (depth == 1)
    {
        for (int i = 0; i < movelist->length; i++)
        {
            if (pos.playMove(&movelist->move[i]))
            {
                //printf("%s ok ", movelist->move[i].toString().c_str());
                retval++;
                pos.unplayMove(&movelist->move[i]);
            }
        }
    }

    else
    {
        for (int i = 0; i < movelist->length; i++)
        {
            if (pos.playMove(&movelist->move[i]))
            {
                //printf("\nMove: %s  ", movelist->move[i].toString().c_str());
                retval += perft(depth - 1, dotests);
                pos.unplayMove(&movelist->move[i]);
            }
        }
    }
    free(movelist);
    //printf("\nAnzahl: %d\n", retval);
    return retval;
}

void perftest(bool dotests, int maxdepth)
{
    struct perftestresultstruct
    {
        string fen;
        unsigned long long nodes[10];
    } perftestresults[] =
    {
        {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            { 1, 20, 400, 8902, 197281, 4865609, 119060324 }
        },
        {
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            { 1, 48,2039,97862, 4085603, 193690690 }
        },
        {
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            { 1, 14, 191, 2812, 43238, 674624, 11030083, 178633661 }
        },
        {
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            { 1, 6, 264, 9467, 422333, 15833292, 706045033 }
        },
        {
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            { 1, 44, 1486, 62379, 2103487, 89941194 }
        },
        {
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
            { 1, 46, 2079, 89890, 3894594, 164075551, 6923051137 }
        },
        {
            "",
            {}
        }
    };

    int i = 0;
    printf("\n\nPerft results for %s (Build %s %s)\n", en.name, __DATE__, __TIME__);
    printf("System: %s\n", GetSystemInfo().c_str());
    printf("Depth = %d      Hash-/Mirror-Tests %s\n", maxdepth, (dotests ? "enabled" : "disabled"));
    printf("========================================================================\n");

    float df;
    U64 totalresult = 0ULL;

    long long perftstarttime = getTime();
    long long perftlasttime = perftstarttime;

    while (perftestresults[i].fen != "")
    {
        pos.getFromFen(perftestresults[i].fen.c_str());
        int j = 0;
        while (perftestresults[i].nodes[j] > 0 && j <= maxdepth)
        {
            long long starttime = getTime();

            U64 result = perft(j, dotests);
            totalresult += result;

            perftlasttime = getTime();
            df = float(perftlasttime - starttime) / (float) en.frequency;
            printf("Perft %d depth %d  : %*llu  %*f sec.  %*d nps ", i + 1, j, 10, result, 10, df, 8, (int)(df > 0.0 ? (double)result / df : 0));
            if (result == perftestresults[i].nodes[j])
                printf("  OK\n");
            else
                printf("  wrong (should be %llu)\n", perftestresults[i].nodes[j]);
            j++;
        }
        if (perftestresults[++i].fen != "")
            printf("\n");
    }
    df = float(perftlasttime - perftstarttime) / (float)en.frequency;
    printf("========================================================================\n");
    printf("Total:             %*llu  %*f sec.  %*d nps \n", 10, totalresult, 10, df, 8, (int)(df > 0.0 ? (double)totalresult / df : 0));
}


void doBenchmark()
{
    struct benchmarkstruct
    {
        string name;
        string fen;
        int depth;
        int resethash;
        long long time;
        long long nodes;
    } benchmark[] =
    {
        {   "Startposition",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            8,
            1
        },
        {   "Lasker Test",
            "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1",
            28,
            1
        },
        {   "Matt in 6",
            "3k4/3P4/3P4/3P4/3P4/1P1P1P2/3P4/1R1K4 w - - 0 1",
            12,
            1
        },
        { "Wacnew 167",
        "7Q/ppp2q2/3p2k1/P2Ppr1N/1PP5/7R/5rP1/6K1 b - - 0 1",
        8,
        1
        },
        { "Wacnew 212",
        "rn1qr2Q/pbppk1p1/1p2pb2/4N3/3P4/2N5/PPP3PP/R4RK1 w - - 0 1",
        8,
        1
        },
        { "Carlos 6",
        "rn1q1r2/1bp1bpk1/p3p2p/1p2N1pn/3P4/1BN1P1B1/PPQ2PPP/2R2RK1 w - - 0 1",
        8,
        1
        },
        {
            "", "", 0, 0
        }
    };

    long long starttime, endtime;

    int i = 0;
    while (benchmark[i].fen != "")
    {
        struct benchmarkstruct *bm = &benchmark[i];
        if (bm->resethash)
            en.setOption("clearhash", "true");

        en.communicate("position fen " + bm->fen);
        starttime = getTime();
        en.communicate("go depth " + to_string(bm->depth));
        endtime = getTime();
        bm->time = endtime - starttime;
        bm->nodes = en.nodes;
        i++;
    }

    i = 0;
    long long totaltime = 0;
    long long totalnodes = 0;
    printf("\n\nBenchmark results for %s (Build %s %s):\n", en.name, __DATE__, __TIME__);
    printf("System: %s\n", GetSystemInfo().c_str());
    printf("========================================================================\n");
    while (benchmark[i].fen != "")
    {
        struct benchmarkstruct *bm = &benchmark[i];
        totaltime += bm->time;
        totalnodes += bm->nodes;
        printf("Bench # %2d (%20s / %2d):  %10f sec.  %10lld nps\n", i + 1, bm->name.c_str(), bm->depth, (float)bm->time / (float)en.frequency, bm->nodes * en.frequency / bm->time);
        i++;
    }
    printf("========================================================================\n");
    printf("Overall:                                 %10f sec.  %*lld nps\n", ((float)totaltime / (float)en.frequency), 10, totalnodes * en.frequency / totaltime);
}



#ifdef _WIN32

void readfromengine(HANDLE pipe, enginestate *es)
{
    DWORD dwRead;
    CHAR chBuf[BUFSIZE];
    vector<string> lines, token;

    do
    {
        memset(chBuf, 0, BUFSIZE);
        ReadFile(pipe, chBuf, BUFSIZE, &dwRead, NULL);
        stringstream ss(chBuf);
        string line;

        while (getline(ss, line)) {
            const char *s = line.c_str();
            if (strstr(s, "uciok") != NULL && es->phase == 0)
                es->phase = 1;
            if (strstr(s, "readyok") != NULL && es->phase == 1)
                es->phase = 2;
            if (es->phase == 2)
            {
                const char *bestmovestr = strstr(s, "bestmove ");
                const char *pv = strstr(s, " pv ");
                const char *score = strstr(s, " cp ");
                const char *mate = strstr(s, " mate ");
                const char *bmptr = NULL;
                if (bestmovestr != NULL)
                {
                    bmptr = bestmovestr;
                }
                if (pv != NULL)
                {
                    bmptr = pv;
                }
                if (bmptr)
                {
                    token = SplitString(bmptr);
                    if (token.size() > 1)
                    {
                        es->enginesbestmove = token[1];
                        string myPv = token[1];
                        if (strstr(es->bestmoves.c_str(), myPv.c_str()) != NULL
                            || es->bestmoves == "" && strstr(es->avoidmoves.c_str(), myPv.c_str()) == NULL)
                        {
                            if (es->firstbesttimesec < 0)
                            {
                                es->firstbesttimesec = (clock() - es->starttime) / CLOCKS_PER_SEC;
                                //printf("%d %d  %d\n", es->firstbesttimesec, es->starttime, clock());
                            }

                            if (score)
                            {
                                vector<string> scoretoken = SplitString(score);
                                if (scoretoken.size() > 1)
                                {
                                    try
                                    {
                                        es->score = stoi(scoretoken[1]);
                                    }
                                    catch (const invalid_argument&) {}
                                }
                            }
                            if (mate)
                            {
                                vector<string> matetoken = SplitString(mate);
                                if (matetoken.size() > 1)
                                {
                                    try
                                    {
                                        es->score = SCOREWHITEWINS - stoi(matetoken[1]);
                                    }
                                    catch (const invalid_argument&) {}
                                }
                            }
                        }
                        else {
                            es->firstbesttimesec = -1;
                            //printf("%d %d  %d\n", es->firstbesttimesec, es->starttime, clock());
                        }
                    }
                }
                if (bestmovestr)
                    es->phase = 3;
            }
        }
    } while (true);
}

BOOL writetoengine(HANDLE pipe, char *s)
{
    DWORD written;
    return WriteFile(pipe, s, (DWORD)strlen(s), &written, NULL);
}

void testengine(string epdfilename, int startnum, string engineprg, string logfilename, string comparefilename, int maxtime, int flags)
{
    struct enginestate es;
    string line;
    thread *readThread;
    ifstream comparefile;
    bool compare = false;
    char buf[1024];

    // Open the epd file for reading
    ifstream epdfile(epdfilename);
    if (!epdfile.is_open())
    {
        printf("Cannot open %s.\n", epdfilename.c_str());
        return;
    }

    // Open the compare file for reading
    if (comparefilename != "")
    {
        comparefile.open(comparefilename);
        if (!comparefile.is_open())
        {
            printf("Cannot open %s.\n", comparefilename.c_str());
            return;
        }
        compare = true;
    }

    // Open the log file for writing
    if (logfilename == "")
        logfilename = epdfilename + ".log";
    ofstream logfile(logfilename, ios_base::app);
    logfile.setf(ios_base::unitbuf);
    if (!logfile.is_open())
    {
        printf("Cannot open %s.\n", logfilename.c_str());
        return;
    }
    logfile << "num passed bestmove foundmove score time\n";

    // Start the engine with linked pipes
    HANDLE g_hChildStd_IN_Rd = NULL;
    HANDLE g_hChildStd_IN_Wr = NULL;
    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0)
        || !SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)
        || !CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &sa, 0)
        || !SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
    {
        printf("Cannot pipe connection to engine process.\n");
        return;
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = g_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = g_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    bSuccess = CreateProcess(NULL, (LPSTR)engineprg.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    if (!bSuccess)
    {
        printf("Cannot create process for engine %s.\n", engineprg.c_str());
        return;
    }
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    readThread = new thread(&readfromengine, g_hChildStd_OUT_Rd, &es);

    // Read epd line by line
    int linenum = 0;
    while (getline(epdfile, line))
    {
        vector<string> fv = SplitString(line.c_str());
        if (fv.size() > 4)
        {
            string fenstr = "";
            string opstr = "";
            // split fen from operation part
            for (int i = 0; i < 4; i++)
                fenstr = fenstr + fv[i] + " ";
            if (pos.getFromFen(fenstr.c_str()) == 0 && ++linenum >= startnum)
            {
                // Get data from compare file
                es.doCompare = false;
                if (compare)
                {
                    vector<string> cv;
                    string compareline;
                    int compareindex = 0;
                    while (compareindex != linenum && getline(comparefile, compareline, '\n'))
                    {
                        cv = SplitString(compareline.c_str());
                        try
                        {
                            compareindex = stoi(cv[0]);
                        }
                        catch (const invalid_argument&) {}

                    }
                    if (compareindex == linenum)
                    {
                        es.doCompare = true;
                        es.comparesuccess = (cv[1] == "+");
                        es.comparescore = SCOREBLACKWINS;
                        es.comparetime = -1;
                        if (cv.size() > 4)
                        {
                            try
                            {
                                es.comparescore = stoi(cv[4]);
                            }
                            catch (const invalid_argument&) {}
                        }
                        if (cv.size() > 5)
                        {
                            try
                            {
                                es.comparetime = stoi(cv[5]);
                            }
                            catch (const invalid_argument&) {}
                            if (es.comparetime == 0 && (flags & 0x1))
                                // nothing to improve; skip this test
                                continue;
                        }
                    }
                }
                // Extract the bm string
                bool searchbestmove = false;
                bool searchavoidmove = false;
                es.bestmoves = "";
                string moveliststr;
                for (int i = 4; i < fv.size(); i++)
                {
                    if (searchbestmove || searchavoidmove)
                    {
                        size_t smk = fv[i].find(';');
                        if (smk != string::npos)
                            fv[i] = fv[i].substr(0, smk);
                        if (moveliststr != "")
                            moveliststr += " ";
                        moveliststr += AlgebraicFromShort(fv[i]);
                        if (smk != string::npos)
                        {
                            if (searchbestmove)
                            {
                                es.bestmoves = moveliststr;
                                searchbestmove = false;
                            }
                            else if (searchavoidmove)
                            {
                                es.avoidmoves = moveliststr;
                                searchavoidmove = false;
                            }
                        }
                    }
                    if (strstr(fv[i].c_str(), "bm") != NULL)
                    {
                        searchbestmove = true;
                        moveliststr = "";
                    }
                    if (strstr(fv[i].c_str(), "am") != NULL)
                    {
                        searchavoidmove = true;
                        moveliststr = "";
                    }
                }

                // Initialize the engine
                es.phase = 0;
                es.score = SCOREBLACKWINS;
                bSuccess = writetoengine(g_hChildStd_IN_Wr, "uci\n");
                while (es.phase == 0)
                    Sleep(1000);
                bSuccess = writetoengine(g_hChildStd_IN_Wr, "isready\n");
                while (es.phase == 1)
                    Sleep(1000);

                es.starttime = clock();
                es.firstbesttimesec = -1;
                sprintf_s(buf, "position fen %s 0 1\ngo infinite\n", fenstr.c_str());
                bSuccess = writetoengine(g_hChildStd_IN_Wr, buf);
                bool engineStopped = false;
                while (es.phase < 3)
                {
                    Sleep(1000);
                    clock_t now = clock();
                    if (!engineStopped
                        && ((now - es.starttime) / CLOCKS_PER_SEC > maxtime
                            || es.score > SCOREWHITEWINS - MAXDEPTH
                            || (flags & 0x2) && es.doCompare && es.comparesuccess && (now - es.starttime) / CLOCKS_PER_SEC > es.comparetime)
                            || (flags & 0x2) && es.firstbesttimesec >= 0 && ((now - es.starttime) / CLOCKS_PER_SEC) > es.firstbesttimesec + 5)
                    {
                        bSuccess = writetoengine(g_hChildStd_IN_Wr, "stop\n");
                        engineStopped = true;
                    }
                }
                if (es.firstbesttimesec >= 0)
                {
                    printf("%d  %s: %s  found: %s  score: %d  time: %d\n", linenum, (es.bestmoves != "" ? "bm" : "am"), (es.bestmoves != "" ? es.bestmoves.c_str() : es.avoidmoves.c_str()), es.enginesbestmove.c_str(), es.score, es.firstbesttimesec);
                    logfile << linenum << " + \"" << (es.bestmoves != "" ? es.bestmoves.c_str() : (es.avoidmoves + "(a)").c_str()) << "\" " << es.enginesbestmove.c_str() << " " << es.score << " " << es.firstbesttimesec << "\n";

                }
                else
                {
                    printf("%d  %s: %s  found: %s ... failed\n", linenum, (es.bestmoves != "" ? "bm" : "am"), (es.bestmoves != "" ? es.bestmoves.c_str() : es.avoidmoves.c_str()), es.enginesbestmove.c_str());
                    logfile << linenum << " - \"" << (es.bestmoves != "" ? es.bestmoves.c_str() : (es.avoidmoves + "(a)").c_str()) << "\" " << es.enginesbestmove.c_str() << "\n";
                }
            }
        }
    }
    bSuccess = writetoengine(g_hChildStd_IN_Wr, "quit\n");
    readThread->detach();
    delete readThread;
}


#else // _WIN32

void testengine(string epdfilename, int startnum, string engineprg, string logfilename, string comparefilename, int maxtime, int flags)
{
    // not yet implemented
}

#endif

int main(int argc, char* argv[])
{
    int startnum = 1;
    int perfmaxdepth = 0;
    bool benchmark = false;
    bool dotests = false;
    bool enginetest = false;
    string epdfile = "";
    string engineprg = "";
    string logfile = "";
    string comparefile = "";
    int maxtime = 0;
    int flags = 0;

    struct arguments {
        const char *cmd;
        const char *info;
        void* variable;
        char type;
        const char *defaultval;
    } allowedargs[] = {
        { "-bench", "Do benchmark test for some positions.", &benchmark, 0, NULL },
        { "-perft", "Do performance and move generator testing.", &perfmaxdepth, 1, "0" },
        { "-dotests","test the hash function and value for positions and mirror (use with -perft)", &dotests, 0, NULL },
        { "-enginetest", "bulk testing of epd files", &enginetest, 0, NULL },
        { "-epdfile", "the epd file to test (use with -enginetest)", &epdfile, 2, "" },
        { "-logfile", "output file (use with -enginetest)", &logfile, 2, "enginetest.log" },
        { "-engineprg", "the uci engine to test (use with -enginetest)", &engineprg, 2, "rubichess.exe" },
        { "-maxtime", "time for each test in seconds (use with -enginetest)", &maxtime, 1, "30" },
        { "-startnum", "number of the test in epd to start with (use with -enginetest)", &startnum, 1, "1" },
        { "-compare", "for fast comparision against logfile from other engine (use with -enginetest)", &comparefile, 2, "" },
        { "-flags", "1=skip easy (0 sec.) compares; 2=break 5 seconds after first find; 4=break after compare time is over (use with -enginetest)", &flags, 1, "0" },
        { NULL, NULL, NULL, 0, NULL }
    };

#ifdef FINDMEMORYLEAKS
    // Get current flag  
    int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

    // Turn on leak-checking bit.  
    tmpFlag |= _CRTDBG_LEAK_CHECK_DF;

    // Set flag to the new value.  
    _CrtSetDbgFlag(tmpFlag);
#endif

    cout.setf(ios_base::unitbuf);

    //engine *myEngine = new engine();

    printf("%s (Build %s %s)\n UCI compatible chess engine by %s\nParameterlist:\n", en.name, __DATE__, __TIME__, en.author);

    for (int j = 0; allowedargs[j].cmd; j++)
    {
        int val = 0;
        for (int i = 1; i < argc; i++)
        {
            if (strcmp(argv[i], allowedargs[j].cmd) == 0)
            {
                val = i;
                break;
            }
        }
        switch (allowedargs[j].type)
        {
        case 0:
            *(bool*)(allowedargs[j].variable) = (val > 0);
            printf(" %s: %s  (%s)\n", allowedargs[j].cmd, *(bool*)(allowedargs[j].variable) ? "yes" : "no", allowedargs[j].info);
            break;
        case 1:
            try { *(int*)(allowedargs[j].variable) = stoi((val > 0 && val < argc - 1 ? argv[val + 1] : allowedargs[j].defaultval)); }
            catch (const invalid_argument&) {}
            printf(" %s: %d  (%s)\n", allowedargs[j].cmd, *(int*)(allowedargs[j].variable), allowedargs[j].info);
            break;
        case 2:
            *(string*)(allowedargs[j].variable) = (val > 0 && val < argc - 1 ? argv[val + 1] : allowedargs[j].defaultval);
            printf(" %s: %s  (%s)\n", allowedargs[j].cmd, (*(string*)(allowedargs[j].variable)).c_str(), allowedargs[j].info);
        }
    }

    printf("Here we go...\n\n");

    if (perfmaxdepth)
    {
        // do a perft test
        perftest(dotests, perfmaxdepth);
    } else if (benchmark)
    {
        // benchmark mode
        doBenchmark();
    } else if (enginetest)
    {
        //engine test mode
        testengine(epdfile, startnum, engineprg, logfile, comparefile, maxtime, flags);
    }
    else {
        // usual uci mode
        en.communicate("");
    }

    return 0;
}
