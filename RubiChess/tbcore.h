/*
  Copyright (c) 2011-2015 Ronald de Man
*/

#ifndef TBCORE_H
#define TBCORE_H

#ifdef _WIN32
#define __attribute__(A) /* do nothing */
#define DECOMP64
#define __builtin_bswap32(x) _byteswap_ulong(x)
#define __builtin_bswap64(x) _byteswap_uint64(x)
#endif


#ifndef _WIN32
#include <pthread.h>
#define SEP_CHAR ':'
#define FD int
#define FD_ERR -1
#else
#include <windows.h>
#define SEP_CHAR ';'
#define FD HANDLE
#define FD_ERR INVALID_HANDLE_VALUE
#endif

#ifndef _WIN32
#define LOCK_T pthread_mutex_t
#define LOCK_INIT(x) pthread_mutex_init(&(x), NULL)
#define LOCK_DESTROY(x) pthread_mutex_destroy(&(x))
#define LOCK(x) pthread_mutex_lock(&(x))
#define UNLOCK(x) pthread_mutex_unlock(&(x))
#else
#define LOCK_T HANDLE
#define LOCK_INIT(x) do { x = CreateMutex(NULL, FALSE, NULL); } while (0)
#define LOCK_DESTROY(x) CloseHandle(x)
#define LOCK(x) WaitForSingleObject(x, INFINITE)
#define UNLOCK(x) ReleaseMutex(x)
#endif

#define WDLSUFFIX ".rtbw"
#define DTZSUFFIX ".rtbz"
#define WDLDIR "RTBWDIR"
#define DTZDIR "RTBZDIR"
#define TBPIECES 7

#define WDL_MAGIC 0x5d23e871
#define DTZ_MAGIC 0xa50c66d7

#define TBHASHBITS 12

typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned char ubyte;
typedef unsigned short ushort;

struct TBHashEntry;

#ifdef DECOMP64
typedef uint64 base_t;
#else
typedef uint32 base_t;
#endif

struct PairsData {
  char *indextable;
  ushort *sizetable;
  ubyte *data;
  ushort *offset;
  ubyte *symlen;
  ubyte *sympat;
  int blocksize;
  int idxbits;
  int min_len;
  base_t base[1]; // C++ complains about base[]...
};

struct TBEntry {
  char *data;
  uint64 key;
  uint64 mapping;
  ubyte ready;
  ubyte num;
  ubyte symmetric;
  ubyte has_pawns;
} __attribute__((__may_alias__));

struct TBEntry_piece {
  char *data;
  uint64 key;
  uint64 mapping;
  ubyte ready;
  ubyte num;
  ubyte symmetric;
  ubyte has_pawns;
  ubyte enc_type;
  struct PairsData *precomp[2];
  uint64 factor[2][TBPIECES];
  ubyte pieces[2][TBPIECES];
  ubyte norm[2][TBPIECES];
};

struct TBEntry_pawn {
  char *data;
  uint64 key;
  uint64 mapping;
  ubyte ready;
  ubyte num;
  ubyte symmetric;
  ubyte has_pawns;
  ubyte pawns[2];
  struct {
    struct PairsData *precomp[2];
    uint64 factor[2][TBPIECES];
    ubyte pieces[2][TBPIECES];
    ubyte norm[2][TBPIECES];
  } file[4];
};

struct DTZEntry_piece {
  char *data;
  uint64 key;
  uint64 mapping;
  ubyte ready;
  ubyte num;
  ubyte symmetric;
  ubyte has_pawns;
  ubyte enc_type;
  struct PairsData *precomp;
  uint64 factor[TBPIECES];
  ubyte pieces[TBPIECES];
  ubyte norm[TBPIECES];
  ubyte flags; // accurate, mapped, side
  ushort map_idx[4];
  ubyte *map;
};

struct DTZEntry_pawn {
  char *data;
  uint64 key;
  uint64 mapping;
  ubyte ready;
  ubyte num;
  ubyte symmetric;
  ubyte has_pawns;
  ubyte pawns[2];
  struct {
    struct PairsData *precomp;
    uint64 factor[TBPIECES];
    ubyte pieces[TBPIECES];
    ubyte norm[TBPIECES];
  } file[4];
  ubyte flags[4];
  ushort map_idx[4][4];
  ubyte *map;
};

struct TBHashEntry {
  uint64 key;
  struct TBEntry *ptr;
};

struct DTZTableEntry {
  uint64 key1;
  uint64 key2;
  struct TBEntry *entry;
};

#endif

