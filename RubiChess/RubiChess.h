#pragma once

#define VERNUM "1.2-dev"

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
    // Powered by Laser :-)
    eval ePawnstormblocked[4][5] = {
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  16, -18), VALUE(  28, -15), VALUE(  30, -16)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   8, -15), VALUE(  30, -16), VALUE(   5,  -8)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  12, -10), VALUE(  -8,   1), VALUE(  -3,  -1)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE( -23,   1), VALUE(  -9,  13), VALUE(   7,   3)  }
    };
    eval ePawnstormfree[4][5] = {
        {  VALUE( -50,  61), VALUE(  94,  53), VALUE( -19,  34), VALUE(  -5,   6), VALUE(   2,   0)  },
        {  VALUE(  22,  53), VALUE(   0,  58), VALUE( -48,  25), VALUE(  -1,   7), VALUE(   6,   4)  },
        {  VALUE( -25,  64), VALUE( -13,  55), VALUE( -44,  24), VALUE( -11,   6), VALUE(  -4,  10)  },
        {  VALUE(  49,  45), VALUE(  -4,  70), VALUE( -18,  15), VALUE( -10,  10), VALUE( -10,  14)  }
    };
    eval ePawnpushthreatbonus =  VALUE(  21,  16);
    eval eSafepawnattackbonus =  VALUE(  64,  30);
    eval eKingshieldbonus =  VALUE(  13,  -5);
    eval eTempo =  VALUE(   9,   8);
    eval ePassedpawnbonus[8] = {  VALUE(   0,   0), VALUE(  -5,   9), VALUE(  -9,  16), VALUE(  -8,  39), VALUE(  13,  65), VALUE(  24, 132), VALUE(  10, 159), VALUE(   0,   0)  };
    eval eAttackingpawnbonus[8] = {  VALUE(   0,   0), VALUE( -50, -51), VALUE( -29, -21), VALUE( -16,  10), VALUE(  -6,  11), VALUE(   1,  55), VALUE(   0,   0), VALUE(   0,   0)  };
    eval eIsolatedpawnpenalty =  VALUE( -14, -10);
    eval eDoublepawnpenalty =  VALUE( -21, -21);
    eval eConnectedbonus =  VALUE(   5,  -3);
    eval eBackwardpawnpenalty =  VALUE(  -9, -11);
    eval eDoublebishopbonus =  VALUE(  58,  35);
    eval eShiftmobilitybonus =  VALUE(   3,   5);
    eval eSlideronfreefilebonus[2] = {  VALUE(  19,  12), VALUE(  38,   0)  };
    eval eMaterialvalue[7] = {  VALUE(   0,   0), VALUE( 100, 100), VALUE( 314, 314), VALUE( 314, 314), VALUE( 483, 483), VALUE( 913, 913), VALUE(32509,32509)  };
    eval eWeakkingringpenalty =  VALUE( -16,   6);
    eval eKingattackweight[7] = {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   8,  -4), VALUE(   4,  -2), VALUE(   4,  -2), VALUE(   0,  10), VALUE(   0,   0)  };
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
           VALUE( 110,  63), VALUE(  95,  62), VALUE(  73,  49), VALUE( 102,   1), VALUE( 105,   9), VALUE(  85,  33), VALUE(  10,  66), VALUE(  48,  59),
           VALUE(  -2,  39), VALUE(   3,  33), VALUE(  19,   9), VALUE(  12, -18), VALUE(  26, -26), VALUE(  31, -10), VALUE(   5,  31), VALUE(   5,  23),
           VALUE(  -7,  24), VALUE(  -7,  19), VALUE( -11,   2), VALUE(  10, -23), VALUE(   9, -14), VALUE(  -8,  -1), VALUE(  -4,  14), VALUE( -12,   9),
           VALUE( -16,   9), VALUE( -17,   8), VALUE(  -3, -12), VALUE(   6, -16), VALUE(   8, -12), VALUE(  -2,  -7), VALUE( -16,   4), VALUE( -25,   2),
           VALUE( -16,  -3), VALUE( -19,   2), VALUE(  -8,  -7), VALUE( -16,   1), VALUE(  -4,   3), VALUE( -12,   7), VALUE(   6,  -8), VALUE( -16,  -7),
           VALUE( -23,   4), VALUE( -22,   3), VALUE( -24,   6), VALUE( -23,  10), VALUE( -24,  15), VALUE(   2,   4), VALUE(   9,  -8), VALUE( -24, -15),
           VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999)  },
        {  VALUE(-152, -49), VALUE( -81, -39), VALUE( -25, -47), VALUE( -88, -30), VALUE( -29, -53), VALUE(-129, -47), VALUE(-194, -23), VALUE( -70, -98),
           VALUE( -35, -35), VALUE(   9, -35), VALUE(  22, -46), VALUE(  39, -46), VALUE(  -1, -32), VALUE(  59, -57), VALUE(   1, -36), VALUE(  -5, -85),
           VALUE( -18, -29), VALUE(  26, -37), VALUE(  60, -34), VALUE(  51, -29), VALUE( 115, -53), VALUE(  61, -36), VALUE(  43, -22), VALUE( -15, -51),
           VALUE(   8, -35), VALUE(  27, -15), VALUE(  44, -21), VALUE(  49,  -3), VALUE(  19,   6), VALUE(  63, -17), VALUE(  13,   7), VALUE(  34, -40),
           VALUE(   3, -40), VALUE(   7, -11), VALUE(  35, -20), VALUE(  39, -12), VALUE(  36,   0), VALUE(  33, -14), VALUE(  33, -16), VALUE(   8, -39),
           VALUE( -14, -51), VALUE(  13, -38), VALUE(  17, -37), VALUE(  32, -25), VALUE(  41, -27), VALUE(  34, -48), VALUE(  33, -45), VALUE(  -2, -45),
           VALUE( -22, -64), VALUE( -14, -29), VALUE(  10, -48), VALUE(  19, -37), VALUE(  22, -37), VALUE(  24, -53), VALUE(  20, -37), VALUE(  10, -67),
           VALUE( -50, -80), VALUE(  -7, -74), VALUE( -18, -44), VALUE(  -4, -29), VALUE(   8, -50), VALUE(   5, -48), VALUE(  -5, -59), VALUE( -39, -92)  },
        {  VALUE(   9, -19), VALUE( -68,   0), VALUE( -61, -21), VALUE( -66,  -1), VALUE( -79,  -4), VALUE( -76,  14), VALUE( -40, -24), VALUE( -42, -17),
           VALUE( -51,   6), VALUE(  15, -15), VALUE( -15, -13), VALUE( -12, -25), VALUE(  13, -17), VALUE(  -5, -17), VALUE(  -1, -18), VALUE( -69, -17),
           VALUE(  -8, -16), VALUE(  -1,  -8), VALUE(  21, -19), VALUE(  20, -18), VALUE(  43, -22), VALUE(  52, -17), VALUE(   9,  -2), VALUE(  10,  -7),
           VALUE(  -5,  -6), VALUE(  15,  -7), VALUE(  15, -10), VALUE(  36,  -5), VALUE(  21,   3), VALUE(  17,  -1), VALUE(  16,   0), VALUE(  -7, -14),
           VALUE(  -4, -10), VALUE(   5, -10), VALUE(   6,   6), VALUE(  37,  -6), VALUE(  41, -10), VALUE(   4,   0), VALUE(  19, -15), VALUE(  11, -29),
           VALUE(  -7, -15), VALUE(  11,  -9), VALUE(   7,  -7), VALUE(   4,   4), VALUE(   1,   6), VALUE(  23, -13), VALUE(  17, -20), VALUE(   2, -16),
           VALUE(  21, -31), VALUE(  10, -26), VALUE(   8, -17), VALUE(  -1, -12), VALUE(  12, -15), VALUE(  21, -22), VALUE(  32, -27), VALUE(  13, -50),
           VALUE(  -3, -28), VALUE(   4, -23), VALUE(  -8, -13), VALUE(   0, -17), VALUE(  -3, -10), VALUE( -14,   2), VALUE( -22, -32), VALUE(  -3, -50)  },
        {  VALUE( -19,  23), VALUE( -14,  19), VALUE( -17,  14), VALUE(  -7,  14), VALUE( -26,  22), VALUE( -36,  35), VALUE(   3,  21), VALUE(  15,  21),
           VALUE( -17,  36), VALUE( -23,  36), VALUE(   9,  25), VALUE(  31,  14), VALUE(  14,  12), VALUE(  34,  18), VALUE(  -9,  32), VALUE(  10,  25),
           VALUE( -31,  34), VALUE( -14,  27), VALUE( -21,  26), VALUE(   0,  18), VALUE(  33,  -2), VALUE(  27,  16), VALUE(  14,  15), VALUE( -17,  22),
           VALUE( -29,  34), VALUE( -29,  32), VALUE(  -8,  19), VALUE(  -7,  25), VALUE(  -5,  16), VALUE(   1,  19), VALUE( -25,  24), VALUE(  -7,  19),
           VALUE( -39,  35), VALUE( -45,  32), VALUE( -26,  23), VALUE( -15,  17), VALUE( -13,  15), VALUE( -14,  14), VALUE(   8,   6), VALUE( -26,   8),
           VALUE( -41,  18), VALUE( -24,  14), VALUE( -36,  28), VALUE( -18,   1), VALUE( -18,   8), VALUE( -18,  -3), VALUE(  -1,   0), VALUE( -30,  11),
           VALUE( -35,   4), VALUE( -33,  10), VALUE( -20,  13), VALUE( -11,   6), VALUE(  -8,  -2), VALUE(  -2,  -5), VALUE(  -8, -12), VALUE( -74,  11),
           VALUE( -14,   4), VALUE( -12,  -8), VALUE(  -5,   4), VALUE(  11, -10), VALUE(   6,  -8), VALUE(  10,  -3), VALUE( -23,   3), VALUE( -11, -17)  },
        {  VALUE( -76,  53), VALUE( -53,  44), VALUE( -70,  71), VALUE( -70,  52), VALUE( -47,  35), VALUE(  43,  25), VALUE( -52,  70), VALUE( -34,  63),
           VALUE( -51,  41), VALUE( -50,  44), VALUE( -24,  45), VALUE( -34,  53), VALUE( -26,  39), VALUE(  14,  26), VALUE(   3,  18), VALUE(  18,  18),
           VALUE( -34,  23), VALUE( -21,  20), VALUE( -36,  35), VALUE( -11,  34), VALUE( -14,  47), VALUE(  33,  35), VALUE(  34, -10), VALUE(   6,  19),
           VALUE( -34,  46), VALUE( -35,  50), VALUE( -19,  46), VALUE( -34,  51), VALUE( -11,  43), VALUE(  -4,  29), VALUE( -13,  61), VALUE(  -6,  26),
           VALUE( -17,  15), VALUE( -16,  25), VALUE( -23,  28), VALUE( -24,  48), VALUE(   0,  21), VALUE( -22,  44), VALUE(   8,  23), VALUE(  -2,   5),
           VALUE( -19,   2), VALUE(  -8,   3), VALUE( -16,  25), VALUE( -14,   5), VALUE( -13,  21), VALUE( -13,  35), VALUE(   9,  -6), VALUE(  -9,  -9),
           VALUE( -13, -17), VALUE(  -7, -12), VALUE(   6, -32), VALUE(   9, -22), VALUE(   7, -15), VALUE(  30, -62), VALUE(  22, -70), VALUE(   7, -72),
           VALUE(  -9, -33), VALUE(  -3, -35), VALUE(   8, -37), VALUE(  15, -33), VALUE(   5, -12), VALUE(  -9, -30), VALUE( -23, -46), VALUE( -13, -48)  },
        {  VALUE( 184, -89), VALUE( 116, -64), VALUE(  65, -44), VALUE(-134,  15), VALUE( 223, -89), VALUE( -52,  14), VALUE( 281,-126), VALUE(  49, -59),
           VALUE(   9, -10), VALUE(   6,  20), VALUE(   3,   5), VALUE(   6,   5), VALUE( -44,  20), VALUE(  60,   9), VALUE(  67,  18), VALUE(  57, -10),
           VALUE(  65,  -9), VALUE(  -8,  37), VALUE(  21,  27), VALUE(   3,  27), VALUE(  31,  20), VALUE(  47,  23), VALUE(  84,  16), VALUE(  38,   0),
           VALUE( -45,   0), VALUE(   6,  13), VALUE( -70,  33), VALUE( -25,  32), VALUE( -62,  33), VALUE(  19,  23), VALUE(  19,  10), VALUE( -87,  14),
           VALUE( -47,  -8), VALUE(  31,  -3), VALUE( -22,  17), VALUE( -84,  44), VALUE( -65,  45), VALUE( -52,  23), VALUE( -11,   6), VALUE( -97,  -2),
           VALUE( -26, -16), VALUE( -12,  -1), VALUE( -33,  18), VALUE( -45,  31), VALUE( -51,  37), VALUE( -41,  22), VALUE( -14,   2), VALUE( -44,  -9),
           VALUE(  38, -32), VALUE(  38, -19), VALUE(  16,  -1), VALUE( -59,  22), VALUE( -41,  24), VALUE( -10,  10), VALUE(  13,  -3), VALUE(  19, -25),
           VALUE(  20, -56), VALUE(  52, -54), VALUE(  28, -30), VALUE( -57,  -7), VALUE(  -2, -35), VALUE( -44,  -5), VALUE(  17, -31), VALUE(   8, -63)  }
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
    uint32_t killer[2][MAXDEPTH];
    uint32_t history[7][64];
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
    uint32_t shortMove2FullMove(uint16_t c); // transfer movecode from tt to full move code without checking if pseudoLegal
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


