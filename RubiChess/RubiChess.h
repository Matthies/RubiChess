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
#define NEWTAPEREDEVAL(v, ph) (((256 - (ph)) * GETMGVAL(v) + (ph) * GETEGVAL(v)) / 256)
#define PSQTINDEX(i,s) ((s) ? (i) : (i) ^ 0x38)

#define NUMOFEVALPARAMS (2*5*4 + 5 + 4*8 + 8 + 5 + 4*28 + 2 + 7 + 1 + 7 + 7*64)
struct evalparamset {
    // Powered by Laser :-)
    eval ePawnstormblocked[4][5] = {
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  16, -18), VALUE(  28, -15), VALUE(  33, -16)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   8, -15), VALUE(  30, -16), VALUE(   6,  -7)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  12, -10), VALUE(  -8,   1), VALUE(  -3,  -1)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE( -25,  -1), VALUE(  -9,  11), VALUE(  10,   4)  }
    };
    eval ePawnstormfree[4][5] = {
        {  VALUE( -51,  61), VALUE(  73,  63), VALUE( -19,  34), VALUE(  -5,   6), VALUE(   2,   0)  },
        {  VALUE(  22,  56), VALUE( -15,  67), VALUE( -56,  29), VALUE(  -1,   7), VALUE(   6,   4)  },
        {  VALUE( -25,  64), VALUE( -40,  67), VALUE( -46,  27), VALUE( -11,   6), VALUE(  -4,  10)  },
        {  VALUE(  49,  47), VALUE(  -3,  70), VALUE( -25,  17), VALUE( -14,  13), VALUE( -10,  13)  }
    };
    eval ePawnpushthreatbonus =  VALUE(  23,  16);
    eval eSafepawnattackbonus =  VALUE(  72,  33);
    eval eHangingpiecepenalty = VALUE( -22, -25);
    eval eKingshieldbonus =  VALUE(  15,  -3);
    eval eTempo =  VALUE(  12,  11);
    eval ePassedpawnbonus[4][8] = {
        {  VALUE(   0,   0), VALUE(   3,   7), VALUE(  -9,  16), VALUE(  -4,  39), VALUE(  28,  48), VALUE(  54,  86), VALUE(  46, 105), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -16,   7), VALUE( -10,  12), VALUE( -16,  32), VALUE(  10,  40), VALUE(  28,  66), VALUE(  12,  53), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   0,   7), VALUE(  -4,  15), VALUE(  -4,  41), VALUE(  12,  84), VALUE(  32, 185), VALUE(  51, 261), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   7,  10), VALUE(  -2,  22), VALUE(   3,  39), VALUE(   8,  65), VALUE(  43,  88), VALUE(  31,  93), VALUE(   0,   0)  }
    };
    eval eAttackingpawnbonus[8] = {  VALUE(   0,   0), VALUE( -50, -66), VALUE( -32, -21), VALUE( -16,  14), VALUE(  -5,  17), VALUE(   2,  55), VALUE(   0,   0), VALUE(   0,   0)  };
    eval eIsolatedpawnpenalty =  VALUE( -15, -11);
    eval eDoublepawnpenalty =  VALUE( -13, -23);
    eval eConnectedbonus =  VALUE(   7,  -4);
    eval eBackwardpawnpenalty =  VALUE(  -9, -11);
    eval eDoublebishopbonus =  VALUE(  60,  35);
    eval eMobilitybonus[4][28] = {
        {  VALUE(  37, -60), VALUE(  43,  -6), VALUE(  49,  -7), VALUE(  54,  -6), VALUE(  62,  -4), VALUE(  63,   5), VALUE(  62,  10), VALUE(  62,  17),
           VALUE(  64,   8), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(  29, -46), VALUE(  36, -23), VALUE(  49, -23), VALUE(  52, -15), VALUE(  56,  -6), VALUE(  60,  14), VALUE(  64,  16), VALUE(  64,  20),
           VALUE(  64,  26), VALUE(  65,  20), VALUE(  66,  23), VALUE(  68,  17), VALUE(  72,  22), VALUE( 100,  -1), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(   8,  -8), VALUE(  17,  18), VALUE(  21,  31), VALUE(  27,  35), VALUE(  30,  37), VALUE(  34,  42), VALUE(  35,  44), VALUE(  39,  45),
           VALUE(  36,  61), VALUE(  35,  61), VALUE(  43,  64), VALUE(  42,  67), VALUE(  37,  72), VALUE(  49,  67), VALUE(  41,  68), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(  -8,   9), VALUE(  -7,  36), VALUE(  -8,  77), VALUE(  -4,  14), VALUE(   1,  46), VALUE(   8,  19), VALUE(   6,  47), VALUE(   8,  51),
           VALUE(   9,  59), VALUE(  13,  55), VALUE(  10,  84), VALUE(  11,  92), VALUE(  12, 102), VALUE(  13, 106), VALUE(  11, 119), VALUE(  14, 133),
           VALUE(  15, 135), VALUE(  21, 133), VALUE(  30, 135), VALUE(  27, 132), VALUE(  33, 139), VALUE(  46, 142), VALUE(  22, 150), VALUE(  75, 115),
           VALUE( 101, 115), VALUE( 162,  76), VALUE( 224,  22), VALUE(  52, 123)  }
    };
    eval eSlideronfreefilebonus[2] = {  VALUE(  18,  11), VALUE(  42,   0)  };
    eval eMaterialvalue[7] = {  VALUE(   0,   0), VALUE( 100, 100), VALUE( 314, 314), VALUE( 314, 314), VALUE( 483, 483), VALUE( 913, 913), VALUE(32509,32509)  };
    eval eWeakkingringpenalty =  VALUE( -17,   7);
    eval eKingattackweight[7] = {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   8,  -4), VALUE(   4,  -2), VALUE(   5,  -3), VALUE(   0,  10), VALUE(   0,   0)  };
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
           VALUE( 111,  63), VALUE(  94,  65), VALUE(  74,  49), VALUE(  90,  16), VALUE( 113,  12), VALUE(  86,  34), VALUE(   4,  66), VALUE(  49,  59),
           VALUE(  -3,  38), VALUE(   3,  32), VALUE(  16,  10), VALUE(   8, -18), VALUE(  23, -24), VALUE(  30,  -9), VALUE(   1,  31), VALUE(   4,  26),
           VALUE(  -7,  22), VALUE(  -7,  19), VALUE( -10,   2), VALUE(  10, -23), VALUE(   9, -14), VALUE(  -7,   0), VALUE(  -2,  14), VALUE( -10,   9),
           VALUE( -20,  10), VALUE( -18,   8), VALUE(  -3, -12), VALUE(   6, -16), VALUE(   7, -11), VALUE(   0,  -7), VALUE( -16,   4), VALUE( -26,   2),
           VALUE( -17,  -3), VALUE( -21,   2), VALUE(  -8,  -7), VALUE( -15,   2), VALUE(  -3,   4), VALUE( -12,   7), VALUE(   6,  -8), VALUE( -16,  -7),
           VALUE( -24,   4), VALUE( -22,   3), VALUE( -25,   6), VALUE( -21,  12), VALUE( -23,  19), VALUE(   2,   4), VALUE(   9,  -8), VALUE( -24, -15),
           VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999)  },
        {  VALUE(-144, -64), VALUE( -75, -39), VALUE( -40, -47), VALUE( -73, -37), VALUE( -18, -50), VALUE(-144, -40), VALUE(-197, -27), VALUE( -72, -99),
           VALUE( -40, -33), VALUE(   3, -33), VALUE(  26, -46), VALUE(  48, -48), VALUE( -10, -31), VALUE(  82, -68), VALUE(  -1, -36), VALUE(   2, -85),
           VALUE( -21, -28), VALUE(  29, -36), VALUE(  69, -32), VALUE(  58, -26), VALUE( 131, -52), VALUE(  76, -36), VALUE(  53, -25), VALUE( -17, -49),
           VALUE(  11, -34), VALUE(  34, -20), VALUE(  46, -11), VALUE(  63,  -3), VALUE(  28,   9), VALUE(  70, -11), VALUE(  19,   7), VALUE(  36, -40),
           VALUE(   8, -45), VALUE(  12, -12), VALUE(  44, -20), VALUE(  49, -11), VALUE(  46,   0), VALUE(  42, -13), VALUE(  35, -14), VALUE(  12, -44),
           VALUE( -11, -54), VALUE(  13, -42), VALUE(  22, -40), VALUE(  34, -24), VALUE(  43, -24), VALUE(  36, -46), VALUE(  34, -44), VALUE(   0, -45),
           VALUE( -17, -66), VALUE( -10, -31), VALUE(   9, -48), VALUE(  20, -38), VALUE(  22, -35), VALUE(  25, -56), VALUE(  20, -37), VALUE(  10, -66),
           VALUE( -48, -80), VALUE(  -1, -73), VALUE( -18, -43), VALUE(  -3, -30), VALUE(   9, -49), VALUE(   8, -45), VALUE(   2, -63), VALUE( -40, -92)  },
        {  VALUE(  28, -27), VALUE( -82,   0), VALUE( -68, -24), VALUE( -72,  -7), VALUE( -74,  -7), VALUE(-118,  17), VALUE( -41, -27), VALUE( -34, -22),
           VALUE( -55,   4), VALUE(  20, -18), VALUE( -12, -15), VALUE( -24, -26), VALUE(  13, -20), VALUE(  -3, -20), VALUE(   9, -21), VALUE( -78, -14),
           VALUE(  -9, -17), VALUE(  -2,  -8), VALUE(  25, -11), VALUE(  28, -18), VALUE(  55, -24), VALUE(  52, -13), VALUE(   9,  -2), VALUE(   9, -11),
           VALUE(  -3,  -6), VALUE(  21,  -8), VALUE(  19,  -5), VALUE(  36,   5), VALUE(  28,  12), VALUE(  22,  -1), VALUE(  23,  -2), VALUE(  -7, -11),
           VALUE(  -4, -10), VALUE(   6,  -9), VALUE(   9,   5), VALUE(  36,   5), VALUE(  45,   2), VALUE(  11,  -1), VALUE(  24, -20), VALUE(  11, -29),
           VALUE(  -6, -17), VALUE(  11,  -9), VALUE(  10,  -6), VALUE(   6,   4), VALUE(   4,   6), VALUE(  25, -11), VALUE(  19, -17), VALUE(   3, -16),
           VALUE(  24, -29), VALUE(  13, -25), VALUE(  10, -19), VALUE(   0, -11), VALUE(  13, -17), VALUE(  24, -20), VALUE(  34, -29), VALUE(  15, -53),
           VALUE(   0, -27), VALUE(   6, -23), VALUE(  -4, -13), VALUE(   2, -17), VALUE(   2, -14), VALUE(  -8,  -1), VALUE( -17, -30), VALUE(   5, -46)  },
        {  VALUE( -19,  26), VALUE(  -4,  28), VALUE( -10,  20), VALUE(  -2,  21), VALUE( -12,  23), VALUE( -35,  38), VALUE(   7,  23), VALUE(  20,  23),
           VALUE( -12,  36), VALUE( -18,  41), VALUE(  16,  30), VALUE(  29,  22), VALUE(  13,  17), VALUE(  56,  15), VALUE( -16,  37), VALUE(   9,  28),
           VALUE( -28,  38), VALUE(  -5,  30), VALUE( -19,  33), VALUE(  -2,  28), VALUE(  38,   6), VALUE(  26,  20), VALUE(  19,  15), VALUE( -15,  26),
           VALUE( -23,  34), VALUE( -29,  34), VALUE(   0,  21), VALUE(  -1,  28), VALUE(   5,  22), VALUE(  13,  19), VALUE( -23,  30), VALUE(   3,  16),
           VALUE( -33,  33), VALUE( -42,  39), VALUE( -20,  25), VALUE( -11,  23), VALUE(  -9,  19), VALUE(  -2,  14), VALUE(  12,  10), VALUE( -23,  11),
           VALUE( -34,  22), VALUE( -20,  15), VALUE( -30,  29), VALUE( -10,   2), VALUE( -13,   8), VALUE( -12,   0), VALUE(   6,  -2), VALUE( -27,   8),
           VALUE( -32,   6), VALUE( -30,  10), VALUE( -19,  15), VALUE(  -9,   8), VALUE(  -9,   6), VALUE(   4,  -3), VALUE(  -6, -11), VALUE( -74,  11),
           VALUE( -10,   8), VALUE(  -8,   0), VALUE(  -2,   4), VALUE(  16, -11), VALUE(  11,  -3), VALUE(  15,  -4), VALUE( -21,   5), VALUE(  -9, -17)  },
        {  VALUE( -72,  59), VALUE( -60,  58), VALUE( -72,  83), VALUE( -76,  59), VALUE( -48,  42), VALUE(  14,  55), VALUE( -47,  72), VALUE( -16,  65),
           VALUE( -52,  49), VALUE( -41,  43), VALUE( -15,  50), VALUE( -34,  65), VALUE( -30,  50), VALUE(  18,  36), VALUE(  10,  26), VALUE(  27,  18),
           VALUE( -35,  30), VALUE(  -9,  30), VALUE( -29,  47), VALUE(  -9,  53), VALUE(  -5,  47), VALUE(  39,  47), VALUE(  42, -10), VALUE(  13,  20),
           VALUE( -28,  47), VALUE( -22,  52), VALUE( -17,  61), VALUE( -30,  76), VALUE(  -8,  57), VALUE(   9,  37), VALUE(  -1,  61), VALUE(   3,  30),
           VALUE( -11,  17), VALUE( -10,  33), VALUE( -12,  29), VALUE( -13,  49), VALUE(  11,  28), VALUE( -10,  45), VALUE(  15,  32), VALUE(   5,  20),
           VALUE( -12,   7), VALUE(  -1,   8), VALUE(  -6,  29), VALUE(  -5,   8), VALUE(  -8,  33), VALUE(  -4,  45), VALUE(  17,  -2), VALUE(  -6,  -4),
           VALUE( -13,  -7), VALUE(  -1, -11), VALUE(   8, -28), VALUE(  15, -17), VALUE(  12,  -2), VALUE(  34, -52), VALUE(  25, -64), VALUE(  16, -72),
           VALUE(  -8, -19), VALUE(   2, -37), VALUE(  10, -28), VALUE(  14, -16), VALUE(  10,  -4), VALUE( -11, -14), VALUE( -30, -21), VALUE( -13, -34)  },
        {  VALUE( 189, -89), VALUE( 116, -49), VALUE(  62, -42), VALUE(-150,  15), VALUE( 224, -88), VALUE( -51,  -4), VALUE( 297,-126), VALUE(  51, -59),
           VALUE(   9,   0), VALUE(   6,  20), VALUE(   7,   2), VALUE(  14,   5), VALUE( -40,  21), VALUE(  60,  13), VALUE(  67,  18), VALUE(  57, -10),
           VALUE(  35,   1), VALUE(  -6,  32), VALUE(  36,  27), VALUE(   1,  27), VALUE(  18,  22), VALUE(  47,  27), VALUE(  86,  16), VALUE(  35,   1),
           VALUE( -45,   0), VALUE(   6,  16), VALUE( -70,  35), VALUE( -19,  32), VALUE( -62,  35), VALUE( -10,  32), VALUE(  20,  12), VALUE( -87,  13),
           VALUE( -46,  -6), VALUE(  32,   0), VALUE( -22,  20), VALUE( -83,  46), VALUE( -64,  46), VALUE( -53,  26), VALUE( -10,   7), VALUE( -97,  -2),
           VALUE( -25, -15), VALUE( -13,   2), VALUE( -30,  18), VALUE( -46,  34), VALUE( -52,  40), VALUE( -40,  23), VALUE( -14,   2), VALUE( -44,  -9),
           VALUE(  43, -32), VALUE(  38, -19), VALUE(  16,   1), VALUE( -53,  24), VALUE( -35,  24), VALUE( -10,  11), VALUE(  17,  -3), VALUE(  19, -25),
           VALUE(  24, -62), VALUE(  52, -54), VALUE(  28, -30), VALUE( -57,  -7), VALUE(  -2, -35), VALUE( -44,  -5), VALUE(  17, -31), VALUE(   8, -63)  }
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
    U64 used;
    U64 size;
    U64 sizemask;
    //chessposition *pos;
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

extern zobrist zb;
extern transposition tp;


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
    int isCheck;
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

enum MoveType { QUIET = 1, CAPTURE = 2, PROMOTE = 4, TACTICAL = 6, ALL = 7, QUIETWITHCHECK = 9 };
enum RootsearchType { SinglePVSearch, MultiPVSearch };

template <MoveType Mt> int CreateMovelist(chessposition *pos, chessmove* m);

class chessposition
{
public:
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
    int isCheck;

    chessmovestack movestack[MAXMOVESEQUENCELENGTH];
    uint16_t excludemovestack[MAXMOVESEQUENCELENGTH];
    int16_t staticevalstack[MAXMOVESEQUENCELENGTH];
#ifdef STACKDEBUG
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
    bool isAttackedByMySlider(int index, U64 occ, int me);  // special simple version to detect giving check by removing blocker
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
    bool moveGivesCheck(uint32_t c);  // simple and imperfect as it doesn't handle special moves and cases (mainly to avoid pruning of important moves)
    bool moveIsPseudoLegal(uint32_t c);     // test if move is possible in current position
    uint32_t shortMove2FullMove(uint16_t c); // transfer movecode from tt to full move code without checking if pseudoLegal
    int getPositionValue();
    int getPawnAndKingValue(pawnhashentry **entry);
    int getValue();

    template <RootsearchType RT> int rootsearch(int alpha, int beta, int depth);
    int alphabeta(int alpha, int beta, int depth, bool nullmoveallowed);
    int getQuiescence(int alpha, int beta, int depth);

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
    U64 nodes;
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

