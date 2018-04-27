#pragma once

#define VERNUM "0.9-dev"

#if 0
#define DEBUG
#endif

#if 0
#define EVALTUNE
#endif

#if 0
#define DEBUGEVAL
#endif

#if 0
#define FPDEBUG
#endif

#if 0
#define FINDMEMORYLEAKS
#endif

#if 0
#define BITBOARD
#else
#define OX88
#endif

#ifdef BITBOARD
#if 1
#define MAGICBITBOARD
#define BOARDVERSION "MB"
#else
#define ROTATEDBITBOARD
#define BOARDVERSION "RB"
#endif
#else
#define BOARDVERSION "Board88"
#endif

#ifdef FINDMEMORYLEAKS
#ifdef _DEBUG  
#define DEBUG_CLIENTBLOCK   new( _CLIENT_BLOCK, __FILE__, __LINE__)  
#else  
#define DEBUG_CLIENTBLOCK  
#endif // _DEBUG  

#ifdef _DEBUG  
#define new DEBUG_CLIENTBLOCK  
#endif  

#define _CRTDBG_MAP_ALLOC
#endif

using namespace std;

#include <stdarg.h>
#include <time.h>
#include <string>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <thread>
#include <map>
#include <time.h>
#include <array>
#include <bitset>
#include <limits.h>
#include <math.h>
#include <regex>

#include "tbprobe.h"

#ifdef _WIN32

#include <AclAPI.h>
#include <intrin.h>
#include <Windows.h>

#ifdef FINDMEMORYLEAKS
#include <crtdbg.h>
#endif


#else //_WIN32

#define sprintf_s sprintf
void Sleep(long x);

#endif

#define ENGINEVER "RubiChess " VERNUM " " BOARDVERSION
#define BUILD __DATE__ " " __TIME__

#define BITSET(x) (mybitset[(x)])
#ifdef _WIN32
#define LSB(i,x) _BitScanForward64((DWORD*)&(i), (x))
#define MSB(i,x) _BitScanReverse64((DWORD*)&(i), (x))
#define POPCOUNT(x) (int)(__popcnt64(x))
#else
#define LSB(i,x) (!(x) ? false : (i = __builtin_ctzll(x), true ))
#define MSB(i,x) (!(x) ? false : (i = (63 - __builtin_clzll(x)), true))
#define POPCOUNT(x) __builtin_popcountll(x)
#endif
#ifdef BITBOARD
#define RANK(x) ((x) >> 3)
#define FILE(x) ((x) & 0x7)
#define INDEX(r,f) (((r) << 3) | (f))
#else
#define RANK(x) ((x) >> 4)
#define FILE(x) ((x) & 0x7)
#define INDEX(r, f) (((r) << 4) | (f))
#endif
#define PROMOTERANK(x) (RANK(x) == 0 || RANK(x) == 7)
#define OUTERFILE(x) (FILE(x) == 0 || FILE(x) == 7)
#define ISNEIGHBOUR(x,y) ((x) >= 0 && (x) < 64 && (y) >= 0 && (y) < 64 && abs(RANK(x) - RANK(y)) <= 1 && abs(FILE(x) - FILE(y)) <= 1)

#ifdef ROTATEDBITBOARD
#define ROT90(x) (rot90[x])
#define ROTA1H8(x) (rota1h8[x])
#define ROTH1A8(x) (roth1a8[x])
#endif
#define S2MSIGN(s) (s ? -1 : 1)


typedef unsigned long long U64;

// Forward definitions
class transposition;
class repetition;
class uci;
class chessposition;
struct pawnhashentry;

//
// utils stuff
//
vector<string> SplitString(const char* s);
unsigned char AlgebraicToIndex(string s, int base);
string IndexToAlgebraic(int i);
string AlgebraicFromShort(string s);
void BitboardDraw(U64 b);
U64 getTime();
#ifdef EVALTUNE
typedef void(*initevalfunc)(void);
bool PGNtoFEN(string pgnfilename);
void TexelTune(string fenfilename);
void registerTuner(int *addr, string name, int def, int index1, int bound1, int index2, int bound2, initevalfunc init, bool notune, int initialdelta = 1);

struct tuningintparameter
{
    int *addr;
    string name;
    int defval;
    int initialdelta;
    int tuned;
    int index1;
    int bound1;
    int index2;
    int bound2;
    void(*init)();
    bool notune;
};

extern int tuningratio;

#define CONSTEVAL

#else //EVALTUNE

#define CONSTEVAL const

#endif

//
// board stuff
//
#define BUFSIZE 4096

#define PieceType unsigned int
#define BLANKTYPE 0
#define PAWN 1
#define KNIGHT 2
#define BISHOP 3
#define ROOK 4
#define QUEEN 5
#define KING 6

#define PieceCode unsigned int
#define BLANK 0
#define WPAWN 2
#define BPAWN 3
#define WKNIGHT 4
#define BKNIGHT 5
#define WBISHOP 6
#define BBISHOP 7
#define WROOK 8
#define BROOK 9
#define WQUEEN 10
#define BQUEEN 11
#define WKING 12
#define BKING 13

#define S2MMASK 0x01
#define WQCMASK 0x02
#define WKCMASK 0x04
#define BQCMASK 0x08
#define BKCMASK 0x10
#define CASTLEMASK 0x1E
#define WQC 1
#define WKC 2
#define BQC 3
#define BKC 4
const int QCMASK[2] = { WQCMASK, BQCMASK };
const int KCMASK[2] = { WKCMASK, BKCMASK };
const int castlerookfrom[] = {0, 0, 7, 56, 63 };
const int castlerookto[] = {0, 3, 5, 59, 61 };

const int EPTSIDEMASK[2] = { 0x8, 0x10 };

#define HASHEXACT 0x00
#define HASHALPHA 0x01
#define HASHBETA 0x02

#define MAXDEPTH 256
#define NOSCORE SHRT_MIN
#define SCOREBLACKWINS (SHRT_MIN + 3 + MAXDEPTH)
#define SCOREWHITEWINS (-SCOREBLACKWINS)
#define SCOREDRAW 0
#define SCORETBWIN 13280

#define MATEFORME(s) (s > SCOREWHITEWINS - MAXDEPTH)
#define MATEFOROPPONENT(s) (s < SCOREBLACKWINS + MAXDEPTH)
#define MATEDETECTED(s) (MATEFORME(s) || MATEFOROPPONENT(s))

#ifdef BITBOARD
/* Offsets for 64Bit  board*/
const int knightoffset[] = { -6, -10, -15, -17, 6, 10, 15, 17 };
const int diagonaloffset[] = { -7, -9, 7, 9 };
const int orthogonaloffset[] = { -8, -1, 1, 8 };
const int orthogonalanddiagonaloffset[] = { -8, -1, 1, 8, -7, -9, 7, 9 };
const int shifting[] = { 0, 0, 0, 1, 2, 3, 0 };

#else
/* Offsets for 0x88 board */
const int knightoffset[] = { -0x0e, -0x12, -0x1f, -0x21, 0x0e, 0x12, 0x1f, 0x21 };
const int diagonaloffset[] = { -0x0f, -0x11, 0x0f, 0x11 };
const int orthogonaloffset[] = { -0x10, -0x01, 0x01, 0x10 };
const int orthogonalanddiagonaloffset[] = { -0x10, -0x01, 0x01, 0x10, -0x0f, -0x11, 0x0f, 0x11 };
#endif


const struct { int offset; bool needsblank; } pawnmove[] = { { 0x10, true }, { 0x0f, false }, { 0x11, false } };
extern CONSTEVAL int materialvalue[];
// values for move ordering
const int mvv[] = { 0U << 28, 1U << 28, 2U << 28, 2U << 28, 3U << 28, 4U << 28, 5U << 28 };
const int lva[] = { 5 << 25, 4 << 25, 3 << 25, 3 << 25, 2 << 25, 1 << 25, 0 << 25 };
#define PVVAL (7 << 28)
#define KILLERVAL1 (1 << 27)
#define KILLERVAL2 (KILLERVAL1 - 1)
#define TBFILTER INT32_MIN

#ifdef BITBOARD
#define ISEPCAPTURE 0x40
#define GETFROM(x) (((x) & 0x0fc0) >> 6)
#define GETTO(x) ((x) & 0x003f)
#define GETEPT(x) (((x) & 0x03f00000) >> 20)
#define GETEPCAPTURE(x) (((x) >> 20) & ISEPCAPTURE)
#else
#define GETFROM(x) ((((x) & 0x0e00) >> 5) | (((x) & 0x01c0) >> 6))
#define GETTO(x) ((((x) & 0x0038) << 1) | ((x) & 0x0007))
#define GETEPT(x) (((x) & 0x0ff00000) >> 20)
#endif
#define GETPROMOTION(x) (((x) & 0xf000) >> 12)
#define GETCAPTURE(x) (((x) & 0xf0000) >> 16)
#define ISTACTICAL(x) ((x) & 0xff000)
#define ISPROMOTION(x) ((x) & 0xf000)
#define ISCAPTURE(x) ((x) & 0xf0000)
#define GETPIECE(x) (((x) & 0xf0000000) >> 28)

#ifdef BITBOARD
// index -> bitboard with only index bit set; use BITSET(i) macro
extern U64 mybitset[64];

// passedPawnMask[18][WHITE]:
// 01110000
// 01110000
// 01110000
// 01110000
// 01110000
// 00o00000
// 00000000
// 00000000
extern U64 passedPawnMask[64][2];

// filebarrierMask[18][WHITE]:
// 00100000
// 00100000
// 00100000
// 00100000
// 00100000
// 00o00000
// 00000000
// 00000000
extern U64 filebarrierMask[64][2];

// neighbourfilesMask[18]:
// 01010000
// 01010000
// 01010000
// 01010000
// 01010000
// 01o10000
// 01010000
// 01010000
extern U64 neighbourfilesMask[64];

// phalanxMask[18]:
// 00000000
// 00000000
// 00000000
// 00000000
// 00000000
// 0xox0000
// 00000000
// 000000o0
extern U64 phalanxMask[64];

// kingshieldMask[6][WHITE]:
// 00000000
// 00000000
// 00000000
// 00000000
// 00000000
// 00000xxx
// 00000xxx
// 000000o0
extern U64 kingshieldMask[64][2];

// fileMask[18]:
// 00100000
// 00100000
// 00100000
// 00100000
// 00100000
// 00x00000
// 00100000
// 00100000
extern U64 fileMask[64];

// rankMask[18]:
// 00000000
// 00000000
// 00000000
// 00000000
// 00000000
// 11x11111
// 00000000
// 00000000
extern U64 rankMask[64];

#endif

struct chessmovestack
{
    int state;
    int ept;
    int kingpos[2];
    unsigned long long hash;
    unsigned long long pawnhash;
    unsigned long long materialhash;
    int halfmovescounter;
    int fullmovescounter;
    int isCheck;
#ifndef BITBOARD
    //int value;
    int numFieldchanges;
    PieceCode code[4];
    int index[4];
#endif
};


class chessmove
{
public:
    // pcpcepepepepccccppppfffffftttttt
    uint32_t code;
    int value;
    int order;

    chessmove();
#ifdef BITBOARD
    chessmove(int from, int to, PieceCode promote, PieceCode capture, PieceCode piece);
    chessmove(int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece);
#else
    chessmove(int from, int to, PieceCode promote, PieceCode capture, PieceCode piece);
    chessmove(int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece);
#endif
    bool operator<(const chessmove cm) const { return (value < cm.value); }// || ((value == cm.value) && (order > cm.order)); }
    bool operator>(const chessmove cm) const { return (value > cm.value); }// || ((value == cm.value) && (order < cm.order)); }
    string toString();
    void print();
};

#define MAXMOVELISTLENGTH 1024
#define MAXMULTIPV 64

class chessmovelist
{
public:
    chessmove move[MAXMOVELISTLENGTH];
    int length = 0;
    chessmovelist();
    string toString();
    string toStringWithValue();
    void print();
    void resetvalue();
    void sort();
};


#ifdef BITBOARD

extern U64 pawn_attacks_occupied[64][2];

#define BOARDSIZE 64
#define RANKMASK 0x38

#ifndef ROTATEDBITBOARD

struct SMagic {
    U64 mask;  // to mask relevant squares of both lines (no outer squares)
    U64 magic; // magic 64-bit factor
};

extern SMagic mBishopTbl[64];
extern SMagic mRookTbl[64];

#define BISHOPINDEXBITS 9
#define ROOKINDEXBITS 12
#define MAGICBISHOPINDEX(m,x) (int)((((m) & mBishopTbl[x].mask) * mBishopTbl[x].magic) >> (64 - BISHOPINDEXBITS))
#define MAGICROOKINDEX(m,x) (int)((((m) & mRookTbl[x].mask) * mRookTbl[x].magic) >> (64 - ROOKINDEXBITS))
#define MAGICBISHOPATTACKS(m,x) (mBishopAttacks[x][MAGICBISHOPINDEX(m,x)])
#define MAGICROOKATTACKS(m,x) (mRookAttacks[x][MAGICROOKINDEX(m,x)])

extern U64 mBishopAttacks[64][1 << BISHOPINDEXBITS];
extern U64 mRookAttacks[64][1 << ROOKINDEXBITS];

#else // ROTATEDBITBOARD

const int rot00shift[64] = {
     1,  1,  1,  1,  1,  1,  1,  1,
     9,  9,  9,  9,  9,  9,  9,  9,
    17, 17, 17, 17, 17, 17, 17, 17,
    25, 25, 25, 25, 25, 25, 25, 25,
    33, 33, 33, 33, 33, 33, 33, 33,
    41, 41, 41, 41, 41, 41, 41, 41,
    49, 49, 49, 49, 49, 49, 49, 49,
    57, 57, 57, 57, 57, 57, 57, 57
};

const int rot90[64] = {
    0,  8, 16, 24, 32, 40, 48, 56,
    1,  9, 17, 25, 33, 41, 49, 57,
    2, 10, 18, 26, 34, 42, 50, 58,
    3, 11, 19, 27, 35, 43, 51, 59,
    4, 12, 20, 28, 36, 44, 52, 60,
    5, 13, 21, 29, 37, 45, 53, 61,
    6, 14, 22, 30, 38, 46, 54, 62,
    7, 15, 23, 31, 39, 47, 55, 63
};

const int rot90shift[64] = {
     1,  9, 17, 25, 33, 41, 49, 57,
     1, 9, 17, 25, 33, 41, 49, 57,
     1, 9, 17, 25, 33, 41, 49, 57,
     1, 9, 17, 25, 33, 41, 49, 57,
     1, 9, 17, 25, 33, 41, 49, 57,
     1, 9, 17, 25, 33, 41, 49, 57,
     1, 9, 17, 25, 33, 41, 49, 57,
     1, 9, 17, 25, 33, 41, 49, 57
};


const int rota1h8[64] = {
    28, 21, 15, 10,  6,  3,  1,  0,
    36, 29, 22, 16, 11,  7,  4,  2,
    43, 37, 30, 23, 17, 12,  8,  5,
    49, 44, 38, 31, 24, 18, 13,  9,
    54, 50, 45, 39, 32, 25, 19, 14,
    58, 55, 51, 46, 40, 33, 26, 20,
    61, 59, 56, 52, 47, 41, 34, 27,
    63, 62, 60, 57, 53, 48, 42, 35
};

const int rota1h8shift[64] = {
    29, 22, 16, 11,  7,  4,  2,  1,
    37, 29, 22, 16, 11,  7,  4,  2,
    44, 37, 29, 22, 16, 11,  7,  4,
    50, 44, 37, 29, 22, 16, 11,  7,
    55, 50, 44, 37, 29, 22, 16, 11,
    59, 55, 50, 44, 37, 29, 22, 16,
    62, 59, 55, 50, 44, 37, 29, 22,
    64, 62, 59, 55, 50, 44, 37, 29
};

const int roth1a8[64] = {
    0,  1,  3,  6, 10, 15, 21, 28,
    2,  4,  7, 11, 16, 22, 29, 36,
    5,  8, 12, 17, 23, 30, 37, 43,
    9, 13, 18, 24, 31, 38, 44, 49,
    14, 19, 25, 32, 39, 45, 50, 54,
    20, 26, 33, 40, 46, 51, 55, 58,
    27, 34, 41, 47, 52, 56, 59, 61,
    35, 42, 48, 53, 57, 60, 62, 63
};

const int roth1a8shift[64] = {
     1,  2,  4,  7, 11, 16, 22, 29,
     2,  4,  7, 11, 16, 22, 29, 37,
     4,  7, 11, 16, 22, 29, 37, 44,
     7, 11, 16, 22, 29, 37, 44, 50,
    11, 16, 22, 29, 37, 44, 50, 55,
    16, 22, 29, 37, 44, 50, 55, 59,
    22, 29, 37, 44, 50, 55, 59, 62,
    29, 37, 44, 50, 55, 59, 62, 64
};

extern U64 diaga1h8_attacks[64][64];
extern U64 diagh1a8_attacks[64][64];
extern U64 rank_attacks[64][64];
extern U64 file_attacks[64][64];

#endif //ROTATEDBITBOARD

class chessposition
{
public:
    U64 piece00[14];
    U64 occupied00[2];
#ifdef ROTATEDBITBOARD
    U64 piece90[14];
    U64 piecea1h8[14];
    U64 pieceh1a8[14];
    U64 occupied90[2];
    U64 occupieda1h8[2];
    U64 occupiedh1a8[2];
#endif
    PieceCode mailbox[BOARDSIZE]; // redundand for faster "which piece is on field x"

    int state;
    int ept;
    int kingpos[2];
    unsigned long long hash;
    unsigned long long pawnhash;
    unsigned long long materialhash;
    int ply;
    int halfmovescounter = 0;
    int fullmovescounter = 0;
    int maxdebugdepth = -1;
    int mindebugdepth = -1;
    chessmovelist pvline;
    chessmovelist actualpath;
    chessmove bestmove[MAXMULTIPV];
    int bestmovescore[MAXMULTIPV];
    unsigned long killer[2][MAXDEPTH];
    unsigned int history[14][64];
    unsigned long long debughash = 0;
    int *positionvaluetable; // value tables for both sides, 7 PieceTypes and 256 phase variations 
    int ph; // to store the phase during different evaluation functions
    int isCheck;
    int useTb;
    int tbPosition;
    chessmovelist rootmovelist;
    chessmove defaultmove; // fallback if search in time trouble didn't finish a single iteration 
    chessposition();
    ~chessposition();
    void init();
    bool operator==(chessposition p);
    bool w2m();
    void BitboardSet(int index, PieceCode p);
    void BitboardClear(int index, PieceCode p);
    void BitboardMove(int from, int to, PieceCode p);
    void BitboardPrint(U64 b);
    int getFromFen(const char* sFen);
    string toFen();
    bool applyMove(string s);
    void print();
    int phase();
    bool isEmpty(int bIndex);
    PieceType Piece(int index);
    bool isOpponent(int bIndex);
    bool isEmptyOrOpponent(int bIndex);
    bool isAttacked(int index);
    U64 attacksTo(int index, int side);
    int getLeastValuablePieceIndex(int to, unsigned int bySide, PieceCode *piece);
    int see(int to);
    int see(int from, int to);
    void testMove(chessmovelist *movelist, int from, int to, PieceCode promote, PieceCode capture, PieceCode piece);
    void testMove(chessmovelist *movelist, int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece);
    chessmovelist* getMoves();
    void getRootMoves();
    void tbFilterRootMoves();
    bool playMove(chessmove *cm);
    void unplayMove(chessmove *cm);
    void playNullMove();
    void unplayNullMove();
    void simplePlay(int from, int to);
    void simpleUnplay(int from, int to, PieceCode capture);
    void getpvline(int depth, int pvnum);
    int getPositionValue();
    int getPawnValue(pawnhashentry **entry);
    int getValue();
#ifdef DEBUG
    void debug(int depth, const char* format, ...);
#endif
#ifdef DEBUGEVAL
    void debugeval(const char* format, ...);
#endif
    int testRepetiton();
    void mirror();
};

#else //BITBOARD

#define BOARDSIZE 128
#define RANKMASK 0x70

class chessposition
{
public:
    PieceCode mailbox[128];
    int state;
    int ept;
    int kingpos[2];
    unsigned long long hash;
    unsigned long long pawnhash;
    //int value;
    int ply;
    int piecenum[14];
    int halfmovescounter = 0;
    int fullmovescounter = 0;
    int maxdebugdepth = -1;
    int mindebugdepth = -1;
    chessmovelist pvline;
    chessmovelist actualpath;
    chessmove bestmove[MAXMULTIPV];
    int bestmovescore[MAXMULTIPV];
    unsigned long killer[3][MAXDEPTH];
    unsigned int history[14][128];
    unsigned long long debughash = 0;
    int *positionvaluetable;     // value tables for both sides, 7 PieceTypes and 256 phase variations 
    int ph; // to store the phase during different evaluation functions
    int isCheck;
    int rootmoves;  // precalculated and used for MultiPV mode
    chessmove defaultmove; // fallback if search in time trouble didn't finish a single iteration 
    chessposition();
    ~chessposition();
    void init();
    bool operator==(chessposition p);
    bool w2m();
    int getFromFen(const char* sFen);
    string toFen();
    bool applyMove(string s);
    void print();
    int phase();
    bool isEmpty(int bIndex);
    PieceType Piece(int index);
    bool isOpponent(int bIndex);
    bool isEmptyOrOpponent(int bIndex);
    bool isAttacked(int bIndex);
    int see(int to);
    int see(int from, int to);
    void testMove(chessmovelist *movelist, int from, int to, PieceCode promote, PieceCode capture, PieceCode piece);
    void testMove(chessmovelist *movelist, int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece);
    chessmovelist* getMoves();
    void getRootMoves();
    bool playMove(chessmove *cm);
    void unplayMove(chessmove *cm);
    void playNullMove();
    void unplayNullMove();
    void simplePlay(int from, int to);
    void simpleUnplay(int from, int to, PieceCode capture);
    void getpvline(int depth, int pvnum);
    void countMaterial();
    int getPositionValue();
    int getPawnValue();
    int getValue();
#ifdef DEBUG
    void debug(int depth, const char* format, ...);
#endif
#ifdef DEBUGEVAL
    void debugeval(const char* format, ...);
#endif
    bool testRepetiton();
    void mirror();
};

#endif

void CreatePositionvalueTable();

#define ENGINERUN 0
#define ENGINEWANTSTOP 1
#define ENGINESTOPSOON 2
#define ENGINESTOPIMMEDIATELY 3
#define ENGINESTOPPED 4
#define ENGINETERMINATEDSEARCH 5

class engine
{
public:
    engine();
    uci *myUci;
    const char* name = ENGINEVER;
    const char* author = "Andreas Matthies";
    bool isWhite;
    unsigned long long nodes;
    unsigned long long tbhits;
#ifdef FPDEBUG
    unsigned long long fpnodes;
    unsigned long long wrongfp;
#endif
#ifdef DEBUG
    unsigned long long qnodes;
	unsigned long long wastedpvsnodes;
    unsigned long long wastedaspnodes;
    unsigned long long pvnodes;
    unsigned long long nopvnodes;
    unsigned long long dpnodes;
    unsigned long long npd[MAXDEPTH];
    fstream fdebug;
#endif
    long long starttime;
    long long endtime1; // time to send STOPSOON signal
    long long endtime2; // time to send STOPPIMMEDIATELY signal
    long long frequency;
    float fh, fhf;
    int wtime, btime, winc, binc, movestogo, depth, maxnodes, mate, movetime, maxdepth;
    bool infinite;
    bool debug = false;
    bool moveoutput;
    int sizeOfTp = 0;
    int moveOverhead;
    int MultiPV;
    bool ponder;
    string SyzygyPath;
    enum { NO, PONDERING, HITPONDER } pondersearch;
    int terminationscore = SHRT_MAX;
    int stopLevel = ENGINESTOPPED;
    void communicate(string inputstring);
    int getScoreFromEnginePoV();
    void setOption(string sName, string sValue);
    bool isPondering() { return (pondersearch == PONDERING); }
    void HitPonder() { pondersearch = HITPONDER; }
    bool testPonderHit() { return (pondersearch == HITPONDER); }
    void resetPonder() { pondersearch = NO; }
};

PieceType GetPieceType(char c);

struct enginestate
{
public:
    int phase;
    string bestmoves;
    string avoidmoves;
    clock_t starttime;
    int firstbesttimesec;
    int score;
    string enginesbestmove;
    bool doCompare;
    bool comparesuccess;
    int comparetime;
    int comparescore;
};

extern chessposition pos;
extern engine en;

#ifdef DEBUG
#define PDEBUG(d, f, ...)  pos.debug(d, f, ##__VA_ARGS__)
#else
#define PDEBUG(d, f, ...)
#endif

//
// transposition stuff
//
typedef unsigned long long u8;
typedef struct ranctx { u8 a; u8 b; u8 c; u8 d; } ranctx;

#define rot(x,k) (((x)<<(k))|((x)>>(64-(k))))

class zobrist
{
public:
    ranctx rnd;
#ifdef BITBOARD
    unsigned long long boardtable[64 * 16];
	unsigned long long cstl[32];
	unsigned long long ept[64];
#else
    unsigned long long boardtable[128 * 16];
	unsigned long long castle[4];
	unsigned long long ep[8];
#endif
    unsigned long long s2m;
    zobrist();
    unsigned long long getRnd();
    u8 getHash();
    u8 getPawnHash();
    u8 getMaterialHash();
    u8 modHash(int i);
};

typedef struct transpositionentry {
    uint32_t hashupper;
    uint32_t movecode;
    short value;
    unsigned char depth;
    char flag;
} S_TRANSPOSITIONENTRY;

class transposition
{
    S_TRANSPOSITIONENTRY *table;
    U64 used;
public:
    U64 size;
    U64 sizemask;
    chessposition *pos;
    ~transposition();
    int setSize(int sizeMb);    // returns the number of Mb not used by allignment
    void clean();
    bool testHash();
    void addHash(int val, int valtype, int depth, uint32_t move);
    void printHashentry();
    bool probeHash(int *val, uint32_t *movecode, int depth, int alpha, int beta);
    short getValue();
    int getValtype();
    int getDepth();
    uint32_t getMoveCode();
    unsigned int getUsedinPermill();
};

typedef struct pawnhashentry {
   uint32_t hashupper;
    U64 passedpawnbb[2];
    U64 isolatedpawnbb[2];
    U64 backwardpawnbb[2];
    int semiopen[2]; 
    short value;
} S_PAWNHASHENTRY;

class pawnhash
{
    S_PAWNHASHENTRY *table;
public:
#ifdef DEBUG
    U64 used;
    int hit;
    int query;
#endif
    U64 size;
    U64 sizemask;
    chessposition *pos;
    ~pawnhash();
    void setSize(int sizeMb);
    void clean();
    bool probeHash(pawnhashentry **entry);
    unsigned int getUsedinPermill();
};

class repetition
{
    unsigned char table[0x10000];
public:
    void clean();
    void addPosition(unsigned long long hash);
    void removePosition(unsigned long long hash);
    int getPositionCount(unsigned long long hash);
};

extern zobrist zb;
extern repetition rp;
extern transposition tp;
extern pawnhash pwnhsh;


/*
http://stackoverflow.com/questions/29990116/alpha-beta-prunning-with-transposition-table-iterative-deepening
https://www.gamedev.net/topic/503234-transposition-table-question/
*/


//
// search stuff
//
int rootsearch(int alpha, int beta, int depth);
int alphabeta(int alpha, int beta, int depth, bool nullmoveallowed);
int getQuiescence(int alpha, int beta, int depth);
void searchguide();


//
// uci stuff
//

enum GuiToken { UNKNOWN, UCI, UCIDEBUG, ISREADY, SETOPTION, REGISTER, UCINEWGAME, POSITION, GO, STOP, PONDERHIT, QUIT };

const map<string, GuiToken> GuiCommandMap = {
    { "uci", UCI },
    { "debug", UCIDEBUG },
    { "isready", ISREADY },
    { "setoption", SETOPTION },
    { "register", REGISTER },
    { "ucinewgame", UCINEWGAME },
    { "position", POSITION },
    { "go", GO },
    { "stop", STOP },
    { "ponderhit", PONDERHIT },
    { "quit", QUIT }
};

class uci
{
    int state;
public:
    GuiToken parse(vector<string>*, string ss);
    void send(const char* format, ...);
};


