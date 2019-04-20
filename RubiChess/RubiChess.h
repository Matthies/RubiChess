/*
  RubiChess is a UCI chess playing engine by Andreas Matthies.

  RubiChess is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  RubiChess is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#define VERNUM "1.4-dev"

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

#ifdef EVALTUNE
#define PREFETCH(a) (void)(0)
#else
#define PREFETCH(a) _mm_prefetch((char*)(a), _MM_HINT_T0)
#endif
#else
#ifdef EVALTUNE
#define PREFETCH(a) (void)(0)
#else
#define PREFETCH(a) __builtin_prefetch(a)
#endif
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
#define RANK2(s) ((s) ? 0x00ff000000000000 : 0x000000000000ff00)
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
    // Powered by Laser games :-)
    eval ePawnstormblocked[4][5] = {
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  12, -16), VALUE(  28, -17), VALUE(  29, -15)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   8, -22), VALUE(  34, -22), VALUE(   3,  -5)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  18, -19), VALUE(  -6,   1), VALUE(  -8,   5)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE( -14,  -6), VALUE(  -9,   8), VALUE(   5,   0)  }
    };
    eval ePawnstormfree[4][5] = {
        {  VALUE( -21,  44), VALUE(  44,  57), VALUE( -30,  31), VALUE(  -8,   5), VALUE(  -1,   1)  },
        {  VALUE( -24,  55), VALUE( -36,  49), VALUE( -52,  18), VALUE(  -4,   5), VALUE(   6,   5)  },
        {  VALUE( -44,  46), VALUE( -25,  43), VALUE( -24,   9), VALUE(  -9,   4), VALUE(  -3,  11)  },
        {  VALUE(  33,  27), VALUE(  -9,  48), VALUE(  -9,  -3), VALUE( -18,   9), VALUE( -13,  11)  }
    };
    eval ePawnpushthreatbonus =  VALUE(  19,  13);
    eval eSafepawnattackbonus =  VALUE(  63,  28);
    eval eHangingpiecepenalty =  VALUE( -23, -36);
    eval eKingshieldbonus =  VALUE(  13,  -2);
    eval eTempo =  VALUE(  28,  26);
    eval ePassedpawnbonus[4][8] = {
        {  VALUE(   0,   0), VALUE(   8,   2), VALUE(  -2,   6), VALUE(   7,  22), VALUE(  33,  36), VALUE(  71,  72), VALUE(  47, 109), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -22,   1), VALUE( -12,  11), VALUE(  -8,  18), VALUE(  15,  24), VALUE(  33,  45), VALUE( -18,  30), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   3,   6), VALUE(   4,   9), VALUE(   8,  36), VALUE(  23,  87), VALUE(  66, 193), VALUE( 112, 290), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  10,   5), VALUE(  -5,  19), VALUE(   2,  33), VALUE(  12,  53), VALUE(  53,  74), VALUE(  51,  61), VALUE(   0,   0)  }
    };
    eval eAttackingpawnbonus[8] = {  VALUE(   0,   0), VALUE( -65, -62), VALUE( -29, -33), VALUE( -19,  -3), VALUE( -10,  10), VALUE(   9,  40), VALUE(   0,   0), VALUE(   0,   0)  };
    eval eIsolatedpawnpenalty =  VALUE( -14, -11);
    eval eDoublepawnpenalty =  VALUE( -10, -23);
    eval eConnectedbonus =  VALUE(   7,  -4);
    eval eBackwardpawnpenalty =  VALUE(  -9,  -9);
    eval eDoublebishopbonus =  VALUE(  61,  35);
    eval eMobilitybonus[4][28] = {
        {  VALUE(  17, -96), VALUE(  37, -28), VALUE(  49,  -9), VALUE(  54,   0), VALUE(  59,  10), VALUE(  65,  21), VALUE(  71,  15), VALUE(  75,  16),
           VALUE(  77,   6), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(  19, -66), VALUE(  29, -32), VALUE(  46,  -9), VALUE(  51,   1), VALUE(  63,  10), VALUE(  68,  16), VALUE(  72,  22), VALUE(  72,  29),
           VALUE(  73,  30), VALUE(  74,  32), VALUE(  74,  32), VALUE(  80,  23), VALUE(  75,  35), VALUE(  61,  15), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE( -58,  -3), VALUE(  14,  -4), VALUE(  28,  34), VALUE(  29,  40), VALUE(  31,  50), VALUE(  36,  52), VALUE(  37,  57), VALUE(  44,  61),
           VALUE(  44,  64), VALUE(  45,  75), VALUE(  50,  73), VALUE(  48,  77), VALUE(  43,  85), VALUE(  47,  83), VALUE(  43,  83), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(-4097,  83), VALUE(   5, -94), VALUE(  -5,  -7), VALUE(   1, -18), VALUE(  -2,  40), VALUE(   1,  72), VALUE(   3,  80), VALUE(   7,  87),
           VALUE(  10,  88), VALUE(  13, 100), VALUE(  14, 121), VALUE(  17, 123), VALUE(  17, 138), VALUE(  18, 138), VALUE(  16, 156), VALUE(  21, 156),
           VALUE(  21, 162), VALUE(  24, 155), VALUE(  25, 162), VALUE(  17, 179), VALUE(  46, 155), VALUE(  80, 143), VALUE(  59, 159), VALUE(  83, 142),
           VALUE(  88, 157), VALUE( 125, 124), VALUE( 139, 127), VALUE(  66, 166)  }
    };
    eval eSlideronfreefilebonus[2] = {  VALUE(  22,   8), VALUE(  43,   2)  };
    eval eMaterialvalue[7] = {  VALUE(   0,   0), VALUE( 100, 100), VALUE( 314, 314), VALUE( 314, 314), VALUE( 483, 483), VALUE( 913, 913), VALUE(32509,32509)  };
    eval eWeakkingringpenalty =  VALUE( -10,   4);
    eval eKingattackweight[7] = {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   6,   0), VALUE(   6,  -1), VALUE(   5,  -1), VALUE(   5,   4), VALUE(   0,   0)  };
    eval eSafecheckbonus[6] = {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  76,  20), VALUE(  20,  43), VALUE(  91,   1), VALUE(  26, 100)  };
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
           VALUE( 138,  66), VALUE(  88,  74), VALUE(  95,  43), VALUE( 121,   8), VALUE( 114,  12), VALUE( 113,  36), VALUE(  19,  65), VALUE(  75,  59),
           VALUE(  -6,  36), VALUE(   0,  30), VALUE(  -6,  11), VALUE(   2, -15), VALUE(  13, -14), VALUE(  21,  -7), VALUE(   2,  30), VALUE(  -2,  30),
           VALUE(  -8,  24), VALUE( -15,  19), VALUE(  -9,  -2), VALUE(   1, -20), VALUE(   0, -14), VALUE(  -5,  -5), VALUE( -11,  11), VALUE( -11,   9),
           VALUE( -21,  11), VALUE( -23,   9), VALUE(  -5, -12), VALUE(  -6, -16), VALUE(  -2, -14), VALUE(  -2,  -9), VALUE( -20,   1), VALUE( -28,   3),
           VALUE( -18,  -2), VALUE( -21,   3), VALUE(  -9,  -8), VALUE( -12,   0), VALUE(   2,  -1), VALUE( -11,   5), VALUE(   7, -14), VALUE( -17,  -7),
           VALUE( -25,   7), VALUE( -17,   2), VALUE( -23,   9), VALUE( -12,  12), VALUE( -15,  17), VALUE(   6,   3), VALUE(  12, -13), VALUE( -23, -14),
           VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999)  },
        {  VALUE(-123, -44), VALUE( -89,  -6), VALUE( -15, -37), VALUE( -46, -29), VALUE(   2, -42), VALUE(-110, -25), VALUE(-167, -22), VALUE( -57, -79),
           VALUE(   2, -25), VALUE(  16, -24), VALUE(  43, -41), VALUE(  59, -34), VALUE(  12, -14), VALUE(  86, -52), VALUE(  20, -16), VALUE(  61, -78),
           VALUE(  -8, -14), VALUE(  34, -27), VALUE(  67, -20), VALUE(  53, -12), VALUE( 109, -34), VALUE(  68, -30), VALUE(  62, -23), VALUE(  -2, -38),
           VALUE(  32, -19), VALUE(  44, -20), VALUE(  50,  -9), VALUE(  73,   0), VALUE(  50,  10), VALUE(  76,  -6), VALUE(  40,   6), VALUE(  60, -27),
           VALUE(  23, -30), VALUE(  25,  -6), VALUE(  51, -16), VALUE(  59,  -7), VALUE(  62,  -5), VALUE(  61, -13), VALUE(  61, -15), VALUE(  39, -26),
           VALUE(  -1, -48), VALUE(  16, -33), VALUE(  23, -35), VALUE(  34, -13), VALUE(  44, -15), VALUE(  38, -42), VALUE(  42, -44), VALUE(   9, -36),
           VALUE( -10, -47), VALUE(   1, -24), VALUE(  13, -39), VALUE(  26, -36), VALUE(  28, -33), VALUE(  26, -53), VALUE(  33, -30), VALUE(  16, -42),
           VALUE( -42, -41), VALUE(   8, -62), VALUE( -10, -29), VALUE(   8, -21), VALUE(  22, -39), VALUE(  14, -42), VALUE(  10, -45), VALUE( -15, -79)  },
        {  VALUE(  36, -16), VALUE( -55,   9), VALUE( -14, -20), VALUE( -52,   4), VALUE( -67,  14), VALUE( -85,  24), VALUE( -44,  -6), VALUE( -25, -15),
           VALUE( -44,  12), VALUE(  21,  -8), VALUE(  -2, -10), VALUE( -30,  -1), VALUE(  12,  -6), VALUE(  -9,  -5), VALUE(   1,   0), VALUE( -63,  -7),
           VALUE(  -3,  -6), VALUE(   5,  -4), VALUE(  18,   0), VALUE(  26,  -7), VALUE(  51, -13), VALUE(  40,   2), VALUE(   0,  17), VALUE(  17,   4),
           VALUE(   6,   4), VALUE(  30,  -2), VALUE(  22,  -1), VALUE(  41,  14), VALUE(  29,  19), VALUE(  26,   7), VALUE(  35,   5), VALUE(  -2,  -1),
           VALUE(  11,  -2), VALUE(  10,  -4), VALUE(  20,  13), VALUE(  38,  14), VALUE(  50,   8), VALUE(  19,   2), VALUE(  24,  -7), VALUE(  35, -20),
           VALUE(   9, -14), VALUE(  24,  -5), VALUE(  13,   2), VALUE(  15,   8), VALUE(  11,   8), VALUE(  29,  -2), VALUE(  27, -15), VALUE(  17,  -5),
           VALUE(  36, -18), VALUE(  18, -22), VALUE(  18, -16), VALUE(   6,  -8), VALUE(  14, -11), VALUE(  25, -11), VALUE(  40, -23), VALUE(  31, -38),
           VALUE(   9, -21), VALUE(  25, -16), VALUE(   5,  -7), VALUE(   1,  -2), VALUE(   9,  -4), VALUE(   0,   3), VALUE(   2, -30), VALUE(  23, -25)  },
        {  VALUE(  19,  27), VALUE(  14,  33), VALUE(   9,  29), VALUE(  19,  29), VALUE(  -2,  45), VALUE(  -8,  45), VALUE(  13,  40), VALUE(  29,  36),
           VALUE(  -1,  51), VALUE( -12,  59), VALUE(  17,  46), VALUE(  25,  37), VALUE(  11,  34), VALUE(  56,  35), VALUE( -10,  61), VALUE(  15,  47),
           VALUE( -14,  53), VALUE(   0,  44), VALUE(  -8,  47), VALUE(  20,  30), VALUE(  35,  27), VALUE(  28,  41), VALUE(  30,  32), VALUE( -22,  53),
           VALUE( -16,  49), VALUE( -21,  44), VALUE(   8,  35), VALUE(   3,  41), VALUE(  20,  30), VALUE(  20,  39), VALUE(  -5,  40), VALUE(   3,  33),
           VALUE( -28,  50), VALUE( -36,  44), VALUE( -13,  38), VALUE(  -3,  34), VALUE(  -6,  35), VALUE(  11,  28), VALUE(  25,  20), VALUE( -16,  25),
           VALUE( -28,  29), VALUE( -14,  27), VALUE( -21,  38), VALUE(  -7,  16), VALUE(  -4,  15), VALUE(  -2,  14), VALUE(  20,  11), VALUE( -15,  14),
           VALUE( -26,  20), VALUE( -19,  16), VALUE( -13,  20), VALUE(  -1,  14), VALUE(   5,   8), VALUE(  15,   2), VALUE(   8,   0), VALUE( -59,  23),
           VALUE(  -6,  14), VALUE(  -6,  14), VALUE(   0,  12), VALUE(  16,   0), VALUE(  18,   0), VALUE(  19,   7), VALUE( -11,  14), VALUE(   1,  -8)  },
        {  VALUE( -20,  49), VALUE( -11,  62), VALUE( -12,  92), VALUE(  -8,  59), VALUE(  -8,  65), VALUE(  71,  52), VALUE(   6,  82), VALUE(  57,  11),
           VALUE( -46,  81), VALUE( -39,  67), VALUE(   0,  63), VALUE(  -8,  78), VALUE( -37,  97), VALUE(   5,  74), VALUE(  -8,  66), VALUE(  31,  41),
           VALUE( -26,  64), VALUE(   6,  54), VALUE( -14,  63), VALUE(  -1,  80), VALUE(   7,  84), VALUE(  33,  74), VALUE(  31,  47), VALUE(  25,  52),
           VALUE(  -7,  54), VALUE(  -9,  53), VALUE(  -3,  71), VALUE( -21, 100), VALUE(  -6, 102), VALUE(  21,  68), VALUE(   3, 105), VALUE(  15,  58),
           VALUE(   2,  28), VALUE(  -1,  52), VALUE(   1,  46), VALUE(  -5,  72), VALUE(  18,  60), VALUE(  -4,  88), VALUE(  24,  80), VALUE(  10,  76),
           VALUE(  -8,  39), VALUE(   7,  31), VALUE(  -6,  68), VALUE(   0,  51), VALUE(  -3,  72), VALUE(   0,  92), VALUE(  28,  43), VALUE(   8,  40),
           VALUE(  -2,  10), VALUE(  10,  10), VALUE(  19, -19), VALUE(  19,   4), VALUE(  20,  14), VALUE(  33, -13), VALUE(  38, -30), VALUE(  30, -53),
           VALUE(   0,  -6), VALUE(  12, -26), VALUE(  13, -18), VALUE(  18,  -9), VALUE(  15,  20), VALUE(  -6,  20), VALUE( -18,  14), VALUE(  -6,  12)  },
        {  VALUE(  79, -41), VALUE( -28,  31), VALUE( -19,   2), VALUE(-116,  29), VALUE(  81, -39), VALUE(  19,  -7), VALUE(  98, -57), VALUE(  30, -91),
           VALUE( -54,  43), VALUE(  12,  33), VALUE(   5,  18), VALUE(   0,  36), VALUE( -31,  56), VALUE(  27,  57), VALUE(  24,  57), VALUE(  23,   7),
           VALUE(   0,  36), VALUE(  57,  26), VALUE(  56,  44), VALUE(  33,  43), VALUE(  20,  52), VALUE(  50,  51), VALUE(  46,  42), VALUE(  32,  11),
           VALUE( -20,   9), VALUE(   4,  34), VALUE( -16,  43), VALUE( -12,  56), VALUE( -24,  53), VALUE(  21,  53), VALUE(  35,  25), VALUE( -39,  -2),
           VALUE( -41,   1), VALUE(  26,  13), VALUE(  -9,  38), VALUE( -49,  61), VALUE( -16,  56), VALUE( -19,  38), VALUE(  17,  12), VALUE( -60, -10),
           VALUE( -25, -16), VALUE(   2,   4), VALUE(  -1,  23), VALUE( -22,  42), VALUE( -20,  44), VALUE(  -7,  26), VALUE(  19,  -3), VALUE( -33, -13),
           VALUE(  41, -38), VALUE(  38, -21), VALUE(  21,   5), VALUE( -48,  31), VALUE( -28,  30), VALUE( -10,  18), VALUE(  17,  -3), VALUE(  13, -30),
           VALUE(  15, -70), VALUE(  51, -63), VALUE(  24, -30), VALUE( -55,  -4), VALUE(  -2, -30), VALUE( -40,  -2), VALUE(  16, -35), VALUE(   8, -72)  }
    };
};

#ifdef EVALTUNE

struct evalparam {
    uint16_t index;
    int16_t g;
};

struct positiontuneset {
    uint8_t ph;
    uint8_t sc;
    uint16_t num;
    int8_t R;
    //int8_t padding[3];
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
bool PGNtoFEN(string pgnfilename, bool quietonly, int ppg);
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

extern int squareDistance[64][64];

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
    uint32_t movecode;
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
#define MAXTHREADS 128
#define CMPLIES 2


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
    void sort(const unsigned int refutetarget = BOARDSIZE);
    void sort(uint32_t hashmove, uint32_t killer1, uint32_t killer2);
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
    int32_t *cmptr[CMPLIES];

public:
    void SetPreferredMoves(chessposition *p);  // for quiescence move selector
    void SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, int nmrfttarget, int excludemove);
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
template <MoveType Mt> void evaluateMoves(chessmovelist *ml, chessposition *pos, int32_t **cmptr);

enum AttackType { FREE, OCCUPIED };

class chessposition
{
public:
    U64 nodes;
    U64 piece00[14];
    U64 occupied00[2];
    U64 attackedBy2[2];
    U64 attackedBy[2][7];

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
    uint32_t movecode;

    uint8_t mailbox[BOARDSIZE]; // redundand for faster "which piece is on field x"
    chessmovestack movestack[MAXMOVESEQUENCELENGTH];
    uint16_t excludemovestack[MAXMOVESEQUENCELENGTH];
    int16_t staticevalstack[MAXMOVESEQUENCELENGTH];
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
    int32_t counterhistory[14][64][14*64];
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
    chessmovelist captureslist[MAXDEPTH];
    chessmovelist quietslist[MAXDEPTH];
    chessmovelist singularcaptureslist[MAXDEPTH];   // extra move lists for singular testing
    chessmovelist singularquietslist[MAXDEPTH];
#ifdef EVALTUNE
    bool isQuiet;
    tuneparamselection tps;
    positiontuneset pts;
    evalparam ev[NUMOFEVALPARAMS];
    void resetTuner();
    void getPositionTuneSet(positiontuneset *p, evalparam *e);
    void copyPositionTuneSet(positiontuneset *from, evalparam *efrom, positiontuneset *to, evalparam *eto);
    string getGradientString();
    int getGradientValue(positiontuneset *p, evalparam *e);
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
    void updateCounterHistory(int32_t **cmptr, int pc, int to, int value);
    void getCmptr(int32_t **cmptr);

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
    int restSizeOfTp = 0;
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

