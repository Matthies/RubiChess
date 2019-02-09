#pragma once

#define VERNUM "1.3-dev"

#if 0
#define SDEBUG
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

#ifdef _WIN32

#include <conio.h>
#include <AclAPI.h>
#include <intrin.h>
#include <Windows.h>

#ifdef STACKDEBUG
#include <DbgHelp.h>
#define myassert(expression, pos, num, ...) (void)((!!(expression)) ||   (GetStackWalk(pos, (const char*)(#expression), (const char*)(__FILE__), (int)(__LINE__), (num), ##__VA_ARGS__), 0))
#else
#define myassert(expression, pos, num, ...) (void)(0)
#endif

#ifdef FINDMEMORYLEAKS
#include <crtdbg.h>
#endif

#else //_WIN32

#define myassert(expression, pos, num, ...) (void)(0)
#define sprintf_s sprintf
void Sleep(long x);

#endif

#ifdef _MSC_VER
// Hack to avoid warning in tbprobe.cpp
#define strcpy(a,b) strcpy_s(a,256,b)
#define strcat(a,b) strcat_s(a,256,b)

#define PREFETCH(a) _mm_prefetch((char*)(a), _MM_HINT_T0)
#else
#define PREFETCH(a) __builtin_prefetch(a)
#endif


#define ENGINEVER "RubiChess " VERNUM
#define BUILD __DATE__ " " __TIME__

#define BITSET(x) (1ULL << (x))
#ifdef _WIN32
#define GETLSB(i,x) _BitScanForward64((DWORD*)&(i), (x))
inline int pullLsb(unsigned long long *x) {
    DWORD i;
    _BitScanForward64(&i, *x);
    *x &= *x - 1;  // this is faster than *x ^= (1ULL << i);
    return i;
}
#define GETMSB(i,x) _BitScanReverse64((DWORD*)&(i), (x))
inline int pullMsb(unsigned long long *x) {
    DWORD i;
    _BitScanReverse64(&i, *x);
    *x ^= (1ULL << i);
    return i;
}
#define POPCOUNT(x) (int)(__popcnt64(x))
#else
#define GETLSB(i,x) (i = __builtin_ctzll(x))
inline int pullLsb(unsigned long long *x) {
    int i = __builtin_ctzll(*x);
    *x &= *x - 1;  // this is faster than *x ^= (1ULL << i);
    return i;
}
#define GETMSB(i,x) (i = (63 - __builtin_clzll(x)))
inline int pullMsb(unsigned long long *x) {
    int i = 63 - __builtin_clzll(*x);
    *x ^= (1ULL << i);
    return i;
}
#define POPCOUNT(x) __builtin_popcountll(x)
#endif

enum { WHITE, BLACK };
#define WHITEBB 0x55aa55aa55aa55aa
#define BLACKBB 0xaa55aa55aa55aa55
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
class searchthread;
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
#define PSQTINDEX(i,s) ((s) ? (i) : (i) ^ 0x38)

#define TAPEREDANDSCALEDEVAL(s, p, c) ((GETMGVAL(s) * (256 - (p)) + GETEGVAL(s) * (p) * (c) / SCALE_NORMAL) / 256)

#define NUMOFEVALPARAMS (2*5*4 + 5 + 4*8 + 8 + 5 + 4*28 + 2 + 7 + 1 + 7 + 6 + 7*64)
struct evalparamset {
    // Powered by Laser :-)
    eval ePawnstormblocked[4][5] = {
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  14, -17), VALUE(  29, -15), VALUE(  31, -15)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  14, -17), VALUE(  37, -16), VALUE(   6,  -7)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  17, -14), VALUE(  -5,   1), VALUE(  -4,   0)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE( -19,  -4), VALUE(  -8,  11), VALUE(   8,   2)  }
    };
    eval ePawnstormfree[4][5] = {
        {  VALUE( -25,  46), VALUE(  41,  59), VALUE( -16,  34), VALUE(  -6,  11), VALUE(  -1,   1)  },
        {  VALUE( -26,  59), VALUE( -60,  63), VALUE( -54,  25), VALUE(   3,  11), VALUE(   5,   6)  },
        {  VALUE( -62,  52), VALUE( -47,  51), VALUE( -37,  18), VALUE(  -6,   5), VALUE(  -7,  11)  },
        {  VALUE(  15,  32), VALUE( -29,  52), VALUE( -26,   9), VALUE( -14,  16), VALUE( -12,  14)  }
    };
    eval ePawnpushthreatbonus =  VALUE(  25,  15);
    eval eSafepawnattackbonus =  VALUE(  67,  18);
    eval eHangingpiecepenalty =  VALUE( -31, -36);
    eval eKingshieldbonus =  VALUE(  15,  -3);
    eval eTempo =  VALUE(  18,  22);
    eval ePassedpawnbonus[4][8] = {
        {  VALUE(   0,   0), VALUE(   5,   3), VALUE(  -4,   6), VALUE(   1,  28), VALUE(  30,  41), VALUE(  71,  70), VALUE(  54, 106), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -18,   9), VALUE(  -8,   9), VALUE( -11,  26), VALUE(  11,  27), VALUE(  31,  48), VALUE(  -2,  32), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   0,   7), VALUE(   6,   9), VALUE(   6,  36), VALUE(  24,  84), VALUE(  55, 192), VALUE( 112, 283), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  11,   7), VALUE(  -2,  16), VALUE(   6,  33), VALUE(  12,  57), VALUE(  55,  77), VALUE(  57,  63), VALUE(   0,   0)  }
    };
    eval eAttackingpawnbonus[8] = {  VALUE(   0,   0), VALUE( -56, -82), VALUE( -28, -29), VALUE( -18,   1), VALUE(  -3,  14), VALUE(   9,  51), VALUE(   0,   0), VALUE(   0,   0)  };
    eval eIsolatedpawnpenalty =  VALUE( -14, -11);
    eval eDoublepawnpenalty =  VALUE( -11, -22);
    eval eConnectedbonus =  VALUE(   7,  -4);
    eval eBackwardpawnpenalty =  VALUE( -10, -10);
    eval eDoublebishopbonus =  VALUE(  61,  35);
    eval eMobilitybonus[4][28] = {
        {  VALUE(  39, -56), VALUE(  46,  -2), VALUE(  53,  -3), VALUE(  58,  -3), VALUE(  63,   0), VALUE(  64,   6), VALUE(  63,  12), VALUE(  62,  18),
           VALUE(  61,  16), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(  30, -45), VALUE(  37, -23), VALUE(  50, -22), VALUE(  53, -14), VALUE(  57,  -2), VALUE(  61,  15), VALUE(  66,  18), VALUE(  66,  25),
           VALUE(  66,  29), VALUE(  68,  26), VALUE(  71,  25), VALUE(  71,  26), VALUE(  74,  29), VALUE(  74,  26), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(  11,  -2), VALUE(  21,  22), VALUE(  24,  31), VALUE(  29,  37), VALUE(  31,  38), VALUE(  33,  50), VALUE(  36,  50), VALUE(  39,  51),
           VALUE(  37,  63), VALUE(  36,  66), VALUE(  42,  70), VALUE(  41,  73), VALUE(  39,  78), VALUE(  47,  72), VALUE(  31,  80), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(  -2,  83), VALUE(  -2,  37), VALUE(  -3,  79), VALUE(   1,  24), VALUE(   4,  66), VALUE(  10,  50), VALUE(   8,  67), VALUE(  11,  63),
           VALUE(  11,  75), VALUE(  14,  69), VALUE(   9,  98), VALUE(  12, 104), VALUE(  11, 119), VALUE(  12, 118), VALUE(  10, 139), VALUE(  12, 143),
           VALUE(  13, 155), VALUE(  20, 153), VALUE(  26, 156), VALUE(  24, 160), VALUE(  35, 156), VALUE(  59, 158), VALUE(  32, 165), VALUE(  69, 142),
           VALUE(  86, 156), VALUE( 111, 116), VALUE( 139, 115), VALUE(  37, 182)  }
    };
    eval eSlideronfreefilebonus[2] = {  VALUE(  20,   4), VALUE(  43,   4)  };
    eval eMaterialvalue[7] = {  VALUE(   0,   0), VALUE( 100, 100), VALUE( 314, 314), VALUE( 314, 314), VALUE( 483, 483), VALUE( 913, 913), VALUE(32509,32509)};
    eval eWeakkingringpenalty =  VALUE( -17,   4);
    eval eKingattackweight[7] = {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   6,  -1), VALUE(   2,   0), VALUE(   3,  -1), VALUE(   1,   7), VALUE(   0,   0) };
    eval eSafecheckbonus[6] = {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  96,  16), VALUE(  17,  44), VALUE(  93,  -2), VALUE(  24, 100)  };
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
           VALUE( 121,  66), VALUE(  86,  66), VALUE(  92,  46), VALUE( 114,   9), VALUE( 133,   3), VALUE( 114,  30), VALUE(   9,  65), VALUE(  63,  59),
           VALUE(  -4,  37), VALUE(   3,  28), VALUE(  10,   8), VALUE(   7, -15), VALUE(  21, -18), VALUE(  33,  -9), VALUE(   5,  27), VALUE(   0,  28),
           VALUE(  -6,  24), VALUE(  -6,  19), VALUE(  -3,   0), VALUE(  11, -22), VALUE(  12, -17), VALUE(   0,  -4), VALUE(   2,  10), VALUE( -10,   9),
           VALUE( -20,  12), VALUE( -16,   8), VALUE(  -1, -12), VALUE(   4, -13), VALUE(   7, -10), VALUE(   0,  -7), VALUE( -14,   2), VALUE( -26,   3),
           VALUE( -18,  -2), VALUE( -22,   2), VALUE(  -7,  -8), VALUE( -15,   2), VALUE(  -2,   4), VALUE( -11,   7), VALUE(   7, -11), VALUE( -15,  -8),
           VALUE( -25,   6), VALUE( -22,   2), VALUE( -26,   8), VALUE( -20,  11), VALUE( -22,  17), VALUE(   4,   3), VALUE(  11, -11), VALUE( -25, -13),
           VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999)  },
        {  VALUE(-143, -59), VALUE( -76, -15), VALUE( -33, -27), VALUE( -78, -25), VALUE(  -8, -42), VALUE(-127, -25), VALUE(-174, -24), VALUE( -74, -81),
           VALUE( -51, -10), VALUE(   6, -21), VALUE(  32, -44), VALUE(  51, -35), VALUE(  -2, -13), VALUE(  91, -52), VALUE(  -1, -16), VALUE(  17, -72),
           VALUE( -19, -14), VALUE(  34, -26), VALUE(  76, -29), VALUE(  60, -20), VALUE( 132, -40), VALUE(  77, -30), VALUE(  56,  -5), VALUE( -11, -40),
           VALUE(  11, -19), VALUE(  41, -20), VALUE(  51,  -9), VALUE(  72,   1), VALUE(  39,  12), VALUE(  77, -10), VALUE(  26,   9), VALUE(  45, -31),
           VALUE(  10, -43), VALUE(  18,  -7), VALUE(  51, -18), VALUE(  53,  -9), VALUE(  54,   3), VALUE(  50, -13), VALUE(  42, -10), VALUE(  17, -37),
           VALUE(  -7, -57), VALUE(  15, -34), VALUE(  26, -36), VALUE(  36, -14), VALUE(  48, -20), VALUE(  39, -44), VALUE(  39, -45), VALUE(   3, -50),
           VALUE( -15, -61), VALUE(  -6, -28), VALUE(  11, -41), VALUE(  24, -37), VALUE(  26, -31), VALUE(  29, -54), VALUE(  24, -30), VALUE(  11, -63),
           VALUE( -43, -73), VALUE(   1, -71), VALUE( -15, -37), VALUE(   0, -28), VALUE(  12, -49), VALUE(   9, -43), VALUE(   4, -62), VALUE( -37, -95)  },
        {  VALUE(  33, -16), VALUE( -45,   2), VALUE( -40, -20), VALUE( -51,  -4), VALUE( -53,   3), VALUE( -90,  30), VALUE( -32, -13), VALUE( -32, -16),
           VALUE( -49,  12), VALUE(  16,  -7), VALUE(  -3,  -9), VALUE( -20,  -9), VALUE(  25, -13), VALUE(  -1, -10), VALUE(   2,   0), VALUE( -67,  -7),
           VALUE(   3,  -7), VALUE(   6,  -4), VALUE(  23,   0), VALUE(  29, -11), VALUE(  63, -22), VALUE(  48,  -2), VALUE(  17,   8), VALUE(  25,  -3),
           VALUE(   3,   2), VALUE(  26,  -2), VALUE(  24,  -4), VALUE(  41,  10), VALUE(  30,  14), VALUE(  31,   2), VALUE(  31,  -1), VALUE(  -1,  -3),
           VALUE(   2,   1), VALUE(  11,  -7), VALUE(  15,   7), VALUE(  37,   6), VALUE(  46,   2), VALUE(  17,   1), VALUE(  26, -15), VALUE(  19, -21),
           VALUE(   0, -14), VALUE(  15,  -5), VALUE(  13,  -2), VALUE(   9,   8), VALUE(   8,   8), VALUE(  26,  -6), VALUE(  23, -15), VALUE(  10, -14),
           VALUE(  30, -24), VALUE(  17, -23), VALUE(  15, -20), VALUE(   4, -10), VALUE(  16, -14), VALUE(  26, -14), VALUE(  37, -26), VALUE(  19, -47),
           VALUE(   1, -22), VALUE(  12, -22), VALUE(  -3, -11), VALUE(   3, -11), VALUE(   5,  -7), VALUE(  -7,   2), VALUE(  -9, -30), VALUE(   6, -38)  },
        {  VALUE(  -3,  31), VALUE(  -3,  37), VALUE(  -8,  30), VALUE(   3,  31), VALUE(  -6,  34), VALUE( -21,  45), VALUE(   0,  41), VALUE(  24,  34),
           VALUE( -10,  50), VALUE( -16,  50), VALUE(  12,  40), VALUE(  22,  32), VALUE(   7,  26), VALUE(  58,  25), VALUE( -13,  51), VALUE(  21,  35),
           VALUE( -25,  46), VALUE(  -4,  38), VALUE( -17,  43), VALUE(  -2,  30), VALUE(  30,  21), VALUE(  22,  33), VALUE(  25,  23), VALUE( -14,  38),
           VALUE( -20,  42), VALUE( -26,  44), VALUE(   1,  26), VALUE(  -1,  38), VALUE(   6,  23), VALUE(  16,  30), VALUE( -13,  35), VALUE(   6,  25),
           VALUE( -30,  39), VALUE( -39,  44), VALUE( -20,  35), VALUE(  -9,  30), VALUE(  -6,  29), VALUE(   4,  18), VALUE(  16,  13), VALUE( -19,  18),
           VALUE( -31,  25), VALUE( -19,  23), VALUE( -25,  34), VALUE(  -8,   5), VALUE(  -9,  13), VALUE(  -8,   5), VALUE(  10,   2), VALUE( -24,  12),
           VALUE( -31,  12), VALUE( -25,  13), VALUE( -16,  19), VALUE(  -7,   8), VALUE(  -4,   5), VALUE(   7,  -2), VALUE(  -5,  -3), VALUE( -73,  18),
           VALUE( -10,  16), VALUE(  -8,   8), VALUE(   0,   6), VALUE(  16,  -6), VALUE(  13,  -3), VALUE(  18,  -1), VALUE( -19,  14), VALUE(  -9,  -8)  },
        {  VALUE( -58,  69), VALUE( -39,  65), VALUE( -48,  91), VALUE( -52,  67), VALUE( -31,  65), VALUE(  38,  59), VALUE(  -3,  75), VALUE(  27,  25),
           VALUE( -56,  78), VALUE( -49,  67), VALUE( -18,  70), VALUE( -32,  78), VALUE( -45,  99), VALUE(  10,  56), VALUE( -14,  66), VALUE(  28,  31),
           VALUE( -36,  57), VALUE( -10,  49), VALUE( -24,  48), VALUE( -15,  78), VALUE(   1,  63), VALUE(  39,  60), VALUE(  35,  32), VALUE(  19,  42),
           VALUE( -24,  65), VALUE( -20,  53), VALUE( -16,  71), VALUE( -33,  85), VALUE( -11,  78), VALUE(  16,  40), VALUE(   6,  78), VALUE(  13,  36),
           VALUE(  -6,  26), VALUE(  -5,  40), VALUE(  -7,  43), VALUE( -10,  50), VALUE(  16,  31), VALUE(  -4,  55), VALUE(  25,  43), VALUE(   9,  28),
           VALUE( -14,  27), VALUE(   1,  19), VALUE(   0,  32), VALUE(   0,   8), VALUE(  -3,  43), VALUE(   2,  47), VALUE(  21,  14), VALUE(   3,   9),
           VALUE(  -8,   0), VALUE(   5,  -3), VALUE(  13, -23), VALUE(  16,  -3), VALUE(  16,   8), VALUE(  37, -43), VALUE(  34, -60), VALUE(  23, -68),
           VALUE(  -8,  -5), VALUE(   5, -29), VALUE(  11, -25), VALUE(  16,  -8), VALUE(  11,  16), VALUE(  -6,  -2), VALUE( -22, -18), VALUE( -14,  -3)  },
        {  VALUE( 116, -87), VALUE( -45,  32), VALUE( -50,  -1), VALUE(-119,  26), VALUE(  77, -51), VALUE(  18, -33), VALUE(  62, -57), VALUE(  59, -94),
           VALUE( -58,  43), VALUE(  12,  32), VALUE(  -1,   9), VALUE( -29,  37), VALUE( -30,  50), VALUE(  23,  55), VALUE(  19,  56), VALUE(  19,   4),
           VALUE(   1,  27), VALUE(  55,  24), VALUE(  28,  40), VALUE(  25,  38), VALUE(  19,  46), VALUE(  46,  44), VALUE(  65,  28), VALUE(  35,   5),
           VALUE( -29,   3), VALUE(  10,  25), VALUE( -30,  43), VALUE( -15,  53), VALUE( -27,  47), VALUE(   4,  48), VALUE(  31,  19), VALUE( -42,  -4),
           VALUE( -45,   1), VALUE(  28,   5), VALUE( -14,  31), VALUE( -65,  59), VALUE( -29,  53), VALUE( -22,  32), VALUE(   0,   9), VALUE( -68, -12),
           VALUE( -22, -16), VALUE(  -7,   1), VALUE( -22,  25), VALUE( -32,  39), VALUE( -38,  44), VALUE( -24,  26), VALUE(  -2,   0), VALUE( -43,  -9),
           VALUE(  43, -32), VALUE(  39, -18), VALUE(  19,   4), VALUE( -50,  25), VALUE( -32,  26), VALUE( -10,  11), VALUE(  17,  -3), VALUE(  19, -25),
           VALUE(  27, -70), VALUE(  54, -58), VALUE(  30, -33), VALUE( -57,  -7), VALUE(  -2, -35), VALUE( -47,  -6), VALUE(  15, -33), VALUE(   8, -63)  }
    };
};

#ifdef EVALTUNE
struct positiontuneset {
    uint8_t ph;
    uint8_t sc;
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

struct tuner {
    thread thr;
    int index;
    int paramindex;
    eval ev[NUMOFEVALPARAMS];
    int paramcount;
    double error;
    bool busy = false;
};

struct tunerpool {
    int lowRunning;
    int highRunning;
    int lastImproved;
    tuner *tn;
};


void registeralltuners(chessposition *pos);

#endif

#define SCALE_NORMAL 128
#define SCALE_DRAW 0
#define SCALE_ONEPAWN 48
#define SCALE_HARDTOWIN 10

//
// utils stuff
//
vector<string> SplitString(const char* s);
unsigned char AlgebraicToIndex(string s, int base);
string IndexToAlgebraic(int i);
string AlgebraicFromShort(string s, chessposition *pos);
void BitboardDraw(U64 b);
U64 getTime();
#ifdef STACKDEBUG
void GetStackWalk(chessposition *pos, const char* message, const char* _File, int Line, int num, ...);
#endif
#ifdef EVALTUNE
typedef void(*initevalfunc)(void);
bool PGNtoFEN(string pgnfilename);
void TexelTune(string fenfilename);

extern int tuningratio;

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
    u8 getHash(chessposition *pos);
    u8 getPawnHash(chessposition *pos);
    u8 getMaterialHash(chessposition *pos);
    u8 modHash(int i);
};

#define TTBUCKETNUM 3


struct transpositionentry {
    uint32_t hashupper;
    uint16_t movecode;
    int16_t value;
    int16_t staticeval;
    uint8_t depth;
    uint8_t boundAndAge;
};

struct transpositioncluster {
    transpositionentry entry[TTBUCKETNUM];
    //char padding[2];
};


#define FIXMATESCOREPROBE(v,p) (MATEFORME(v) ? (v) - p : (MATEFOROPPONENT(v) ? (v) + p : v))
#define FIXMATESCOREADD(v,p) (MATEFORME(v) ? (v) + p : (MATEFOROPPONENT(v) ? (v) - p : v))

class transposition
{
public:
    transpositioncluster *table;
    U64 size;
    U64 sizemask;
    int numOfSearchShiftTwo;
    ~transposition();
    int setSize(int sizeMb);    // returns the number of Mb not used by allignment
    void clean();
    void addHash(U64 hash, int val, int16_t staticeval, int bound, int depth, uint16_t movecode);
    void printHashentry(U64 hash);
    bool probeHash(U64 hash, int *val, int *staticeval, uint16_t *movecode, int depth, int alpha, int beta, int ply);
    uint16_t getMoveCode(U64 hash);
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


class Pawnhash
{
public:
    S_PAWNHASHENTRY *table;
    U64 size;
    U64 sizemask;
    Pawnhash(int sizeMb);
    ~Pawnhash();
    bool probeHash(U64 hash, pawnhashentry **entry);
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


#define MATERIALHASHSIZE 0x10000
#define MATERIALHASHMASK (MATERIALHASHSIZE - 1)


struct Materialhashentry {
    U64 hash;
    int scale[2];
};


class Materialhash
{
public:
    Materialhashentry table[MATERIALHASHSIZE];
    bool probeHash(U64 hash, Materialhashentry **entry);
};


extern zobrist zb;
extern transposition tp;
extern Materialhash mh;


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
#define HASHEXACT 0x00

#define MAXDEPTH 256
#define NOSCORE SHRT_MIN
#define SCOREBLACKWINS (SHRT_MIN + 3 + MAXDEPTH)
#define SCOREWHITEWINS (-SCOREBLACKWINS)
#define SCOREDRAW 0
#define SCORETBWIN 30000

#define MATEFORME(s) ((s) > SCOREWHITEWINS - MAXDEPTH)
#define MATEFOROPPONENT(s) ((s) < SCOREBLACKWINS + MAXDEPTH)
#define MATEDETECTED(s) (MATEFORME(s) || MATEFOROPPONENT(s))

/* Offsets for 64Bit  board*/
const int knightoffset[] = { -6, -10, -15, -17, 6, 10, 15, 17 };
const int diagonaloffset[] = { -7, -9, 7, 9 };
const int orthogonaloffset[] = { -8, -1, 1, 8 };
const int orthogonalanddiagonaloffset[] = { -8, -1, 1, 8, -7, -9, 7, 9 };
const int shifting[] = { 0, 0, 0, 1, 2, 3, 0 };

const struct { int offset; bool needsblank; } pawnmove[] = { { 0x10, true }, { 0x0f, false }, { 0x11, false } };
extern const int materialvalue[];
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
#define GETTACTICALVALUE(x) (materialvalue[GETCAPTURE(x) >> 1] + (ISPROMOTION(x) ? materialvalue[GETPROMOTION(x) >> 1] - materialvalue[PAWN] : 0))

#define GIVECHECKFLAG 0x08000000
#define GIVESCHECK(x) ((x) & GIVECHECKFLAG)

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
    U64 isCheckbb;
};

#define MAXMOVELISTLENGTH 256	// for lists of possible pseudo-legal moves
#define MAXMOVESEQUENCELENGTH 512	// for move sequences in a game


class chessmove
{
public:
    // ppppyxeeeeeeccccrrrrfffffftttttt
    // t(6): index of 'to'-square
    // f(6): index of 'from'-square
    // r(4): piececode of promote
    // c(4): piececode of capture
    // e(4): index of ep capture target
    // x(1): flags an ep capture move
    // y(1): flags a move givin check (not every move that gives check is flagged!); not implemented yet
    // p(4): piececode of the moving piece

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
#define MAXTHREADS 64


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
    void sort(int limit, uint32_t hashmove, uint32_t killer1, uint32_t killer2);
};


enum MoveSelector_State { INITSTATE, HASHMOVESTATE, TACTICALINITSTATE, TACTICALSTATE, KILLERMOVE1STATE, KILLERMOVE2STATE,
    QUIETINITSTATE, QUIETSTATE, BADTACTICALSTATE, BADTACTICALEND, EVASIONINITSTATE, EVASIONSTATE };

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
    int legalmovenum;
    bool onlyGoodCaptures;

public:
    void SetPreferredMoves(chessposition *p);  // for quiescence move selector
    void SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, int nmrfttarget);
    ~MoveSelector();
    chessmove* next();
};

extern U64 pawn_attacks_to[64][2];
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

enum MoveType { QUIET = 1, CAPTURE = 2, PROMOTE = 4, TACTICAL = 6, ALL = 7, EVASION = 8, QUIETWITHCHECK = 9 };
enum RootsearchType { SinglePVSearch, MultiPVSearch };

template <MoveType Mt> int CreateMovelist(chessposition *pos, chessmove* m);
enum AttackType { FREE, OCCUPIED };

class chessposition
{
public:
    U64 nodes;
    U64 piece00[14];
    U64 occupied00[2];
    U64 attackedBy2[2];
    U64 attackedBy[2][7];
    PieceCode mailbox[BOARDSIZE]; // redundand for faster "which piece is on field x"

    // The following block is mapped/copied to the movestack, so its important to keep the order
    int state;
    int ept;
    int kingpos[2];
    unsigned long long hash;
    unsigned long long pawnhash;
    unsigned long long materialhash;
    int halfmovescounter;
    int fullmovescounter;
    U64 isCheckbb;

    chessmovestack movestack[MAXMOVESEQUENCELENGTH];
    uint16_t excludemovestack[MAXMOVESEQUENCELENGTH];
    int16_t staticevalstack[MAXMOVESEQUENCELENGTH];
#if defined(STACKDEBUG) || defined(SDEBUG)
    uint32_t movecodestack[MAXMOVESEQUENCELENGTH];
#endif
    int mstop;      // 0 at last non-reversible move before root, rootheight at root position
    int ply;        // 0 at root position
    int rootheight; // fixed stack offset in root position 
    int seldepth;
    chessmovelist rootmovelist;
    chessmovesequencelist pvline;
    chessmove bestmove[MAXMULTIPV];
    int bestmovescore[MAXMULTIPV];
    uint32_t killer[2][MAXDEPTH];
    int32_t history[2][64][64];
    uint32_t bestFailingLow;
    Pawnhash *pwnhsh;
    repetition rp;
    int threadindex;
#ifdef SDEBUG
    unsigned long long debughash = 0;
    uint16_t pvdebug[MAXMOVESEQUENCELENGTH];
    bool debugRecursive;
    bool debugOnlySubtree;
    uint32_t pvtable[MAXDEPTH][MAXDEPTH];
#endif
    int ph; // to store the phase during different evaluation functions
    int sc; // to stor scaling factor used for evaluation
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
    bool w2m();
    void BitboardSet(int index, PieceCode p);
    void BitboardClear(int index, PieceCode p);
    void BitboardMove(int from, int to, PieceCode p);
    void BitboardPrint(U64 b);
    int getFromFen(const char* sFen);
    string toFen();
    bool applyMove(string s);
    void print(ostream* os = &cout);
    int phase();
    U64 movesTo(PieceCode pc, int from);
    bool isAttacked(int index);
    U64 isAttackedByMySlider(int index, U64 occ, int me);  // special simple version to detect giving check by removing blocker
    U64 attackedByBB(int index, U64 occ);  // returns bitboard of all pieces of both colors attacking index square 
    template <AttackType At> U64 isAttackedBy(int index, int col);    // returns the bitboard of cols pieces attacking the index square; At controls if pawns are moved to block or capture
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
    bool moveGivesCheck(uint32_t c);  // simple and imperfect as it doesn't handle special moves and cases (mainly to avoid pruning of important moves)
    bool moveIsPseudoLegal(uint32_t c);     // test if move is possible in current position
    uint32_t shortMove2FullMove(uint16_t c); // transfer movecode from tt to full move code without checking if pseudoLegal
    int getPositionValue();
    int getPawnAndKingValue(pawnhashentry **entry);
    int getValue();
    int getScaling(int col);

    template <RootsearchType RT> int rootsearch(int alpha, int beta, int depth);
    int alphabeta(int alpha, int beta, int depth, bool nullmoveallowed);
    int getQuiescence(int alpha, int beta, int depth);
    void updateHistory(int side, int from, int to, int value);

#ifdef SDEBUG
    void updatePvTable(uint32_t movecode);
    string getPv();
    bool triggerDebug(chessmove* nextmove);
    void sdebug(int indent, const char* format, ...);
#endif
#ifdef DEBUGEVAL
    void debugeval(const char* format, ...);
#endif
    int testRepetiton();
    void mirror();
};


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
    ~engine();
    uci *myUci;
    const char* name = ENGINEVER;
    const char* author = "Andreas Matthies";
    bool isWhite;
    U64 tbhits;
    U64 starttime;
    U64 endtime1; // time to send STOPSOON signal
    U64 endtime2; // time to send STOPPIMMEDIATELY signal
    U64 frequency;
    float fh, fhf;
    int wtime, btime, winc, binc, movestogo, maxnodes, mate, movetime, maxdepth;
    bool infinite;
    bool debug = false;
    bool moveoutput;
    int sizeOfTp = 0;
    int sizeOfPh;
    int moveOverhead;
    int MultiPV;
    bool ponder;
    string SyzygyPath;
    bool Syzygy50MoveRule;
    int Threads;
    searchthread *sthread;
    enum { NO, PONDERING, HITPONDER } pondersearch;
    int terminationscore = SHRT_MAX;
    int lastReport;
    int benchscore;
    int benchdepth;
    int stopLevel = ENGINESTOPPED;
#ifdef STACKDEBUG
    string assertfile = "";
#endif
    void communicate(string inputstring);
    void setOption(string sName, string sValue);
    void allocThreads();
    void allocPawnhash();
    U64 getTotalNodes();
    bool isPondering() { return (pondersearch == PONDERING); }
    void HitPonder() { pondersearch = HITPONDER; }
    bool testPonderHit() { return (pondersearch == HITPONDER); }
    void resetPonder() { pondersearch = NO; }
    long long perft(int depth, bool dotests);
    void prepareThreads();
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

extern engine en;

#ifdef SDEBUG
#define SDEBUGPRINT(b, d, f, ...) if (b) sdebug(d, f, ##__VA_ARGS__)
#else
#define SDEBUGPRINT(b, d, f, ...)
#endif


//
// search stuff
//
class searchthread
{
public:
    chessposition pos;
    Pawnhash *pwnhsh;
    thread thr;
    int index;
    int depth;
    int numofthreads;
    int lastCompleteDepth;
    searchthread *searchthreads;
    searchthread();
    ~searchthread();
};

void searchguide();
void searchinit();
void resetEndTime(int constantRootMoves, bool complete = true);

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


//
// TB stuff
//
extern int TBlargest; // 5 if 5-piece tables, 6 if 6-piece tables were found.

void init_tablebases(char *path);
int probe_wdl(int *success, chessposition *pos);
int probe_dtz(int *success, chessposition *pos);
int root_probe(chessposition *pos);
int root_probe_wdl(chessposition *pos);

