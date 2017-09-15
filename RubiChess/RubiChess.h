#pragma once

using namespace std;

#if 0
#define DEBUG
#endif

#if 0
#define DEBUGEVAL
#endif

#if 0
#define FINDMEMORYLEAKS
#endif

#if 0
#define BITBOARD
#else
#define OX88
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

typedef unsigned long long U64;

#ifdef BITBOARD
#define ENGINEVER "RubiChess V0.5-alpha Bitboard";

#else
#define ENGINEVER "RubiChess V0.5-alpha 0x88-Board";

#endif



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
#define RANK(x) ((x) >> 3)
#define FILE(x) ((x) & 0x7)
#define PROMOTERANK(x) (RANK(x) == 0 || RANK(x) == 7)

#define ROT90(x) (rot90[x])
//#define ROT90(x) ((((x) & 0x38) >> 3) | (((x) & 0x07) << 3))
#define ROTA1H8(x) (rota1h8[x])
#define ROTH1A8(x) (roth1a8[x])

#define S2MSIGN(s) (s ? -1 : 1)
// Forward definitions
class transposition;
class repetition;
class uci;
class chessposition;


//
// utils stuff
//
vector<string> SplitString(const char* s);
unsigned char AlgebraicToIndex(string s, int base);
string AlgebraicFromShort(string s);
void BitboardDraw(U64 b);
U64 getTime();

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
#define SCOREBLACKWINS (SHRT_MIN + 3 + MAXDEPTH)
#define SCOREWHITEWINS (-SCOREBLACKWINS)
#define SCOREDRAW 0
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


//struct pawnmove { char offset; bool blank; };
const struct { int offset; bool needsblank; } pawnmove[] = { { 0x10, true }, { 0x0f, false }, { 0x11, false } };
const int materialvalue[] = { 0, 100, 320, 330, 500, 900, SCOREWHITEWINS };
// values for move ordering
const unsigned int mvv[] = { 0U << 29, 1U << 29, 2U << 29, 2U << 29, 3U << 29, 4U << 29, 5U << 29 };
const unsigned int lva[] = { 5 << 26, 4 << 26, 3 << 26, 3 << 26, 2 << 26, 1 << 26, 0 << 26 };
#define PVVAL (7 << 29)
#define KILLERVAL1 (1 << 28)
#define KILLERVAL2 (KILLERVAL1 - 1)

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
#ifdef BITBOARD
#define GETPIECE(x) (((x) & 0xf0000000) >> 28)
#else
#define GETCASTLE(x) (((x) & 0xf0000000) >> 28)
#endif

struct chessmovestack
{
    int state;
    int ept;
    int kingpos[2];
    unsigned long long hash;
    int halfmovescounter;
    int fullmovescounter;
#ifndef BITBOARD
    int value;
    int numFieldchanges;
    PieceCode code[4];
    int index[4];
#endif
};


class chessmove
{
public:
    // pcpcepepepepccccppppfffffftttttt
    unsigned long code;
    unsigned int value;

    chessmove();
    chessmove(unsigned long code);
#ifdef BITBOARD
    chessmove(int from, int to, PieceCode promote, PieceCode capture, PieceCode piece);
    chessmove(int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece);
#else
    chessmove(int from, int to, PieceCode promote, PieceCode capture);
    chessmove(int from, int to, PieceCode promote, PieceCode capture, int ept);
#endif
    bool operator<(const chessmove cm) const { return value < cm.value; }
    bool operator>(const chessmove cm) const { return value > cm.value; }
    //bool operator<(const chessmove cm) const { return ((code & 0xfff) < (cm.code & 0xfff)); }
    bool operator ==(const chessmove cm) const { return code == cm.code; }
    //static bool asc(const chessmove cm1, const chessmove cm2) { return cm1.value < cm2.value; }
    //static bool desc(const chessmove cm1, const chessmove cm2) { return cm2.value < cm1.value; }
    static bool cptr(chessmove cm1, chessmove cm2);
    string toString();
    void print();
};

#define MAXMOVELISTLENGTH 1024
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


class chessposition
{
public:
    U64 piece00[14];
    U64 piece90[14];
    U64 piecea1h8[14];
    U64 pieceh1a8[14];
    U64 occupied00[2];
    U64 occupied90[2];
    U64 occupieda1h8[2];
    U64 occupiedh1a8[2];
    PieceCode mailbox[64]; // redundand for faster "which piece is on field x"

    U64 attacks_from[64];
    U64 attacks_to[64];

    int state;
    int ept;
    int kingpos[2];
    unsigned long long hash;
    int ply;
    int halfmovescounter = 0;
    int fullmovescounter = 0;
    int maxdebugdepth = -1;
    int mindebugdepth = -1;
    chessmovelist pvline;
    chessmovelist actualpath;
    chessmove bestmove;
    unsigned long killer[3][MAXDEPTH];
    unsigned int history[14][64];
    unsigned long long debughash = 0;
    int *positionvaluetable; // value tables for both sides, 7 PieceTypes and 256 phase variations 

    chessposition();
    ~chessposition();
    bool operator==(chessposition p);
    bool w2m();
    void BitboardSet(int index, PieceCode p);
    void BitboardClear(int index, PieceCode p);
    void BitboardMove(int from, int to, PieceCode p);
    int getFromFen(const char* sFen);
    bool applyMove(string s);
    void print();
    int phase();
    bool isOnBoard(int bIndex);
    bool isEmpty(int bIndex);
    PieceType Piece(int index);
    bool isOpponent(int bIndex);
    bool isEmptyOrOpponent(int bIndex);
    bool isAttacked(int index);
    U64 attacksTo(int index, int side);
    bool checkForChess();
    int see(int to);
    int see(int from, int to);
    void testMove(chessmovelist *movelist, int from, int to, PieceCode promote, PieceCode capture, PieceCode piece);
    void testMove(chessmovelist *movelist, int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece);
    chessmovelist* getMoves();
    bool playMove(chessmove *cm);
    void playMoveFast(chessmove *cm);
    void unplayMove(chessmove *cm);
    void unplayMoveFast(chessmove *cm);
    void playNullMove();
    void unplayNullMove();
    void simplePlay(int from, int to);
    void simpleUnplay(int from, int to, PieceCode capture);
    void getpvline(int depth);
    int countMaterial();
    int getPositionValue();
    int getValue();
    int* GetPositionvalueTable();
    void debug(int depth, const char* format, ...);
    void debugeval(const char* format, ...);
    bool testRepetiton();
    void mirror();
};

#else //BITBOARD

class chessposition
{
public:
    PieceCode board[128];
    int state;
    int ept;
    int kingpos[2];
    unsigned long long hash;
    int value;
    int ply;
    int piecenum[14];
    int halfmovescounter = 0;
    int fullmovescounter = 0;
    int maxdebugdepth = -1;
    int mindebugdepth = -1;
    chessmovelist pvline;
    chessmovelist actualpath;
    chessmove bestmove;
    unsigned long killer[3][MAXDEPTH];
    unsigned int history[14][128];
    unsigned long long debughash = 0;
    int *positionvaluetable;     // value tables for both sides, 7 PieceTypes and 256 phase variations 

    chessposition();
    ~chessposition();
    bool operator==(chessposition p);
    bool w2m();
    int getFromFen(const char* sFen);
    bool applyMove(string s);
    void print();
    int phase();
    bool isOnBoard(int bIndex);
    bool isEmpty(int bIndex);
    PieceType Piece(int index);
    bool isOpponent(int bIndex);
    bool isEmptyOrOpponent(int bIndex);
    bool isAttacked(int bIndex);
    bool checkForChess();
    int see(int to);
    int see(int from, int to);
    void testMove(chessmovelist *movelist, int from, int to, PieceCode promote);
    chessmovelist* getMoves();
    bool playMove(chessmove *cm);
    void playMoveFast(chessmove *cm);
    void unplayMove(chessmove *cm);
    void unplayMoveFast(chessmove *cm);
    void playNullMove();
    void unplayNullMove();
    void simplePlay(int from, int to);
    void simpleUnplay(int from, int to, PieceCode capture);
    void getpvline(int depth);
    void countMaterial();
    int getPositionValue();
    int getValue();
    int* GetPositionvalueTable();
    void debug(int depth, const char* format, ...);
    void debugeval(const char* format, ...);
    bool testRepetiton();
    void mirror();
};

#endif

#define ENGINERUN 0
#define ENGINEWANTSTOP 1
#define ENGINESTOPSOON 2
#define ENGINESTOPIMMEDIATELY 3
#define ENGINESTOPPED 4

class engine
{
public:
    engine();
    uci *myUci;
	const char* name = ENGINEVER
    const char* author = "Andreas Matthies";
    bool isWhite;
    unsigned long nodes;
#ifdef DEBUG
    unsigned long qnodes;
	unsigned long wastednodes;
#endif
    long long starttime;
    long long endtime;
    long long frequency;
    float fh, fhf;
    unsigned int wtime, btime, winc, binc, movestogo, depth, maxnodes, mate, movetime, maxdepth;
    bool infinite;
    bool debug = false;
    int sizeOfTp = 0;
    int moveOverhead;
    int stopLevel;
    void communicate(string inputstring);
    int getScoreFromEnginePoV();
    void setOption(string sName, string sValue);
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
    u8 modHash(int i);
};

typedef struct transpositionentry {
    unsigned long hashupper;
    unsigned long movecode;
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
    transposition();
    ~transposition();
    void setSize(int sizeMb);
    void clean();
    bool testHash();
    void addHash(int val, int valtype, int depth, unsigned long move);
    void printHashentry();
    bool probeHash(int *val, unsigned long *movecode, int depth, int alpha, int beta);
    short getValue();
    int getValtype();
    int getDepth();
    class chessmove getMove();
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

/*
http://stackoverflow.com/questions/29990116/alpha-beta-prunning-with-transposition-table-iterative-deepening
https://www.gamedev.net/topic/503234-transposition-table-question/
*/


//
// search stuff
//
#define TIMETOUSESLOTS 32
int alphabeta(engine *en, int alpha, int beta, int depth, bool nullmoveallowed);
int getQuiescence(engine *en, int alpha, int beta, int depth, bool force);
void searchguide(engine *en);


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


