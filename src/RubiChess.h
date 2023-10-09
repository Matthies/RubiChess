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

#define VERNUMLEGACY 2023
#define NNUEDEFAULT nn-c257b2ebf1-20230812.nnue

// enable this switch for faster SSE2 code using 16bit integers
#define FASTSSE2

// Enable to get statistical values about various search features
//#define STATISTICS

// Enable to debug the search against a gives pv
//#define SDEBUG

// Enable to debug the time management
//#define TDEBUG

// Enable to debug tablebase probing
// level 1: only root probing
// level 2: probe_dtz, probe_wdl
// level 3: everything
//#define TBDEBUG 1

// Enable this for texel tuning
//#define EVALTUNE

// Enable this to expose the evaluation and NNUE parameters as UCI options
//#define EVALOPTIONS

// Enable this to expose the search parameters as UCI options
//#define SEARCHOPTIONS

// Enable this to enable NNUE training code
//#define NNUELEARN

// Enable this to enable NNUE debug output
//#define NNUEDEBUG

// Enable this to compile support for asserts including stack trace
// MSVC only, link with DbgHelp.lib
//#define STACKDEBUG

// Enable this to find memory leaks with the MSVC debug build
//#define FINDMEMORYLEAKS


#ifdef FINDMEMORYLEAKS
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
#include <set>
#include <sys/stat.h>
#ifdef USE_ZLIB
#include "zlib/zlib.h"
#endif

#define USE_SIMD
#if defined(USE_SSE2)
#include <immintrin.h>
#elif defined(USE_NEON)
#include <arm_neon.h>
#else
#undef USE_SIMD
#endif

using namespace std;

namespace rubichess {


#ifdef _WIN32

#include <conio.h>
#include <AclAPI.h>
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
void Sleep(long x);
#if defined(__ANDROID__) or defined(__APPLE__)
#define allocalign64(x) malloc(x)
#define freealigned64(x) free(x)
#else
#define allocalign64(x) aligned_alloc(64, x)
#define freealigned64(x) free(x)
#endif
#endif



typedef unsigned long long U64;
typedef signed long long S64;

typedef unsigned int PieceCode;
typedef unsigned int PieceType;


#ifdef _MSC_VER
#if defined(_M_X64) || defined(_M_ARM64)
#define IS_64BIT
#endif
#ifdef EVALTUNE
#define PREFETCH(a) (void)(0)
#else
#ifdef _M_X64
#define PREFETCH(a) _mm_prefetch((char*)(a), _MM_HINT_T0)
#else
#define PREFETCH(a) (void)(0)
#endif
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

#ifndef GITVER
#define VERSION TOSTRING(VERNUMLEGACY)
#else
#define VERSION GITVER
#endif
#define ENGINEVER "RubiChess " VERSION
#ifdef GITID
#define BUILD "commit " GITID " " COMPILER
#else
#define BUILD __DATE__ " " __TIME__ " " COMPILER
#endif

#define BITSET(x) (1ULL << (x))
#define MORETHANONE(x) ((x) & ((x) - 1))
#define ONEORZERO(x) (!MORETHANONE(x))

template  <typename T> int popcount_legacy(T v)
{
    v = v - ((v >> 1) & (T)~(T)0 / 3);
    v = (v & (T)~(T)0 / 15 * 3) + ((v >> 2) & (T)~(T)0 / 15 * 3);
    v = (v + (v >> 4)) & (T)~(T)0 / 255 * 15;
    return  (T)(v * ((T)~(T)0 / 255)) >> (sizeof(T) - 1) * CHAR_BIT;
}

#if defined(_MSC_VER)
#ifdef USE_BMI1
#include <immintrin.h>
#define GETLSB32(i,x) (i =(int) _tzcnt_u32(x))
#ifdef _M_X64
#define GETLSB(i,x) (i =(int) _tzcnt_u64(x))
inline int pullLsb(U64* x) {
    int i;
    i = (int)_tzcnt_u64(*x);
    *x = _blsr_u64(*x);
    return i;
}
#define GETMSB(i,x) (i = 63 ^ (int) _lzcnt_u64(x))
inline int pullMsb(U64* x) {
    int i;
    i = 63 ^ (int)_lzcnt_u64(*x);
    *x ^= (1ULL << i);
    return i;
}
#else // _M_X64
#define GETLSB(i,x) (i =(int) ((x) & 0xffffffff ?  _tzcnt_u32((x) & 0xffffffff) : _tzcnt_u32((x) >> 32) + 32))
inline int pullLsb(U64* x) {
    int i;
    i = (int)(*(x) & 0xffffffff ? _tzcnt_u32(*(x) & 0xffffffff) : _tzcnt_u32(*(x) >> 32) + 32);
    *x ^= (1ULL << i);
    return i;
}
#define GETMSB(i,x) (i = 63 ^ (int) ((x) >> 32 ? _lzcnt_u32((x) >> 32) : _lzcnt_u32((x) & 0xffffffff) + 32))
inline int pullMsb(U64* x) {
    int i;
    i = 63 ^ (int)((*(x) >> 32) ? _lzcnt_u32(*(x) >> 32) : _lzcnt_u32(*(x) & 0xffffffff) + 32);
    *x ^= (1ULL << i);
    return i;
}
#endif
#else // USE_BMI1

#define GETLSB32(i,x) _BitScanForward((DWORD*)&(i), (x))
#if defined(_M_X64) || defined(_M_ARM64)
#define GETLSB(i,x) _BitScanForward64((DWORD*)&(i), (x))
inline int pullLsb(U64* x) {
    DWORD i;
    _BitScanForward64(&i, *x);
    *x &= *x - 1;
    return i;
}
#define GETMSB(i,x) _BitScanReverse64((DWORD*)&(i), (x))
inline int pullMsb(U64* x) {
    DWORD i;
    _BitScanReverse64(&i, *x);
    *x ^= (1ULL << i);
    return i;
}
#else // _M_X64 || _M_ARM64
#define GETLSB(i,x) ((x) & 0xffffffff ? _BitScanForward((DWORD*)&(i), (x) & 0xffffffff) : (_BitScanForward((DWORD*)&(i), (x) >> 32), i += 32))
inline int pullLsb(U64* x) {
    DWORD i;
    (*(x) & 0xffffffff ? _BitScanForward(&i, *(x) & 0xffffffff) : (_BitScanForward(&i, *(x) >> 32), i += 32));
    *x ^= (1ULL << i);
    return i;
}
#define GETMSB(i,x) (((x) >> 32) ? (_BitScanReverse((DWORD*)&i, (x) >> 32), (i) += 32) : _BitScanReverse((DWORD*)&i, (x) & 0xffffffff))
inline int pullMsb(U64* x) {
    DWORD i;
    (*(x) >> 32 ? _BitScanReverse(&i, *(x) >> 32) : (_BitScanReverse(&i, *(x) & 0xffffffff), i += 32));
    *x ^= (1ULL << i);
    return i;
}



#endif
#endif
#if !defined(__clang_major__) && (defined(_M_ARM) || defined(_M_ARM64))
#define POPCOUNT(x) popcount_legacy(x)
#define POPCOUNT32(x) popcount_legacy(x)
#else
#ifdef _M_X64
#define POPCOUNT(x) (int)(__popcnt64(x))
#define POPCOUNT32(x) __popcnt(x)
#else
#define POPCOUNT(x) (int)(__popcnt((unsigned int)((x) >> 32)) + __popcnt((unsigned int)((x) & 0xffffffff)))
#define POPCOUNT32(x) __popcnt(x)
#endif
#endif
#else // _MSC_VER
#ifdef USE_BMI1
#define GETLSB32(i,x) (i =(int) _tzcnt_u32(x))
#ifdef IS_64BIT
#define GETLSB(i,x) (i =  _tzcnt_u64(x))
inline int pullLsb(U64* x) {
    int i = _tzcnt_u64(*x);
    *x = _blsr_u64(*x);
    return i;
}
#define GETMSB(i,x) (i = (63 ^ _lzcnt_u64(x)))
inline int pullMsb(U64* x) {
    int i = 63 ^ _lzcnt_u64(*x);
    *x ^= (1ULL << i);
    return i;
}
#else // IS_64BIT
#define GETLSB(i,x) (i =(int) ((x) & 0xffffffff ?  _tzcnt_u32((x) & 0xffffffff) : _tzcnt_u32((x) >> 32) + 32))
inline int pullLsb(U64* x) {
    int i;
    i = (int)(*(x) & 0xffffffff ? _tzcnt_u32(*(x) & 0xffffffff) : _tzcnt_u32(*(x) >> 32) + 32);
    *x ^= (1ULL << i);
    return i;
}
#define GETMSB(i,x) (i = 63 ^ (int) ((x) >> 32 ? _lzcnt_u32((x) >> 32) : _lzcnt_u32((x) & 0xffffffff) + 32))
inline int pullMsb(U64* x) {
    int i;
    i = 63 ^ (int)((*(x) >> 32) ? _lzcnt_u32(*(x) >> 32) : _lzcnt_u32(*(x) & 0xffffffff) + 32);
    *x ^= (1ULL << i);
    return i;
}
#endif
#else
#define GETLSB(i,x) (i = __builtin_ctzll(x))
#define GETLSB32(i,x) (i =(int) __builtin_ctz(x))
inline int pullLsb(U64* x) {
    int i = __builtin_ctzll(*x);
    *x &= *x - 1;
    return i;
}
#define GETMSB(i,x) (i = (63 ^ __builtin_clzll(x)))
inline int pullMsb(U64* x) {
    int i = 63 - __builtin_clzll(*x);
    *x ^= (1ULL << i);
    return i;
}
#endif
#define POPCOUNT(x) __builtin_popcountll(x)
#define POPCOUNT32(x) __builtin_popcount(x)
#endif

enum Color { WHITE, BLACK };
#define WHITEBB 0x55aa55aa55aa55aa
#define BLACKBB 0xaa55aa55aa55aa55
#define FLANKLEFT  0x0f0f0f0f0f0f0f0f
#define FLANKRIGHT 0xf0f0f0f0f0f0f0f0
#define CENTER 0x0000001818000000
#define CORNERS 0x8100000000000081
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


// Forward definitions
class transposition;
class chessposition;
class searchthread;
struct pawnhashentry;

// some general constants
#define MAXMULTIPV 64
#define MAXTHREADS  256
#define MAXHASH     0x100000  // 1TB ... never tested
#define DEFAULTHASH 16

#define MAXDEPTH 256
#define MOVESTACKRESERVE 48     // to avoid checking for height reaching MAXDEPTH in probe_wds and getQuiescence
#define PREROOTMOVES 128        // max. 100 moves are stored for repetition detection

#define NOSCORE SHRT_MIN
#define SCOREBLACKWINS (SHRT_MIN + 3 + 2 * MAXDEPTH)
#define SCOREWHITEWINS (-SCOREBLACKWINS)
#define SCOREDRAW 0
#define SCORETBWIN 29900
#define SCORETBWININMAXPLY (SCORETBWIN - MAXDEPTH)

#define MATEFORME(s) ((s) > SCOREWHITEWINS - MAXDEPTH)
#define MATEFOROPPONENT(s) ((s) < SCOREBLACKWINS + MAXDEPTH)
#define MATEDETECTED(s) (MATEFORME(s) || MATEFOROPPONENT(s))
#define MATEIN(s) (s > 0 ? (SCOREWHITEWINS - s + 1) / 2 : (SCOREBLACKWINS - s) / 2)

const int phasefactor[] = { 0, 0, 1, 1, 2, 4, 0 };

//
// eval stuff
//

#define GETMGVAL(v) ((int16_t)(((uint32_t)(v) + 0x8000) >> 16))
#define GETEGVAL(v) ((int16_t)((v) & 0xffff))


#if defined(EVALTUNE) || defined(EVALOPTIONS)
#define EPSCONST
#else
#define EPSCONST const
#endif

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
    eval eNocastlepenalty = VALUE(-5, 5);
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

void registerallevals(chessposition* pos = nullptr);
void initPsqtable();
void initBitmaphelper();

#define SCALE_NORMAL 128
#define SCALE_DRAW 0
#define SCALE_ONEPAWN 48
#define SCALE_HARDTOWIN 10
#define SCALE_OCB 32

enum EvalType { NOTRACE, TRACE };

const int uciscorescaling = 285;    // = 2^16 / (score in cp of 50%-win-prob. which can be calculated with https://github.com/vondele/WLD_model )
#define UCISCORE(v) (((v) * uciscorescaling * 100) >> 16)


//
// Texel tuning related stuff
//
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

struct precalculated {
    int32_t v;
    int complexity;
    int32_t vi;
    uint16_t index;
    int16_t g;
};

struct tuner {
    thread thr;
    bool inUse;
    int paramindex;
    int iteration;
    eval ev[NUMOFEVALPARAMS];
    int paramcount;
    double starterror;
    double error;
    bool busy = false;
    precalculated* precalcptr;
};

struct tunerpool {
    int lastImprovedIteration;
    int iThreads;
    tuner tn[MAXTHREADS];
    vector<bool> tuninginprogress;
};

extern int tuningratio;
extern string pgnfilename;
extern string fentuningfiles;
extern bool quietonly;
extern string correlationlist;
extern int ppg;

bool PGNtoFEN(int depth);
void getCoeffsFromFen(string fenfilenames);
void TexelTune();
void parseTune(vector<string> commandargs);
void tuneInit();
void tuneCleanup();
typedef void(*initevalfunc)(void);

#endif

//
// utils stuff
//
typedef struct ranctx { U64 a; U64 b; U64 c; U64 d; } ranctx;

void raninit(ranctx* x, U64 seed);
U64 ranval(ranctx* x);
string frcStartFen(int numWhite = -1, int numBlack = -1);
U64 calc_key_from_pcs(int *pcs, int mirror);
void getPcsFromStr(const char* str, int *pcs);
U64 calc_key_from_str(const char* str);
void getFenAndBmFromEpd(string input, string *fen, string *bm, string *am);
vector<string> SplitString(const char* s);
unsigned char AlgebraicToIndex(string s);
string IndexToAlgebraic(int i);
void BitboardDraw(U64 b);
U64 getTime();
string CurrentWorkingDir();
#ifdef _WIN32
void* my_large_malloc(size_t s);
void my_large_free(void *m);
#else
#define my_large_malloc(x) allocalign64(x)
#define my_large_free(m) freealigned64(m)
#endif
#ifdef STACKDEBUG
void GetStackWalk(chessposition *pos, const char* message, const char* _File, int Line, int num, ...);
#endif


//
// NNUE stuff
//
#define NNUEDEFAULTSTR TOSTRING(NNUEDEFAULT)

enum NnueType { NnueDisabled = 0, NnueArchV1, NnueArchV5 };

// The following constants were introduced in original NNUE port from Shogi
#define NNUEFILEVERSIONROTATE       0x7AF32F16u
#define NNUEFILEVERSIONNOBPZ        0x7AF32F17u
#define NNUEFILEVERSIONSFNNv5_1024  0x7af32f20u
#define NNUEFILEVERSIONSFNNv5_512   0x7af32f30u
#define NNUEFILEVERSIONSFNNv5_768   0x7af32f31u
#define NNUENETLAYERHASH            0xCC03DAE4u
#define NNUECLIPPEDRELUHASH         0x538D24C7u
#define NNUEFEATUREHASH_HalfKP      0x5D69D5B8u
#define NNUEFEATUTEHASH_HalfKAv2_hm 0x7f234cb8u
#define NNUEINPUTSLICEHASH          0xEC42E90Du

#define ORIENT(c,i) ((c) ? (i) ^ 0x3f : (i))
#define HMORIENT(c,i,k) (i ^ (bool(c) * 56) ^ ((FILE(k) < 4) * 7))
#define MULTIPLEOFN(i,n) (((i) + (n - 1)) / n * n)

#if defined(USE_SSE2) && !defined(USE_SSSE3) && defined FASTSSE2
// for native SSE2 platforms we have faster intrinsics for 16bit integers
#define USE_FASTSSE2
typedef int16_t weight_t;
typedef int16_t clipped_t;
#else
typedef int8_t weight_t;
typedef int8_t clipped_t;
#endif

// All pieces and both kings for HalfKA are inputs => 32 dimensions
typedef struct {
    size_t size;
    unsigned values[32];
} NnueIndexList;

typedef struct {
    int dirtyNum;
    PieceCode pc[3];
    int from[3];
    int to[3];
} DirtyPiece;


class NnueNetsource {
public:
    ~NnueNetsource() {
        if (readbuffer)
            freealigned64(readbuffer);
        readbuffer = nullptr;
    };
    unsigned char* readbuffer;
    size_t readbuffersize;
    unsigned char* next;
    bool open();
    bool read(unsigned char* target, size_t readsize);
    bool write(unsigned char* source, size_t writesize);
    bool endOfNet();
};


class NnueArchitecture
{
public:
    virtual bool ReadFeatureWeights(NnueNetsource* nr, bool bpz) = 0;
    virtual bool ReadWeights(NnueNetsource* nr, uint32_t nethash) = 0;
    virtual void WriteFeatureWeights(NnueNetsource* nr, bool bpz) = 0;
    virtual void WriteWeights(NnueNetsource* nr, uint32_t nethash) = 0;
    virtual void RescaleLastLayer(int ratio64) = 0;
    virtual string GetArchName() = 0;
    virtual string GetArchDescription() = 0;
    virtual uint32_t GetFtHash() = 0;
    virtual uint32_t GetHash() = 0;
    virtual int GetEval(chessposition* pos) = 0;
    virtual int16_t* GetFeatureWeight() = 0;
    virtual int16_t* GetFeatureBias() = 0;
    virtual int32_t* GetFeaturePsqtWeight() = 0;
    virtual uint32_t GetFileVersion() = 0;
    virtual int16_t* CreateAccumulationStack() = 0;
    virtual int32_t* CreatePsqtAccumulationStack() = 0;
    virtual unsigned int GetAccumulationSize() = 0;
    virtual unsigned int GetPsqtAccumulationSize() = 0;
    virtual size_t GetNetworkFilesize() = 0;
#ifdef STATISTICS
    virtual void SwapInputNeurons(unsigned int i1, unsigned int i2) = 0;
    virtual void Statistics(bool verbose, bool sort) = 0;
#endif
};


extern NnueType NnueReady;
extern NnueArchitecture* NnueCurrentArch;

#ifdef NNUEINCLUDED
extern const char  _binary_net_nnue_start;
extern const char  _binary_net_nnue_end;
#endif


class NnueLayer
{
public:
    NnueLayer* previous;
#if defined(USE_AVX2)
    static constexpr size_t SimdWidth = 32;
#else
    static constexpr size_t SimdWidth = 16;
#endif

    NnueLayer(NnueLayer* prev) { previous = prev; }
    virtual bool ReadWeights(NnueNetsource* nr) = 0;
    virtual void WriteWeights(NnueNetsource* nr) = 0;
    virtual uint32_t GetHash() = 0;
};


template <int ftdims, int inputdims, int psqtbuckets>
class NnueFeatureTransformer : public NnueLayer
{
public:
    alignas(64) int16_t bias[ftdims];
    alignas(64) int16_t weight[ftdims * inputdims];
    alignas(64) int32_t psqtWeights[psqtbuckets ? psqtbuckets * inputdims : 1];    // hack to avoid zero-sized array

    NnueFeatureTransformer() : NnueLayer(NULL) {}
    bool ReadFeatureWeights(NnueNetsource* nr, bool bpz);
    bool ReadWeights(NnueNetsource* nr) {
        if (previous) return previous->ReadWeights(nr);
        return true;
    }
    void WriteFeatureWeights(NnueNetsource* nr, bool bpz);
    void WriteWeights(NnueNetsource* nr) {
        if (previous)
            previous->WriteWeights(nr);
    }
    uint32_t GetFtHash(NnueType nt) {
        if (nt == NnueArchV5)
            return NNUEFEATUTEHASH_HalfKAv2_hm;
        else
            return NNUEFEATUREHASH_HalfKP;
    }
    uint32_t GetHash() {
        return NNUEINPUTSLICEHASH ^ (ftdims * 2);
    };
#ifdef STATISTICS
    void SwapWeights(unsigned int i1, unsigned int i2) {
        int16_t bias_temp = bias[i1];
        bias[i1] = bias[i2];
        bias[i2] = bias_temp;
        for (unsigned int i = 0; i < inputdims; i++)
        {
            int offset = i * ftdims;
            int16_t weight_temp = weight[offset + i1];
            weight[offset + i1] = weight[offset + i2];
            weight[offset + i2] = weight_temp;
        }
    }
#endif
};

template <unsigned int dims, unsigned int clippingshift>
class NnueClippedRelu : public NnueLayer
{
public:
    NnueClippedRelu(NnueLayer* prev) : NnueLayer(prev) {}
    bool ReadWeights(NnueNetsource* nr) {
        return (previous ? previous->ReadWeights(nr) : true);
    }
    void WriteWeights(NnueNetsource* nr) {
        if (previous)
            previous->WriteWeights(nr);
    }
    uint32_t GetHash() {
        return NNUECLIPPEDRELUHASH + previous->GetHash();
    }
    void Propagate(int32_t *input, clipped_t *output);
};

template <unsigned int dims>
class NnueSqrClippedRelu : public NnueLayer
{
public:
    NnueSqrClippedRelu(NnueLayer* prev) : NnueLayer(prev) {}
    bool ReadWeights(NnueNetsource* nr) {
        return (previous ? previous->ReadWeights(nr) : true);
    }
    void WriteWeights(NnueNetsource* nr) {
        if (previous)
            previous->WriteWeights(nr);
    }
    uint32_t GetHash() {
        return NNUECLIPPEDRELUHASH + previous->GetHash();
    }
    void Propagate(int32_t* input, clipped_t* output);
};

template <unsigned int inputdims, unsigned int outputdims>
class NnueNetworkLayer : public NnueLayer
{
#if defined (USE_AVX512)
    static constexpr unsigned int InputSimdWidth = 64;
    static constexpr unsigned int MaxNumOutputRegs = 16;
#elif defined (USE_AVX2)
    static constexpr unsigned int InputSimdWidth = 32;
    static constexpr unsigned int MaxNumOutputRegs = 8;
#elif defined (USE_SSSE3)
    static constexpr unsigned int InputSimdWidth = 16;
    static constexpr unsigned int MaxNumOutputRegs = 8;
#elif defined (USE_NEON)
    static constexpr unsigned int InputSimdWidth = 8;
    static constexpr unsigned int MaxNumOutputRegs = 8;
#else
    static constexpr unsigned int InputSimdWidth = 1;
    static constexpr unsigned int MaxNumOutputRegs = 1;
#endif

    static constexpr unsigned int paddedInputdims = MULTIPLEOFN(inputdims, 32);
    static constexpr unsigned int paddedOutputdims = MULTIPLEOFN(outputdims, 32);
    static constexpr unsigned int NumOutputRegsBig = (MaxNumOutputRegs < outputdims ? MaxNumOutputRegs : outputdims);
    static constexpr unsigned int NumOutputRegsSmall = (outputdims / (SimdWidth / 4) > 1 ? outputdims / (SimdWidth / 4) : 1);
    static constexpr unsigned int SmallBlockSize = InputSimdWidth;
    static constexpr unsigned int BigBlockSize = NumOutputRegsBig * paddedInputdims;
    static constexpr unsigned int NumSmallBlocksInBigBlock = (BigBlockSize < SmallBlockSize ? 1 : BigBlockSize / SmallBlockSize);   // avoid warning of MSVC compiler
    static constexpr unsigned int NumSmallBlocksPerOutput = (paddedInputdims < SmallBlockSize ? 1 : paddedInputdims / SmallBlockSize);
    static constexpr unsigned int NumBigBlocks = outputdims / NumOutputRegsBig;
    static constexpr unsigned int OutputSimdWidth = SimdWidth / 4;

#if defined(USE_SSSE3) || defined(USE_ARM64)
#define USE_PROPAGATESPARSE
    static constexpr bool useSparsePropagation = (paddedInputdims >= 512);
    void PropagateSparse(clipped_t* input, int32_t* output);
#else
    static constexpr bool useSparsePropagation = false;
#endif
#if defined(USE_SSSE3)
#define USE_PROPAGATESMALL
    static constexpr bool useSmallLayerPropagation = (paddedInputdims < 128);
#else
    static constexpr bool useSmallLayerPropagation = false;
#endif
#if  defined (USE_SSSE3) || defined(USE_NEON)
#define USE_PROPAGATEBIG
#if defined (USE_SSSE3)
    static constexpr bool useShuffledWeights = true;
#else
    static constexpr bool useShuffledWeights = useSparsePropagation;
#endif
    static constexpr bool useBigLayerPropagation = (!useSparsePropagation && paddedInputdims >= 128);
#else
    static constexpr bool useBigLayerPropagation = false;
    static constexpr bool useShuffledWeights = false;

#endif

public:
    alignas(64)int32_t bias[outputdims];
    alignas(64)weight_t weight[paddedInputdims * outputdims];
#ifdef STATISTICS
    U64 nonzeroevals[paddedInputdims] = { 0 };
    U64 total_evals = 0;
    U64 total_count = 0;
    void SwapWeights(unsigned int i1, unsigned int i2) {
        for (int i = 0; i < outputdims; i++)
        {
            int offset = i * inputdims;
            weight_t weight_temp = weight[shuffleWeightIndex(offset + i1)];
            weight[shuffleWeightIndex(offset + i1)] = weight[shuffleWeightIndex(offset + i2)];
            weight[shuffleWeightIndex(offset + i2)] = weight_temp;
        }
    }
#endif

    NnueNetworkLayer(NnueLayer* prev) : NnueLayer(prev) {}
    bool ReadWeights(NnueNetsource* nr);
    bool OverflowPossible();
    void WriteWeights(NnueNetsource* nr);
    uint32_t GetHash() {
        return (NNUENETLAYERHASH + outputdims) ^ (previous->GetHash() >> 1) ^ (previous->GetHash() << 31);
    }
    void Propagate(clipped_t *input, int32_t *output);
    void PropagateBigLayer(clipped_t* input, int32_t* output);
    void PropagateSmallLayer(clipped_t* input, int32_t* output);
    void PropagateNative(clipped_t* input, int32_t* output);
    inline unsigned int shuffleWeightIndex(unsigned int idx)
    {
        if (useBigLayerPropagation)
        {
            const int smallBlock = (idx / SmallBlockSize) % NumSmallBlocksInBigBlock;
            const int smallBlockCol = smallBlock / NumSmallBlocksPerOutput;
            const int smallBlockRow = smallBlock % NumSmallBlocksPerOutput;
            const int bigBlock = idx / BigBlockSize;
            const int rest = idx % SmallBlockSize;

            return bigBlock * BigBlockSize
                + smallBlockRow * SmallBlockSize * NumOutputRegsBig
                + smallBlockCol * SmallBlockSize
                + rest;
        }
        else {
            if (useShuffledWeights)
                return (idx / 4) % (paddedInputdims / 4) * outputdims * 4 + idx / paddedInputdims * 4 + idx % 4;
            else
                return idx;
        }
    }
};


void NnueInit();
void NnueRemove();
bool NnueReadNet(NnueNetsource* nr);
void NnueWriteNet(vector<string> args);
#ifdef EVALOPTIONS
void NnueRegisterEvals();
#endif

struct PackedSfen { uint8_t data[32]; };

struct PackedSfenValue
{
    PackedSfen sfen;
    int16_t score;
    uint16_t move;
    uint16_t gamePly;
    int8_t game_result;
    uint8_t padding;
};

struct Binpack
{
    char *base;
    char **data = nullptr;
    char *flushAt = nullptr;
    uint8_t consumedBits = 0;
    int16_t score;
    int16_t lastScore;
    uint16_t move;      // binpack encoded move
    uint16_t gamePly;
    int8_t gameResult;
    uint16_t compressedmoves = 0;
    char *compmvsptr;
    uint32_t fullmove;
    uint32_t lastFullmove;
    chessposition *inpos;
    chessposition *outpos;
    bool debug() { return false; /*size_t offset = *data - base; return numChunks == 3 && offset > 0x170 && offset < 0x200; */ }
};

void gensfen(vector<string> args);
void convert(vector<string> args);
void learn(vector<string> args);

//
// transposition stuff
//
#define BOUNDMASK       0x03
#define HASHALPHA       0x01
#define HASHBETA        0x02
#define HASHUNKNOWN     0x00
#define HASHEXACT       0x03
#define AGESHIFT        2
#define AGEINC          (1 << AGESHIFT)
#define AGEMASK         ((0xff << AGESHIFT) & 0xff)
#define AGECYCLE        (255 + AGEINC)
#define TTDEPTH_OFFSET  -1  // we don't save negative depth to tt so -1 should be okay to detect free entries by testing depth == 0

class zobrist
{
public:
    U64 boardtable[64 * 16];
    U64 cstl[32];
    U64 ept[64];
    U64 s2m;
    ranctx rnd;
    zobrist();
    U64 getRnd();
    U64 getHash(chessposition *pos);
    U64 getPawnHash(chessposition *pos);
    U64 getMaterialHash(chessposition *pos);
};

#define TTBUCKETNUM 3

typedef uint16_t hashupper_t;
#define GETHASHUPPER(x) (hashupper_t)((x) >> (64 - sizeof(hashupper_t) * 8))

struct ttentry {
    hashupper_t hashupper;
    uint16_t movecode;
    int16_t value;
    int16_t staticeval;
    uint8_t depth;
    uint8_t boundAndAge;
};

struct transpositioncluster {
    ttentry entry[TTBUCKETNUM];
    uint8_t padding[(64 - sizeof(ttentry) * TTBUCKETNUM) % 16];
#ifdef SDEBUG
    U64 debugHash;
    int debugIndex;
    string debugStoredBy;
#endif
};


#define FIXMATESCOREPROBE(v,p) (MATEFORME(v) ? (v) - p : (MATEFOROPPONENT(v) ? (v) + p : v))
#define FIXMATESCOREADD(v,p) (MATEFORME(v) ? (v) + p : (MATEFOROPPONENT(v) ? (v) - p : v))
#define FIXDEPTHFROMTT(d) (d + TTDEPTH_OFFSET)

class transposition
{
public:
    transpositioncluster *table;
    size_t size;
    size_t sizemask;
    uint8_t numOfSearchShiftTwo;
    ~transposition();
    int setSize(int sizeMb);    // returns the number of Mb not used by allignment
    void clean();
    void addHash(ttentry* entry, U64 hash, int val, int16_t staticeval, int bound, int depth, uint16_t movecode);
    void printHashentry(U64 hash);
    ttentry* probeHash(U64 hash, bool *bFound);
    uint16_t getMoveCode(U64 hash);
    unsigned int getUsedinPermill();
    void nextSearch() { numOfSearchShiftTwo = (numOfSearchShiftTwo + AGEINC) & AGEMASK; }
#ifdef SDEBUG
    void markDebugSlot(U64 h, int i) {
        table[h & sizemask].debugHash = h; table[h & sizemask].debugIndex = i;
    }
    int isDebugPosition(U64 h) { return (h != table[h & sizemask].debugHash) ? -1 : table[h & sizemask].debugIndex; }
    string debugGetPv(U64 h, int p) {
        transpositioncluster* data = &table[h & sizemask];
        for (int i = 0; i < TTBUCKETNUM; i++)
        {
            ttentry *e = &(data->entry[i]);
            if (e->hashupper == GETHASHUPPER(h))
                return "Depth=" + to_string(e->depth) + " Value=" + to_string(FIXMATESCOREPROBE(e->value, p)) + "(" + to_string(e->boundAndAge & BOUNDMASK) + ")  pv=" + data->debugStoredBy;
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
#define GETCASTLERIGHTS(c,s) ((c) ? (s) & (BQCMASK | BKCMASK) : (s) & (WQCMASK | WKCMASK))

#define WQC 1
#define WKC 2
#define BQC 3
#define BKC 4
const int QCMASK[2] = { WQCMASK, BQCMASK };
const int KCMASK[2] = { WKCMASK, BKCMASK };
const int castlerookto[4] = { 3, 5, 59, 61 };
const int castlekingto[4] = { 2, 6, 58, 62 };


/* Offsets for 64Bit  board*/
const int knightoffset[] = { -6, -10, -15, -17, 6, 10, 15, 17 };
const int diagonaloffset[] = { -7, -9, 7, 9 };
const int orthogonaloffset[] = { -8, -1, 1, 8 };
const int orthogonalanddiagonaloffset[] = { -8, -1, 1, 8, -7, -9, 7, 9 };

const struct { int offset; bool needsblank; } pawnmove[] = { { 0x10, true }, { 0x0f, false }, { 0x11, false } };
extern const int materialvalue[];
extern int psqtable[14][64];
extern EPSCONST evalparamset eps;

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
//       x                          bad see flag
//          xxx                     EP capture target rank (file in to/from)
//           xx                     castle index (00=white queen 01=white king 10=black queen 11=black king)
//             xxxx                 captured piece code
//                 xxxx             promotion piece code
//                     xxxxxx       from square
//                           xxxxxx to square
class chessmove
{
public:
    uint32_t code;
    int value;

    chessmove();
    chessmove(uint32_t c);
    chessmove(int from, int to, PieceCode piece);
    chessmove(int from, int to, PieceCode capture, PieceCode piece);
    chessmove(int from, int to, PieceCode promote, PieceCode capture, PieceCode piece);
    chessmove(int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece);

    bool operator<(const chessmove cm) const { return (value < cm.value); }
    bool operator>(const chessmove cm) const { return (value > cm.value); }
    string toString();
    void print();
};


#define CASTLEFLAG    0x08000000
#define EPCAPTUREFLAG 0x04000000
#define BADSEEFLAG    0x02000000
#define GETFROM(x) (((x) & 0x0fc0) >> 6)
#define GETTO(x) ((x) & 0x003f)
#define GETCORRECTTO(x) (ISCASTLE(x) ? castlekingto[GETCASTLEINDEX(x)] : GETTO(x))
#define GETEPT(x) (((x) & 0x00700000) ? (((x) & 0x00700000) >> 17) | ((x) & 0x07) : 0)
#define ADDEPT(f,t) ((((f) + (t)) / 16) << 20)
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

struct chessmovestack
{
    int state;
    uint8_t ept;
    uint8_t kingpos[2];
    U64 hash;
    U64 pawnhash;
    U64 materialhash;
    int halfmovescounter;
    int fullmovescounter;
    U64 isCheckbb;
    U64 kingPinned;
    int lastnullmove;
    unsigned int threatSquare;
};

#define MAXMOVELISTLENGTH 256   // for lists of possible pseudo-legal moves

string moveToString(uint32_t mc);

class chessmovelist
{
public:
    int length;
    chessmove move[MAXMOVELISTLENGTH];
#ifdef SDEBUG
    int lastvalue;
#endif
	chessmovelist();
	string toString();
	void print();
    chessmove* getNextMove(int minval = INT_MIN);
    uint32_t getAndRemoveNextMove();
};


enum MoveSelector_State { HASHMOVESTATE, TACTICALINITSTATE, TACTICALSTATE, KILLERMOVE1STATE, KILLERMOVE2STATE,
    COUNTERMOVESTATE, QUIETINITSTATE, QUIETSTATE, BADTACTICALSTATE, BADTACTICALEND, EVASIONINITSTATE, EVASIONSTATE };

class MoveSelector
{
public:
    chessposition *pos;
    chessmovelist* captures;
    chessmovelist* quiets;
    int state;
    bool onlyGoodCaptures;
    uint32_t hashmove;
    uint32_t killermove1;
    uint32_t killermove2;
    uint32_t countermove;
    int margin;
    char padding[8];
#ifdef SDEBUG
    int value;
#endif
#ifdef STATISTICS
    int depth;
    int PvNode;
    int numOfQuiets;
    int numOfCaptures;
    int numOfEvasions;
#endif
    void SetPreferredMoves(chessposition *p);  // for quiescence move selector
    void SetPreferredMoves(chessposition *p, int m, int excludemove);  // for probcut move selector
    void SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, uint32_t counter, int excludemove);
    uint32_t next();
};

extern U64 pawn_attacks_to[64][2];
extern U64 pawn_attacks_from[64][2];
extern U64 knight_attacks[64];
extern U64 king_attacks[64];
extern U64 pawn_moves_to[64][2];
extern U64 pawn_moves_to_double[64][2];
extern U64 epthelper[64];

struct SMagic {
    U64 mask;  // to mask relevant squares of both lines (no outer squares)
    U64 magic; // magic 64-bit factor
};

extern SMagic mBishopTbl[64];
extern SMagic mRookTbl[64];

#define BISHOPINDEXBITS 9
#define ROOKINDEXBITS 12

#if defined(USE_BMI2) && (defined(_M_X64) || defined(IS_64BIT))
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
enum PruneType { Prune, MatePrune, NoPrune };

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
    PVA_FUTILITYPRUNED, PVA_SEEPRUNED, PVA_BADHISTORYPRUNED, PVA_MULTICUT, PVA_BESTMOVE, PVA_NOTBESTMOVE, PVA_OMITTED, PVA_BETACUT, PVA_BELOWALPHA,
    PVA_CHECHMATE, PVA_STALEMATE
};
#endif

// Replace the occupied bitboards with the first two so far unused piece bitboards
#define occupied00 piece00

class chessposition
{
public:
    // everything up to member 'history' is copied from rootpos to every thread's position in engine::prepareThreads()
    int ply;
    int piececount;
    U64 piece00[14];
    U64 attackedBy2[2];
    U64 attackedBy[2][7];
    uint8_t mailbox[BOARDSIZE];
    U64 threats;

    // The following block is mapped/copied to the movestack, so its important to keep the order
    int state;
    uint8_t ept;
    uint8_t kingpos[2];
    U64 hash;
    U64 pawnhash;
    U64 materialhash;
    int halfmovescounter;
    int fullmovescounter;
    U64 isCheckbb;
    U64 kingPinned;
    int lastnullmove;
    unsigned int threatSquare;

    int prerootmovenum;
    chessmovelist rootmovelist;
    uint32_t killer[MAXDEPTH][2];   // Hmmm. killer[0][] not initialized/reset to 0??
    uint32_t bestFailingLow;        // Hmmm. bestFailingLow not initialized/reset to 0??
    int failhighcount[MAXDEPTH];
    int psqval;
    int phcount;                    // weighted number of pieces (0..24)
    int contempt;
    int useTb;
    int useRootmoveScore;
    int tbPosition;
    uint32_t defaultmove;           // only mainthread needs it  -- fallback if search in time trouble didn't finish a single iteration
    int castlerights[64];
    int castlerookfrom[4];
    U64 castleblockers[4];
    U64 castlekingwalk[4];
#ifdef SDEBUG
    U64 debughash = 0;
    uint32_t pvmovecode[MAXDEPTH];
#endif

    // The following part of the chessposition object is reset via resetStats()
    int16_t history[2][65][64][64];
    int16_t counterhistory[14][64][14 * 64];
    int16_t tacticalhst[7][64][6];
    uint32_t countermove[14][64];
    int16_t* prerootconthistptr[4];
    int16_t* conthistptr[MAXDEPTH];
    int he_threshold;
    U64 he_yes;
    U64 he_all;

    // The following members get an explicit init in engine::prepareThreads()
    U64 nodes;
    U64 tbhits;
    int nullmoveside;
    int nullmoveply;
    int nodesToNextCheck;
    uint32_t bestmove;
    int threadindex;                                // to signal that thread is alive
    int bestmovescore[MAXMULTIPV];                  // init only for [0]; maybe better in search?
    uint32_t pondermove;

    // The following members (almost) don't need an init
    int seldepth;
    int sc;
    U64 nodespermove[0x10000];                      // init in prepare only for thread #0
    chessmovelist captureslist[MAXDEPTH];
    chessmovelist quietslist[MAXDEPTH];
    chessmovelist singularcaptureslist[MAXDEPTH];
    chessmovelist singularquietslist[MAXDEPTH];
    uint32_t pvtable[MAXDEPTH][MAXDEPTH];
    uint32_t multipvtable[MAXMULTIPV][MAXDEPTH];
    uint32_t lastpv[MAXDEPTH];
    int CurrentMoveNum[MAXDEPTH];
    chessmovestack prerootmovestack[PREROOTMOVES];      // explicit copy from rootpos up to frame prerootmovenum including first frame of regular stack
    chessmovestack movestack[MAXDEPTH];                 // frame 0 copied from rootpos
    uint32_t prerootmovecode[PREROOTMOVES];             // explicit copy from rootpos up to frame prerootmovenum including first regular movecode
    uint32_t movecode[MAXDEPTH];
    uint16_t excludemovestack[MAXDEPTH];                // init in prepare only for excludemovestack[0]
    int16_t staticevalstack[MAXDEPTH];
    Materialhash mtrlhsh;                               // init in alloc
    Pawnhash pwnhsh;                                    // init in alloc
    bool computationState[MAXDEPTH][2];
    int16_t* accumulation;
    int32_t* psqtAccumulation;
    DirtyPiece dirtypiece[MAXDEPTH];
    uint32_t quietMoves[MAXDEPTH][MAXMOVELISTLENGTH];
    uint32_t tacticalMoves[MAXDEPTH][MAXMOVELISTLENGTH];
    alignas(64) MoveSelector moveSelector[MAXDEPTH];
    MoveSelector extensionMoveSelector[MAXDEPTH];
#ifdef SDEBUG
    int pvmovevalue[MAXDEPTH];
    int pvalpha[MAXDEPTH];
    int pvbeta[MAXDEPTH];
    int pvdepth[MAXDEPTH];
    int pvmovenum[MAXDEPTH];
    PvAbortType pvaborttype[MAXDEPTH];
    int pvabortscore[MAXDEPTH];
    string pvadditionalinfo[MAXDEPTH];
#endif
#ifdef EVALTUNE
    bool isQuiet;
    bool noQs;
    tuneparamselection tps;
    positiontuneset pts;
    evalparam ev[NUMOFEVALPARAMS];
    void resetTuner();
    void getPositionTuneSet(positiontuneset* p, evalparam* e);
    void copyPositionTuneSet(positiontuneset* from, evalparam* efrom, positiontuneset* to, evalparam* eto);
    string getCoeffString();
#endif
    bool w2m();
    void BitboardSet(int index, PieceCode p);
    void BitboardClear(int index, PieceCode p);
    void BitboardMove(int from, int to, PieceCode p);
    void BitboardPrint(U64 b);
    int getFromFen(const char* sFen);
    void initCastleRights(int rookfiles[2][2], int kingfile[2]);
    string toFen();
    uint32_t applyMove(string s, bool resetMstop = true);
    void print(ostream* os = &cout);
    int getPhase() { return (max(0, 24 - phcount) * 255 + 12) / 24; }
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
    template <bool LiteMode> bool playMove(uint32_t mc);
    template <bool LiteMode> void unplayMove(uint32_t mc);
    void playNullMove();
    void unplayNullMove();
    U64 nextHash(uint32_t mc);
    template <int Me> void updatePins();
    template <int Me> void updateThreats();
    template <int Me> bool sliderAttacked(int index, U64 occ);
    bool moveGivesCheck(uint32_t c);  // simple and imperfect as it doesn't handle special moves and cases (mainly to avoid pruning of important moves)
    bool moveIsPseudoLegal(uint32_t c);     // test if move is possible in current position
    uint32_t shortMove2FullMove(uint16_t c); // transfer movecode from tt to full move code without checking if pseudoLegal
    int getpsqval(bool showDetails = false);  // only for eval trace
    template <EvalType Et, int Me> int getGeneralEval(positioneval *pe);
    int getFrcCorrection();
    bool isEndgame(int *score);
    template <EvalType Et, PieceType Pt, int Me> int getPieceEval(positioneval *pe);
    template <EvalType Et, int Me> int getLateEval(positioneval *pe);
    template <EvalType Et, int Me> void getPawnAndKingEval(pawnhashentry *entry);
    template <EvalType Et> int getEval();
    void getScaling(Materialhashentry *mhentry);
    int getComplexity(int eval, pawnhashentry *phentry, Materialhashentry *mhentry);

    template <RootsearchType RT> int rootsearch(int alpha, int beta, int *depth, int inWindowLast, int maxmoveindex = 0);
    template <PruneType Pt> int alphabeta(int alpha, int beta, int depth, bool cutnode);
    template <PruneType Pt> int getQuiescence(int alpha, int beta, int depth);
    void updateHistory(uint32_t code, int value);
    void updateTacticalHst(uint32_t code, int value);
    void updatePvTable(uint32_t mc, bool recursive);
    void updateMultiPvTable(int pvindex, uint32_t mc);
    string getPv(uint32_t *table);
    int applyPv(uint32_t* table);
    void reapplyPv(uint32_t* table, int num);
    int getHistory(uint32_t code);
    int getTacticalHst(uint32_t code);
    void resetStats();
    inline bool CheckForImmediateStop();
    int CreateEvasionMovelist(chessmove* mstart);
    template <MoveType Mt> int CreateMovelist(chessmove* mstart);
    template <PieceType Pt, Color me> inline int CreateMovelistPiece(chessmove* mstart, U64 occ, U64 targets);
    template <MoveType Mt, Color me> inline int CreateMovelistPawn(chessmove* mstart);
    template <Color me> inline int CreateMovelistCastle(chessmove* mstart);
    template <MoveType Mt> void evaluateMoves(chessmovelist* ml);
    int probe_wdl(int* success);
    int probe_dtz(int* success);
    int root_probe_dtz();
    int root_probe_wdl();
    int probe_ab(int alpha, int beta, int* success);
    int probe_wdl_table(int* success);
    int probe_dtz_table(int wdl, int* success);
    string AlgebraicFromShort(string s);
#ifdef SDEBUG
    bool triggerDebug(uint16_t* nextmove);
    void pvdebugout();
#endif
    int testRepetition();
    template <NnueType Nt, Color c> void HalfkpAppendActiveIndices(NnueIndexList *active);
    template <NnueType Nt, Color c> void HalfkpAppendChangedIndices(DirtyPiece* dp, NnueIndexList *add, NnueIndexList *remove);
    template <NnueType Nt, Color c, unsigned int NnueFtHalfdims, unsigned int NnuePsqtBuckets> void UpdateAccumulator();
    template <NnueType Nt, unsigned int NnueFtHalfdims, unsigned int NnuePsqtBuckets> int Transform(clipped_t *output, int bucket = 0);
    int NnueGetEval();
#ifdef NNUELEARN
    void toSfen(PackedSfen *sfen);
    int getFromSfen(PackedSfen* sfen);
    void getPosFromBinpack(Binpack* bp);
    int getNextFromBinpack(Binpack *bp);
    void posToBinpack(Binpack* bp);
    void nextToBinpack(Binpack* bp);
    void copyToLight(chessposition *target);     //fast copy for follow up detection
    bool followsTo(chessposition *src, uint32_t mc);    // check if this position is reached by src with move mc
    void fixEpt();
#endif
};


//
// engine stuff
//
void uciSetLogFile();
void engineHeader();

class GuiCommunication {
private:
    ostream* myos;
    ofstream logstream;
    U64 logStartTime = 0ULL;
    U64 freq;
    string timestamp() {
        U64 timeDiff = (getTime() - logStartTime) * 1000 / freq;
        U64 ms = timeDiff % 1000;
        U64 s = (timeDiff / 1000);
        stringstream ts;
        ts << setfill(' ') << setw(6) << s << "." << setw(3) << setfill('0') << ms;
        return ts.str();
    }
public:
    GuiCommunication()
    {
        myos = &cout;
        freq = 0;
    }
    template <typename T>
    GuiCommunication& operator<<(const T& thing) {
        *myos << thing;
        if (freq)
            logstream << timestamp() << " < " << thing;
        return *this;
    }
    void fromGui(string input)
    {
        if (freq)
            logstream << timestamp() << " > " << input << "\n";
    }
    bool openLog(string filename, U64 fr, bool ap) {
        freq = 0;
        if (logstream)
            logstream.close();
        if (filename == "")
            return true;
        ios_base::openmode om = ios::out;
        if (ap)
            om |= ios::app;
        logstream.open(filename, om);
        if (!logstream)
            return false;
        logStartTime = getTime();
        freq = fr;
        auto now = chrono::system_clock::now();
        time_t now_time = chrono::system_clock::to_time_t(now);
        logstream << timestamp() << " Logging started at " << ctime(&now_time);
        return true;
    }
    void log(string input) {
        if (freq)
            logstream << timestamp() << " < " << input;
    }
    void switchStream() {
        myos = (myos == &cout ? &cerr : &cout);
    }
};


enum GuiToken { UNKNOWN, UCI, UCIDEBUG, ISREADY, SETOPTION, REGISTER, UCINEWGAME, POSITION, GO, STOP, WAIT, PONDERHIT, QUIT, EVAL, PERFT, BENCH, TUNE, GENSFEN, CONVERT, LEARN, EXPORT, STATS };

const map<string, GuiToken> GuiCommandMap = {
    { "export", EXPORT },
#ifdef EVALTUNE
    { "tune", TUNE },
#endif
#ifdef NNUELEARN
    { "gensfen", GENSFEN },
    { "convert", CONVERT },
    { "learn", LEARN },
#endif
#ifdef STATISTICS
    { "stats", STATS },
#endif
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
    { "wait", WAIT },
    { "eval", EVAL },
    { "perft", PERFT },
    { "bench", BENCH }
};

class engine;   //forward definition

// order of ucioptiontypes is important for (not) setting default at registration
enum ucioptiontype { ucicheck, ucispin, ucicombo, ucistring, ucibutton, ucieval, ucisearch, ucinnuebias, ucinnueweight };

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
enum ponderstate_t { NO, PONDERING };


#define CPUSSE2     (1 << 0)
#define CPUSSSE3    (1 << 1)
#define CPUPOPCNT   (1 << 2)
#define CPULZCNT    (1 << 3)
#define CPUBMI1     (1 << 4)
#define CPUAVX2     (1 << 5)
#define CPUBMI2     (1 << 6)
#define CPUAVX512   (1 << 7)
#define CPUNEON     (1 << 8)
#define CPUARM64    (1 << 9)

class compilerinfo
{
    const string strCpuFeatures[10] = { "sse2","ssse3","popcnt","lzcnt","bmi1","avx2","bmi2", "avx512", "neon", "arm64"};
public:
    const U64 binarySupports = 0ULL
#ifdef USE_POPCNT
        | CPUPOPCNT
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
#ifdef USE_BMI1
        | CPUBMI1 | CPULZCNT
#endif
#ifdef USE_BMI2
        | CPUBMI2
#endif
#ifdef USE_AVX512
        | CPUAVX512
#endif
#ifdef USE_NEON
        | CPUNEON
#endif
#ifdef USE_ARM64
        | CPUARM64
#endif
        ;

    U64 machineSupports;
    string system;
    int cpuVendor;
    int cpuFamily;
    int cpuModel;
    compilerinfo() { GetSystemInfo(); }
    void GetSystemInfo();
    string SystemName() { return system; }
    string PrintCpuFeatures(U64 features, bool onlyHighest = false);
    int GetProcessId();
};



class engine
{
public:
    engine(compilerinfo *c);
    ~engine();
    const string author = "Andreas Matthies";
    U64 thinkstarttime;
    U64 clockstarttime;
    U64 clockstoptime;
    U64 lastmovetime;
    U64 lastclockstarttime;
    U64 endtime1; // time to stop before starting next iteration
    U64 endtime2; // time to stop immediately
    U64 frequency;
    int mytime, yourtime, myinc, yourinc, movestogo, mate, movetime, maxdepth;
    int lastmytime, lastmyinc;
    U64 maxnodes;
    bool tmEnabled;
    bool debug = false;
    bool evaldetails = false;
    bool moveoutput;
    int stopLevel = ENGINETERMINATEDSEARCH;
    int Hash;
    int restSizeOfTp = 0;
    int sizeOfPh;
    int moveOverhead;
    int maxMeasuredGuiOverhead;
    int maxMeasuredEngineOverhead;
    int MultiPV;
    bool ponder;
    bool chess960;
    string SyzygyPath;
    bool Syzygy50MoveRule = true;
    int SyzygyProbeLimit;
    string BookFile;
    bool BookBestMove;
    int BookDepth;
    int Contempt;
    int RatingAdv;
    int ContemptRatio;
    int ResultingContempt;
    int LimitNps;
    chessposition rootposition;
    int Threads;
    int oldThreads;
    searchthread *sthread;
    ponderstate_t pondersearch;
    int ponderhitbonus;
    int lastReport;
    int lastbestmovescore;
    int benchdepth;
    bool prepared;
    string benchmove;
    string benchpondermove;
    ucioptions_t ucioptions;
    compilerinfo* compinfo;
    string ExecPath;
    set<string> searchmoves;

#ifdef STACKDEBUG
    string assertfile = "";
#endif
#ifdef TDEBUG
    int t1stop = 0;     // regular stop
    int t2stop = 0;     // immediate stop
    bool bStopCount;
#endif
    string LogFile;
    bool usennue;
    string NnueNetpath; // UCI option, can be <Default>
    bool NnueUseDefault() {
        return (NnueNetpath == "<Default>");
    }
    string GetNnueNetPath() {
#ifdef NNUEINCLUDED
        return TOSTRING(NNUEINCLUDED);
#else
        if (NnueUseDefault())
            return NNUEDEFAULTSTR;
        else
            return NnueNetpath;
#endif
    }

    string NnueSha256FromName() {
        string path = GetNnueNetPath();
        size_t s1, s2;
        if ((s2 = path.rfind('-')) != string::npos
            && (s1 = path.rfind('-', s2 - 1)) != string::npos
            && s2 - s1 == 11)
            // Most probably a Rubi net; shorten name to 5 digits
            return path.substr(s1 + 1, 5);
        else
            return "<unknown>";
    }
#ifdef _WIN32
    bool allowlargepages;
#endif
    string name(bool full = true) {
        string sbinary = compinfo->PrintCpuFeatures(compinfo->binarySupports, true);
        sbinary = (sbinary != "" ? " (" + sbinary + ")" : "");
        if (!full)
            return string(ENGINEVER) + sbinary;

        string sNnue = "";
        if (NnueReady) sNnue = " NN-" + NnueSha256FromName();
#ifdef IS_64BIT
        sbinary += " 64Bit";
#else
        sbinary += " 32Bit";
#endif
        return string(ENGINEVER) + sNnue +  sbinary;
    };
    GuiToken parse(vector<string>*, string ss);
    void communicate(string inputstring);
    void allocThreads();
    void getNodesAndTbhits(U64 *nodes, U64 *tbhits);
    U64 perft(int depth, bool printsysteminfo = false);
    void bench(int constdepth, string epdfilename, int consttime, int startnum, bool openbench);
    void prepareThreads();
    void resetStats();
    void registerOptions();
    void measureOverhead(bool wasPondering);
    template <RootsearchType RT> void searchStart();
    void searchWaitStop(bool forceStop = true);
    void resetEndTime(U64 nowTime, int constantRootMoves = 0, int bestmovenodesratio = 128);
    void startSearchTime(bool ponderhit);
};

PieceType GetPieceType(char c);
char PieceChar(PieceCode c, bool lower = false);

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
extern GuiCommunication guiCom;


#ifdef SDEBUG
#define SDEBUGDO(c, s) if (c) {s}
#else
#define SDEBUGDO(c, s)
#endif


//
// search stuff
//

#ifdef SEARCHOPTIONS
#define SPSCONST
void searchtableinit();
class searchparam {
public:
    int val;
    string name;

    searchparam(const char* c) {
        string s(c);
        size_t i = s.find('/');
        val = stoi(s.substr(i + 1));
        name = "S_" + s.substr(0, i);
        en.ucioptions.Register((void*)&val, name, ucisearch, to_string(val), 0, 0, searchtableinit);
    }
    operator int() const { return val; }
};

#define SP(x,y) x = #x "/" #y

#else // SEARCHOPTIONS
#define SPSCONST const
typedef const int searchparam;
#define SP(x,y) x = y
#endif

struct searchparamset {
#ifdef EVALTUNE
    searchparam SP(deltapruningmargin, 4000);
#else
    searchparam SP(deltapruningmargin, 160);
#endif
    // LMR table
    searchparam SP(lmrlogf0, 150);
    searchparam SP(lmrf0, 60);
    searchparam SP(lmrlogf1, 150);
    searchparam SP(lmrf1, 43);
    searchparam SP(lmrmindepth, 3);
    searchparam SP(lmrstatsratio, 870);
    searchparam SP(lmropponentmovecount, 15);
    // LMP table
    searchparam SP(lmpf0, 59);
    searchparam SP(lmppow0, 48);
    searchparam SP(lmpf1, 74);
    searchparam SP(lmppow1, 170);
    // Razoring
    searchparam SP(razormargin, 250);
    searchparam SP(razordepthfactor, 50);
    //futility pruning
    searchparam SP(futilityreversedepthfactor, 70);
    searchparam SP(futilityreverseimproved, 20);
    searchparam SP(futilitymargin, 10);
    searchparam SP(futilitymarginperdepth, 59);
    // null move
    searchparam SP(nmmindepth, 2);
    searchparam SP(nmmredbase, 4);
    searchparam SP(nmmreddepthratio, 6);
    searchparam SP(nmmredevalratio, 150);
    searchparam SP(nmmredpvfactor, 2);
    searchparam SP(nmverificationdepth, 12);
    //Probcut
    searchparam SP(probcutmindepth, 5);
    searchparam SP(probcutmargin, 100);
    // Threat pruning
    searchparam SP(threatprunemargin, 30);
    searchparam SP(threatprunemarginimprove, 0);

    // No hashmovereduction
    searchparam SP(nohashreductionmindepth, 3);
    // SEE prune
    searchparam SP(seeprunemarginperdepth, -20);
    searchparam SP(seeprunequietfactor, 4);
    // Singular extension
    searchparam SP(singularmindepth, 8);
    searchparam SP(singularmarginperdepth, 2);
    // History extension
    searchparam SP(histextminthreshold, 9);
    searchparam SP(histextmaxthreshold, 15);
    searchparam SP(aspincratio, 4);
    searchparam SP(aspincbase, 2);
    searchparam SP(aspinitialdelta, 8);
};

extern SPSCONST searchparamset sps;

class searchthread
{
public:
    uint64_t toppadding[8];
    chessposition pos;
    thread thr;
    int index;
    int depth;
    int lastCompleteDepth;
    U64 nps;
#ifdef NNUELEARN
    PackedSfenValue* psvbuffer;
    PackedSfenValue* psv;
    int totalchunks;
    int chunkstate[2];
#endif
    uint64_t bottompadding[8];
};


void searchinit();
template <RootsearchType RT> void mainSearch(searchthread* thr);

//
// TB stuff
//
extern int TBlargest; // 5 if 5-piece tables, 6 if 6-piece tables were found.

//void init_tablebases(char *path);

#ifdef TBDEBUG
#define TBDEBUGDO(l,s) if ((l) <= TBDEBUG) {s}
#else
#define TBDEBUGDO(l,s)
#endif


//
// statistics stuff
//
#ifdef STATISTICS
class statistic
{
public:
    int qs_mindepth;
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
    U64 prune_threat;           // nodes pruned by (no opponents) threat

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
    S64 red_history;            // total reduction by history (can be positive and negative)
    S64 red_historyabs;         // total absolute reduction changes of history
    S64 red_pv;                 // total reduction by pv nodes
    S64 red_correction;         // total reduction correction by over-/underflow

    U64 extend_singular;        // total singular extensions
    U64 extend_endgame;         // total endgame extensions
    U64 extend_history;         // total history extensions

    U64 nnue_accupdate_all;     // total number of calls to UpdateAccumulator
    U64 nnue_accupdate_cache;   // total number of already up-to-date accumulators
    U64 nnue_accupdate_inc;     // total number of incremental updates
    U64 nnue_accupdate_full;    // total number of full updates


#define MAXSTATDEPTH 30
#define MAXSTATMOVES 128
    U64 ms_n[2][MAXSTATDEPTH];                          // total instantiations of moveselector in depth n (0 -> QS, MAXSTATDEPTH-1: ProbCut)
    U64 ms_moves[2][MAXSTATDEPTH];                      // total number of moves delivered in depth n
    U64 ms_tactic_stage[2][MAXSTATDEPTH][MAXSTATMOVES]; // how many times was the (good) tactic stage entered with a move list of length m
    U64 ms_tactic_moves[2][MAXSTATDEPTH][MAXSTATMOVES]; // total number of goodtactical moves delivered in depth n with a move list of length m
    U64 ms_spcl_stage[2][MAXSTATDEPTH];                 // how many times was the stage with special quiets (killer, counter) entered
    U64 ms_spcl_moves[2][MAXSTATDEPTH];                 // total number of special quiet moves delivered in depth n
    U64 ms_quiet_stage[2][MAXSTATDEPTH][MAXSTATMOVES];  // how many times was the quiet stage entered with a move list of length m
    U64 ms_quiet_moves[2][MAXSTATDEPTH][MAXSTATMOVES];  // total number of quiet moves delivered in depth n with a move list of length m
    U64 ms_badtactic_stage[2][MAXSTATDEPTH];            // how many times was the quiet stage entered
    U64 ms_badtactic_moves[2][MAXSTATDEPTH];            // total number of special quiet moves delivered in depth n
    U64 ms_evasion_stage[2][MAXSTATDEPTH][MAXSTATMOVES];// how many times was the evasion stage entered with a move list of length m
    U64 ms_evasion_moves[2][MAXSTATDEPTH][MAXSTATMOVES];// total number of evasion moves delivered in depth n with a move list of length m

    bool outputDone = false;

    void output(vector<string> args);
};

extern statistic statistics;

// some macros to limit the ifdef STATISTICS inside the code
#define STATISTICSINC(x)        statistics.x++
#define STATISTICSADD(x, v)     statistics.x += (v)
#define STATISTICSDO(x)         x

#else
#define STATISTICSINC(x)
#define STATISTICSADD(x, v)
#define STATISTICSDO(x)
#endif

//
// book stuff
//

struct bookentry {
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
};


class polybook
{
public:
    bookentry* table = nullptr;
    size_t entrynum;
    size_t iBest;
    int currentDepth;
    ranctx rnd;
    ~polybook();
    U64 GetHash(chessposition* p);
    bool Open(string filename);
    uint32_t GetMove(chessposition* p);
};

extern polybook pbook;


//
// SIMD definitions
//

namespace Simd {

#if USE_AVX512
    // 512bit intrinsics
    inline void m512_add_dpbusd_32(__m512i& acc, __m512i a, __m512i b) {
        __m512i product0 = _mm512_maddubs_epi16(a, b);
        product0 = _mm512_madd_epi16(product0, _mm512_set1_epi16(1));
        acc = _mm512_add_epi32(acc, product0);
    }

    inline void m512_add_dpbusd_32x2(__m512i& acc, __m512i a0, __m512i b0,  __m512i a1, __m512i b1) {
#if defined (USE_VNNI)
        acc = _mm512_dpbusd_epi32(acc, a0, b0);
        acc = _mm512_dpbusd_epi32(acc, a1, b1);
#else
        __m512i product0 = _mm512_maddubs_epi16(a0, b0);
        __m512i product1 = _mm512_maddubs_epi16(a1, b1);
        product0 = _mm512_adds_epi16(product0, product1);
        product0 = _mm512_madd_epi16(product0, _mm512_set1_epi16(1));
        acc = _mm512_add_epi32(acc, product0);
#endif
    }

    inline int m512_hadd(__m512i sum, int bias) {
        return _mm512_reduce_add_epi32(sum) + bias;
    }

    inline __m512i m512_hadd128x16_interleave(__m512i sum0, __m512i sum1, __m512i sum2, __m512i sum3) {
        __m512i sum01a = _mm512_unpacklo_epi32(sum0, sum1);
        __m512i sum01b = _mm512_unpackhi_epi32(sum0, sum1);
        __m512i sum23a = _mm512_unpacklo_epi32(sum2, sum3);
        __m512i sum23b = _mm512_unpackhi_epi32(sum2, sum3);
        __m512i sum01 = _mm512_add_epi32(sum01a, sum01b);
        __m512i sum23 = _mm512_add_epi32(sum23a, sum23b);
        __m512i sum0123a = _mm512_unpacklo_epi64(sum01, sum23);
        __m512i sum0123b = _mm512_unpackhi_epi64(sum01, sum23);
        return _mm512_add_epi32(sum0123a, sum0123b);
    }

    inline __m128i m512_haddx4(__m512i sum0, __m512i sum1, __m512i sum2, __m512i sum3, __m128i bias) {
        __m512i sum = m512_hadd128x16_interleave(sum0, sum1, sum2, sum3);
        __m256i sum256lo = _mm512_castsi512_si256(sum);
        __m256i sum256hi = _mm512_extracti64x4_epi64(sum, 1);
        sum256lo = _mm256_add_epi32(sum256lo, sum256hi);
        __m128i sum128lo = _mm256_castsi256_si128(sum256lo);
        __m128i sum128hi = _mm256_extracti128_si256(sum256lo, 1);
        return _mm_add_epi32(_mm_add_epi32(sum128lo, sum128hi), bias);
    }
#endif

#ifdef USE_AVX2
    // 256bit intrinsics
    inline void m256_add_dpbusd_32(__m256i& acc, __m256i a, __m256i b) {
        __m256i product0 = _mm256_maddubs_epi16(a, b);
        product0 = _mm256_madd_epi16(product0, _mm256_set1_epi16(1));
        acc = _mm256_add_epi32(acc, product0);
    }

    inline void m256_add_dpbusd_32x2(__m256i& acc, __m256i a0, __m256i b0, __m256i a1, __m256i b1) {
        __m256i product0 = _mm256_maddubs_epi16(a0, b0);
        __m256i product1 = _mm256_maddubs_epi16(a1, b1);
        product0 = _mm256_adds_epi16(product0, product1);
        product0 = _mm256_madd_epi16(product0, _mm256_set1_epi16(1));
        acc = _mm256_add_epi32(acc, product0);
    }

    inline int m256_hadd(__m256i sum, int bias) {
        __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, 0x4E)); //_MM_PERM_BADC
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, 0xB1)); //_MM_PERM_CDAB
        return _mm_cvtsi128_si32(sum128) + bias;
    }

    inline __m128i m256_haddx4(__m256i sum0, __m256i sum1, __m256i sum2, __m256i sum3, __m128i bias) {
        sum0 = _mm256_hadd_epi32(sum0, sum1);
        sum2 = _mm256_hadd_epi32(sum2, sum3);
        sum0 = _mm256_hadd_epi32(sum0, sum2);
        __m128i sum128lo = _mm256_castsi256_si128(sum0);
        __m128i sum128hi = _mm256_extracti128_si256(sum0, 1);
        return _mm_add_epi32(_mm_add_epi32(sum128lo, sum128hi), bias);
    }
#endif

#ifdef USE_SSSE3
    // 128bit intrinsics
    inline void m128_add_dpbusd_32(__m128i& acc, __m128i a, __m128i b) {
        __m128i product0 = _mm_maddubs_epi16(a, b);
        product0 = _mm_madd_epi16(product0, _mm_set1_epi16(1));
        acc = _mm_add_epi32(acc, product0);
    }

    inline void m128_add_dpbusd_32x2(__m128i& acc, __m128i a0, __m128i b0, __m128i a1, __m128i b1) {
        __m128i product0 = _mm_maddubs_epi16(a0, b0);
        __m128i product1 = _mm_maddubs_epi16(a1, b1);
        product0 = _mm_adds_epi16(product0, product1);
        product0 = _mm_madd_epi16(product0, _mm_set1_epi16(1));
        acc = _mm_add_epi32(acc, product0);
    }

    inline int m128_hadd(__m128i sum, int bias) {
        sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4E)); //_MM_PERM_BADC
        sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xB1)); //_MM_PERM_CDAB
        return _mm_cvtsi128_si32(sum) + bias;
    }

    inline __m128i m128_haddx4(__m128i sum0, __m128i sum1, __m128i sum2, __m128i sum3, __m128i bias) {
        sum0 = _mm_hadd_epi32(sum0, sum1);
        sum2 = _mm_hadd_epi32(sum2, sum3);
        sum0 = _mm_hadd_epi32(sum0, sum2);
        return _mm_add_epi32(sum0, bias);
    }
#endif

#ifdef USE_NEON
#ifdef USE_ARM64
    inline void neon_m128_add_dpbusd_32(int32x4_t& acc, int8x16_t a, int8x16_t b) {
        int16x8_t product0 = vmull_s8(vget_low_s8(a), vget_low_s8(b));
        int16x8_t product1 = vmull_high_s8(a, b);
        int16x8_t sum = vpaddq_s16(product0, product1);
        acc = vpadalq_s16(acc, sum);
    }
#ifdef USE_DOTPROD
    inline void dotprod_m128_add_dpbusd_32(int32x4_t& acc, int8x16_t a, int8x16_t b) {
        acc = vdotq_s32(acc, a, b);
    }
#endif
#endif

    inline int neon_m128_reduce_add_epi32(int32x4_t s) {
#if !defined(__clang_major__) && (defined(_M_ARM) || defined(_M_ARM64))
        return s.n128_i32[0] + s.n128_i32[1] + s.n128_i32[2] + s.n128_i32[3];
#else
        return s[0] + s[1] + s[2] + s[3];
#endif
    }
    inline int neon_m128_hadd(int32x4_t sum, int bias) {
        return neon_m128_reduce_add_epi32(sum) + bias;
    }

    inline int32x4_t neon_m128_haddx4(int32x4_t sum0, int32x4_t sum1, int32x4_t sum2, int32x4_t sum3, int32x4_t bias) {
#if !defined(__clang_major__) && (defined(_M_ARM) || defined(_M_ARM64))
        int32x4_t hsums;
        hsums.n128_i32[0] = neon_m128_reduce_add_epi32(sum0);
        hsums.n128_i32[1] = neon_m128_reduce_add_epi32(sum1);
        hsums.n128_i32[2] = neon_m128_reduce_add_epi32(sum2);
        hsums.n128_i32[3] = neon_m128_reduce_add_epi32(sum3);
#else
        int32x4_t hsums{
          neon_m128_reduce_add_epi32(sum0),
          neon_m128_reduce_add_epi32(sum1),
          neon_m128_reduce_add_epi32(sum2),
          neon_m128_reduce_add_epi32(sum3)
      };
#endif
        return vaddq_s32(hsums, bias);
    }

    inline void neon_m128_add_dpbusd_epi32x2(int32x4_t& acc, int8x8_t a0, int8x8_t b0, int8x8_t a1, int8x8_t b1) {
        int16x8_t product = vmull_s8(a0, b0);
        product = vmlal_s8(product, a1, b1);
        acc = vpadalq_s16(acc, product);
    }

#endif
}


} // namespace rubichess


void init_tablebases(char *path);
