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

#define VERNUM "1.5-dev"

#if 0
#define SDEBUG
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

using namespace std;

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
#define SQVALUE(i, v) eval(1, i, v)
#define SQEVAL(e, f, s) ((e).addGrad(f, s), (e) * (f))
#define SQRESULT(v,s) ( v > 0 ? ((int32_t)((uint32_t)((v) * (v) * S2MSIGN(s) / 2048) << 16) + ((v) * S2MSIGN(s) / 16)) : 0 )

#else // EVALTUNE

#define SQVALUE(i, v) (v)
#define VALUE(m, e) ((int32_t)((uint32_t)(m) << 16) + (e))
#define EVAL(e, f) ((e) * (f))
#define SQEVAL(e, f, s) ((e) * (f))
#define SQRESULT(v,s) ( v > 0 ? VALUE((v) * (v) * S2MSIGN(s) / 2048, (v) * S2MSIGN(s) / 16) : 0 )
typedef const int32_t eval;
#endif

#define PSQTINDEX(i,s) ((s) ? (i) : (i) ^ 0x38)

#define TAPEREDANDSCALEDEVAL(s, p, c) ((GETMGVAL(s) * (256 - (p)) + GETEGVAL(s) * (p) * (c) / SCALE_NORMAL) / 256)

#define NUMOFEVALPARAMS (2*5*4 + 5 + 4*8 + 2*8 + 8 + 2 + 5*6 + 2 + 4*28 + 2 + 7 + 1 + 7 + 6 + 6 + 2 + 7*64)
struct evalparamset {
    // Powered by Laser games :-)
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
    eval eTempo =  VALUE(  30,  27);
    eval ePassedpawnbonus[4][8] = {
        {  VALUE(   0,   0), VALUE(  10,   4), VALUE(   0,   8), VALUE(  10,  25), VALUE(  37,  39), VALUE(  76,  71), VALUE(  44, 108), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE( -19,  -5), VALUE( -15,  12), VALUE(  -5,  17), VALUE(  17,  31), VALUE(  38,  38), VALUE( -12,  11), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   3,   7), VALUE(   6,  12), VALUE(  10,  41), VALUE(  26,  95), VALUE(  67, 200), VALUE( 120, 301), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(   9,   7), VALUE(  -3,  21), VALUE(   1,  39), VALUE(  16,  54), VALUE(  58,  73), VALUE(  34,  55), VALUE(   0,   0)  }
    };
    eval ePotentialpassedpawnbonus[2][8] = {
        {  VALUE(   0,   0), VALUE(  28, -15), VALUE(  -6,   4), VALUE(  13,   5), VALUE(  32,  51), VALUE(  72,  81), VALUE(  44, 108), VALUE(   0,   0)  },
        {  VALUE(   0,   0), VALUE(  -1,  -3), VALUE(   0,  -3), VALUE(   5,   5), VALUE(  21,  33), VALUE(  58,  62), VALUE( -14,  10), VALUE(   0,   0)  }
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
    eval eMobilitybonus[4][28] = {
        {  VALUE(  15, -97), VALUE(  35, -30), VALUE(  48,  -9), VALUE(  54,   2), VALUE(  60,  15), VALUE(  67,  27), VALUE(  73,  27), VALUE(  80,  24),
           VALUE(  83,  17), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(  19, -46), VALUE(  31, -19), VALUE(  48,   3), VALUE(  54,  13), VALUE(  66,  21), VALUE(  72,  27), VALUE(  77,  31), VALUE(  77,  37),
           VALUE(  79,  39), VALUE(  79,  41), VALUE(  79,  38), VALUE(  79,  32), VALUE(  75,  42), VALUE(  75,  20), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE( -60,  13), VALUE(  14,   5), VALUE(  30,  41), VALUE(  33,  54), VALUE(  34,  65), VALUE(  36,  70), VALUE(  38,  74), VALUE(  44,  75),
           VALUE(  46,  82), VALUE(  48,  93), VALUE(  50,  92), VALUE(  48,  95), VALUE(  46, 100), VALUE(  48,  98), VALUE(  44,  98), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0),
           VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0), VALUE(   0,   0)  },
        {  VALUE(-4097,  83), VALUE(   7,-166), VALUE(  -3,  -7), VALUE(   1,  34), VALUE(   1,  58), VALUE(   0, 125), VALUE(   2, 134), VALUE(   7, 128),
           VALUE(   7, 150), VALUE(  13, 141), VALUE(  14, 163), VALUE(  16, 166), VALUE(  17, 173), VALUE(  18, 189), VALUE(  15, 206), VALUE(  20, 206),
           VALUE(  20, 217), VALUE(  22, 211), VALUE(  19, 218), VALUE(  14, 232), VALUE(  49, 201), VALUE(  71, 194), VALUE(  76, 198), VALUE(  83, 187),
           VALUE(  72, 218), VALUE( 118, 187), VALUE( 102, 190), VALUE(  98, 190)  }
    };
    eval eSlideronfreefilebonus[2] = {  VALUE(  21,   7), VALUE(  43,   1)  };
    eval eMaterialvalue[7] = {  VALUE(   0,   0), VALUE( 100, 100), VALUE( 314, 314), VALUE( 314, 314), VALUE( 483, 483), VALUE( 913, 913), VALUE(32509,32509)  };
    eval eKingshieldbonus =  VALUE(  15,  -2);
    eval eWeakkingringpenalty =  SQVALUE(   1,  72);
    eval eKingattackweight[7] = {  SQVALUE(   1,   0), SQVALUE(   1,   0), SQVALUE(   1,  14), SQVALUE(   1,  21), SQVALUE(   1,  14), SQVALUE(   1,  27), SQVALUE(   1,   0)  };
    eval eSafecheckbonus[6] = {  SQVALUE(   1,   0), SQVALUE(   1,   0), SQVALUE(   1, 341), SQVALUE(   1,  55), SQVALUE(   1, 315), SQVALUE(   1, 218)  };
    eval eKingdangerbyqueen =  SQVALUE(   1,-149);
    eval eKingringattack[6] = {  SQVALUE(   1,  54), SQVALUE(   1,   0), SQVALUE(   1,  15), SQVALUE(   1,   0), SQVALUE(   1,   0), SQVALUE(   1, -30)  };
    eval eKingdangeradjust =  SQVALUE(   1,  10);
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
           VALUE( 134,  79), VALUE(  87,  88), VALUE(  94,  57), VALUE( 114,  24), VALUE( 116,  25), VALUE( 114,  43), VALUE(  22,  74), VALUE(  77,  79),
           VALUE( -10,  45), VALUE(  -5,  37), VALUE( -10,  19), VALUE(  -5,  -7), VALUE(   7, -11), VALUE(  19,  -2), VALUE(   0,  36), VALUE(  -6,  31),
           VALUE( -10,  27), VALUE( -17,  22), VALUE( -10,  -3), VALUE(   0, -22), VALUE(   3, -17), VALUE(  -5,  -5), VALUE( -15,  14), VALUE( -11,   9),
           VALUE( -23,  15), VALUE( -25,   9), VALUE(  -6, -12), VALUE(  -5, -18), VALUE(  -2, -15), VALUE(  -2, -12), VALUE( -18,   0), VALUE( -30,   5),
           VALUE( -21,   4), VALUE( -20,   4), VALUE( -10,  -8), VALUE( -13,  -3), VALUE(   3,  -4), VALUE( -11,   2), VALUE(   8, -16), VALUE( -20,  -6),
           VALUE( -25,  10), VALUE( -16,  -2), VALUE( -21,   7), VALUE( -13,  13), VALUE( -10,  11), VALUE(   6,   2), VALUE(  16, -16), VALUE( -23, -14),
           VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999)  },
        {  VALUE(-124, -38), VALUE( -89,   5), VALUE( -21, -24), VALUE( -49, -16), VALUE(   2, -39), VALUE(-115, -15), VALUE(-168,  -5), VALUE( -59, -70),
           VALUE(  12, -17), VALUE(  21,  -9), VALUE(  48, -36), VALUE(  61, -29), VALUE(  17,  -6), VALUE(  86, -41), VALUE(  23,  -2), VALUE(  72, -67),
           VALUE( -14,   5), VALUE(  34, -19), VALUE(  66, -12), VALUE(  60,  -7), VALUE( 115, -38), VALUE(  73, -28), VALUE(  72, -24), VALUE(   0, -30),
           VALUE(  37,  -7), VALUE(  51, -21), VALUE(  53,   0), VALUE(  77,   6), VALUE(  58,  10), VALUE(  80, -10), VALUE(  47,   6), VALUE(  68, -21),
           VALUE(  29, -19), VALUE(  29,   1), VALUE(  55,  -9), VALUE(  62,  -1), VALUE(  72,  -6), VALUE(  66,  -5), VALUE(  73, -10), VALUE(  43,  -7),
           VALUE(   3, -35), VALUE(  18, -24), VALUE(  24, -28), VALUE(  35,  -9), VALUE(  44,  -8), VALUE(  40, -35), VALUE(  43, -30), VALUE(  12, -19),
           VALUE(  -5, -37), VALUE(   6, -17), VALUE(  15, -34), VALUE(  27, -28), VALUE(  32, -24), VALUE(  26, -41), VALUE(  37, -18), VALUE(  19, -28),
           VALUE( -38, -31), VALUE(  14, -45), VALUE(  -7, -14), VALUE(  14,  -2), VALUE(  30, -31), VALUE(  15, -24), VALUE(  15, -30), VALUE( -18, -55)  },
        {  VALUE(  38,   1), VALUE( -54,  20), VALUE(  -6,  -7), VALUE( -52,  13), VALUE( -69,  20), VALUE( -86,  35), VALUE( -60,   0), VALUE( -24,  -8),
           VALUE( -46,  22), VALUE(  21,   5), VALUE(  -1,   2), VALUE( -24,   7), VALUE(  13,   8), VALUE(  -7,   7), VALUE(   9,   4), VALUE( -70,  11),
           VALUE(   0,   5), VALUE(   2,  12), VALUE(  16,  18), VALUE(  29,   4), VALUE(  60,  -5), VALUE(  41,   6), VALUE(   7,  18), VALUE(  25,   6),
           VALUE(   3,  16), VALUE(  38,   5), VALUE(  30,   4), VALUE(  39,  26), VALUE(  32,  24), VALUE(  26,  14), VALUE(  45,   7), VALUE(   6,   6),
           VALUE(  15,  10), VALUE(  14,   4), VALUE(  26,  16), VALUE(  41,  21), VALUE(  50,  12), VALUE(  25,  12), VALUE(  31,   1), VALUE(  44, -18),
           VALUE(  16,  -4), VALUE(  29,   1), VALUE(  16,   9), VALUE(  18,   9), VALUE(  16,  16), VALUE(  32,   2), VALUE(  33,  -9), VALUE(  23,   0),
           VALUE(  39,  -6), VALUE(  24, -21), VALUE(  21, -10), VALUE(  10,  -1), VALUE(  17, -10), VALUE(  29,  -4), VALUE(  45, -21), VALUE(  34, -33),
           VALUE(  15, -11), VALUE(  33, -15), VALUE(  12,   0), VALUE(   5,   6), VALUE(  15,   0), VALUE(   6,   5), VALUE(   3, -23), VALUE(  27, -27)  },
        {  VALUE(  29,  42), VALUE(  14,  56), VALUE(   9,  43), VALUE(  27,  44), VALUE(   0,  54), VALUE( -11,  64), VALUE(  12,  52), VALUE(  37,  52),
           VALUE(   4,  64), VALUE(   1,  66), VALUE(  19,  56), VALUE(  33,  45), VALUE(  14,  43), VALUE(  49,  43), VALUE( -10,  67), VALUE(  13,  56),
           VALUE( -16,  75), VALUE(  11,  68), VALUE(   6,  66), VALUE(  28,  47), VALUE(  57,  38), VALUE(  36,  54), VALUE(  39,  45), VALUE( -11,  58),
           VALUE( -11,  72), VALUE( -10,  71), VALUE(  18,  53), VALUE(  13,  58), VALUE(  34,  47), VALUE(  36,  49), VALUE(   0,  57), VALUE(  11,  51),
           VALUE( -16,  67), VALUE( -20,  64), VALUE(  -7,  58), VALUE(   6,  50), VALUE(   4,  46), VALUE(  22,  35), VALUE(  40,  25), VALUE(  -8,  43),
           VALUE( -19,  50), VALUE(  -1,  44), VALUE( -11,  57), VALUE(   3,  28), VALUE(   4,  31), VALUE(   8,  22), VALUE(  28,  17), VALUE( -11,  32),
           VALUE( -14,  33), VALUE( -13,  36), VALUE(  -5,  42), VALUE(   4,  27), VALUE(  12,  18), VALUE(  17,  17), VALUE(  10,  15), VALUE( -54,  45),
           VALUE(   0,  32), VALUE(   1,  23), VALUE(   1,  31), VALUE(  21,  13), VALUE(  17,  13), VALUE(  20,  14), VALUE(  -5,  25), VALUE(   8,   8)  },
        {  VALUE( -27,  96), VALUE( -11, 105), VALUE(  -3, 135), VALUE(  -9, 104), VALUE( -12, 113), VALUE(  66,  87), VALUE(  -3, 140), VALUE(  50,  68),
           VALUE( -38, 113), VALUE( -40, 119), VALUE(  -1, 107), VALUE(  -8, 114), VALUE( -37, 132), VALUE(   1, 102), VALUE( -11, 115), VALUE(  15,  94),
           VALUE( -24, 122), VALUE(   2, 102), VALUE(  -9, 121), VALUE(   9, 124), VALUE(  19, 113), VALUE(  24, 104), VALUE(  27,  86), VALUE(  11,  98),
           VALUE(  -3, 116), VALUE(  -7, 125), VALUE(   6, 106), VALUE( -15, 132), VALUE(   1, 138), VALUE(  23, 104), VALUE(  12, 154), VALUE(  15, 116),
           VALUE(   5,  90), VALUE(   3, 101), VALUE(   2, 107), VALUE(  -2, 108), VALUE(  24,  97), VALUE(   6, 122), VALUE(  28, 131), VALUE(  20, 117),
           VALUE(  -3,  80), VALUE(  10,  82), VALUE(  -6, 105), VALUE(   2,  89), VALUE(   3,  96), VALUE(   8, 119), VALUE(  37,  62), VALUE(  14,  85),
           VALUE(  -1,  54), VALUE(   8,  54), VALUE(  19,  30), VALUE(  16,  60), VALUE(  16,  57), VALUE(  35,  12), VALUE(  40, -12), VALUE(  27,  -1),
           VALUE(   2,  15), VALUE(   1,  22), VALUE(   8,  31), VALUE(  18,  18), VALUE(   8,  45), VALUE(  -3,  31), VALUE( -26,  40), VALUE( -14,  75)  },
        {  VALUE(  78, -34), VALUE( -38,  32), VALUE( -17,  -7), VALUE(-114,  40), VALUE(  84, -44), VALUE(  14,   0), VALUE( 108, -76), VALUE(  29, -88),
           VALUE( -78,  58), VALUE(   6,  27), VALUE( -11,  19), VALUE( -22,  40), VALUE( -31,  52), VALUE(  11,  58), VALUE(  10,  60), VALUE(  13,   6),
           VALUE( -11,  36), VALUE(  24,  31), VALUE(  33,  52), VALUE(  28,  48), VALUE(   1,  52), VALUE(  43,  54), VALUE(  41,  39), VALUE(  11,   8),
           VALUE( -19,   3), VALUE(  -9,  31), VALUE( -14,  43), VALUE( -14,  60), VALUE( -26,  59), VALUE(  16,  58), VALUE(  35,  27), VALUE( -39,  -1),
           VALUE( -54,   7), VALUE(  19,  10), VALUE( -25,  43), VALUE( -67,  68), VALUE( -31,  64), VALUE( -35,  46), VALUE(   9,  10), VALUE( -60, -10),
           VALUE( -26, -10), VALUE( -10,   4), VALUE(  -8,  25), VALUE( -40,  48), VALUE( -52,  56), VALUE( -22,  34), VALUE(  14,  -4), VALUE( -30, -10),
           VALUE(  44, -34), VALUE(  34, -18), VALUE(   9,   6), VALUE( -57,  32), VALUE( -37,  33), VALUE( -19,  20), VALUE(  10,  -3), VALUE(  20, -25),
           VALUE(  22, -69), VALUE(  51, -66), VALUE(  26, -25), VALUE( -59,   3), VALUE(  -3, -30), VALUE( -42,   1), VALUE(  18, -37), VALUE(  14, -71)  }
    };
};

#ifdef EVALTUNE

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
    int used[NUMOFEVALPARAMS];

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
const int mvv[] = { 0U << 27, 1U << 27, 2U << 27, 2U << 27, 3U << 27, 4U << 27, 5U << 27 };
const int lva[] = { 5 << 24, 4 << 24, 3 << 24, 3 << 24, 2 << 24, 1 << 24, 0 << 24 };
#define PVVAL (7 << 27)
#define KILLERVAL1 (1 << 26)
#define KILLERVAL2 (KILLERVAL1 - 1)
#define NMREFUTEVAL (1 << 25)
#define BADTACTICALFLAG (1 << 30)
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
    void sort();
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
    int capturemovenum;
    int quietmovenum;
    int legalmovenum;
    bool onlyGoodCaptures;
    int16_t *cmptr[CMPLIES];

public:
    void SetPreferredMoves(chessposition *p);  // for quiescence move selector
    void SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, int excludemove);
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
template <MoveType Mt> void evaluateMoves(chessmovelist *ml, chessposition *pos, int16_t **cmptr);

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
    int nullmoveside;
    int nullmoveply = 0;
    chessmovelist rootmovelist;
    chessmove bestmove[MAXMULTIPV];
    int bestmovescore[MAXMULTIPV];
    int lastbestmovescore;
    chessmove pondermove;
    uint32_t killer[2][MAXDEPTH];
    int16_t history[2][64][64];
    int16_t counterhistory[14][64][14*64];
    uint32_t bestFailingLow;
    Pawnhash *pwnhsh;
    repetition rp;
    int threadindex;
#ifdef SDEBUG
    unsigned long long debughash = 0;
    uint16_t pvdebug[MAXMOVESEQUENCELENGTH];
    bool debugRecursive;
    bool debugOnlySubtree;
#endif
    uint32_t pvtable[MAXDEPTH][MAXDEPTH];
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
    bool moveGivesCheck(uint32_t c);  // simple and imperfect as it doesn't handle special moves and cases (mainly to avoid pruning of important moves)
    bool moveIsPseudoLegal(uint32_t c);     // test if move is possible in current position
    uint32_t shortMove2FullMove(uint16_t c); // transfer movecode from tt to full move code without checking if pseudoLegal
    template <EvalType Et> int getPositionValue();
    template <EvalType Et> int getPawnAndKingValue(pawnhashentry **entry);
    template <EvalType Et> int getValue();
    int getScaling(int col);

    template <RootsearchType RT> int rootsearch(int alpha, int beta, int depth);
    int alphabeta(int alpha, int beta, int depth);
    int getQuiescence(int alpha, int beta, int depth);
    void updateHistory(uint32_t code, int16_t **cmptr, int value);
    void getCmptr(int16_t **cmptr);
    void updatePvTable(uint32_t movecode, bool recursive);
    string getPv();
    int getHistory(uint32_t code, int16_t **cmptr);

#ifdef SDEBUG
    bool triggerDebug(chessmove* nextmove);
    void sdebug(int indent, const char* format, ...);
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

