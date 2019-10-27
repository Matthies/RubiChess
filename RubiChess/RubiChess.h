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

#define VERNUM "1.7-dev"

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

#define ENGINEVER "RubiChess " VERNUM
#define BUILD __DATE__ " " __TIME__

#define BITSET(x) (1ULL << (x))
#define ONEORZERO(x) (!((x) & ((x) - 1)))
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
#define RANK8(s) ((s) ? 0x00000000000000ff : 0xff00000000000000)
#define FILEABB 0x0101010101010101
#define FILEHBB 0x8080808080808080
#define ISOUTERFILE(x) (FILE(x) == 0 || FILE(x) == 7)
#define ISNEIGHBOUR(x,y) ((x) >= 0 && (x) < 64 && (y) >= 0 && (y) < 64 && abs(RANK(x) - RANK(y)) <= 1 && abs(FILE(x) - FILE(y)) <= 1)

#define S2MSIGN(s) (s ? -1 : 1)


typedef unsigned long long U64;

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
    int type;
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
#define SQRESULT(v,s) ( v > 0 ? ((int32_t)((uint32_t)((v) * (v) * S2MSIGN(s) / 2048) << 16) + ((v) * S2MSIGN(s) / 16)) : 0 )
#define CVALUE(v) eval(v)
#define CEVAL(e, f) ((e).addGrad(f), (e) * (f))
#else // EVALTUNE

#define SQVALUE(i, v) (v)
#define VALUE(m, e) ((int32_t)((uint32_t)(m) << 16) + (e))
#define EVAL(e, f) ((e) * (f))
#define SQEVAL(e, f, s) ((e) * (f))
#define SQRESULT(v,s) ( v > 0 ? VALUE((v) * (v) * S2MSIGN(s) / 2048, (v) * S2MSIGN(s) / 16) : 0 )
#define CVALUE(v) (v)
#define CEVAL(e, f) ((e) * (f))
typedef const int32_t eval;
#endif

#define PSQTINDEX(i,s) ((s) ? (i) : (i) ^ 0x38)

#define TAPEREDANDSCALEDEVAL(s, p, c) ((GETMGVAL(s) * (256 - (p)) + GETEGVAL(s) * (p) * (c) / SCALE_NORMAL) / 256)

struct evalparamset {
    // Powered by Laser games :-)
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
    eval ePotentialpassedpawnbonus[2][8] = {
        {  VALUE(   0,   0), VALUE(  28, -15), VALUE(  -3,  14), VALUE(  17,   5), VALUE(  37,  60), VALUE(  75,  81), VALUE(  44, 108), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  -2,   0), VALUE(   1,   2), VALUE(   6,   6), VALUE(  20,  25), VALUE(  53,   7), VALUE( -14,  10), VALUE(   0,   0)  }
    };
    eval eAttackingpawnbonus[8] = {  VALUE(   0,   0), VALUE( -48,  12), VALUE( -14,   4), VALUE( -14,  -6), VALUE( -14,  -6), VALUE( -15,   1), VALUE(   0,   0), VALUE(   0,   0)  };
    eval eIsolatedpawnpenalty =  VALUE( -13, -12);
    eval eDoublepawnpenalty =  VALUE(  -9, -21);
    eval eConnectedbonus[6][5] = {
        {  VALUE(   7,  -7), VALUE(   8,  -5), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(   3,  -8), VALUE(   3,  -7), VALUE(  10,   7), VALUE(  15,  15), VALUE(  18,  18)  },
        {  VALUE(  24,  50), VALUE(   5,  -1), VALUE(  10,   0), VALUE(  17,   3), VALUE(  16,   9)  },
        {  VALUE(  -5,  10), VALUE(   0,  10), VALUE(  15,   5), VALUE(  38,  14), VALUE(  32,   5)  },
        {  VALUE(  36, 184), VALUE(  79,  72), VALUE(  31,  36), VALUE(   7,  38), VALUE(   3,  94)  },
        {  VALUE( -57, 253), VALUE(  38, 232), VALUE( 132, 124), VALUE(   7,4096), VALUE(   0,   4)  }
    };
    eval eBackwardpawnpenalty =  VALUE( -10, -15);
    eval eDoublebishopbonus =  VALUE(  56,  38);
    eval ePawnblocksbishoppenalty = VALUE(-2, -4);
    eval eMobilitybonus[4][28] = {
        {  VALUE(  16, -90), VALUE(  38, -26), VALUE(  51,   1), VALUE(  57,  13), VALUE(  64,  27), VALUE(  71,  37), VALUE(  77,  36), VALUE(  84,  36),
           VALUE(  86,  30), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(  21, -25), VALUE(  34,  -6), VALUE(  51,  13), VALUE(  59,  23), VALUE(  69,  34), VALUE(  75,  40), VALUE(  81,  43), VALUE(  81,  50),
           VALUE(  84,  51), VALUE(  83,  55), VALUE(  85,  52), VALUE(  81,  47), VALUE(  72,  59), VALUE(  72,  31), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE( -57,  11), VALUE(  14,  17), VALUE(  31,  57), VALUE(  34,  70), VALUE(  36,  82), VALUE(  38,  90), VALUE(  40,  92), VALUE(  46,  96),
           VALUE(  47, 101), VALUE(  50, 111), VALUE(  54, 109), VALUE(  50, 115), VALUE(  49, 118), VALUE(  50, 116), VALUE(  44, 116), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(-4097,  83), VALUE(  13,-157), VALUE(  -2,  31), VALUE(   3,  75), VALUE(   5,  94), VALUE(   4, 152), VALUE(   6, 161), VALUE(  11, 177),
           VALUE(  10, 192), VALUE(  15, 187), VALUE(  14, 212), VALUE(  17, 213), VALUE(  16, 227), VALUE(  18, 229), VALUE(  17, 242), VALUE(  19, 251),
           VALUE(  20, 253), VALUE(  21, 256), VALUE(  18, 263), VALUE(  13, 270), VALUE(  45, 244), VALUE(  67, 233), VALUE(  72, 241), VALUE(  80, 230),
           VALUE(  69, 264), VALUE( 108, 231), VALUE( 107, 230), VALUE( 114, 198)  }
    };
    eval eRookon7thbonus =  VALUE(  -1,  22);
    eval eSlideronfreefilebonus[2] = {  VALUE(  21,   7), VALUE(  43,   1)  };
    eval eMaterialvalue[7] = {  VALUE(   0,   0), VALUE( 100, 100), VALUE( 314, 314), VALUE( 314, 314), VALUE( 483, 483), VALUE( 913, 913), VALUE(0,0)  };
    eval eKingshieldbonus =  VALUE(  15,  -2);
    eval eWeakkingringpenalty =  SQVALUE(   1,  67);
    eval eKingattackweight[7] = {  SQVALUE(   1,   0), SQVALUE(   1,   0), SQVALUE(   1,  30), SQVALUE(   1,  16), SQVALUE(   1,  16), SQVALUE(   1,  39), SQVALUE(   1,   0)  };
    eval eSafecheckbonus[6] = {  SQVALUE(   1,   0), SQVALUE(   1,   0), SQVALUE(   1, 282), SQVALUE(   1,  55), SQVALUE(   1, 243), SQVALUE(   1, 216)  };
    eval eKingdangerbyqueen =  SQVALUE(   1,-156);
    eval eKingringattack[6] = {  SQVALUE(   1,  87), SQVALUE(   1,   0), SQVALUE(   1,  28), SQVALUE(   1,   0), SQVALUE(   1,   0), SQVALUE(   1, -17)  };
    eval eKingdangeradjust =  SQVALUE(   1,   9);
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
           VALUE( 140,  54), VALUE(  73,  67), VALUE( 110,  41), VALUE( 135,  28), VALUE( 124,  40), VALUE( 109,  50), VALUE(  23,  81), VALUE(  50,  75),
           VALUE( -12,  39), VALUE( -24,  38), VALUE(  -4,  12), VALUE(  -3,  -2), VALUE(   7,   4), VALUE(  55,   6), VALUE(  -6,  43), VALUE( -19,  43),
           VALUE( -25,  36), VALUE( -20,  25), VALUE( -14,  13), VALUE( -13,   2), VALUE(  11,  -1), VALUE(   0,   4), VALUE( -15,  18), VALUE( -18,  19),
           VALUE( -31,  24), VALUE( -30,  21), VALUE( -17,  10), VALUE(  -2,   8), VALUE(   0,   7), VALUE(  -7,   7), VALUE( -20,  13), VALUE( -27,   9),
           VALUE( -27,  18), VALUE( -27,  12), VALUE( -15,   7), VALUE( -14,  12), VALUE(   0,  16), VALUE( -17,  10), VALUE(  -7,   3), VALUE( -20,   4),
           VALUE( -26,  25), VALUE( -28,  23), VALUE( -20,  16), VALUE( -21,   6), VALUE( -11,  30), VALUE(   6,  13), VALUE(   2,   9), VALUE( -27,   7),
           VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999)  },
        {  VALUE(-124, -24), VALUE( -59,   7), VALUE( -21,  -2), VALUE( -37,   7), VALUE(   4,   1), VALUE( -82, -10), VALUE(-136,   9), VALUE( -72, -62),
           VALUE( -36,  -5), VALUE(  -7,   3), VALUE(  29,  -2), VALUE(  61,  -8), VALUE(  17,  -8), VALUE(  83, -29), VALUE(   9,  -6), VALUE(  -3, -23),
           VALUE(  -4,   3), VALUE(  42,   4), VALUE(  38,  16), VALUE(  64,   8), VALUE(  91, -13), VALUE(  83, -13), VALUE(  58, -15), VALUE(  23, -13),
           VALUE(  20,  15), VALUE(  40,   5), VALUE(  63,   8), VALUE(  88,  13), VALUE(  66,  16), VALUE(  89,   6), VALUE(  38,   7), VALUE(  54, -11),
           VALUE(  19,   0), VALUE(  35,  13), VALUE(  51,  14), VALUE(  49,  25), VALUE(  65,  23), VALUE(  63,   4), VALUE(  64,   7), VALUE(  36,  10),
           VALUE( -23,  -2), VALUE(   2,  -4), VALUE(  22,  -5), VALUE(  22,  17), VALUE(  34,  13), VALUE(  31,  -8), VALUE(  32, -11), VALUE(   9,   2),
           VALUE( -20,   5), VALUE( -18,  -3), VALUE(  -5,  -9), VALUE(  19,  -7), VALUE(  21,  -6), VALUE(  18,  -9), VALUE(  16,   0), VALUE(   8,  17),
           VALUE( -67,  13), VALUE(  -3,   2), VALUE( -21,  -2), VALUE(  12, -10), VALUE(  10,   2), VALUE(  10, -10), VALUE(  -2,   8), VALUE( -30,   8)  },
        {  VALUE(   7,  25), VALUE( -54,  45), VALUE(  10,  28), VALUE( -50,  41), VALUE( -53,  37), VALUE( -23,  23), VALUE( -30,   9), VALUE( -24,  14),
           VALUE( -24,  22), VALUE(  -1,  17), VALUE(   0,  24), VALUE(  14,  25), VALUE(  16,  13), VALUE(  -4,  19), VALUE( -18,  13), VALUE( -27,  15),
           VALUE(  14,  27), VALUE(  29,  24), VALUE(  30,  22), VALUE(  51,   7), VALUE(  35,  10), VALUE(  71,  19), VALUE(  27,  24), VALUE(  52,   9),
           VALUE(   9,  28), VALUE(  35,  29), VALUE(  40,  22), VALUE(  53,  29), VALUE(  59,  22), VALUE(  39,  27), VALUE(  52,  16), VALUE(   4,  24),
           VALUE(  14,  29), VALUE(  14,  26), VALUE(  31,  33), VALUE(  51,  26), VALUE(  46,  27), VALUE(  38,  24), VALUE(  34,  11), VALUE(  52,  -5),
           VALUE(  16,   6), VALUE(  35,  26), VALUE(  24,  21), VALUE(  26,  26), VALUE(  30,  32), VALUE(  31,  20), VALUE(  40,   8), VALUE(  30,  15),
           VALUE(  29,   9), VALUE(  24,   2), VALUE(  32,   0), VALUE(   8,  17), VALUE(  17,  19), VALUE(  38,   1), VALUE(  48,   5), VALUE(  38, -13),
           VALUE(  15,   3), VALUE(  35,   0), VALUE(  23,   2), VALUE(   6,  11), VALUE(  16,  13), VALUE(   2,  27), VALUE(  17,   5), VALUE(  26,  -6)  },
        {  VALUE(  29,  60), VALUE(   6,  68), VALUE(  11,  75), VALUE(  21,  74), VALUE(   2,  71), VALUE(   8,  79), VALUE(  12,  70), VALUE(  64,  53),
           VALUE(   5,  66), VALUE(  -5,  78), VALUE(  19,  79), VALUE(  33,  71), VALUE(  13,  73), VALUE(  22,  64), VALUE(   3,  68), VALUE(  34,  55),
           VALUE(  -9,  81), VALUE(  22,  73), VALUE(  17,  80), VALUE(  23,  66), VALUE(  59,  50), VALUE(  61,  53), VALUE(  69,  48), VALUE(  41,  53),
           VALUE(  -2,  78), VALUE(   1,  82), VALUE(  19,  75), VALUE(  15,  70), VALUE(  24,  56), VALUE(  30,  55), VALUE(  29,  60), VALUE(  16,  62),
           VALUE( -16,  73), VALUE( -21,  71), VALUE(  -7,  67), VALUE(   6,  61), VALUE(   4,  62), VALUE(  -8,  62), VALUE(  37,  36), VALUE(   7,  51),
           VALUE( -19,  65), VALUE( -12,  50), VALUE( -12,  61), VALUE(  -8,  56), VALUE(   4,  46), VALUE(  10,  41), VALUE(  39,  18), VALUE(  13,  20),
           VALUE( -11,  54), VALUE( -14,  52), VALUE(  -5,  54), VALUE(  -2,  50), VALUE(  11,  41), VALUE(  17,  32), VALUE(  27,  25), VALUE(  -2,  41),
           VALUE(   6,  51), VALUE(   1,  43), VALUE(   1,  49), VALUE(  11,  39), VALUE(  16,  35), VALUE(  19,  42), VALUE(  26,  32), VALUE(   4,  51)  },
        {  VALUE(  -6,  62), VALUE( -17,  80), VALUE(  -3,  86), VALUE(   5,  95), VALUE(  -9, 110), VALUE(  22,  86), VALUE(  -3, 112), VALUE(  10,  51),
           VALUE( -32, 107), VALUE( -49, 122), VALUE( -52, 162), VALUE( -26, 130), VALUE( -51, 162), VALUE( -35, 134), VALUE( -56, 168), VALUE(  19, 125),
           VALUE(  -5, 121), VALUE(  -7, 122), VALUE( -15, 159), VALUE(   7, 144), VALUE( -10, 171), VALUE(  -8, 159), VALUE( -22, 170), VALUE(   8, 117),
           VALUE( -13, 147), VALUE(   1, 145), VALUE(  -5, 144), VALUE( -16, 161), VALUE( -22, 176), VALUE(  11, 139), VALUE(  11, 182), VALUE(  15, 141),
           VALUE(  -2, 145), VALUE(  -6, 147), VALUE(   2, 135), VALUE(   0, 144), VALUE(   8, 134), VALUE(   4, 155), VALUE(  21, 150), VALUE(  23, 142),
           VALUE(  -9, 128), VALUE(   8, 124), VALUE(  -5, 139), VALUE(  -1, 127), VALUE(   4, 136), VALUE(  11, 140), VALUE(  32, 117), VALUE(  17, 107),
           VALUE(  -3, 114), VALUE(   2,  99), VALUE(  11, 105), VALUE(  16, 115), VALUE(  14, 117), VALUE(  22,  80), VALUE(  30,  63), VALUE(  34,  64),
           VALUE(  -7,  81), VALUE( -10,  89), VALUE(  -6,  99), VALUE(   8,  97), VALUE(   3,  92), VALUE( -20,  97), VALUE(  -1, 118), VALUE( -14, 137)  },
        {  VALUE( -48,-117), VALUE(  56, -13), VALUE( -10,  14), VALUE(-114,  52), VALUE( -43,  60), VALUE(-112,  78), VALUE( -21, -15), VALUE(  27, -87),
           VALUE( -78,  22), VALUE(   6,  46), VALUE( -12,  79), VALUE( -53,  73), VALUE( -76,  84), VALUE(   8,  75), VALUE( -21,  75), VALUE(-141,  34),
           VALUE(-168,  28), VALUE( -20,  55), VALUE( -32,  64), VALUE( -62,  86), VALUE( -27,  78), VALUE(  12,  61), VALUE( -52,  63), VALUE(-115,  31),
           VALUE(-114,   5), VALUE( -27,  31), VALUE( -76,  62), VALUE(-105,  81), VALUE( -39,  72), VALUE( -47,  62), VALUE( -50,  38), VALUE(-148,  20),
           VALUE(-101,  -2), VALUE( -71,  26), VALUE( -39,  48), VALUE( -72,  63), VALUE( -63,  59), VALUE( -54,  45), VALUE( -68,  25), VALUE(-153,   4),
           VALUE( -40, -24), VALUE( -16,  -3), VALUE( -42,  27), VALUE( -49,  42), VALUE( -46,  43), VALUE( -37,  25), VALUE( -30,   7), VALUE( -56,  -7),
           VALUE(  37, -44), VALUE(   4,  -9), VALUE( -11,   7), VALUE( -41,  15), VALUE( -45,  19), VALUE( -26,  15), VALUE(   4,  -3), VALUE(   7, -27),
           VALUE(  19, -76), VALUE(  45, -57), VALUE(  29, -23), VALUE( -66,   3), VALUE(  -8, -16), VALUE( -43,  -1), VALUE(  17, -32), VALUE(  22, -67)  }
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


void registeralltuners(chessposition *pos);

#endif

#define SCALE_NORMAL 128
#define SCALE_DRAW 0
#define SCALE_ONEPAWN 48
#define SCALE_HARDTOWIN 10

enum EvalType { NOTRACE, TRACE};

//
// utils stuff
//
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

#define STARTFEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

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
#define SCORETBWIN 29900

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
#define ISCASTLE(c) (((c) & 0xe0000249) == 0xc0000000)
#define GIVECHECKFLAG 0x08000000
#define GIVESCHECK(x) ((x) & GIVECHECKFLAG)

#define PAWNATTACK(s, p) ((s) ? (((p) & ~FILEHBB) >> 7) | (((p) & ~FILEABB) >> 9) : (((p) & ~FILEABB) << 7) | (((p) & ~FILEHBB) << 9))
#define PAWNPUSH(s, p) ((s) ? ((p) >> 8) : ((p) << 8))
#define PAWNPUSHINDEX(s, i) ((s) ? (i) - 8 : (i) + 8)

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
    chessmove* getNextMove(int minval);
};


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
#define MAGICBISHOPINDEX(m,x) (int)((((m) & mBishopTbl[x].mask) * mBishopTbl[x].magic) >> (64 - BISHOPINDEXBITS))
#define MAGICROOKINDEX(m,x) (int)((((m) & mRookTbl[x].mask) * mRookTbl[x].magic) >> (64 - ROOKINDEXBITS))
#define MAGICBISHOPATTACKS(m,x) (mBishopAttacks[x][MAGICBISHOPINDEX(m,x)])
#define MAGICROOKATTACKS(m,x) (mRookAttacks[x][MAGICROOKINDEX(m,x)])

extern U64 mBishopAttacks[64][1 << BISHOPINDEXBITS];
extern U64 mRookAttacks[64][1 << ROOKINDEXBITS];

enum MoveType { QUIET = 1, CAPTURE = 2, PROMOTE = 4, TACTICAL = 6, ALL = 7, EVASION = 8, QUIETWITHCHECK = 9 };
enum RootsearchType { SinglePVSearch, MultiPVSearch, PonderSearch };

template <MoveType Mt> int CreateMovelist(chessposition *pos, chessmove* m);
template <MoveType Mt> void evaluateMoves(chessmovelist *ml, chessposition *pos, int16_t **cmptr);

enum AttackType { FREE, OCCUPIED, OCCUPIEDANDKING };

struct positioneval {
    pawnhashentry *phentry;
    int kingattackpiececount[2][7] = { 0 };
    int kingringattacks[2] = { 0 };
    int kingattackers[2];
};


class chessposition
{
public:
    U64 nodes;
    U64 piece00[14];
    U64 occupied00[2];
    U64 attackedBy2[2];
    U64 attackedBy[2][7];
    U64 kingPinned[2];

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
    int nullmoveside;
    int nullmoveply = 0;
    chessmovelist rootmovelist;
    chessmove bestmove;
    int bestmovescore[MAXMULTIPV];
    int lastbestmovescore;
    chessmove pondermove;
    uint32_t killer[MAXDEPTH][2];
    uint32_t countermove[14][64];
    int16_t history[2][64][64];
    int16_t counterhistory[14][64][14*64];
    uint32_t bestFailingLow;
    Pawnhash *pwnhsh;
    int threadindex;
    int psqval;
#ifdef SDEBUG
    unsigned long long debughash = 0;
    uint16_t pvdebug[MAXMOVESEQUENCELENGTH];
    bool debugRecursive;
    bool debugOnlySubtree;
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
    tuneparamselection tps;
    positiontuneset pts;
    evalparam ev[NUMOFEVALPARAMS];
    void resetTuner();
    void getPositionTuneSet(positiontuneset *p, evalparam *e);
    void copyPositionTuneSet(positiontuneset *from, evalparam *efrom, positiontuneset *to, evalparam *eto);
    string getGradientString();
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
    void updatePins();
    bool moveGivesCheck(uint32_t c);  // simple and imperfect as it doesn't handle special moves and cases (mainly to avoid pruning of important moves)
    bool moveIsPseudoLegal(uint32_t c);     // test if move is possible in current position
    uint32_t shortMove2FullMove(uint16_t c); // transfer movecode from tt to full move code without checking if pseudoLegal
    void getpsqval();  // only for eval trace
    template <EvalType Et, int Me> int getGeneralEval(positioneval *pe);
    template <EvalType Et, PieceType Pt, int Me> int getPieceEval(positioneval *pe);
    template <EvalType Et, int Me> int getLateEval(positioneval *pe);
    template <EvalType Et, int Me> void getPawnAndKingEval(pawnhashentry *entry);
    template <EvalType Et> int getEval();
    int getScaling(int col);

    template <RootsearchType RT> int rootsearch(int alpha, int beta, int depth);
    int alphabeta(int alpha, int beta, int depth);
    int getQuiescence(int alpha, int beta, int depth);
    void updateHistory(uint32_t code, int16_t **cmptr, int value);
    void getCmptr(int16_t **cmptr);
    void updatePvTable(uint32_t movecode, bool recursive);
    void updateMultiPvTable(int pvindex, uint32_t movecode, bool recursive);
    string getPv(uint32_t *table);
    int getHistory(uint32_t code, int16_t **cmptr);

#ifdef SDEBUG
    bool triggerDebug(chessmove* nextmove);
    void sdebug(int indent, const char* format, ...);
#endif
    int testRepetiton();
    void mirror();
};

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

//
// engine stuff
//

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
    const char* name = ENGINEVER;
    const char* author = "Andreas Matthies";
    bool isWhite;
    U64 tbhits;
    U64 starttime;
    U64 endtime1; // time to send STOPSOON signal
    U64 endtime2; // time to send STOPPIMMEDIATELY signal
    U64 frequency;
    int wtime, btime, winc, binc, movestogo, mate, movetime, maxdepth;
    U64 maxnodes;
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
    bool Syzygy50MoveRule = true;
    int SyzygyProbeLimit;
    chessposition rootposition;
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
#ifdef TDEBUG
    int t1stop = 0;     // regular stop
    int t2stop = 0;     // immediate stop
    bool bStopCount;
#endif
    GuiToken parse(vector<string>*, string ss);
    void send(const char* format, ...);
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
// TB stuff
//
extern int TBlargest; // 5 if 5-piece tables, 6 if 6-piece tables were found.

void init_tablebases(char *path);
int probe_wdl(int *success, chessposition *pos);
int probe_dtz(int *success, chessposition *pos);
int root_probe_dtz(chessposition *pos);
int root_probe_wdl(chessposition *pos);

