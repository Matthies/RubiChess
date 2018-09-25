#pragma once

#define VERNUM "1.1-dev"

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
#define FINDMEMORYLEAKS
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

#define ENGINEVER "RubiChess " VERNUM
#define BUILD __DATE__ " " __TIME__

#define BITSET(x) (1ULL << (x))
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
#define RRANK(x,s) ((s) ? ((x) >> 3) ^ 7 : ((x) >> 3))
#define FILE(x) ((x) & 0x7)
#define INDEX(r,f) (((r) << 3) | (f))
#define BORDERDIST(f) ((f) > 3 ? 7 - (f) : (f))
#define PROMOTERANK(x) (RANK(x) == 0 || RANK(x) == 7)
#define PROMOTERANKBB 0xff000000000000ff
#define RANK3(s) ((s) ? 0x0000ff0000000000 : 0x0000000000ff0000)
#define RANK7(s) ((s) ? 0x000000000000ff00 : 0x00ff000000000000)
#define FILEABB 0x0101010101010101
#define FILEHBB 0x8080808080808080
#define ISOUTERFILE(x) (FILE(x) == 0 || FILE(x) == 7)
#define ISNEIGHBOUR(x,y) ((x) >= 0 && (x) < 64 && (y) >= 0 && (y) < 64 && abs(RANK(x) - RANK(y)) <= 1 && abs(FILE(x) - FILE(y)) <= 1)

#define S2MSIGN(s) (s ? -1 : 1)


typedef unsigned long long U64;

// Forward definitions
class transposition;
class repetition;
class uci;
class chessposition;
struct pawnhashentry;


//
// eval stuff
//
#ifdef EVALTUNE
class eval {
    int32_t v;
    int g;
public:
    eval() { v = g = 0; }
    eval(int m, int e) {
        v = ((int32_t)((uint32_t)(m) << 16) + (e)); g = 0;
    }
    operator int() const { return v; }
    void addGrad(int i) { this->g += i; }
    int getGrad() { return this->g; }
    void resetGrad() { g = 0; }
};
#define VALUE(m, e) eval(m, e)
#define VALUEG(g, e, v) ((g) ? )
#define EVAL(e, f) ((e).addGrad(f), (e) * (f))
#else
#define VALUE(m, e) ((int32_t)((uint32_t)(m) << 16) + (e))
#define EVAL(e, f) ((e) * (f))
typedef const int32_t eval;
#endif
#define GETMGVAL(v) ((int16_t)(((uint32_t)(v) + 0x8000) >> 16))
#define GETEGVAL(v) ((int16_t)((v) & 0xffff))
#define NEWTAPEREDEVAL(v, ph) (((256 - (ph)) * GETMGVAL(v) + (ph) * GETEGVAL(v)) / 256)
#define PSQTINDEX(i,s) ((s) ? (i) : (i) ^ 0x38)

#define NUMOFEVALPARAMS (4 + 8 + 8 + 6 + 2 + 7 + 1 + 7 + 7*64 + 2*5*4)
struct evalparamset {
    eval ePawnstormblocked[4][5] = {
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE( -22,   9), VALUE(   7,   0), VALUE(  18, -19)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   2,  -7), VALUE(  21,  -6), VALUE(  -2,   3)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  -6,  -6), VALUE(  -1,   1), VALUE( -13,  -3)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE( -16,   2), VALUE(  -9,   8), VALUE(  10, -14)  }
    };
    eval ePawnstormfree[4][5] = {
        {  VALUE( -20,  66), VALUE(  97,  38), VALUE( -29,  38), VALUE( -21,   8), VALUE(   0,   0)  },
        {  VALUE(  52,  40), VALUE(  62,  10), VALUE( -30,  20), VALUE(   1,   6), VALUE(   0,   5)  },
        {  VALUE(  30,  54), VALUE(  83,  19), VALUE( -21,  21), VALUE(  -8,   4), VALUE(  -2,   7)  },
        {  VALUE(  33,  38), VALUE(  78,  25), VALUE( -13,  13), VALUE(  -4,   6), VALUE(  -2,   6)  }
    };
    eval ePawnpushthreatbonus =  VALUE(  19,   8);
    eval eSafepawnattackbonus =  VALUE(  44,  24);
    eval eKingshieldbonus =  VALUE(  17,  -9);
    eval eTempo =  VALUE(  10,   1);
    eval ePassedpawnbonus[8] = {  VALUE(   0,   0), VALUE(  -9,  16), VALUE( -13,  21), VALUE( -18,  42), VALUE(   6,  68), VALUE(  34, 104), VALUE(  78, 106), VALUE(   0,   0)  };
    eval eAttackingpawnbonus[8] = {  VALUE(   0,   0), VALUE( -21, -19), VALUE( -18, -15), VALUE(  -8,  -6), VALUE( -10,   5), VALUE(  27,  32), VALUE(   0,   0), VALUE(   0,   0)  };
    eval eIsolatedpawnpenalty =  VALUE( -14, -14);
    eval eDoublepawnpenalty =  VALUE( -22, -13);
    eval eConnectedbonus =  VALUE(   5,   1);
    eval eBackwardpawnpenalty =  VALUE(  -5, -17);
    eval eDoublebishopbonus =  VALUE(  36,  38);
    eval eShiftmobilitybonus =  VALUE(   3,  -1);
    eval eSlideronfreefilebonus[2] = {  VALUE(  13,  19), VALUE(  38,   5)  };
    eval eMaterialvalue[7] = {  VALUE(   0,   0), VALUE( 100, 100), VALUE( 314, 314), VALUE( 314, 314), VALUE( 483, 483), VALUE( 913, 913), VALUE(32509,32509)  };
    eval eWeakkingringpenalty =  VALUE( -14,   0);
    eval eKingattackweight[7] = {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   5,  -2), VALUE(   3,   0), VALUE(   4,   0), VALUE(   3,   8), VALUE(   0,   0)  };
    eval ePsqt[7][64] = {
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999),
           VALUE( 137,   9), VALUE(  62,  54), VALUE(  98,  57), VALUE( 134,  -9), VALUE(  63,   9), VALUE(  72,  48), VALUE(  22,  46), VALUE(  66,  29),
           VALUE(  16,  38), VALUE(  23,  24), VALUE(  26,   2), VALUE(  30, -13), VALUE(  46, -33), VALUE(  64, -27), VALUE(  39,  15), VALUE(  42,  -7),
           VALUE(   1,  24), VALUE(   4,  17), VALUE(  -9,   0), VALUE(  16, -17), VALUE(  16, -13), VALUE(  -6,  -4), VALUE(   1,   4), VALUE( -15,  -5),
           VALUE(   0,  11), VALUE( -18,  10), VALUE(   6,  -5), VALUE(  19, -17), VALUE(  15,  -7), VALUE(   2,  -8), VALUE( -12,   1), VALUE( -20,   0),
           VALUE(   1,   6), VALUE( -13,  -1), VALUE(  -1,  -7), VALUE( -12,  -3), VALUE(  -4,  -7), VALUE( -16,  -1), VALUE(   4,  -9), VALUE(  -8, -10),
           VALUE( -14,  13), VALUE( -19,   2), VALUE( -20,   1), VALUE( -24, -24), VALUE( -22, -11), VALUE(   2,   0), VALUE(   7,  -7), VALUE( -23,  -8),
           VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999)  },
        {  VALUE(-208, -15), VALUE( -66, -37), VALUE(  -1, -47), VALUE( -85, -12), VALUE(  32, -50), VALUE( -55, -40), VALUE( -66,   2), VALUE( -74, -94),
           VALUE( -39, -41), VALUE(  22, -44), VALUE(  -5, -17), VALUE(  48, -37), VALUE(  25, -32), VALUE(  48, -51), VALUE(  23, -44), VALUE( -16, -63),
           VALUE( -34, -37), VALUE(  38, -37), VALUE(  17,  -5), VALUE(  63, -12), VALUE( 106, -34), VALUE(  38,  26), VALUE(  22, -28), VALUE( -24, -36),
           VALUE(  -5, -52), VALUE(   4, -16), VALUE(  43,  -4), VALUE(  24,   3), VALUE(   2,  14), VALUE(  63, -46), VALUE(  10, -14), VALUE(  19, -49),
           VALUE( -18, -10), VALUE( -10, -12), VALUE(  16,  -7), VALUE(   5,   1), VALUE(   7,   4), VALUE(   9,   2), VALUE(  14,  -5), VALUE(  -6, -39),
           VALUE( -34, -58), VALUE(  -1, -49), VALUE(  -9, -19), VALUE(  12, -20), VALUE(  20, -15), VALUE(   9, -31), VALUE(  16, -46), VALUE( -28, -42),
           VALUE( -55, -67), VALUE( -32, -25), VALUE( -32, -59), VALUE(  -3, -41), VALUE(   5, -41), VALUE(   4, -44), VALUE(  13, -40), VALUE( -17, -71),
           VALUE( -67,-142), VALUE( -31, -96), VALUE( -53, -54), VALUE( -29, -48), VALUE( -13, -75), VALUE( -19, -65), VALUE( -21, -84), VALUE( -84,-102)  },
        {  VALUE( -20,  -1), VALUE(  -6, -11), VALUE( -42,   2), VALUE(  -5,   3), VALUE( -24,   7), VALUE(   7,   1), VALUE( -41,   0), VALUE( -50,  -3),
           VALUE( -40,  -6), VALUE(   0,   2), VALUE( -13,  -4), VALUE(  13,   3), VALUE(   5,   0), VALUE(  13, -13), VALUE(  -8,   1), VALUE( -39,   6),
           VALUE( -14,  10), VALUE(   7,   3), VALUE(   8,   2), VALUE(  23,   4), VALUE(  18,  13), VALUE(  66,  11), VALUE(  22,  -5), VALUE(  18,   3),
           VALUE(   0, -14), VALUE(   6,  23), VALUE(  10,   4), VALUE(  45,   9), VALUE(  13,  22), VALUE(  37,   5), VALUE(   4,   3), VALUE(   4, -22),
           VALUE( -15, -10), VALUE(   3,   0), VALUE(  13,  19), VALUE(  31,   7), VALUE(  30,   8), VALUE(  -8,  11), VALUE(  15,  -2), VALUE( -13,   1),
           VALUE(  -9,  -9), VALUE(  21,   0), VALUE(   2,   1), VALUE(   4,  16), VALUE( -10,  17), VALUE(  26,  -4), VALUE(  15, -16), VALUE(  -5,  -1),
           VALUE(  10, -55), VALUE(   0, -16), VALUE(   8,  -5), VALUE( -10,  -7), VALUE(   8,  -7), VALUE(  11, -16), VALUE(  27, -19), VALUE(  -3, -75),
           VALUE(  -6, -37), VALUE(  -7, -47), VALUE( -19, -43), VALUE( -21, -25), VALUE(   2, -28), VALUE( -21, -17), VALUE( -38, -38), VALUE(  -7, -84)  },
        {  VALUE(  14,  -4), VALUE(  19,  13), VALUE(  25,  10), VALUE(  58, -12), VALUE(  23,  20), VALUE(  23,  29), VALUE(  66,  17), VALUE(  28,   8),
           VALUE(  24,  19), VALUE(  11,  21), VALUE(  31,  20), VALUE(  64,   6), VALUE(  47,  15), VALUE(  31,  18), VALUE(  29,  18), VALUE(  40,  11),
           VALUE( -11,  21), VALUE( -17,  26), VALUE(  17,  12), VALUE(  37,   2), VALUE(  59,  -9), VALUE(  65,  -5), VALUE(  58,   3), VALUE(  10,   8),
           VALUE( -17,  10), VALUE(   6,  16), VALUE(  13,   4), VALUE(  10,   3), VALUE(   4,  -3), VALUE(   1,   5), VALUE(   4,  -5), VALUE( -26,  12),
           VALUE( -28,   7), VALUE( -25,   3), VALUE( -12,   9), VALUE( -16,   7), VALUE(  -6,   3), VALUE( -18,   3), VALUE(  -1,  -4), VALUE( -15,  -1),
           VALUE( -48,  -7), VALUE( -28,  -3), VALUE( -33,   2), VALUE( -15, -11), VALUE( -18,  -5), VALUE( -22, -13), VALUE( -19,  -9), VALUE( -47,   1),
           VALUE( -25, -24), VALUE( -24, -21), VALUE( -10, -15), VALUE(  -9, -21), VALUE( -14, -31), VALUE( -10, -27), VALUE( -24, -20), VALUE( -73, -20),
           VALUE( -13, -10), VALUE(  -7, -15), VALUE(   0,  -3), VALUE(   0,  -5), VALUE(   1, -13), VALUE(   0,  -5), VALUE( -53,  10), VALUE( -18, -24)  },
        {  VALUE( -46,   1), VALUE( -12, -11), VALUE(  15,  -1), VALUE(  -5,  31), VALUE(   6,  -2), VALUE(   1,  11), VALUE(  53, -40), VALUE( -45,  48),
           VALUE( -13, -11), VALUE( -48,  40), VALUE( -17,  52), VALUE(   2,  34), VALUE(  13,  23), VALUE(  17,  23), VALUE( -16,  33), VALUE(  21,  17),
           VALUE( -13,  10), VALUE( -13,  -9), VALUE( -33,  20), VALUE(  13,  20), VALUE(   8,  17), VALUE(  60,  -7), VALUE(  48, -24), VALUE(  23,  12),
           VALUE( -17,   1), VALUE( -25,  30), VALUE(  -1,  19), VALUE( -29,  51), VALUE(  -5,  36), VALUE( -15,   4), VALUE(   6,   8), VALUE(  -9,  33),
           VALUE( -11, -23), VALUE( -18,   0), VALUE( -16,  29), VALUE( -33,  55), VALUE(  -9,  23), VALUE( -11,  18), VALUE(   4,   3), VALUE(   3, -36),
           VALUE( -13, -12), VALUE(  -2,  -1), VALUE( -14,  19), VALUE( -20,   9), VALUE(  -2, -15), VALUE( -10,  26), VALUE(  -1,   8), VALUE( -10, -29),
           VALUE( -10, -19), VALUE( -11, -12), VALUE(   0, -21), VALUE(  -2, -20), VALUE(   9, -33), VALUE(  21, -68), VALUE( -12, -73), VALUE( -36, -85),
           VALUE( -12, -59), VALUE(  -6, -20), VALUE(  17, -52), VALUE(  13, -51), VALUE(  19, -65), VALUE( -39, -55), VALUE(  16,-223), VALUE(  -4,-119)  },
        {  VALUE( -34, -79), VALUE( 148, -82), VALUE( -27, -75), VALUE( 135, -59), VALUE(-125, -21), VALUE(  26,  14), VALUE(  47, -32), VALUE(-108, -91),
           VALUE( -20, -38), VALUE(  -3,   2), VALUE( -27,  23), VALUE( -63,   2), VALUE( -13,  -8), VALUE(-155, -30), VALUE( -46, -20), VALUE( -77, -76),
           VALUE(  64, -18), VALUE( -82,  46), VALUE(-106,  37), VALUE( -60,  28), VALUE( -32,  10), VALUE( -17,  16), VALUE( -60,  12), VALUE( -20,  -5),
           VALUE( -17,   5), VALUE(  -2,  12), VALUE( -37,  31), VALUE( -75,  29), VALUE( -45,  24), VALUE(  -8,  16), VALUE(  -9,  10), VALUE( -20,   1),
           VALUE(   0, -15), VALUE(  30,  -5), VALUE(  -7,  16), VALUE( -31,  28), VALUE( -23,  28), VALUE(  -3,  16), VALUE(  17,   4), VALUE( -37,   0),
           VALUE(   3, -25), VALUE(  26,  -1), VALUE(  -8,  15), VALUE( -33,  21), VALUE(  -7,  21), VALUE(  -8,  15), VALUE(  -7,   4), VALUE(  -8, -11),
           VALUE(  12, -32), VALUE(  -2,  10), VALUE( -20,   6), VALUE( -55,  16), VALUE( -32,  10), VALUE(  -8,   9), VALUE(   7,   3), VALUE(  19, -23),
           VALUE( -88,  17), VALUE(   3, -25), VALUE(   4, -16), VALUE( -75, -14), VALUE(  -9, -50), VALUE( -53, -12), VALUE(  19, -29), VALUE(  11, -55)  }
    };
};

#ifdef EVALTUNE
struct positiontuneset {
    uint8_t ph;
    int16_t g[NUMOFEVALPARAMS];   // maybe even char could be enough if parameters don't exceed this 
    int8_t R;                    
};

struct tuneparamselection {
    eval *ev[NUMOFEVALPARAMS];
    string name[NUMOFEVALPARAMS];
    bool tune[NUMOFEVALPARAMS];
    int index1[NUMOFEVALPARAMS];
    int bound1[NUMOFEVALPARAMS];
    int index2[NUMOFEVALPARAMS];
    int bound2[NUMOFEVALPARAMS];

    int count;
};

//extern tuneparamselection tps;

#endif

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

extern int tuningratio;

#endif

//
// board stuff
//
#define BOARDSIZE 64
#define RANKMASK 0x38

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

#define BOUNDMASK 0x03 
#define HASHALPHA 0x01
#define HASHBETA 0x02
#define HASHEXACT 0x03

#define MAXDEPTH 256
#define NOSCORE SHRT_MIN
#define SCOREBLACKWINS (SHRT_MIN + 3 + MAXDEPTH)
#define SCOREWHITEWINS (-SCOREBLACKWINS)
#define SCOREDRAW 0
#define SCORETBWIN 30000

#define MATEFORME(s) (s > SCOREWHITEWINS - MAXDEPTH)
#define MATEFOROPPONENT(s) (s < SCOREBLACKWINS + MAXDEPTH)
#define MATEDETECTED(s) (MATEFORME(s) || MATEFOROPPONENT(s))

/* Offsets for 64Bit  board*/
const int knightoffset[] = { -6, -10, -15, -17, 6, 10, 15, 17 };
const int diagonaloffset[] = { -7, -9, 7, 9 };
const int orthogonaloffset[] = { -8, -1, 1, 8 };
const int orthogonalanddiagonaloffset[] = { -8, -1, 1, 8, -7, -9, 7, 9 };
const int shifting[] = { 0, 0, 0, 1, 2, 3, 0 };

const struct { int offset; bool needsblank; } pawnmove[] = { { 0x10, true }, { 0x0f, false }, { 0x11, false } };
extern const int prunematerialvalue[];
// values for move ordering
const int mvv[] = { 0U << 28, 1U << 28, 2U << 28, 2U << 28, 3U << 28, 4U << 28, 5U << 28 };
const int lva[] = { 5 << 25, 4 << 25, 3 << 25, 3 << 25, 2 << 25, 1 << 25, 0 << 25 };
#define PVVAL (7 << 28)
#define KILLERVAL1 (1 << 27)
#define KILLERVAL2 (KILLERVAL1 - 1)
#define NMREFUTEVAL (1 << 26)
#define TBFILTER INT32_MIN

#define ISEPCAPTURE 0x40
#define GETFROM(x) (((x) & 0x0fc0) >> 6)
#define GETTO(x) ((x) & 0x003f)
#define GETEPT(x) (((x) & 0x03f00000) >> 20)
#define GETEPCAPTURE(x) (((x) >> 20) & ISEPCAPTURE)

#define GETPROMOTION(x) (((x) & 0xf000) >> 12)
#define GETCAPTURE(x) (((x) & 0xf0000) >> 16)
#define ISTACTICAL(x) ((x) & 0xff000)
#define ISPROMOTION(x) ((x) & 0xf000)
#define ISCAPTURE(x) ((x) & 0xf0000)
#define GETPIECE(x) (((x) & 0xf0000000) >> 28)
#define GETTACTICALVALUE(x) (prunematerialvalue[GETCAPTURE(x) >> 1] + (ISPROMOTION(x) ? prunematerialvalue[GETPROMOTION(x) >> 1] - prunematerialvalue[PAWN] : 0))

#define PAWNATTACK(s, p) ((s) ? (((p) & ~FILEHBB) >> 7) | (((p) & ~FILEABB) >> 9) : (((p) & ~FILEABB) << 7) | (((p) & ~FILEHBB) << 9))
#define PAWNPUSH(s, p) ((s) ? ((p) >> 8) : ((p) << 8))

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

// kingdangerMask[14][WHITE]:
// 00000000
// 00000000
// 00000000
// 00000000
// 00000xxx
// 00000xxx
// 00000xox
// 00000xxx
extern U64 kingdangerMask[64][2];

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

// betweenMask[18][45]:
// 00000000
// 00000000
// 00000x00
// 00001000
// 00010000
// 00x00000
// 00000000
// 00000000
extern U64 betweenMask[64][64];

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
    uint32_t movecode;
};


class chessmove
{
public:
    // pcpcepepepepccccppppfffffftttttt
    uint32_t code;
    int value;

    chessmove();
    chessmove(int from, int to, PieceCode piece);
    chessmove(int from, int to, PieceCode capture, PieceCode piece);
    chessmove(int from, int to, PieceCode promote, PieceCode capture, PieceCode piece);
    chessmove(int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece);

    bool operator<(const chessmove cm) const { return (value < cm.value); }
    bool operator>(const chessmove cm) const { return (value > cm.value); }
    string toString();
    void print();
};

#define MAXMULTIPV 64

#define MAXMOVELISTLENGTH 256	// for lists of possible pseudo-legal moves
#define MAXMOVESEQUENCELENGTH 512	// for move sequences in a game


// FIXME: This is ugly! Almost the same classes with doubled code.
class chessmovesequencelist
{
public:
	int length;
	chessmove move[MAXMOVESEQUENCELENGTH];
	chessmovesequencelist();
	string toString();
	void print();
};


class chessmovelist
{
public:
    int length;
    chessmove move[MAXMOVELISTLENGTH];
	chessmovelist();
	string toString();
	string toStringWithValue();
	void print();
    void sort(int limit, const unsigned int refutetarget = BOARDSIZE);
};


enum MoveSelector_State { INITSTATE, HASHMOVESTATE, TACTICALINITSTATE, TACTICALSTATE, KILLERMOVE1STATE, KILLERMOVE2STATE, QUIETINITSTATE, QUIETSTATE, BADTACTICALSTATE };

class MoveSelector
{
    chessposition *pos;
public:
    int state;
    chessmovelist* captures;
    chessmovelist* quiets;
    chessmove hashmove;
    chessmove killermove1;
    chessmove killermove2;
    int refutetarget;
    int capturemovenum;
    int quietmovenum;

public:
    void SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, int nmrfttarget);
    ~MoveSelector();
    chessmove* next();
};

extern U64 pawn_attacks_occupied[64][2];
extern U64 knight_attacks[64];
extern U64 king_attacks[64];

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

enum EvalTrace { NOTRACE, TRACE };

enum MoveType { QUIET = 1, CAPTURE = 2, PROMOTE = 4, TACTICAL = 6, ALL = 7, QUIETWITHCHECK = 9 };

template <MoveType Mt> int CreateMovelist(chessposition *pos, chessmove* m);

class chessposition
{
public:
    U64 piece00[14];
    U64 occupied00[2];
    U64 attackedBy2[2];
    U64 attackedBy[2][7];
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
    int seldepth;
#ifdef DEBUG
    int maxdebugdepth = -1;
    int mindebugdepth = -1;
#endif
    chessmovelist rootmovelist;
    chessmovesequencelist pvline;
    int rootheight;
    chessmove bestmove[MAXMULTIPV];
    int bestmovescore[MAXMULTIPV];
    unsigned long killer[2][MAXDEPTH];
    unsigned int history[7][64];
#ifdef DEBUG    
    unsigned long long debughash = 0;
#endif
    //int *positionvaluetable; // value tables for both sides, 7 PieceTypes and 256 phase variations 
    int ph; // to store the phase during different evaluation functions
    int isCheck;
    int useTb;
    int useRootmoveScore;
    int tbPosition;
    chessmove defaultmove; // fallback if search in time trouble didn't finish a single iteration
#ifdef EVALTUNE
    tuneparamselection tps;
    positiontuneset pts;
    void resetTuner();
    void getPositionTuneSet(positiontuneset *p);
    void copyPositionTuneSet(positiontuneset *from, positiontuneset *to);
    string getGradientString();
    int getGradientValue(positiontuneset *p);
#endif
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
    PieceType Piece(int index);
    bool isAttacked(int index);
    U64 attackedByBB(int index, U64 occ);
    bool see(uint32_t move, int threshold);
    int getBestPossibleCapture();
    int getMoves(chessmove *m, MoveType t = ALL);
    void getRootMoves();
    void tbFilterRootMoves();
    void prepareStack();
    string movesOnStack();
    bool playMove(chessmove *cm);
    void unplayMove(chessmove *cm);
    void playNullMove();
    void unplayNullMove();
    void getpvline(int depth, int pvnum);
    bool moveIsPseudoLegal(uint32_t c);     // test if move is possible in current position
    uint32_t chessposition::shortMove2FullMove(uint16_t c); // transfer movecode from tt to full move code without checking if pseudoLegal
    template <EvalTrace> int getPositionValue();
    template <EvalTrace> int getPawnAndKingValue(pawnhashentry **entry);
    template <EvalTrace> int getValue();
#ifdef DEBUG
    void debug(int depth, const char* format, ...);
#endif
#ifdef DEBUGEVAL
    void debugeval(const char* format, ...);
#endif
    int testRepetiton();
    void mirror();
};

int getValueNoTrace(chessposition *p);
int getValueTrace(chessposition *p);


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
    int wtime, btime, winc, binc, movestogo, maxnodes, mate, movetime, maxdepth;
    bool infinite;
    bool debug = false;
    bool verbose;
    bool moveoutput;
    int sizeOfTp = 0;
    int moveOverhead;
    int MultiPV;
    bool ponder;
    string SyzygyPath;
    bool Syzygy50MoveRule;
    enum { NO, PONDERING, HITPONDER } pondersearch;
    int terminationscore = SHRT_MAX;
    int benchscore;
    int benchdepth;
    int stopLevel = ENGINESTOPPED;
    void communicate(string inputstring);
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
    int allscore;
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
    unsigned long long boardtable[64 * 16];
	unsigned long long cstl[32];
	unsigned long long ept[64];
    unsigned long long s2m;
    zobrist();
    unsigned long long getRnd();
    u8 getHash();
    u8 getPawnHash();
    u8 getMaterialHash();
    u8 modHash(int i);
};

#define TTBUCKETNUM 3


struct transpositionentry {
    uint32_t hashupper;
    uint16_t movecode;
    int16_t value;
    uint8_t depth;
    uint8_t boundAndAge;
};

struct transpositioncluster {
    transpositionentry entry[TTBUCKETNUM];
    char padding[2];
};

class transposition
{
    transpositioncluster *table;
    U64 used;
public:
    U64 size;
    U64 sizemask;
    chessposition *pos;
    int numOfSearchShiftTwo;
    ~transposition();
    int setSize(int sizeMb);    // returns the number of Mb not used by allignment
    void clean();
    bool testHash();
    void addHash(int val, int bound, int depth, uint16_t movecode);
    void printHashentry();
    bool probeHash(int *val, uint16_t *movecode, int depth, int alpha, int beta);
#if 0
    short getValue();
    int getValtype();
    int getDepth();
#endif
    uint16_t getMoveCode();
    unsigned int getUsedinPermill();
    void nextSearch() { numOfSearchShiftTwo = (numOfSearchShiftTwo + 4) & 0xfc; }
};

typedef struct pawnhashentry {
   uint32_t hashupper;
    U64 passedpawnbb[2];
    U64 isolatedpawnbb[2];
    U64 backwardpawnbb[2];
    int semiopen[2];
    U64 attacked[2];
    U64 attackedBy2[2];
    int32_t value;
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
void searchinit();


//
// uci stuff
//

enum GuiToken { UNKNOWN, UCI, UCIDEBUG, ISREADY, SETOPTION, REGISTER, UCINEWGAME, POSITION, GO, STOP, PONDERHIT, QUIT, EVAL };

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
    { "quit", QUIT },
    { "eval", EVAL }
};

class uci
{
    int state;
public:
    GuiToken parse(vector<string>*, string ss);
    void send(const char* format, ...);
};


