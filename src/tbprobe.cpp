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

  This part of RubiChess is copyright (c) 2013-2015 Ronald de Man
  and was adapted to RubiChess by Andreas Matthies
*/

// The probing code currently expects a little-endian architecture (e.g. x86).

// Define DECOMP64 when compiling for a 64-bit platform.
// 32-bit is only supported for 5-piece tables, because tables are mmap()ed
// into memory.
#define IS_64BIT
#ifdef IS_64BIT
#define DECOMP64
#endif


#include "RubiChess.h"
#include "tbcore.h"

#define SYZYGY2RUBI_PT(x) ((((x) & 0x7) << 1) | (((x) & 0x8) >> 3))

int TBlargest = 0;

#include "tbcore.c"

// Given a position with 6 or fewer pieces, produce a text string
// of the form KQPvKRP, where "KQP" represents the white pieces if
// mirror == 0 and the black pieces if mirror == 1.
// No need to make this very efficient.
static void prt_str(char *str, int color, chessposition *pos)
{
    PieceType pt;
    int i;

    for (pt = KING; pt >= PAWN; pt--)
        for (i = POPCOUNT(pos->piece00[(pt << 1) | color]); i > 0; i--)
            *str++ = pchr[6 - pt];
    *str++ = 'v';
    color ^= S2MMASK;
    for (pt = KING; pt >= PAWN; pt--)
        for (i = POPCOUNT(pos->piece00[(pt << 1) | color]); i > 0; i--)
            *str++ = pchr[6 - pt];
    *str++ = 0;
}

// Given a position, produce a 64-bit material signature key.
// If the engine supports such a key, it should equal the engine's key.
// Again no need to make this very efficient.
static uint64_t calc_key(int mirror, chessposition *pos)
{
    int color;
    PieceType pt;
    int i;
    uint64_t key = 0;
    color = !mirror ? 0 : 1;


    for (pt = PAWN; pt <= KING; pt++)
        for (i = POPCOUNT(pos->piece00[(pt << 1) | color]); i > 0; i--)
            key ^= zb.boardtable[((i - 1) << 4) | (pt << 1)];
    color ^= S2MMASK;
    for (pt = PAWN; pt <= KING; pt++)
        for (i = POPCOUNT(pos->piece00[(pt << 1) | color]); i > 0; i--)
            key ^= zb.boardtable[((i - 1) << 4) | (pt << 1) | 1];

    return key;
}


// probe_wdl_table and probe_dtz_table require similar adaptations.
int chessposition::probe_wdl_table(int *success)
{
    TBEntry *ptr;
    uint64_t idx;
    uint64_t key;
    int i;
    uint8_t res;
    int p[TBPIECES];

    // Obtain the position's material signature key.
    key = materialhash;

    // Test for KvK.
    if (key == (zb.boardtable[WKING] ^ zb.boardtable[BKING]))
        return 0;

    int hashIdx = key >> (64 - TBHASHBITS);
    while (TB_hash[hashIdx].key && TB_hash[hashIdx].key != key)
        hashIdx = (hashIdx + 1) & ((1 << TBHASHBITS) - 1);
    ptr = TB_hash[hashIdx].ptr;
    if (!ptr) {
        *success = 0;
        return 0;
    }

    if (!ptr->ready) {
        LOCK(TB_mutex);
        if (!ptr->ready) {
            char str[16];
            prt_str(str, ptr->key != key, this);
            if (!init_table_wdl(ptr, str)) {
                ptr->key = 0ULL;
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
            bside = (w2m());
        }
        else {
            cmirror = mirror = 0;
            bside = !(w2m());
        }
    }
    else {
        cmirror = w2m() ? 0 : 8;
        mirror = w2m() ? 0 : 0x38;
        bside = 0;
    }

    // p[i] is to contain the square 0-63 (A1-H8) for a piece of type
    // pc[i] ^ cmirror, where 1 = white pawn, ..., 14 = black king.
    // Pieces of the same type are guaranteed to be consecutive.
    if (!ptr->has_pawns) {
        TBEntry_piece *entry = (TBEntry_piece *)ptr;
        uint8_t *pc = entry->pieces[bside];
        for (i = 0; i < entry->num;) {
            U64 bb = piece00[SYZYGY2RUBI_PT(pc[i] ^ cmirror)];
            int index;
            while (bb)
            {
                index = pullLsb(&bb);
                p[i++] = index;
            };
        }
        idx = encode_piece(entry, entry->norm[bside], p, entry->factor[bside]);
        res = *decompress_pairs(entry->precomp[bside], idx);
    }
    else {
        TBEntry_pawn *entry = (TBEntry_pawn *)ptr;
        int k = entry->file[0].pieces[0][0] ^ cmirror;
        U64 bb = piece00[SYZYGY2RUBI_PT(k)];
        i = 0;
        int index;
        while (bb)
        {
            index = pullLsb(&bb);
            p[i++] = index ^ mirror;
        };
        int f = pawn_file(entry, p);
        uint8_t *pc = entry->file[f].pieces[bside];
        for (; i < entry->num;) {
            bb = piece00[SYZYGY2RUBI_PT(pc[i] ^ cmirror)];
            while (bb)
            {
                index = pullLsb(&bb);
                p[i++] = index ^ mirror;
            };
        }
        idx = encode_pawn(entry, entry->file[f].norm[bside], p, entry->file[f].factor[bside]);
        res = *decompress_pairs(entry->file[f].precomp[bside], idx);
    }

    return ((int)res) - 2;
}

// The value of wdl MUST correspond to the WDL value of the position without
// en passant rights.
int chessposition::probe_dtz_table(int wdl, int *success)
{
    TBEntry *ptr;
    uint64_t idx;
    int i, res;
    int p[TBPIECES];

    // Obtain the position's material signature key.
    uint64_t key = materialhash;

    if (DTZ_table[0].key1 != key && DTZ_table[0].key2 != key) {
        for (i = 1; i < DTZ_ENTRIES; i++)
            if (DTZ_table[i].key1 == key || DTZ_table[i].key2 == key) break;
        if (i < DTZ_ENTRIES) {
            DTZTableEntry table_entry = DTZ_table[i];
            for (; i > 0; i--)
                DTZ_table[i] = DTZ_table[i - 1];
            DTZ_table[0] = table_entry;
        }
        else {
            int hashIdx = key >> (64 - TBHASHBITS);
            while (TB_hash[hashIdx].key && TB_hash[hashIdx].key != key)
                hashIdx = (hashIdx + 1) & ((1 << TBHASHBITS) - 1);
            ptr = TB_hash[hashIdx].ptr;
            if (!ptr) {
                *success = 0;
                return 0;
            }
            char str[16];
            int mirror = (ptr->key != key);
            prt_str(str, mirror, this);
            if (DTZ_table[DTZ_ENTRIES - 1].entry)
                free_dtz_entry(DTZ_table[DTZ_ENTRIES - 1].entry);
            for (i = DTZ_ENTRIES - 1; i > 0; i--)
                DTZ_table[i] = DTZ_table[i - 1];
            load_dtz_table(str, calc_key(mirror, this), calc_key(!mirror, this));
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
            bside = (w2m());
        }
        else {
            cmirror = mirror = 0;
            bside = !(w2m());
        }
    }
    else {
        cmirror = w2m() ? 0 : 8;
        mirror = w2m() ? 0 : 0x38;
        bside = 0;
    }

    if (!ptr->has_pawns) {
        DTZEntry_piece *entry = (DTZEntry_piece *)ptr;
        if ((entry->flags & 1) != bside && !entry->symmetric) {
            *success = -1;
            return 0;
        }
        uint8_t *pc = entry->pieces;
        for (i = 0; i < entry->num;) {
            U64 bb = piece00[SYZYGY2RUBI_PT(pc[i] ^ cmirror)];
            int index;
            while (bb)
            {
                index = pullLsb(&bb);
                p[i++] = index;
            }
        }
        idx = encode_piece((TBEntry_piece *)entry, entry->norm, p, entry->factor);

        uint8_t* w = decompress_pairs(entry->precomp, idx);
        res = w[0] + ((w[1] & 0x0f) << 8);

        if (entry->flags & 2) {
            uint8_t* map = entry->map;
            uint16_t* midx = entry->map_idx;
            int m = wdl_to_map[wdl + 2];
            if (!(entry->flags & 16)) {
                res = map[midx[m] + res];
            }
            else {
                res = ((uint16_t*)map)[midx[m] + res];
            }
        }

        if (!(entry->flags & pa_flags[wdl + 2]) || (wdl & 1))
            res *= 2;
    }
    else {
        DTZEntry_pawn *entry = (DTZEntry_pawn *)ptr;
        int k = entry->file[0].pieces[0] ^ cmirror;
        U64 bb = piece00[SYZYGY2RUBI_PT(k)];
        i = 0;
        int index;
        while (bb)
        {
            index = pullLsb(&bb);
            p[i++] = index ^ mirror;
        }
        int f = pawn_file((TBEntry_pawn *)entry, p);
        if ((entry->flags[f] & 1) != bside) {
            *success = -1;
            return 0;
        }
        uint8_t *pc = entry->file[f].pieces;
        for (; i < entry->num;) {
            bb = piece00[SYZYGY2RUBI_PT(pc[i] ^ cmirror)];
            while (bb)
            {
                index = pullLsb(&bb);
                p[i++] = index ^ mirror;
            }
        }
        idx = encode_pawn((TBEntry_pawn *)entry, entry->file[f].norm, p, entry->file[f].factor);
        uint8_t* w = decompress_pairs(entry->file[f].precomp, idx);
        res = w[0] + ((w[1] & 0x0f) << 8);

        if (entry->flags[f] & 2) {
            uint8_t* map = entry->map;
            uint16_t* midx = entry->map_idx[f];
            int m = wdl_to_map[wdl + 2];
            if (!(entry->flags[f] & 16)) {
                res = map[midx[m] + res];
            }
            else {
                res = ((uint16_t*)map)[midx[m] + res];
            }
        }

        if (!(entry->flags[f] & pa_flags[wdl + 2]) || (wdl & 1))
            res *= 2;
    }

    return res;
}


int chessposition::probe_ab(int alpha, int beta, int *success)
{
    int v;

    // Generate (at least) all legal captures including (under)promotions.
    // It is OK to generate more, as long as they are filtered out below.
    chessmovelist movelist;
    prepareStack();
    movelist.length = CreateMovelist<ALL>(&movelist.move[0]);
    for (int i = 0; i < movelist.length; i++)
    {
        uint32_t mc = movelist.move[i].code;
        if (ISCAPTURE(mc) || ISPROMOTION(mc) || isCheckbb)
        {
            if (playMove(mc))
            {
                //printf("probe_ab (ply=%d) testing capture/promotion/evasion move %s...\n", ply, moveToString(mc).c_str());
                v = -probe_ab(-beta, -alpha, success);
                //printf("probe_ab (ply=%d) tested  capture/promotion/evasion move %s... v=%d\n", ply, moveToString(mc).c_str(), v);
                unplayMove(mc);
                if (*success == 0) return 0;
                if (v > alpha) {
                    if (v >= beta)
                    {
                        return v;
                    }
                    alpha = v;
                }
            }
        }
    }

    v = probe_wdl_table(success);

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
int chessposition::probe_wdl(int *success)
{
    *success = 1;
    int best_cap = -3, best_ep = -3;
    int i;

    // Generate (at least) all legal captures including (under)promotions.
    chessmovelist movelist;
    prepareStack();
    movelist.length = CreateMovelist<ALL>(&movelist.move[0]);

    // We do capture resolution, letting best_cap keep track of the best
    // capture without ep rights and letting best_ep keep track of still
    // better ep captures if they exist.
    for (i = 0; i < movelist.length; i++)
    {
        uint32_t mc = movelist.move[i].code;
        if (ISCAPTURE(mc))
        {
            if (playMove(mc))
            {
                //printf("probe_wdl (ply=%d) testing capture/promotion/evasion move %s...\n", ply, moveToString(mc).c_str());
                int v = -probe_ab(-2, -best_cap, success);
                //printf("probe_wdl (ply=%d) tested  capture/promotion/evasion move %s... v=%d\n", ply, moveToString(mc).c_str(), v);
                unplayMove(mc);
                if (*success == 0) return 0;
                if (v > best_cap) {
                    if (v == 2) {
                        *success = 2;
                        return 2;
                    }
                    if (!ISEPCAPTURE(mc))
                        best_cap = v;
                    else if (v > best_ep)
                        best_ep = v;
                }

            }
        }
    }

    int v = probe_wdl_table(success);
    if (*success == 0)
        return 0;

    // Now max(v, best_cap) is the WDL value of the position without ep rights.
    // If the position without ep rights is not stalemate or no ep captures
    // exist, then the value of the position is max(v, best_cap, best_ep).
    // If the position without ep rights is stalemate and best_ep > -3,
    // then the value of the position is best_ep (and we will have v == 0).

    if (best_ep > best_cap) {
        if (best_ep > v) { // ep capture (possibly cursed losing) is best.
            *success = 2;
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
        return best_cap;
    }

    // Now handle the stalemate case.
    if (best_ep > -3 && v == 0) {
        // Check for stalemate in the position with ep captures.
        for (i = 0; i < movelist.length; i++)
        {
            uint32_t mc = movelist.move[i].code;
            if (ISEPCAPTURE(mc))
                continue;

            if (playMove(mc))
            {
                unplayMove(mc);
                break;
            }
        }

        if (i == movelist.length) { // Stalemate detected.
            *success = 2;
            return best_ep;
        }
    }

    // Stalemate / en passant not an issue, so v is the correct value.
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
int chessposition::probe_dtz(int *success)
{
    int wdl = probe_wdl(success);
    if (*success == 0)
        return 0;

    // If draw, then dtz = 0.
    if (wdl == 0)
        return 0;

    // Check for winning (cursed) capture or ep capture as only best move.
    if (*success == 2)
        return wdl_to_dtz[wdl + 2];

    chessmovelist movelist;

    // If winning, check for a winning pawn move.
    if (wdl > 0) {
        // Generate at least all legal non-capturing pawn moves
        // including non-capturing promotions.
        prepareStack();
        movelist.length = CreateMovelist<ALL>(&movelist.move[0]);

        for (int i = 0; i < movelist.length; i++)
        {
            uint32_t mc = movelist.move[i].code;
            if (ISCAPTURE(mc) || (GETPIECE(mc) >> 1) != PAWN)
                continue;

            if (playMove(mc))
            {
                //printf("probe_dtz (ply=%d)testing non-capture pawn move %s...\n", ply, moveToString(mc).c_str());
                int v = -probe_wdl(success);
                //printf("probe_dtz (ply=%d)tested  non-capture pawn move %s... v=%d\n", ply, moveToString(mc).c_str(), v);
                unplayMove(mc);
                if (*success == 0)
                    return 0;

                if (v == wdl)
                    return wdl_to_dtz[wdl + 2];
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
        prepareStack();
        movelist.length = CreateMovelist<ALL>(&movelist.move[0]);
    }
    for (int i = 0; i < movelist.length; i++)
    {
        // We can skip pawn moves and captures.
        // If wdl > 0, we already caught them. If wdl < 0, the initial value
        // of best already takes account of them.
        uint32_t mc = movelist.move[i].code;
        if (ISCAPTURE(mc) || (GETPIECE(mc) >> 1) == PAWN)
            continue;

        if (playMove(mc))
        {
            //printf("probe_dtz (ply=%d) testing non-pawn non-capture %s... \n", ply, moveToString(mc).c_str());
            int v = -probe_dtz(success);
            //printf("probe_dtz (ply=%d) tested  non-pawn non-capture %s... v=%d\n", ply, moveToString(mc).c_str(), v);
            unplayMove(mc);
            if (*success == 0)
                return 0;

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
    return best;
}


static int wdl_to_Value[5] = {
    -SCORETBWIN,
    SCOREDRAW - 2,
    SCOREDRAW,
    SCOREDRAW + 2,
    SCORETBWIN
};

// Use the DTZ tables to filter out moves that don't preserve the win or draw.
// If the position is lost, but DTZ is fairly high, only keep moves that
// maximise DTZ.
//
// A return value of 0 indicates that not all probes were successful and that
// no moves were filtered out.
int chessposition::root_probe_dtz()
{
    int success;

    int dtz = probe_dtz(&success);
    if (!success)
        return 0;

    bool isBadMove;

    // Probe each move.
    for (int i = 0; i < rootmovelist.length; i++)
    {
        chessmove *m = &rootmovelist.move[i];
        isBadMove = !see(m->code, 0);
        playMove(m->code);
        //printf("info string root_probe_dtz (ply=%d) Testing move %s...\n", ply, m->toString().c_str());
        int v = 0;
        if (isCheckbb && dtz > 0) {
            chessmovelist nextmovelist;
            prepareStack();
            nextmovelist.length = CreateMovelist<ALL>(&nextmovelist.move[0]);
            bool foundevasion = false;
            for (int j = 0; j < nextmovelist.length; j++)
            {
                uint32_t nmc = nextmovelist.move[j].code;
                if (playMove(nmc))
                {
                    foundevasion = true;
                    unplayMove(nmc);
                    break;
                }
            }
            if (!foundevasion)
                v = 1;
        }
        if (!v) {
            if (halfmovescounter != 0) {
                v = -probe_dtz(&success);
                if (v > 0) v++;
                else if (v < 0) v--;
            }
            else {
                v = -probe_wdl(&success);
                v = wdl_to_dtz[v + 2];
            }

            // Flag moves with good DTZ that sac a piece
            if (isBadMove && v > 0)
                v += 0x10000;
        }

        //printf("info string root_probe_dtz (ply=%d) Tested  move %s... value=%d\n", ply, m->toString().c_str(), v);
        unplayMove(m->code);
        if (!success)
            return 0;

        m->value = v;
    }

    // Obtain 50-move counter for the root position.
    int cnt50 = halfmovescounter;

    // Test if there was any single repetition since last halfmove reset
    bool hasRepetition = false;
    for (int revply = 0, i = PREROOTMOVES; !hasRepetition &&  revply < prerootmovenum; revply++, i--) {
        U64 h = prerootmovestack[i].hash;
        int j = i - 4;
        while (i - j + revply <= prerootmovenum) {
            if (prerootmovestack[j].hash == h) {
                hasRepetition = true;
                break;
            }
            j = j - 2;
        }
    }
    
    // Now be a bit smart about filtering out moves.
    int mi = 0;
    if (dtz > 0) { // winning (or 50-move rule draw)
        int best = 0x1ffff;
        for (int i = 0; i < rootmovelist.length; i++)
        {
            chessmove *m = &rootmovelist.move[i];
            int v = m->value;
            if (v > 0 && v < best)
                best = v;
        }

        // If the current phase has not seen repetitions, then try all moves
        // that stay safely within the 50-move budget, if there are any.
        while (mi < rootmovelist.length)
        {
            int v = rootmovelist.move[mi].value;

            if (v <= 0 || ((hasRepetition || (v & 0xffff) + cnt50 >= 100) && (v & 0xffff) > best))
            {
                // delete moves that are known for not winning or not optimal and we already have a repetition
                rootmovelist.length--;
                swap(rootmovelist.move[mi], rootmovelist.move[rootmovelist.length]);
            }
            else
            {
                isBadMove = (v & 0x10000);
                v = (v & 0xffff);
                if (!en.Syzygy50MoveRule || v + cnt50 <= 100)
                    // win
                    rootmovelist.move[mi].value = SCORETBWIN - v - (hasRepetition ? cnt50 : 0) - isBadMove * 0x100;
                else
                    // cursed win = draw
                    rootmovelist.move[mi].value = SCOREDRAW;
                //printf("info string root_probe_dtz (ply=%d) Final value for move %s... value=%d rep=%d\n", ply, rootmovelist.move[mi].toString().c_str(), rootmovelist.move[mi].value, hasRepetition);
                mi++;
            }
        }
    }
    else if (dtz < 0) {
        int best = 0;
        for (int i = 0; i < rootmovelist.length; i++)
        {
            int v = rootmovelist.move[i].value;
            if (v < best)
                best = v;
        }
        while (mi < rootmovelist.length)
        {
            int v = rootmovelist.move[mi].value;
            if (en.Syzygy50MoveRule && -best + cnt50 > 100 && -v + cnt50 <= 100)
            {
                // We can reach a draw by 50-moves-rule so delete moves that don't preserve this
                rootmovelist.length--;
                swap(rootmovelist.move[mi], rootmovelist.move[rootmovelist.length]);
            }
            else
            {
                if (!en.Syzygy50MoveRule || -v + cnt50 <= 100)
                    // We will probably lose
                    rootmovelist.move[mi].value = -SCORETBWIN - v;
                else
                    // We can reach a draw by 50-moves-rule
                    rootmovelist.move[mi].value = SCOREDRAW;
                //printf("info string root_probe_dtz (ply=%d) Final value for move %s... value=%d rep=%d\n", ply, rootmovelist.move[mi].toString().c_str(), rootmovelist.move[mi].value, hasRepetition);
                mi++;
            }
        }
    }
    else { // drawing
        while (mi < rootmovelist.length)
        {
            if (rootmovelist.move[mi].value < 0)
            {
                // We can reach a draw so delete moves that don't preserve this
                rootmovelist.length--;
                swap(rootmovelist.move[mi], rootmovelist.move[rootmovelist.length]);
            }
            else {
                mi++;
            }
        }
    }

    // Be fair and report the expected score in any case (even if the opponenmt has a hard job to save the win/draw
    useRootmoveScore = 1;
    return 1;
}

// Use the WDL tables to filter out moves that don't preserve the win or draw.
// This is a fallback for the case that some or all DTZ tables are missing.
//
// A return value of 0 indicates that not all probes were successful and that
// no moves were filtered out.
int chessposition::root_probe_wdl()
{
    int success;
    int best = -2;

    // Probe each move.
    for (int i = 0; i < rootmovelist.length; i++)
    {
        chessmove *m = &rootmovelist.move[i];
        playMove(m->code);
        //printf("info string root_probe_wdl (ply=%d) Testing move %s...\n", ply, m->toString().c_str());
        int v = -probe_wdl(&success);
        //printf("info string root_probe_wdl (ply=%d) Tested  move %s... value=%d\n", ply, m->toString().c_str(), v);
        unplayMove(m->code);
        if (!success)
            return 0;
        if (!en.Syzygy50MoveRule)
            v = v > 0 ? 2 : v < 0 ? -2 : 0;

        m->value = v;

        if (v > best)
            best = v;
    }

    int mi = 0;
    while (mi < rootmovelist.length)
    {
        chessmove *m = &rootmovelist.move[mi];
        if (m->value < best)
        {
            // Delete non-optimal move
            //printf("info string root_probe_wdl (ply=%d) Removing non-optimal move %s...\n", ply, m->toString().c_str());
            rootmovelist.length--;
            swap(rootmovelist.move[mi], rootmovelist.move[rootmovelist.length]);
        }
        else {
            //printf("info string root_probe_wdl (ply=%d) Optimal move %s gets value %d\n", ply, m->toString().c_str(), wdl_to_Value[m->value + 2]);
            m->value = wdl_to_Value[m->value + 2];
            mi++;
        }
    }

    return (best <= 0); // When winning we need to search for the best move
}
