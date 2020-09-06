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

#define VERNUM "1.9"
//#define VERSTABLE

#if 0
#define STATISTICS
#endif

#if 0
#define SDEBUG
#endif

#if 0
#define TDEBUG
#endif

#if 0
#define EVALTUNE
#endif

#if 0
#define EVALOPTIONS
#endif

#if 0
#define FINDMEMORYLEAKS
#endif

#if 1
#define NNUE
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

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <iostream>
#include <iomanip>
#include <stdarg.h>
#include <time.h>
#include <string>
#include <iostream>
#include <fstream>
#include <list>
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

#define allocalign64(x) _aligned_malloc(x, 64)
#define freealigned64(x) _aligned_free(x)

#else //_WIN32

#define myassert(expression, pos, num, ...) (void)(0)
#define sprintf_s sprintf
void Sleep(long x);
#ifdef __ANDROID__
#define allocalign64(x) malloc(x)
#define freealigned64(x) free(x)
#else
#define allocalign64(x) aligned_alloc(64, x)
#define freealigned64(x) free(x)
#endif
#endif

using namespace std;

#ifdef _MSC_VER
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

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define CPUVENDORUNKNOWN    0
#define CPUVENDORINTEL      1
#define CPUVENDORAMD        2


#if defined(__clang_major__)
#define COMPILER "Clang " TOSTRING(__clang_major__)
#elif defined(__GNUC__)
#define COMPILER "GCC " TOSTRING(__GNUC__)
#elif defined(_MSC_VER)
#define COMPILER "MSVC " TOSTRING(_MSC_VER)
#else
#define COMPILER "unknown compiler"
#endif

#ifndef VERSTABLE
#ifdef GITVER
#define VERSION VERNUM "-dev " GITVER
#else
#define VERSION VERNUM "-dev"
#endif
#else
#define VERSION VERNUM " "
#endif
#define ENGINEVER "RubiChess " VERSION
#ifdef GITID
#define BUILD __DATE__ " " __TIME__ " commit " GITID " " COMPILER
#else
#define BUILD __DATE__ " " __TIME__ " " COMPILER
#endif

#define BITSET(x) (1ULL << (x))
#define MORETHANONE(x) ((x) & ((x) - 1)) 
#define ONEORZERO(x) (!MORETHANONE(x))
#if defined(_MSC_VER)
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
#define FLANKLEFT  0x0f0f0f0f0f0f0f0f
#define FLANKRIGHT 0xf0f0f0f0f0f0f0f0
#define CENTER 0x0000001818000000
#define OUTPOSTAREA(s) ((s) ? 0x000000ffffff0000 : 0x0000ffffff000000)
#define RANK(x) ((x) >> 3)
#define RRANK(x,s) ((s) ? ((x) >> 3) ^ 7 : ((x) >> 3))
#define FILE(x) ((x) & 0x7)
#define INDEX(r,f) (((r) << 3) | (f))
#define BORDERDIST(f) ((f) > 3 ? 7 - (f) : (f))
#define PROMOTERANK(x) (RANK(x) == 0 || RANK(x) == 7)
#define PROMOTERANKBB 0xff000000000000ff
#define RANK1(s) ((s) ? 0xff00000000000000 : 0x00000000000000ff)
#define RANK2(s) ((s) ? 0x00ff000000000000 : 0x000000000000ff00)
#define RANK3(s) ((s) ? 0x0000ff0000000000 : 0x0000000000ff0000)
#define RANK7(s) ((s) ? 0x000000000000ff00 : 0x00ff000000000000)
#define RANK8(s) ((s) ? 0x00000000000000ff : 0xff00000000000000)
#define FILEABB 0x0101010101010101
#define FILEHBB 0x8080808080808080
#define ISOUTERFILE(x) (FILE(x) == 0 || FILE(x) == 7)
#define ISNEIGHBOUR(x,y) ((x) >= 0 && (x) < 64 && (y) >= 0 && (y) < 64 && abs(RANK(x) - RANK(y)) <= 1 && abs(FILE(x) - FILE(y)) <= 1)

#define S2MSIGN(s) (s ? -1 : 1)


typedef unsigned long long U64;
typedef signed long long S64;

typedef unsigned int PieceCode;
typedef unsigned int PieceType;

// Forward definitions
class transposition;
class chessposition;
class searchthread;
struct pawnhashentry;


//
// eval stuff
//

#define GETMGVAL(v) ((int16_t)(((uint32_t)(v) + 0x8000) >> 16))
#define GETEGVAL(v) ((int16_t)((v) & 0xffff))

#ifdef EVALTUNE

class eval;
class sqevallist
{
public:
    sqevallist() {
        count = 0;
    }
    eval* ptr[1024];
    int count;
    void store(eval *s) { ptr[count++] = s; }
};

extern sqevallist sqglobal;

class eval {
public:
    int type;  // 0=linear mg->eg  1=constant  2=squre  3=only eg (0->eg)
    int groupindex;
    int32_t v;
    int g[2];
    eval() { v = g[0] = g[1] = 0; }
    eval(int m, int e) {
        v = ((int32_t)((uint32_t)(m) << 16) + (e)); g[0] = 0; type = 0;
    }
    eval(int t, int i, int x) { type = t; groupindex = i; v = x; sqglobal.store(this); }
    eval(int x) { v = (x); g[0] = 0; type = 1; }
    bool operator !=(const eval &x) { return this->type != x.type || this->v != x.v; }
    operator int() const { return v; }
    void addGrad(int i, int s = 0) { this->g[s] += i; }
    int getGrad(int s = 0) { return this->g[s]; }
    void resetGrad() { g[0] = g[1] = 0; }
    void replace(int i, int16_t b) { if (!i) v = ((int32_t)((uint32_t)GETMGVAL(v) << 16) + b); else v = ((int32_t)((uint32_t)b << 16) + GETEGVAL(v)); }
    void replace(int16_t b) { v = b; }
};


#define VALUE(m, e) eval(m, e)
#define EVAL(e, f) ((e).addGrad(f), (e) * (f))
#define SQVALUE(i, v) eval(2, i, v)
#define SQEVAL(e, f, s) ((e).addGrad(f, s), (e) * (f))
#define SQRESULT(v,s) ( v > 0 ? ((int32_t)((uint32_t)((((v) * (v)) & 0xffffff) * S2MSIGN(s) / 2048) << 16) + ((v) * S2MSIGN(s) / 16)) : 0 )
#define CVALUE(v) eval(v)
#define CEVAL(e, f) ((e).addGrad(f), (e) * (f))
#define EVALUE(v) eval(3, 0, v)
#define EEVAL(e, f) ((e).addGrad(f), (e) * (f))
#else // EVALTUNE

#define SQVALUE(i, v) (v)
#define VALUE(m, e) ((int32_t)((uint32_t)(m) << 16) + (e))
#define EVAL(e, f) ((e) * (f))
#define SQEVAL(e, f, s) ((e) * (f))
#define SQRESULT(v,s) ( v > 0 ? VALUE((((v) * (v)) & 0xffffff) * S2MSIGN(s) / 2048, (v) * S2MSIGN(s) / 16) : 0 )
#define CVALUE(v) (v)
#define CEVAL(e, f) ((e) * (f))
#define EVALUE(e) VALUE(0, e)
#define EEVAL(e, f) ((e) * (f))

#ifdef EVALOPTIONS
typedef int32_t eval;
#else
typedef const int32_t eval;
#endif

#endif

#define PSQTINDEX(i,s) ((s) ? (i) : (i) ^ 0x38)

#define TAPEREDANDSCALEDEVAL(s, p, c) ((GETMGVAL(s) * (256 - (p)) + GETEGVAL(s) * (p) * (c) / SCALE_NORMAL) / 256)

struct evalparamset {
    // Tuned with Lichess-quiet (psqt), lc0games (kingdanger), manually (complex) and Laser games (everything else)
    eval eComplexpawnsbonus =  EVALUE(   4);
    eval eComplexpawnflanksbonus =  EVALUE(  66);
    eval eComplexonlypawnsbonus =  EVALUE(  71);
    eval eComplexadjust =  EVALUE(-100);
    eval eTempo =  CVALUE(  20);
    eval eKingpinpenalty[6] = {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  38, -74), VALUE(  65, -61), VALUE( -29,  68), VALUE( -44, 163)  };
    eval ePawnstormblocked[4][5] = {
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  -6,  -4), VALUE(  26, -11), VALUE(  30, -11)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(  -3, -13), VALUE(  24, -19), VALUE(   8,  -8)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE(   6, -13), VALUE(  -9,   2), VALUE(  -5,   4)  },
        {  VALUE(   0,   0), VALUE(   0,   0), VALUE( -25,  -3), VALUE(  -9,   7), VALUE(   9,  -1)  }
    };
    eval ePawnstormfree[4][5] = {
        {  VALUE(   9,  46), VALUE(  37,  67), VALUE( -33,  37), VALUE( -11,   8), VALUE(  -3,   4)  },
        {  VALUE( -13,  64), VALUE( -32,  62), VALUE( -61,  31), VALUE( -10,   5), VALUE(   3,   3)  },
        {  VALUE( -28,  45), VALUE( -12,  46), VALUE( -27,  14), VALUE( -12,   3), VALUE(  -4,   8)  },
        {  VALUE(  31,  29), VALUE( -10,  57), VALUE( -19,   2), VALUE( -16,   5), VALUE( -13,  12)  }
    };
    eval ePawnpushthreatbonus =  VALUE(  20,  13);
    eval eSafepawnattackbonus =  VALUE(  66,  25);
    eval eHangingpiecepenalty =  VALUE( -23, -36);
    eval ePassedpawnbonus[4][8] = {
        {  VALUE(   0,   0), VALUE(  10,   4), VALUE(   0,   8), VALUE(  10,  25), VALUE(  35,  46), VALUE(  74,  81), VALUE(  44, 108), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -19,  -4), VALUE( -15,  12), VALUE(  -5,  17), VALUE(  17,  34), VALUE(  42,  60), VALUE( -12,  42), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   3,   7), VALUE(   6,  12), VALUE(  10,  41), VALUE(  26,  95), VALUE(  64, 197), VALUE( 105, 289), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   9,   7), VALUE(  -3,  19), VALUE(   1,  39), VALUE(  16,  54), VALUE(  58,  73), VALUE(  19,  69), VALUE(   0,   0)  }
    };
    eval eKingsupportspasserbonus[7][8] = {
        {  VALUE(   0,   0), VALUE(  13,   7), VALUE(  30,  -6), VALUE(  57,  -6), VALUE(  72, -13), VALUE( 138, -32), VALUE( 143, -23), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  18,   7), VALUE(  -8,   7), VALUE(  10,   0), VALUE(  18, -21), VALUE( 107, -73), VALUE( 151, -70), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  21,  -3), VALUE(  -7,   0), VALUE(  -6, -16), VALUE( -15, -41), VALUE(  37, -82), VALUE( 122,-120), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   9,   6), VALUE(  -7,   7), VALUE(  -5, -17), VALUE( -28, -45), VALUE( -23, -79), VALUE(  34,-111), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  10,   3), VALUE(  11, -13), VALUE(   6, -22), VALUE(  -6, -62), VALUE( -57, -73), VALUE(  12,-104), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   9,   7), VALUE(  19,  -6), VALUE(  37, -29), VALUE(  -1, -52), VALUE(   1, -99), VALUE( -39, -67), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -11,  18), VALUE( -30,  21), VALUE(   9, -27), VALUE( -10, -61), VALUE( -17, -92), VALUE( -39, -97), VALUE(   0,   0)  }
    };
    eval eKingdefendspasserpenalty[7][8] = {
        {  VALUE(   0,   0), VALUE(  62,   4), VALUE( -15,  49), VALUE(  10,  16), VALUE(  -5,  37), VALUE(  36,  18), VALUE(  23, -18), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  37,   4), VALUE(  10,  -1), VALUE(  12,   4), VALUE(  40,   0), VALUE(  45,   7), VALUE(  16,  53), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  38,  -3), VALUE(  11,   1), VALUE(  11,   2), VALUE(  28,  18), VALUE(  56,  69), VALUE(  29,  97), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  11,  -2), VALUE(   5,  -1), VALUE(   1,   5), VALUE(  21,  69), VALUE(  43, 118), VALUE(   3, 145), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   1, -13), VALUE(   0,  -3), VALUE(  -8,  40), VALUE(  14,  82), VALUE(  48, 121), VALUE(   9, 152), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -17,  -5), VALUE( -15,  15), VALUE(  -7,  31), VALUE(  13,  73), VALUE(   7, 143), VALUE(  34, 138), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -33,  -3), VALUE( -29,  18), VALUE( -11,  31), VALUE(   0,  70), VALUE(   6, 125), VALUE(  83,  77), VALUE(   0,   0)  }
    };
    eval ePotentialpassedpawnbonus[4][8] = {
        {  VALUE(   0,   0), VALUE(  35,  10), VALUE(   3,   3), VALUE(  14,   7), VALUE(  34,   7), VALUE(  92,  48), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  -2,   1), VALUE(  -2,   3), VALUE(   3,   0), VALUE(   3, -20), VALUE(  53,   6), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -12,  16), VALUE( -21,  -6), VALUE(   5,  34), VALUE(  45,  15), VALUE( 101,  80), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -43,   9), VALUE( -15,  17), VALUE(   0,  31), VALUE(  33,  31), VALUE(  71,  95), VALUE(   0,   0), VALUE(   0,   0)  }
    };
    eval eAttackingpawnbonus[8] = {  VALUE(   0,   0), VALUE( -32,  12), VALUE( -22, -12), VALUE(  -6,  -6), VALUE( -12,  -6), VALUE( -13,  -2), VALUE(   0,   0), VALUE(   0,   0)  };
    eval eIsolatedpawnpenalty[2][8] = {
        {  VALUE( -15,  -1), VALUE( -19,   6), VALUE( -23, -10), VALUE( -28, -14), VALUE( -34, -12), VALUE( -22, -13), VALUE( -21,   1), VALUE( -20,   6)  },
        {  VALUE( -10,  -5), VALUE( -11,  -7), VALUE( -15, -10), VALUE( -13,  -7), VALUE( -17,  -3), VALUE(  -7, -12), VALUE(  -4, -15), VALUE( -14,  -4)  }
    };
    eval eDoublepawnpenalty =  VALUE( -11, -23);
    eval eConnectedbonus[6][6] = {
        {  VALUE(   0,   0), VALUE(  10,  -2), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   4,   3), VALUE(  12,  12), VALUE(  28,  17), VALUE(  36,  20), VALUE(  56,  22)  },
        {  VALUE(   0,   0), VALUE(  15,   4), VALUE(  16,   8), VALUE(  26,  16), VALUE(  26,   9), VALUE( -10,  14)  },
        {  VALUE(   0,   0), VALUE(  21,  29), VALUE(  21,  16), VALUE(  44,  34), VALUE(  33,  17), VALUE( 117, -57)  },
        {  VALUE(   0,   0), VALUE(  72,  98), VALUE(  53,  54), VALUE(  72,  80), VALUE(  35,  86), VALUE( -57, 253)  },
        {  VALUE(   0,   0), VALUE(  38, 241), VALUE( 130, 110), VALUE(   7, 400), VALUE(   0, 641), VALUE(   0,   0)  }
    };
    eval eBackwardpawnpenalty[2][8] = {
        {  VALUE(   1, -19), VALUE( -18, -15), VALUE( -27, -11), VALUE( -39, -11), VALUE( -45, -10), VALUE( -38,  -6), VALUE( -30,  -7), VALUE(  -9,  -2)  },
        {  VALUE(  -2,  -5), VALUE(  -3,  -5), VALUE( -12,  -5), VALUE(  -2,  -9), VALUE(  -4,  -6), VALUE(  -9,  -8), VALUE( -11,  -7), VALUE( -14,  -4)  }
    };
    eval eDoublebishopbonus =  VALUE(  56,  38);
    eval ePawnblocksbishoppenalty =  VALUE( -10, -18);
    eval eBishopcentercontrolbonus =  VALUE(  25,  13);
    eval eKnightOutpost =  VALUE(   15,  15);
    eval eMobilitybonus[4][28] = {
        {  VALUE(  16, -90), VALUE(  38, -26), VALUE(  51,   1), VALUE(  57,  13), VALUE(  64,  27), VALUE(  71,  37), VALUE(  77,  36), VALUE(  84,  36),
           VALUE(  86,  30), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(  -8, -25), VALUE(  23,  -7), VALUE(  45,   2), VALUE(  55,  21), VALUE(  65,  31), VALUE(  74,  35), VALUE(  80,  43), VALUE(  81,  50),
           VALUE(  86,  51), VALUE(  85,  58), VALUE(  86,  52), VALUE(  95,  55), VALUE( 100,  54), VALUE(  89,  60), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE( -57,  11), VALUE(  14,  17), VALUE(  31,  57), VALUE(  34,  70), VALUE(  36,  82), VALUE(  38,  90), VALUE(  40,  92), VALUE(  46,  96),
           VALUE(  47, 101), VALUE(  50, 111), VALUE(  54, 109), VALUE(  50, 115), VALUE(  49, 118), VALUE(  50, 116), VALUE(  44, 116), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(   0,-200), VALUE(  13,-157), VALUE(  -2,  31), VALUE(   3,  75), VALUE(   5,  94), VALUE(   4, 152), VALUE(   6, 161), VALUE(  11, 177),
           VALUE(  10, 192), VALUE(  15, 187), VALUE(  14, 212), VALUE(  17, 213), VALUE(  16, 227), VALUE(  18, 229), VALUE(  17, 242), VALUE(  19, 251),
           VALUE(  20, 253), VALUE(  21, 256), VALUE(  18, 263), VALUE(  13, 270), VALUE(  45, 244), VALUE(  67, 233), VALUE(  72, 241), VALUE(  80, 230),
           VALUE(  69, 264), VALUE( 108, 231), VALUE( 107, 230), VALUE( 114, 198)  }
    };
    eval eRookon7thbonus =  VALUE(  -1,  22);
    eval eRookonkingarea =  VALUE(   7,  -6);
    eval eBishoponkingarea =  VALUE(  10,   2);
    eval eQueenattackedbysliderpenalty =  VALUE( -30,  17);
    eval eMinorbehindpawn[6] = {  VALUE(   1,  14), VALUE(  12,  10), VALUE(  15,  11), VALUE(  24,   9), VALUE(  37,  11), VALUE(  89, 110)  };
    eval eSlideronfreefilebonus[2] = {  VALUE(  21,   7), VALUE(  43,   1)  };
    eval eMaterialvalue[7] = {  VALUE(   0,   0), VALUE( 100, 100), VALUE( 314, 314), VALUE( 314, 314), VALUE( 483, 483), VALUE( 913, 913), VALUE(   0,   0)  };
    eval eKingshieldbonus =  VALUE(  15,  -2);
    eval eWeakkingringpenalty =  SQVALUE(   1,  70);
    eval eKingattackweight[7] = {  SQVALUE(   1,   0), SQVALUE(   1,   0), SQVALUE(   1,  25), SQVALUE(   1,  11), SQVALUE(   1,  15), SQVALUE(   1,  42), SQVALUE(   1,   0)  };
    eval eSafecheckbonus[6] = {  SQVALUE(   1,   0), SQVALUE(   1,   0), SQVALUE(   1, 282), SQVALUE(   1,  55), SQVALUE(   1, 244), SQVALUE(   1, 210)  };
    eval eKingdangerbyqueen =  SQVALUE(   1,-163);
    eval eKingringattack[6] = {  SQVALUE(   1, 111), SQVALUE(   1,   0), SQVALUE(   1,  31), SQVALUE(   1,   0), SQVALUE(   1,   0), SQVALUE(   1, -15)  };
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
           VALUE( 147,  76), VALUE(  63,  95), VALUE( 122,  60), VALUE( 144,  41), VALUE( 125,  63), VALUE(  91,  79), VALUE(  26,  98), VALUE(  41, 108),
           VALUE( -17,  46), VALUE( -25,  39), VALUE(  -5,  16), VALUE(  -4,   1), VALUE(   6,   8), VALUE(  56,  10), VALUE(  -7,  45), VALUE( -23,  53),
           VALUE( -27,  39), VALUE( -23,  26), VALUE( -16,   9), VALUE( -15,   4), VALUE(   9,  -1), VALUE(   0,   2), VALUE( -19,  18), VALUE( -19,  21),
           VALUE( -31,  24), VALUE( -34,  20), VALUE( -17,  10), VALUE(  -5,   8), VALUE(   0,   7), VALUE(  -8,  10), VALUE( -22,  13), VALUE( -26,   9),
           VALUE( -27,  18), VALUE( -32,  13), VALUE( -15,   7), VALUE( -14,  12), VALUE(   0,  16), VALUE( -17,  10), VALUE( -12,   3), VALUE( -21,   5),
           VALUE( -28,  28), VALUE( -29,  22), VALUE( -21,  19), VALUE( -25,  16), VALUE( -14,  32), VALUE(   6,  14), VALUE(   2,   9), VALUE( -27,   9),
           VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999)  },
        {  VALUE(-135,  18), VALUE( -53,  45), VALUE( -56,  47), VALUE( -37,  44), VALUE(   4,  39), VALUE( -85,  28), VALUE(-131,  36), VALUE( -84, -38),
           VALUE( -31,  30), VALUE( -15,  50), VALUE(  19,  49), VALUE(  63,  30), VALUE(  16,  29), VALUE(  85,  13), VALUE(   9,  31), VALUE(  -8,  14),
           VALUE(  -1,  40), VALUE(  44,  44), VALUE(  42,  57), VALUE(  61,  57), VALUE(  94,  33), VALUE(  83,  30), VALUE(  64,  27), VALUE(  37,  18),
           VALUE(  13,  62), VALUE(  38,  50), VALUE(  61,  59), VALUE(  90,  53), VALUE(  66,  63), VALUE(  86,  51), VALUE(  40,  46), VALUE(  55,  33),
           VALUE(  16,  49), VALUE(  35,  52), VALUE(  49,  58), VALUE(  46,  74), VALUE(  60,  69), VALUE(  58,  56), VALUE(  64,  51), VALUE(  35,  55),
           VALUE( -27,  39), VALUE(  -1,  35), VALUE(  20,  35), VALUE(  17,  56), VALUE(  30,  50), VALUE(  30,  34), VALUE(  33,  30), VALUE(   6,  39),
           VALUE( -32,  54), VALUE( -19,  27), VALUE( -11,  40), VALUE(  15,  38), VALUE(  18,  33), VALUE(  15,  29), VALUE(   7,  43), VALUE(   1,  59),
           VALUE( -64,  50), VALUE(  -7,  41), VALUE( -24,  45), VALUE(   4,  48), VALUE(  12,  41), VALUE(  11,  28), VALUE(  -5,  42), VALUE( -32,  42)  },
        {  VALUE(  -8,  56), VALUE( -49,  95), VALUE(  32,  75), VALUE( -29,  93), VALUE( -42,  83), VALUE( -13,  79), VALUE( -21,  54), VALUE( -37,  52),
           VALUE(  -5,  61), VALUE( -18,  53), VALUE(   6,  74), VALUE(  31,  68), VALUE(  22,  64), VALUE(   3,  66), VALUE( -28,  43), VALUE( -21,  60),
           VALUE(  29,  67), VALUE(  36,  76), VALUE(  24,  55), VALUE(  64,  50), VALUE(  40,  57), VALUE(  72,  53), VALUE(  40,  67), VALUE(  67,  55),
           VALUE(  16,  80), VALUE(  44,  77), VALUE(  53,  68), VALUE(  62,  75), VALUE(  74,  66), VALUE(  44,  75), VALUE(  64,  65), VALUE(  14,  72),
           VALUE(  26,  72), VALUE(  24,  75), VALUE(  44,  75), VALUE(  60,  72), VALUE(  53,  73), VALUE(  50,  69), VALUE(  42,  59), VALUE(  68,  46),
           VALUE(  26,  46), VALUE(  49,  72), VALUE(  14,  57), VALUE(  32,  68), VALUE(  38,  75), VALUE(  22,  53), VALUE(  54,  54), VALUE(  40,  56),
           VALUE(  36,  52), VALUE(  17,  33), VALUE(  36,  42), VALUE(  17,  62), VALUE(  26,  60), VALUE(  45,  45), VALUE(  36,  37), VALUE(  44,  40),
           VALUE(  21,  31), VALUE(  48,  36), VALUE(  37,  53), VALUE(  15,  57), VALUE(  33,  63), VALUE(  16,  68), VALUE(  34,  55), VALUE(  31,  24)  },
        {  VALUE(  36, 113), VALUE( -12, 136), VALUE(  12, 131), VALUE(  12, 136), VALUE(   2, 123), VALUE(  20, 128), VALUE(  22, 121), VALUE(  54, 109),
           VALUE(   5, 114), VALUE(  -8, 125), VALUE(  10, 129), VALUE(  23, 120), VALUE(  15, 113), VALUE(  14, 114), VALUE(   9, 114), VALUE(  37, 102),
           VALUE(  -7, 141), VALUE(  21, 133), VALUE(  17, 138), VALUE(  21, 120), VALUE(  64, 103), VALUE(  64, 105), VALUE(  83, 106), VALUE(  47, 108),
           VALUE(  -7, 136), VALUE(   6, 138), VALUE(  18, 132), VALUE(  13, 127), VALUE(  22, 114), VALUE(  31, 113), VALUE(  33, 114), VALUE(  30, 111),
           VALUE(  -7, 119), VALUE( -15, 122), VALUE(  -8, 127), VALUE(   4, 116), VALUE(   1, 116), VALUE(  -7, 122), VALUE(  33,  96), VALUE(  16, 101),
           VALUE( -13, 108), VALUE( -11,  97), VALUE( -12, 113), VALUE(  -4, 109), VALUE(  10,  96), VALUE(  14,  93), VALUE(  47,  70), VALUE(  16,  82),
           VALUE(  -9, 101), VALUE( -15, 104), VALUE(   1, 104), VALUE(   1, 100), VALUE(  11,  95), VALUE(  20,  85), VALUE(  29,  79), VALUE(   4,  87),
           VALUE(   5, 102), VALUE(   0,  96), VALUE(   3,  97), VALUE(  10,  91), VALUE(  17,  83), VALUE(  19,  92), VALUE(  29,  82), VALUE(   7,  96)  },
        {  VALUE(  17, 139), VALUE( -31, 195), VALUE(  17, 178), VALUE(  47, 170), VALUE(  11, 205), VALUE(   6, 208), VALUE(   3, 217), VALUE( -11, 183),
           VALUE( -29, 208), VALUE( -54, 232), VALUE( -59, 269), VALUE( -40, 258), VALUE( -60, 277), VALUE( -45, 256), VALUE( -73, 301), VALUE(  22, 210),
           VALUE(  -7, 231), VALUE( -12, 234), VALUE( -19, 272), VALUE(  -4, 273), VALUE( -16, 279), VALUE(  -9, 265), VALUE( -22, 268), VALUE(   9, 230),
           VALUE( -12, 253), VALUE(   0, 261), VALUE(  -7, 253), VALUE( -17, 272), VALUE( -26, 292), VALUE(   6, 254), VALUE(  10, 293), VALUE(  14, 247),
           VALUE(  -2, 244), VALUE( -10, 265), VALUE(  -1, 243), VALUE(   1, 245), VALUE(   7, 235), VALUE(   7, 249), VALUE(  25, 238), VALUE(  22, 243),
           VALUE( -12, 234), VALUE(   7, 230), VALUE(  -4, 241), VALUE(  -2, 232), VALUE(   9, 227), VALUE(  14, 229), VALUE(  27, 234), VALUE(  16, 199),
           VALUE(  -4, 208), VALUE(  -2, 212), VALUE(  11, 206), VALUE(  16, 211), VALUE(  15, 215), VALUE(  18, 194), VALUE(  24, 178), VALUE(  38, 161),
           VALUE( -15, 208), VALUE( -15, 204), VALUE( -11, 223), VALUE(   8, 187), VALUE(  -2, 209), VALUE( -29, 222), VALUE(   8, 195), VALUE(  -7, 227)  },
        {  VALUE(-116,-112), VALUE(  22, -21), VALUE( -13,  20), VALUE( -84,  45), VALUE(-171,  79), VALUE(-115,  85), VALUE( -23,  -7), VALUE( -99, -75),
           VALUE(-107,  24), VALUE( -11,  61), VALUE( -23,  93), VALUE(-103,  87), VALUE(-108,  92), VALUE( -38,  78), VALUE(  -6,  73), VALUE(-141,  24),
           VALUE(-178,  28), VALUE( -20,  51), VALUE( -43,  67), VALUE( -69,  85), VALUE( -36,  81), VALUE( -10,  64), VALUE( -52,  61), VALUE(-131,  33),
           VALUE(-156,  13), VALUE( -33,  31), VALUE( -72,  59), VALUE( -69,  75), VALUE( -37,  71), VALUE( -32,  55), VALUE( -50,  41), VALUE(-158,  22),
           VALUE(-134,   6), VALUE( -81,  27), VALUE( -38,  46), VALUE( -85,  63), VALUE( -53,  54), VALUE( -54,  43), VALUE( -67,  22), VALUE(-160,   2),
           VALUE( -55, -23), VALUE( -27,   1), VALUE( -50,  25), VALUE( -58,  41), VALUE( -59,  44), VALUE( -42,  24), VALUE( -30,   4), VALUE( -61,  -7),
           VALUE(  34, -43), VALUE(   3,  -9), VALUE( -19,  13), VALUE( -43,  17), VALUE( -49,  20), VALUE( -29,  15), VALUE(   3,  -4), VALUE(   9, -28),
           VALUE(  16, -73), VALUE(  45, -54), VALUE(  28, -23), VALUE( -70,   2), VALUE( -11, -16), VALUE( -45,   1), VALUE(  18, -31), VALUE(  24, -67)  }
    };
};

#ifdef EVALTUNE

#define NUMOFEVALPARAMS (sizeof(evalparamset) / sizeof(eval))

struct evalparam {
    uint16_t index;
    int16_t g[2];
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
    U64 used[NUMOFEVALPARAMS];

    int count;
};

struct tuner {
    thread thr;
    int index;
    int paramindex;
    eval ev[NUMOFEVALPARAMS];
    int paramcount;
    double starterror;
    double error;
    bool busy = false;
};

struct tunerpool {
    int lowRunning;
    int highRunning;
    int lastImproved;
    tuner *tn;
};

#endif

void registerallevals(chessposition *pos = nullptr);
void initPsqtable();

#define SCALE_NORMAL 128
#define SCALE_DRAW 0
#define SCALE_ONEPAWN 48
#define SCALE_HARDTOWIN 10
#define SCALE_OCB 32

enum EvalType { NOTRACE, TRACE };

//
// utils stuff
//
U64 calc_key_from_pcs(int *pcs, int mirror);
void getPcsFromStr(const char* str, int *pcs);
void getFenAndBmFromEpd(string input, string *fen, string *bm, string *am);
vector<string> SplitString(const char* s);
unsigned char AlgebraicToIndex(string s);
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
void TexelTune(string fenfilename, bool noqs, bool bOptimizeK, string correlation);

extern int tuningratio;

#endif

//
// NNUE stuff
//

#define NNUEFILEVERSION     0x7AF32F16u
#define NNUENETLAYERHASH    0xCC03DAE4u
#define NNUECLIPPEDRELUHASH 0x538D24C7u
#define NNUEFEATUREHASH     (0x5D69D5B9u ^ true)
#define NNUEINPUTSLICEHASH  0xEC42E90Du

#define ORIENT(c,i) ((c) ? (i) ^ 0x3f : (i))
#define MAKEINDEX(c,s,p,k) (ORIENT(c, s) + PieceToIndex[c][p] + PS_END * (k))


const int NnueFtHalfdims = 256;
const int NnueFtOutputdims = NnueFtHalfdims * 2;
const int NnueFtInputdims = 64 * 641;
const int NnueClippingShift = 6;
const int NnueValueScale = 16;

#if (defined(USE_SSE2) || defined(USE_MMX)) && !defined(USE_SSSE3)
// FIXME: This is not implemented yet
typedef int16_t clipped_t;
typedef int16_t weight_t;
#else
typedef int8_t weight_t;
typedef int8_t clipped_t;
#endif

// All pieces besides kings are inputs => 30 dimensions
typedef struct {
    size_t size;
    unsigned values[30];
} NnueIndexList;

typedef struct {
    int dirtyNum;
    PieceCode pc[3];
    int from[3];
    int to[3];
} DirtyPiece;


extern bool NnueReady;


class NnueLayer
{
public:
    NnueLayer* previous;

    NnueLayer(NnueLayer* prev) { previous = prev; }
    virtual bool ReadWeights(ifstream* is) = 0;
    virtual uint32_t GetHash() = 0;
};

class NnueFeatureTransformer : public NnueLayer
{
public:
    int16_t* bias;
    int16_t* weight;

    NnueFeatureTransformer();
    virtual ~NnueFeatureTransformer();
    bool ReadWeights(ifstream* is);
    uint32_t GetHash();
};

class NnueClippedRelu : public NnueLayer
{
public:
    int dims;
    NnueClippedRelu(NnueLayer* prev, int d);
    virtual ~NnueClippedRelu() {};
    bool ReadWeights(ifstream* is);
    uint32_t GetHash();
    void Propagate(int32_t *input, clipped_t *output);
};

class NnueInputSlice : public NnueLayer
{
public:
    const int outputdims = 512;

    NnueInputSlice();
    virtual ~NnueInputSlice() {};
    bool ReadWeights(ifstream* is);
    uint32_t GetHash();
};

class NnueNetworkLayer : public NnueLayer
{
public:
    int inputdims;
    int outputdims;

    int32_t* bias;
    int8_t* weight;

    NnueNetworkLayer(NnueLayer* prev, int id, int od);
    virtual ~NnueNetworkLayer();
    bool ReadWeights(ifstream* is);
    uint32_t GetHash();
    void Propagate(clipped_t *input, int32_t *output);
};

class NnueAccumulator
{
public:
    alignas(64) int16_t accumulation[2][256];
    int score;
    int computationState;
};


void NnueInit();
void NnueRemove();
void NnueReadNet(string path);



//
// transposition stuff
//
typedef unsigned long long u8;
typedef struct ranctx { u8 a; u8 b; u8 c; u8 d; } ranctx;

#define rot(x,k) (((x)<<(k))|((x)>>(64-(k))))

#define BOUNDMASK   0x03 
#define HASHALPHA   0x01
#define HASHBETA    0x02
#define HASHEXACT   0x00

class zobrist
{
public:
    unsigned long long boardtable[64 * 16];
    unsigned long long cstl[32];
    unsigned long long ept[64];
    unsigned long long s2m;
    ranctx rnd;
    zobrist();
    unsigned long long getRnd();
    u8 getHash(chessposition *pos);
    u8 getPawnHash(chessposition *pos);
    u8 getMaterialHash(chessposition *pos);
};

#define TTBUCKETNUM 3

typedef uint16_t hashupper_t;
#define GETHASHUPPER(x) (hashupper_t)((x) >> (64 - sizeof(hashupper_t) * 8))

struct transpositionentry {
    hashupper_t hashupper;
    uint16_t movecode;
    int16_t value;
    int16_t staticeval;
    uint8_t depth;
    uint8_t boundAndAge;
};

struct transpositioncluster {
    transpositionentry entry[TTBUCKETNUM];
    uint8_t padding[(64 - sizeof(transpositionentry) * TTBUCKETNUM) % 16];
#ifdef SDEBUG
    U64 debugHash;
    int debugIndex;
    string debugStoredBy;
#endif
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
#ifdef SDEBUG
    void markDebugSlot(U64 h, int i) {
        table[h & sizemask].debugHash = h; table[h & sizemask].debugIndex = i;
    }
    int isDebugPosition(U64 h) { return (h != table[h & sizemask].debugHash) ? -1 : table[h & sizemask].debugIndex; }
    string debugGetPv(U64 h) {
        transpositioncluster* data = &table[h & sizemask];
        for (int i = 0; i < TTBUCKETNUM; i++)
        {
            transpositionentry *e = &(data->entry[i]);
            if (e->hashupper == GETHASHUPPER(h))
                return "Depth=" + to_string(e->depth) + " Value=" + to_string(e->value) + "(" + to_string(e->boundAndAge & BOUNDMASK) + ")  pv=" + data->debugStoredBy;
        }
        return "";
    }
    void debugSetPv(U64 h, string s) {
        U64 i = h & sizemask;
        if (table[i].debugHash != h) return;
        table[i].debugStoredBy = s;
    }
#endif
};


typedef struct pawnhashentry {
    uint32_t hashupper;
    int32_t value;
    U64 passedpawnbb[2];
    U64 attacked[2];
    U64 attackedBy2[2];
    bool bothFlanks;
    unsigned char semiopen[2];
} S_PAWNHASHENTRY;


class Pawnhash
{
public:
    S_PAWNHASHENTRY *table;
    U64 sizemask;
    void setSize(int sizeMb);
    void remove();
    bool probeHash(U64 hash, pawnhashentry **entry);
};


#define MATERIALHASHSIZE 0x10000
#define MATERIALHASHMASK (MATERIALHASHSIZE - 1)


struct Materialhashentry {
    U64 hash;
    int scale[2];
    bool onlyPawns;
    int numOfPawns;
    int(*endgame)(chessposition*);
};


class Materialhash
{
public:
    Materialhashentry *table;
    void init();
    void remove();
    bool probeHash(U64 hash, Materialhashentry **entry);
};


extern zobrist zb;
extern transposition tp;


//
// board stuff
//

#define STARTFEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

#define BOARDSIZE 64
#define RANKMASK 0x38

#define BUFSIZE 4096

#define BLANKTYPE 0
#define PAWN 1
#define KNIGHT 2
#define BISHOP 3
#define ROOK 4
#define QUEEN 5
#define KING 6

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

// My son wants this in binary :-) but -pendantic claims that it's not C11 standard :-(
#define S2MMASK     0x01
#define WQCMASK     0x02
#define WKCMASK     0x04
#define BQCMASK     0x08
#define BKCMASK     0x10
#define CASTLEMASK  0x1e
#define GETCASTLEFILE(s,i) (((s) >> (i * 4 + 8)) & 0x7)
#define SETCASTLEFILE(f,i) (((f) << (i * 4 + 8)) | (WQCMASK << i))

#define WQC 1
#define WKC 2
#define BQC 3
#define BKC 4
const int QCMASK[2] = { WQCMASK, BQCMASK };
const int KCMASK[2] = { WKCMASK, BKCMASK };
const int castlerookto[4] = { 3, 5, 59, 61 };
const int castlekingto[4] = { 2, 6, 58, 62 };

#define MAXDEPTH 256
#define MOVESTACKRESERVE 48     // to avoid checking for height reaching MAXDEPTH in probe_wds and getQuiescence

#define NOSCORE SHRT_MIN
#define SCOREBLACKWINS (SHRT_MIN + 3 + 2 * MAXDEPTH)
#define SCOREWHITEWINS (-SCOREBLACKWINS)
#define SCOREDRAW 0
#define SCORETBWIN 29900
#define SCOREWONENDGAME 10000

#define MATEFORME(s) ((s) > SCOREWHITEWINS - MAXDEPTH)
#define MATEFOROPPONENT(s) ((s) < SCOREBLACKWINS + MAXDEPTH)
#define MATEDETECTED(s) (MATEFORME(s) || MATEFOROPPONENT(s))

/* Offsets for 64Bit  board*/
const int knightoffset[] = { -6, -10, -15, -17, 6, 10, 15, 17 };
const int diagonaloffset[] = { -7, -9, 7, 9 };
const int orthogonaloffset[] = { -8, -1, 1, 8 };
const int orthogonalanddiagonaloffset[] = { -8, -1, 1, 8, -7, -9, 7, 9 };

const struct { int offset; bool needsblank; } pawnmove[] = { { 0x10, true }, { 0x0f, false }, { 0x11, false } };
extern const int materialvalue[];
extern int psqtable[14][64];
extern evalparamset eps;

// values for move ordering
const int mvv[] = { 0U << 27, 1U << 27, 2U << 27, 2U << 27, 3U << 27, 4U << 27, 5U << 27 };
const int lva[] = { 5 << 24, 4 << 24, 3 << 24, 3 << 24, 2 << 24, 1 << 24, 0 << 24 };
#define PVVAL (7 << 27)
#define KILLERVAL1 (1 << 26)
#define KILLERVAL2 (KILLERVAL1 - 1)
#define NMREFUTEVAL (1 << 25)
#define BADTACTICALFLAG (1 << 31)


// 32bit move code has the following format
// 10987654321098765432109876543210 Bit#
// xxxx                             Piece code
//     x                            Castle flag
//      x                           EP capture flag
//       xxxxxx                     EP capture target square
//           xx                     castle index (00=white queen 01=white king 10=black queen 11=black king)
//             xxxx                 captured piece code
//                 xxxx             promotion piece code
//                     xxxxxx       from square
//                           xxxxxx to square
#define EPCAPTUREFLAG 0x4000000
#define CASTLEFLAG    0x8000000
#define GETFROM(x) (((x) & 0x0fc0) >> 6)
#define GETTO(x) ((x) & 0x003f)
#define GETCORRECTTO(x) (ISCASTLE(x) ? castlekingto[GETCASTLEINDEX(x)] : GETTO(x))
#define GETEPT(x) (((x) & 0x03f00000) >> 20)
#define ISEPCAPTURE(x) ((x) & EPCAPTUREFLAG)
#define ISCASTLE(x) ((x) & CASTLEFLAG)
#define GETCASTLEINDEX(x) (((x) & 0x00300000) >> 20)
#define ISEPCAPTUREORCASTLE(x) ((x) & (EPCAPTUREFLAG | CASTLEFLAG))

#define GETPROMOTION(x) (((x) & 0xf000) >> 12)
#define GETCAPTURE(x) (((x) & 0xf0000) >> 16)
#define ISTACTICAL(x) ((x) & 0xff000)
#define ISPROMOTION(x) ((x) & 0xf000)
#define ISCAPTURE(x) ((x) & 0xf0000)
#define GETPIECE(x) (((x) & 0xf0000000) >> 28)
#define GETTACTICALVALUE(x) (materialvalue[GETCAPTURE(x) >> 1] + (ISPROMOTION(x) ? materialvalue[GETPROMOTION(x) >> 1] - materialvalue[PAWN] : 0))

#define PAWNATTACK(s, p) ((s) ? (((p) & ~FILEHBB) >> 7) | (((p) & ~FILEABB) >> 9) : (((p) & ~FILEABB) << 7) | (((p) & ~FILEHBB) << 9))
#define PAWNPUSH(s, p) ((s) ? ((p) >> 8) : ((p) << 8))
#define PAWNPUSHINDEX(s, i) ((s) ? (i) - 8 : (i) + 8)
#define PAWNPUSHDOUBLEINDEX(s, i) ((s) ? (i) - 16 : (i) + 16)

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
extern int castlerookfrom[4];
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
    int lastnullmove;
    uint32_t movecode;
    U64 kingPinned;
};

#define MAXMOVELISTLENGTH 256   // for lists of possible pseudo-legal moves


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
#define MAXTHREADS  256
#define MAXHASH     0x100000  // 1TB ... never tested
#define DEFAULTHASH 16

class chessmovelist
{
public:
    int length;
    chessmove move[MAXMOVELISTLENGTH];
	chessmovelist();
	string toString();
	string toStringWithValue();
	void print();
    chessmove* getNextMove(int minval);
};

#define CMPLIES 2

enum MoveSelector_State { INITSTATE, HASHMOVESTATE, TACTICALINITSTATE, TACTICALSTATE, KILLERMOVE1STATE, KILLERMOVE2STATE,
    COUNTERMOVESTATE, QUIETINITSTATE, QUIETSTATE, BADTACTICALSTATE, BADTACTICALEND, EVASIONINITSTATE, EVASIONSTATE };

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
    chessmove countermove;
    int legalmovenum;
    bool onlyGoodCaptures;
    int16_t *cmptr[CMPLIES];

public:
    void SetPreferredMoves(chessposition *p);  // for quiescence move selector
    void SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, uint32_t counter, int excludemove);
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

#ifdef USE_BMI2
#include <immintrin.h>
#define BISHOPINDEX(occ,i) (int)(_pext_u64(occ, mBishopTbl[i].mask))
#define ROOKINDEX(occ,i) (int)(_pext_u64(occ, mRookTbl[i].mask))
#else
#define BISHOPINDEX(occ,i) (int)((((occ) & mBishopTbl[i].mask) * mBishopTbl[i].magic) >> (64 - BISHOPINDEXBITS))
#define ROOKINDEX(occ,i) (int)((((occ) & mRookTbl[i].mask) * mRookTbl[i].magic) >> (64 - ROOKINDEXBITS))
#endif
#define BISHOPATTACKS(m,x) (mBishopAttacks[x][BISHOPINDEX(m,x)])
#define ROOKATTACKS(m,x) (mRookAttacks[x][ROOKINDEX(m,x)])

extern U64 mBishopAttacks[64][1 << BISHOPINDEXBITS];
extern U64 mRookAttacks[64][1 << ROOKINDEXBITS];

enum MoveType { QUIET = 1, CAPTURE = 2, PROMOTE = 4, TACTICAL = 6, ALL = 7 };
enum RootsearchType { SinglePVSearch, MultiPVSearch };

int CreateEvasionMovelist(chessposition *pos, chessmove* mstart);
template <MoveType Mt> int CreateMovelist(chessposition *pos, chessmove* mstart);
template <PieceType Pt> inline int CreateMovelistPiece(chessposition *pos, chessmove* mstart, U64 occ, U64 targets, int me);
template <MoveType Mt> inline int CreateMovelistPawn(chessposition *pos, chessmove* mstart, int me);
inline int CreateMovelistCastle(chessposition *pos, chessmove* mstart, int me);
template <MoveType Mt> void evaluateMoves(chessmovelist *ml, chessposition *pos, int16_t **cmptr);

enum AttackType { FREE, OCCUPIED, OCCUPIEDANDKING };

struct positioneval {
    pawnhashentry *phentry;
    Materialhashentry *mhentry;
    int kingattackpiececount[2][7] = { { 0 } };
    int kingringattacks[2] = { 0 };
    int kingattackers[2];
};

#ifdef SDEBUG
enum PvAbortType {
    PVA_UNKNOWN = 0, PVA_FROMTT, PVA_DIFFERENTFROMTT, PVA_RAZORPRUNED, PVA_REVFUTILITYPRUNED, PVA_NMPRUNED, PVA_PROBCUTPRUNED, PVA_LMPRUNED,
    PVA_FUTILITYPRUNED, PVA_SEEPRUNED, PVA_BADHISTORYPRUNED, PVA_MULTICUT, PVA_BESTMOVE, PVA_NOTBESTMOVE, PVA_OMITTED, PVA_BETACUT, PVA_BELOWALPHA }; 
#endif

// Replace the occupied bitboards with the first two so far unused piece bitboards
#define occupied00 piece00

class chessposition
{
public:
    U64 nodes;
    int mstop;      // 0 at last non-reversible move before root, rootheight at root position
    int ply;        // 0 at root position

    U64 piece00[14];
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
    int lastnullmove;
    uint32_t movecode;
    U64 kingPinned;

    uint8_t mailbox[BOARDSIZE]; // redundand for faster "which piece is on field x"
    chessmovestack movestack[MAXDEPTH];
    uint16_t excludemovestack[MAXDEPTH];
    int16_t staticevalstack[MAXDEPTH];

    int rootheight; // fixed stack offset in root position 
    int seldepth;
    int nullmoveside;
    int nullmoveply = 0;
    chessmovelist rootmovelist;
    chessmove bestmove;
    int bestmovescore[MAXMULTIPV];
    int lastbestmovescore;
    chessmove pondermove;
    int LegalMoves[MAXDEPTH];
    uint32_t killer[MAXDEPTH][2];
    uint32_t bestFailingLow;
    int threadindex;
    int psqval;
#ifdef SDEBUG
    unsigned long long debughash = 0;
    struct {
        uint32_t code;
        U64 hash;
    } pvdebug[MAXDEPTH];
    int pvalpha[MAXDEPTH];
    int pvbeta[MAXDEPTH];
    int pvdepth[MAXDEPTH];
    int pvmovenum[MAXDEPTH];
    PvAbortType pvaborttype[MAXDEPTH];
    int pvabortval[MAXDEPTH];
    string pvadditionalinfo[MAXDEPTH];
#endif
    uint32_t pvtable[MAXDEPTH][MAXDEPTH];
    uint32_t multipvtable[MAXMULTIPV][MAXDEPTH];
    uint32_t lastpv[MAXDEPTH];
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
    bool noQs;
    tuneparamselection tps;
    positiontuneset pts;
    evalparam ev[NUMOFEVALPARAMS];
    void resetTuner();
    void getPositionTuneSet(positiontuneset *p, evalparam *e);
    void copyPositionTuneSet(positiontuneset *from, evalparam *efrom, positiontuneset *to, evalparam *eto);
    string getGradientString();
#endif
    // ...
    int16_t history[2][64][64];
    int16_t counterhistory[14][64][14 * 64];
    uint32_t countermove[14][64];
    int he_threshold;
    U64 he_yes;
    U64 he_all;
    Materialhash mtrlhsh;
    Pawnhash pwnhsh;
#ifdef NNUE
    NnueAccumulator accumulator[MAXDEPTH];
    DirtyPiece dirtypiece[MAXDEPTH];
#endif
    bool w2m();
    void BitboardSet(int index, PieceCode p);
    void BitboardClear(int index, PieceCode p);
    void BitboardMove(int from, int to, PieceCode p);
    void BitboardPrint(U64 b);
    int getFromFen(const char* sFen);
    string toFen();
    uint32_t applyMove(string s, bool resetMstop = true);
    void print(ostream* os = &cout);
    int phase();
    U64 movesTo(PieceCode pc, int from);
    template <PieceType Pt> U64 pieceMovesTo(int from);
    bool isAttacked(int index, int me);
    U64 isAttackedByMySlider(int index, U64 occ, int me);  // special simple version to detect giving check by removing blocker
    U64 attackedByBB(int index, U64 occ);  // returns bitboard of all pieces of both colors attacking index square 
    template <AttackType At> U64 isAttackedBy(int index, int col);    // returns the bitboard of cols pieces attacking the index square; At controls if pawns are moved to block or capture
    bool see(uint32_t move, int threshold);
    int getBestPossibleCapture();
    void getRootMoves();
    void tbFilterRootMoves();
    void prepareStack();
    string movesOnStack();
    bool playMove(chessmove *cm);
    void unplayMove(chessmove *cm);
    void playNullMove();
    void unplayNullMove();
    template <int Me> void updatePins();
    template <int Me> bool sliderAttacked(int index, U64 occ);
    bool moveGivesCheck(uint32_t c);  // simple and imperfect as it doesn't handle special moves and cases (mainly to avoid pruning of important moves)
    bool moveIsPseudoLegal(uint32_t c);     // test if move is possible in current position
    uint32_t shortMove2FullMove(uint16_t c); // transfer movecode from tt to full move code without checking if pseudoLegal
    int getpsqval(bool showDetails = false);  // only for eval trace and mirror test
    template <EvalType Et, int Me> int getGeneralEval(positioneval *pe);
    template <EvalType Et, PieceType Pt, int Me> int getPieceEval(positioneval *pe);
    template <EvalType Et, int Me> int getLateEval(positioneval *pe);
    template <EvalType Et, int Me> void getPawnAndKingEval(pawnhashentry *entry);
    template <EvalType Et> int getEval();
    void getScaling(Materialhashentry *mhentry);
    int getComplexity(int eval, pawnhashentry *phentry, Materialhashentry *mhentry);

    template <RootsearchType RT> int rootsearch(int alpha, int beta, int depth, int inWindowLast);
    int alphabeta(int alpha, int beta, int depth);
    int getQuiescence(int alpha, int beta, int depth);
    void updateHistory(uint32_t code, int16_t **cmptr, int value);
    void getCmptr(int16_t **cmptr);
    void updatePvTable(uint32_t mc, bool recursive);
    void updateMultiPvTable(int pvindex, uint32_t mc);
    string getPv(uint32_t *table);
    int getHistory(uint32_t code, int16_t **cmptr);
    inline void CheckForImmediateStop();

#ifdef SDEBUG
    bool triggerDebug(chessmove* nextmove);
    void pvdebugout();
#endif
    int testRepetiton();
    void mirror();
#ifdef NNUE
    void HalfkpAppendActiveIndices(int c, NnueIndexList *active);
    void AppendActiveIndices(NnueIndexList active[2]);
    void HalfkpAppendChangedIndices(int c, NnueIndexList *add, NnueIndexList *remove);
    void AppendChangedIndices(NnueIndexList add[2], NnueIndexList remove[2], bool reset[2]);
    void RefreshAccumulator();
    bool UpdateAccumulator();
    void Transform(clipped_t *output);
    int NnueGetEval();
#endif
};

//
// uci stuff
//

enum GuiToken { UNKNOWN, UCI, UCIDEBUG, ISREADY, SETOPTION, REGISTER, UCINEWGAME, POSITION, GO, STOP, PONDERHIT, QUIT, EVAL, PERFT
};

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
    { "eval", EVAL },
    { "perft", PERFT }
};

//
// engine stuff
//
class engine;   //forward definition

// order of ucioptiontypes is important for (not) setting default at registration
enum ucioptiontype { ucicheck, ucispin, ucicombo, ucistring, ucibutton, ucieval };

struct ucioption_t
{
    string name;
    ucioptiontype type;
    string def;
    int min;
    int max;
    string varlist;
    void *enginevar;
    void (*setoption)();
};

class ucioptions_t
{
    map<string, ucioption_t> optionmap;
public:
    void Register(void *e, string n, ucioptiontype t, string d, int mi = 0, int ma = 0, void(*setop)() = nullptr, string v = ""); //engine*, ucioption_t*
    void Set(string n, string v, bool force = false);
    void Print();
};

typedef map<string, ucioption_t>::iterator optionmapiterator;


#define ENGINERUN 0
#define ENGINEWANTSTOP 1
#define ENGINESTOPIMMEDIATELY 2
#define ENGINETERMINATEDSEARCH 3

#define NODESPERCHECK 0xfff
enum ponderstate_t { NO, PONDERING, HITPONDER };


#define CPUMMX      (1 << 0)
#define CPUSSE2     (1 << 1)
#define CPUSSSE3    (1 << 2)
#define CPUPOPCNT   (1 << 3)
#define CPUAVX2     (1 << 4)
#define CPUBMI2     (1 << 5)

#define STRCPUFEATURELIST  { "mmx","sse2","ssse3","popcnt","avx2","bmi2" }


extern const string strCpuFeatures[];

class compilerinfo
{
public:
    const U64 binarySupports = 0ULL
#ifdef USE_POPCNT
        | CPUPOPCNT
#endif
#ifdef USE_MMX
        | CPUMMX
#endif
#ifdef USE_SSE2
        | CPUSSE2
#endif
#ifdef USE_SSSE3
        | CPUSSSE3
#endif
#ifdef USE_AVX2
        | CPUAVX2
#endif
#ifdef USE_BMI2
        | CPUBMI2
#endif
        ;

    U64 machineSupports;
    string system;
    int cpuVendor;
    compilerinfo();
    void GetSystemInfo();
    string SystemName();
    string PrintCpuFeatures(U64 features, bool onlyHighest = false);
};


class engine
{
public:
    engine(compilerinfo *c);
    ~engine();
    const char* author = "Andreas Matthies";
    bool isWhite;
    U64 tbhits;
    U64 starttime;
    U64 endtime1; // time to stop before starting next iteration
    U64 endtime2; // time to stop immediately
    U64 frequency;
    int wtime, btime, winc, binc, movestogo, mate, movetime, maxdepth;
    U64 maxnodes;
    bool infinite;
    bool debug = false;
    bool evaldetails = false;
    bool moveoutput;
    int stopLevel = ENGINETERMINATEDSEARCH;
    int Hash;
    int restSizeOfTp = 0;
    int sizeOfPh;
    int moveOverhead;
    int MultiPV;
    bool ponder;
    bool chess960;
    string SyzygyPath;
    bool Syzygy50MoveRule = true;
    int SyzygyProbeLimit;
    chessposition rootposition;
    int Threads;
    int oldThreads;
    searchthread *sthread;
    ponderstate_t pondersearch;
    bool ponderhit;
    int terminationscore = SHRT_MAX;
    int lastReport;
    int benchdepth;
    string benchmove;
    ucioptions_t ucioptions;
    compilerinfo* compinfo;

#ifdef STACKDEBUG
    string assertfile = "";
#endif
#ifdef TDEBUG
    int t1stop = 0;     // regular stop
    int t2stop = 0;     // immediate stop
    bool bStopCount;
#endif
#ifdef NNUE
    string NnueNetpath;
#endif

    string name() {
        string sbinary = compinfo->PrintCpuFeatures(compinfo->binarySupports, true);
        return string(ENGINEVER) + (sbinary != "" ? " (" + sbinary + ")" : "");
    };
    GuiToken parse(vector<string>*, string ss);
    void send(const char* format, ...);
    void communicate(string inputstring);
    void allocThreads();
    U64 getTotalNodes();
    long long perft(int depth, bool dotests);
    void prepareThreads();
    void resetStats();
};

PieceType GetPieceType(char c);

// enginestate is for communication with external engine process
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
    bool doEval;
    bool comparesuccess;
    int comparetime;
    int comparescore;
};

extern engine en;
extern compilerinfo cinfo;

#ifdef SDEBUG
#define SDEBUGDO(c, s) if (c) {s}
#else
#define SDEBUGDO(c, s)
#endif


//
// search stuff
//
class searchthread
{
public:
    chessposition pos;
    thread thr;
    int index;
    int depth;
    int numofthreads;
    int lastCompleteDepth;
    // adjust padding to align searchthread at 64 bytes
    uint8_t padding[16];

    searchthread *searchthreads;
};

void searchStart();
void searchWaitStop(bool forceStop = true);
void searchinit();
void resetEndTime(int constantRootMoves, bool complete = true);


//
// TB stuff
//
extern int TBlargest; // 5 if 5-piece tables, 6 if 6-piece tables were found.

void init_tablebases(char *path);
int probe_wdl(int *success, chessposition *pos);
int probe_dtz(int *success, chessposition *pos);
int root_probe_dtz(chessposition *pos);
int root_probe_wdl(chessposition *pos);


//
// statistics stuff
//
#ifdef STATISTICS
struct statistic {
    U64 qs_n[2];                // total calls to qs split into no check / check
    U64 qs_tt;                  // qs hits tt
    U64 qs_pat;                 // qs returns with pat score
    U64 qs_delta;               // qs return with delta pruning before move loop
    U64 qs_loop_n;              // qs enters moves loop
    U64 qs_move_delta;          // qs moves delta-pruned
    U64 qs_moves;               // moves done in qs
    U64 qs_moves_fh;            // qs moves that cause a fail high

    U64 ab_n;                   // total calls to alphabeta
    U64 ab_pv;                  // number of PV nodes
    U64 ab_tt;                  // alphabeta exit by tt hit
    U64 ab_draw_or_win;         // alphabeta returns draw or mate score
    U64 ab_qs;                  // alphabeta calls qsearch
    U64 ab_tb;                  // alphabeta exits with tb score

    U64 prune_futility;         // nodes pruned by reverse futility
    U64 prune_nm;               // nodes pruned by null move;
    U64 prune_probcut;          // nodes pruned by PobCut
    U64 prune_multicut;         // nodes pruned by Multicut (detected by failed singular test)

    U64 moves_loop_n;           // counts how often the moves loop is entered
    U64 moves_n[2];             // all moves in alphabeta move loop split into quites ans tactical
    U64 moves_pruned_lmp;       // moves pruned by lmp
    U64 moves_pruned_futility;  // moves pruned by futility
    U64 moves_pruned_badsee;    // moves pruned by bad see
    U64 moves_played[2];        // moves that are played split into quites ans tactical
    U64 moves_fail_high;        // moves that cause a fail high;
    U64 moves_bad_hash;         // hash moves that are repicked in the bad tactical stage

    U64 red_total;              // total reductions
    U64 red_lmr[2];             // total late-move-reductions for (not) improved moves
    U64 red_pi[2];              // number of quiets moves that are reduced split into (not) / improved moves
    S64 red_history;            // total reduction by history
    S64 red_pv;                 // total reduction by pv nodes
    S64 red_correction;         // total reduction correction by over-/underflow

    U64 extend_singular;        // total singular extensions
    U64 extend_endgame;        // total endgame extensions
    U64 extend_history;        // total history extensions
};

extern struct statistic statistics;

void search_statistics();

// some macros to limit the ifdef STATISTICS inside the code
#define STATISTICSINC(x)        statistics.x++ 
#define STATISTICSADD(x, v)     statistics.x += (v)
#define STATISTICSDO(x)         x

#else
#define STATISTICSINC(x)
#define STATISTICSADD(x, v)
#define STATISTICSDO(x)
#endif


