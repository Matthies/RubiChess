/*
  Copyright (c) 2013-2015 Ronald de Man
  This file may be redistributed and/or modified without restrictions.

  tbprobe.cpp contains the Stockfish-specific routines of the
  tablebase probing code. It should be relatively easy to adapt
  this code to other chess engines.
*/

// The probing code currently expects a little-endian architecture (e.g. x86).

// Define DECOMP64 when compiling for a 64-bit platform.
// 32-bit is only supported for 5-piece tables, because tables are mmap()ed
// into memory.
#ifdef IS_64BIT
#define DECOMP64
#endif

#include "RubiChess.h"

#include "tbprobe.h"
#include "tbcore.h"

#define SYZYGY2RUBI_PT(x) ((((x) & 0x7) << 1) | (((x) & 0x8) >> 3))

#if 0
namespace Zobrist {
  extern Key psq[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
}
#endif

int TBlargest = 0;

#include "tbcore.c"

// Given a position with 6 or fewer pieces, produce a text string
// of the form KQPvKRP, where "KQP" represents the white pieces if
// mirror == 0 and the black pieces if mirror == 1.
// No need to make this very efficient.
static void prt_str(char *str, int color)
{
  PieceType pt;
  int i;
  
  for (pt = KING; pt >= PAWN; pt--)
    for (i = POPCOUNT(pos.piece00[(pt << 1) | color]); i > 0; i--)
      *str++ = pchr[6 - pt];
  *str++ = 'v';
  color ^= S2MMASK;
  for (pt = KING; pt >= PAWN; pt--)
    for (i = POPCOUNT(pos.piece00[(pt << 1) | color]); i > 0; i--)
      *str++ = pchr[6 - pt];
  *str++ = 0;
}

// Given a position, produce a 64-bit material signature key.
// If the engine supports such a key, it should equal the engine's key.
// Again no need to make this very efficient.
static uint64 calc_key(int mirror)
{
    int color;
  PieceType pt;
  int i;
  uint64 key = 0;
  color = !mirror ? 0 : 1;


  for (pt = PAWN; pt <= KING; pt++)
    for (i = POPCOUNT(pos.piece00[(pt << 1) | color]); i > 0; i--)
        key ^= zb.boardtable[((i - 1) << 4) | (pt << 1)];
        //key ^= Zobrist::psq[WHITE][pt][i - 1];
  color ^= S2MMASK;
  for (pt = PAWN; pt <= KING; pt++)
    for (i = POPCOUNT(pos.piece00[(pt << 1) | color]); i > 0; i--)
        key ^= zb.boardtable[((i - 1) << 4) | (pt << 1) | 1];
        //key ^= Zobrist::psq[BLACK][pt][i - 1];

  return key;
}

// Produce a 64-bit material key corresponding to the material combination
// defined by pcs[16], where pcs[1], ..., pcs[6] is the number of white
// pawns, ..., kings and pcs[9], ..., pcs[14] is the number of black
// pawns, ..., kings.
// Again no need to be efficient here.
static uint64 calc_key_from_pcs(int *pcs, int mirror)
{
  int color;
  PieceType pt;
  int i;
  uint64 key = 0;

  color = !mirror ? 0 : 8;
  for (pt = PAWN; pt <= KING; pt++)
    for (i = 0; i < pcs[color + pt]; i++)
        key ^= zb.boardtable[(i << 4) | (pt << 1)];
        //key ^= Zobrist::psq[WHITE][pt][i];
  color ^= 8;
  for (pt = PAWN; pt <= KING; pt++)
    for (i = 0; i < pcs[color + pt]; i++)
        key ^= zb.boardtable[(i << 4) | (pt << 1) | 1];
  //key ^= Zobrist::psq[BLACK][pt][i];

  return key;
}

// probe_wdl_table and probe_dtz_table require similar adaptations.
static int probe_wdl_table(int *success)
{
    struct TBEntry *ptr;
    struct TBHashEntry *ptr2;
    uint64 idx;
    uint64 key;
    int i;
    ubyte res;
    int p[TBPIECES];

    // Obtain the position's material signature key.
    key = pos.materialhash;

    // Test for KvK.
    if (key == (zb.boardtable[WKING << 4] ^ zb.boardtable[BKING << 4]))
        return 0;

    ptr2 = TB_hash[key >> (64 - TBHASHBITS)];
    for (i = 0; i < HSHMAX; i++)
        if (ptr2[i].key == key) break;
    if (i == HSHMAX) {
        *success = 0;
        return 0;
    }

    ptr = ptr2[i].ptr;
    if (!ptr->ready) {
        LOCK(TB_mutex);
        if (!ptr->ready) {
            char str[16];
            prt_str(str, ptr->key != key);
            if (!init_table_wdl(ptr, str)) {
                ptr2[i].key = 0ULL;
                *success = 0;
                UNLOCK(TB_mutex);
                return 0;
            }
            // Memory barrier to ensure ptr->ready = 1 is not reordered.
#ifdef __GNUC__
            __asm__ __volatile__("" ::: "memory");
#elif defined(_MSC_VER)
            MemoryBarrier();
#endif
            ptr->ready = 1;
        }
        UNLOCK(TB_mutex);
    }

    int bside, mirror, cmirror;
    if (!ptr->symmetric) {
        if (key != ptr->key) {
            cmirror = 8;
            mirror = 0x38;
            bside = (pos.w2m());
        }
        else {
            cmirror = mirror = 0;
            bside = !(pos.w2m());
        }
    }
    else {
        cmirror = pos.w2m() ? 0 : 8;
        mirror = pos.w2m() == 0 ? 0 : 0x38;
        bside = 0;
    }

    // p[i] is to contain the square 0-63 (A1-H8) for a piece of type
    // pc[i] ^ cmirror, where 1 = white pawn, ..., 14 = black king.
    // Pieces of the same type are guaranteed to be consecutive.
    if (!ptr->has_pawns) {
        struct TBEntry_piece *entry = (struct TBEntry_piece *)ptr;
        ubyte *pc = entry->pieces[bside];
        for (i = 0; i < entry->num;) {
            U64 bb = pos.piece00[SYZYGY2RUBI_PT(pc[i] ^ cmirror)];
            int index;
            while (LSB(index, bb))
            {
                p[i++] = index;
                bb ^= BITSET(index);
            };
        }
        idx = encode_piece(entry, entry->norm[bside], p, entry->factor[bside]);
        res = decompress_pairs(entry->precomp[bside], idx);
    }
    else {
        struct TBEntry_pawn *entry = (struct TBEntry_pawn *)ptr;
        int k = entry->file[0].pieces[0][0] ^ cmirror;
        U64 bb = pos.piece00[SYZYGY2RUBI_PT(k)];
        i = 0;
        int index;
        while (LSB(index, bb))
        {
            p[i++] = index ^ mirror;
            bb ^= BITSET(index);
        };
        int f = pawn_file(entry, p);
        ubyte *pc = entry->file[f].pieces[bside];
        for (; i < entry->num;) {
            bb = pos.piece00[SYZYGY2RUBI_PT(pc[i] ^ cmirror)];
            int index;
            while (LSB(index, bb))
            {
                p[i++] = index ^ mirror;
                bb ^= BITSET(index);
            };
        }
        idx = encode_pawn(entry, entry->file[f].norm[bside], p, entry->file[f].factor[bside]);
        res = decompress_pairs(entry->file[f].precomp[bside], idx);
    }

    return ((int)res) - 2;
}

// The value of wdl MUST correspond to the WDL value of the position without
// en passant rights.
static int probe_dtz_table(int wdl, int *success)
{
  struct TBEntry *ptr;
  uint64 idx;
  int i, res;
  int p[TBPIECES];

  // Obtain the position's material signature key.
  uint64 key = pos.materialhash;

  if (DTZ_table[0].key1 != key && DTZ_table[0].key2 != key) {
    for (i = 1; i < DTZ_ENTRIES; i++)
      if (DTZ_table[i].key1 == key || DTZ_table[i].key2 == key) break;
    if (i < DTZ_ENTRIES) {
      struct DTZTableEntry table_entry = DTZ_table[i];
      for (; i > 0; i--)
	DTZ_table[i] = DTZ_table[i - 1];
      DTZ_table[0] = table_entry;
    } else {
      struct TBHashEntry *ptr2 = TB_hash[key >> (64 - TBHASHBITS)];
      for (i = 0; i < HSHMAX; i++)
	if (ptr2[i].key == key) break;
      if (i == HSHMAX) {
	*success = 0;
	return 0;
      }
      ptr = ptr2[i].ptr;
      char str[16];
      int mirror = (ptr->key != key);
      prt_str(str, mirror);
      if (DTZ_table[DTZ_ENTRIES - 1].entry)
	free_dtz_entry(DTZ_table[DTZ_ENTRIES-1].entry);
      for (i = DTZ_ENTRIES - 1; i > 0; i--)
	DTZ_table[i] = DTZ_table[i - 1];
      load_dtz_table(str, calc_key(mirror), calc_key(!mirror));
    }
  }

  ptr = DTZ_table[0].entry;
  if (!ptr) {
    *success = 0;
    return 0;
  }

  int bside, mirror, cmirror;
  if (!ptr->symmetric) {
    if (key != ptr->key) {
      cmirror = 8;
      mirror = 0x38;
      bside = (pos.w2m());
    } else {
      cmirror = mirror = 0;
      bside = !(pos.w2m());
    }
  } else {
    cmirror = pos.w2m() ? 0 : 8;
    mirror = pos.w2m() ? 0 : 0x38;
    bside = 0;
  }

  if (!ptr->has_pawns) {
    struct DTZEntry_piece *entry = (struct DTZEntry_piece *)ptr;
    if ((entry->flags & 1) != bside && !entry->symmetric) {
      *success = -1;
      return 0;
    }
    ubyte *pc = entry->pieces;
    for (i = 0; i < entry->num;) {
      U64 bb = pos.piece00[SYZYGY2RUBI_PT(pc[i] ^ cmirror)];
      int index;
      while (LSB(index, bb))
      {
          p[i++] = index;
          bb ^= BITSET(index);
      }
    }
    idx = encode_piece((struct TBEntry_piece *)entry, entry->norm, p, entry->factor);
    res = decompress_pairs(entry->precomp, idx);

    if (entry->flags & 2)
      res = entry->map[entry->map_idx[wdl_to_map[wdl + 2]] + res];

    if (!(entry->flags & pa_flags[wdl + 2]) || (wdl & 1))
      res *= 2;
  } else {
    struct DTZEntry_pawn *entry = (struct DTZEntry_pawn *)ptr;
    int k = entry->file[0].pieces[0] ^ cmirror;
    U64 bb = pos.piece00[SYZYGY2RUBI_PT(k)];
    i = 0;
    int index;
    while (LSB(index, bb))
    {
        p[i++] = index ^ mirror;
        bb ^= BITSET(index);
    }
    int f = pawn_file((struct TBEntry_pawn *)entry, p);
    if ((entry->flags[f] & 1) != bside) {
      *success = -1;
      return 0;
    }
    ubyte *pc = entry->file[f].pieces;
    for (; i < entry->num;) {
        bb = pos.piece00[SYZYGY2RUBI_PT(pc[i] ^ cmirror)];
        int index;
        while (LSB(index, bb))
        {
            p[i++] = index ^ mirror;
            bb ^= BITSET(index);
        }
    }
    idx = encode_pawn((struct TBEntry_pawn *)entry, entry->file[f].norm, p, entry->file[f].factor);
    res = decompress_pairs(entry->file[f].precomp, idx);

    if (entry->flags[f] & 2)
      res = entry->map[entry->map_idx[f][wdl_to_map[wdl + 2]] + res];

    if (!(entry->flags[f] & pa_flags[wdl + 2]) || (wdl & 1))
      res *= 2;
  }

  return res;
}

#if 0
// Add underpromotion captures to list of captures.
static ExtMove *add_underprom_caps(Position& pos, MoveStack *stack, MoveStack *end)
{
  ExtMove *moves, *extra = end;

  for (moves = stack; moves < end; moves++) {
    Move move = moves->move;
    if (type_of(move) == PROMOTION && !pos.empty(to_sq(move))) {
      (*extra++).move = (Move)(move - (1 << 12));
      (*extra++).move = (Move)(move - (2 << 12));
      (*extra++).move = (Move)(move - (3 << 12));
    }
  }

  return extra;
}
#endif

#if 1
static int probe_ab(int alpha, int beta, int *success)
{
    int v;

    // Generate (at least) all legal captures including (under)promotions.
    // It is OK to generate more, as long as they are filtered out below.
    chessmovelist* movelist = pos.getMoves();
    for (int i = 0; i < movelist->length; i++)
    {
        chessmove *m = &movelist->move[i];
        if (ISCAPTURE(m->code) || ISPROMOTION(m->code) || pos.isCheck)
        {
            if (pos.playMove(m))
            {
                v = -probe_ab(-beta, -alpha, success);
                pos.unplayMove(m);
                if (*success == 0) return 0;
                if (v > alpha) {
                    if (v >= beta)
                    {
                        free(movelist);
                        return v;
                    }
                    alpha = v;
                }
            }
        }
    }

    v = probe_wdl_table(success);

    free(movelist);

    return alpha >= v ? alpha : v;
}

// Probe the WDL table for a particular position.
//
// If *success != 0, the probe was successful.
//
// If *success == 2, the position has a winning capture, or the position
// is a cursed win and has a cursed winning capture, or the position
// has an ep capture as only best move.
// This is used in probe_dtz().
//
// The return value is from the point of view of the side to move:
// -2 : loss
// -1 : loss, but draw under 50-move rule
//  0 : draw
//  1 : win, but draw under 50-move rule
//  2 : win
int probe_wdl(int *success)
{
  *success = 1;
  int best_cap = -3, best_ep = -3;
  int i;

  // Generate (at least) all legal captures including (under)promotions.
  chessmovelist* movelist = pos.getMoves();

  // We do capture resolution, letting best_cap keep track of the best
  // capture without ep rights and letting best_ep keep track of still
  // better ep captures if they exist.
  for (i = 0; i < movelist->length; i++)
  {
      chessmove *m = &movelist->move[i];
      if (ISCAPTURE(m->code) || ISPROMOTION(m->code) || pos.isCheck)
      {
          if (pos.playMove(m))
          {
              int v = -probe_ab(-2, -best_cap, success);
              pos.unplayMove(m);
              if (*success == 0) return 0;
              if (v > best_cap) {
                  if (v == 2) {
                      *success = 2;
                      free(movelist);
                      return 2;
                  }
                  if (!GETEPCAPTURE(m->code))
                      best_cap = v;
                  else if (v > best_ep)
                      best_ep = v;
              }

          }
      }
  }

  int v = probe_wdl_table(success);
  if (*success == 0) return 0;

  // Now max(v, best_cap) is the WDL value of the position without ep rights.
  // If the position without ep rights is not stalemate or no ep captures
  // exist, then the value of the position is max(v, best_cap, best_ep).
  // If the position without ep rights is stalemate and best_ep > -3,
  // then the value of the position is best_ep (and we will have v == 0).

  if (best_ep > best_cap) {
    if (best_ep > v) { // ep capture (possibly cursed losing) is best.
      *success = 2;
      free(movelist);
      return best_ep;
    }
    best_cap = best_ep;
  }

  // Now max(v, best_cap) is the WDL value of the position unless
  // the position without ep rights is stalemate and best_ep > -3.

  if (best_cap >= v) {
    // No need to test for the stalemate case here: either there are
    // non-ep captures, or best_cap == best_ep >= v anyway.
    *success = 1 + (best_cap > 0);
    free(movelist);
    return best_cap;
  }

  // Now handle the stalemate case.
  if (best_ep > -3 && v == 0) {
    // Check for stalemate in the position with ep captures.
      for (i = 0; i < movelist->length; i++)
      {
          chessmove *m = &movelist->move[i];
          if (GETEPCAPTURE(m->code))
              continue;

          if (pos.playMove(m))
          {
              pos.unplayMove(m);
              break;
          }
      }

    if (i == movelist->length) { // Stalemate detected.
      *success = 2;
      free(movelist);
      return best_ep;
    }
  }

  // Stalemate / en passant not an issue, so v is the correct value.

  free(movelist);
  return v;
}

static int wdl_to_dtz[] = {
  -1, -101, 0, 101, 1
};

// Probe the DTZ table for a particular position.
// If *success != 0, the probe was successful.
// The return value is from the point of view of the side to move:
//         n < -100 : loss, but draw under 50-move rule
// -100 <= n < -1   : loss in n ply (assuming 50-move counter == 0)
//         0	    : draw
//     1 < n <= 100 : win in n ply (assuming 50-move counter == 0)
//   100 < n        : win, but draw under 50-move rule
//
// If the position is mate, -1 is returned instead of 0.
//
// The return value n can be off by 1: a return value -n can mean a loss
// in n+1 ply and a return value +n can mean a win in n+1 ply. This
// cannot happen for tables with positions exactly on the "edge" of
// the 50-move rule.
//
// This means that if dtz > 0 is returned, the position is certainly
// a win if dtz + 50-move-counter <= 99. Care must be taken that the engine
// picks moves that preserve dtz + 50-move-counter <= 99.
//
// If n = 100 immediately after a capture or pawn move, then the position
// is also certainly a win, and during the whole phase until the next
// capture or pawn move, the inequality to be preserved is
// dtz + 50-movecounter <= 100.
//
// In short, if a move is available resulting in dtz + 50-move-counter <= 99,
// then do not accept moves leading to dtz + 50-move-counter == 100.
//
int probe_dtz(int *success)
{
    chessmovelist* movelist = nullptr;

    int wdl = probe_wdl(success);
    if (*success == 0) return 0;

    // If draw, then dtz = 0.
    if (wdl == 0) return 0;

    // Check for winning (cursed) capture or ep capture as only best move.
    if (*success == 2)
        return wdl_to_dtz[wdl + 2];

    // If winning, check for a winning pawn move.
    if (wdl > 0) {
        // Generate at least all legal non-capturing pawn moves
        // including non-capturing promotions.
        movelist = pos.getMoves();

        for (int i = 0; i < movelist->length; i++)
        {
            chessmove *m = &movelist->move[i];
            if (ISCAPTURE(m->code) || (GETPIECE(m->code) >> 1) != PAWN)
                continue;

            if (pos.playMove(m))
            {
                int v = -probe_wdl(success);
                pos.unplayMove(m);
                if (*success == 0) return 0;
                if (v == wdl)
                {
                    free(movelist);
                    return wdl_to_dtz[wdl + 2];
                }
            }
        }
    }

    // If we are here, we know that the best move is not an ep capture.
    // In other words, the value of wdl corresponds to the WDL value of
    // the position without ep rights. It is therefore safe to probe the
    // DTZ table with the current value of wdl.

    int dtz = probe_dtz_table(wdl, success);
    if (*success >= 0)
        return wdl_to_dtz[wdl + 2] + ((wdl > 0) ? dtz : -dtz);

    // *success < 0 means we need to probe DTZ for the other side to move.
    int best;
    if (wdl > 0) {
        best = INT32_MAX;
        // If wdl > 0, we already generated all moves.
    }
    else {
        // If (cursed) loss, the worst case is a losing capture or pawn move
        // as the "best" move, leading to dtz of -1 or -101.
        // In case of mate, this will cause -1 to be returned.
        best = wdl_to_dtz[wdl + 2];
        movelist = pos.getMoves();
    }
    for (int i = 0; i < movelist->length; i++)
    {
        chessmove *m = &movelist->move[i];
        if (ISCAPTURE(m->code) || (GETPIECE(m->code) >> 1) == PAWN)
            continue;

        if (pos.playMove(m))
        {
            int v = -probe_dtz(success);
            pos.unplayMove(m);
            if (*success == 0)
            {
                free(movelist);
                return 0;
            }
            if (wdl > 0) {
                if (v > 0 && v + 1 < best)
                    best = v + 1;
            }
            else {
                if (v - 1 < best)
                    best = v - 1;

            }
        }
    }
    if (movelist)
        free(movelist);
    return best;
}

#if 0
// Check whether there has been at least one repetition of positions
// since the last capture or pawn move.
static int has_repeated(StateInfo *st)
{
  while (1) {
    int i = 4, e = std::min(st->rule50, st->pliesFromNull);
    if (e < i)
      return 0;
    StateInfo *stp = st->previous->previous;
    do {
      stp = stp->previous->previous;
      if (stp->key == st->key)
	return 1;
      i += 2;
    } while (i <= e);
    st = st->previous;
  }
}
#endif

static int wdl_to_Value[5] = {
    SCOREBLACKWINS,
    SCOREDRAW - 2,
    SCOREDRAW,
    SCOREDRAW + 2,
    SCOREWHITEWINS
};

// Use the DTZ tables to filter out moves that don't preserve the win or draw.
// If the position is lost, but DTZ is fairly high, only keep moves that
// maximise DTZ.
//
// A return value of 0 indicates that not all probes were successful and that
// no moves were filtered out.
int root_probe(int &TBScore)
{
    int success;

    int dtz = probe_dtz(&success);
    if (!success)
        return 0;

    // Probe each move.
    for (int i = 0; i < pos.rootmovelist.length; i++)
    {
        chessmove *m = &pos.rootmovelist.move[i];
        pos.playMove(m);
        int v = 0;
        if (pos.isCheck && dtz > 0) {
            chessmovelist* nextmovelist = pos.getMoves();
            bool foundevasion = false;
            for (int j = 0; j < nextmovelist->length; j++)
            {
                if (pos.playMove(&nextmovelist->move[j]))
                {
                    foundevasion = true;
                    pos.unplayMove(&nextmovelist->move[j]);
                    break;
                }
            }
            free(nextmovelist);
            if (!foundevasion)
                v = 1;
        }
        if (!v) {
            if (pos.halfmovescounter != 0) {
                v = -probe_dtz(&success);
                if (v > 0) v++;
                else if (v < 0) v--;
            }
            else {
                v = -probe_wdl(&success);
                v = wdl_to_dtz[v + 2];
            }
        }

        pos.unplayMove(m);
        if (!success)
        {
            return 0;
        }
        m->value = v;
    }

    // Obtain 50-move counter for the root position.
    // In Stockfish there seems to be no clean way, so we do it like this:
    int cnt50 = pos.halfmovescounter;

    // Use 50-move counter to determine whether the root position is
    // won, lost or drawn.
    int wdl = 0;
    if (dtz > 0)
        wdl = (dtz + cnt50 <= 100) ? 2 : 1;
    else if (dtz < 0)
        wdl = (-dtz + cnt50 <= 100) ? -2 : -1;

    // Determine the score to report to the user.
    TBScore = wdl_to_Value[wdl + 2];
    // If the position is winning or losing, but too few moves left, adjust the
    // score to show how close it is to winning or losing.
    // NOTE: int(PawnValueEg) is used as scaling factor in score_to_uci().
    if (wdl == 1 && dtz <= 100)
        TBScore = 100 - (dtz + cnt50) / 2;
    else if (wdl == -1 && dtz >= -100)
        TBScore = -(100 + (dtz - cnt50) / 2);

    // Now be a bit smart about filtering out moves.
    size_t j = 0;
    if (dtz > 0) { // winning (or 50-move rule draw)
        int best = 0xffff;
        for (int i = 0; i < pos.rootmovelist.length; i++)
        {
            chessmove *m = &pos.rootmovelist.move[i];
            int v = m->value;
            if (v > 0 && v < best)
                best = v;
        }

        int max = best;
        // If the current phase has not seen repetitions, then try all moves
        // that stay safely within the 50-move budget, if there are any.
        if (!pos.testRepetiton() && best + cnt50 <= 99)
            max = 99 - cnt50;

        for (int i = 0; i < pos.rootmovelist.length; i++)
        {
            int v = pos.rootmovelist.move[i].value;
            if (v > 0 && v <= max)
                pos.rootmovelist.move[j++] = pos.rootmovelist.move[i];
}
    }
    else if (dtz < 0) {
        int best = 0;
        for (int i = 0; i < pos.rootmovelist.length; i++)
        {
            int v = pos.rootmovelist.move[i].value;
            if (v < best)
                best = v;
        }
        // Try all moves, unless we approach or have a 50-move rule draw.
        if (-best * 2 + cnt50 < 100)
            return 1;

        for (int i = 0; i < pos.rootmovelist.length; i++)
        {
            if (pos.rootmovelist.move[i].value == best)
                pos.rootmovelist.move[j++] = pos.rootmovelist.move[i];
        }
    }
    else { // drawing
           // Try all moves that preserve the draw.
        for (int i = 0; i < pos.rootmovelist.length; i++)
        {
            if (pos.rootmovelist.move[i].value == 0)
                pos.rootmovelist.move[j++] = pos.rootmovelist.move[i];
        }
    }
    //Search::RootMoves.resize(j, Search::RootMove(MOVE_NONE));

    return 1;
}

#if 0
// Use the WDL tables to filter out moves that don't preserve the win or draw.
// This is a fallback for the case that some or all DTZ tables are missing.
//
// A return value of 0 indicates that not all probes were successful and that
// no moves were filtered out.
int root_probe_wdl(int& TBScore)
{
  int success;

  int wdl = probe_wdl(pos, &success);
  if (!success) return false;
  TBScore = wdl_to_Value[wdl + 2];

  StateInfo st;
  CheckInfo ci(pos);

  int best = -2;

  // Probe each move.
  for (size_t i = 0; i < Search::RootMoves.size(); i++) {
    Move move = Search::RootMoves[i].pv[0];
    pos.do_move(move, st, ci, pos._check(move, ci));
    int v = -probe_wdl(pos, &success);
    pos.undo_move(move);
    if (!success) return false;
    Search::RootMoves[i].score = (Value)v;
    if (v > best)
      best = v;
  }

  size_t j = 0;
  for (size_t i = 0; i < Search::RootMoves.size(); i++) {
    if (Search::RootMoves[i].score == best)
      Search::RootMoves[j++] = Search::RootMoves[i];
  }
  Search::RootMoves.resize(j, Search::RootMove(MOVE_NONE));

  return 1;
}
#endif

#endif
