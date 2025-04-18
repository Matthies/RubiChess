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


#include "RubiChess.h"

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/mman.h> // madvise
#endif

using namespace rubichess;

namespace rubichess {

zobrist::zobrist()
{
    raninit(&rnd, 0);
    int i;
    int j = 0;
    for (i = 0; i < 128 * 16; i++)
    {
        unsigned long long r = getRnd();
        // make the boardtable and so the hash compatible with older 0x88-board representation
        if (!((i >> 4) & 0x88))
            boardtable[j++] = r;
    }
    // make hashing simpler but stay compatible to board88
    unsigned long long castle[4];
    unsigned long long ep[8];
    for (i = 0; i < 4; i++)
        castle[i] = getRnd();
    for (i = 0; i < 8; i++)
        ep[i] = getRnd();
    for (i = 0; i < 32; i++)
    {
        cstl[i] = 0ULL;
        for (j = 0; j < 4; j++)
        {
            if (i & (1 << (j+1)))
                cstl[i] ^= castle[j];
        }
    }
    for (i = 0; i < 64; i++)
    {
        ept[i] = 0ULL;
        if (RANK(i) == 2 || RANK(i) == 5)
            ept[i] = ep[FILE(i)];
    }

    s2m = getRnd();
}


unsigned long long zobrist::getRnd()
{
    return ranval(&rnd);
}


void zobrist::getAllHashes(chessposition* pos)
{
    U64 hash = 0, pawnhash = 0, nonpawnhash[2] = {0};

    int i;
    int state = pos->state;
    for (i = WPAWN; i <= BKING; i++)
    {
        U64 pmask = pos->piece00[i];
        unsigned int index;
        while (pmask)
        {
            index = pullLsb(&pmask);
            hash ^= boardtable[(index << 4) | i];
            if (i <= BPAWN)
                pawnhash ^= boardtable[(index << 4) | i];
            else
                nonpawnhash[i & S2MMASK] ^= boardtable[(index << 4) | i];
        }
    }

    if (state & S2MMASK)
        hash ^= s2m;

    hash ^= cstl[state & CASTLEMASK];
    hash ^= ept[pos->ept];

    pos->hash = hash;
    pos->pawnhash = pawnhash;
    pos->nonpawnhash[WHITE] = nonpawnhash[WHITE];
    pos->nonpawnhash[BLACK] = nonpawnhash[BLACK];
}


U64 zobrist::getPawnKingHash(chessposition* pos)
{
    return pos->pawnhash ^ boardtable[(pos->kingpos[0] << 4) | WKING] ^ boardtable[(pos->kingpos[1] << 4) | BKING];
}


U64 zobrist::getMaterialHash(chessposition* pos)
{
    U64 hash = 0;
    for (PieceCode pc = WPAWN; pc <= BKING; pc++)
    {
        int count = POPCOUNT(pos->piece00[pc]);
        for (int i = 0; i < count; i++)
            hash ^= zb.boardtable[(i << 4) | pc];
    }
    return hash;
}



transposition::~transposition()
{
    if (size > 0)
        my_large_free(table);
}

int transposition::setSize(int sizeMb)
{
    int restMb = 0;
    int msb = 0;
    if (size > 0)
        my_large_free(table);
    size_t clustersize = sizeof(transpositioncluster);
#ifdef SDEBUG
    // Don't use the debugging part of the cluster for calculation of size to get consistent search with non SDEBUG
    clustersize = offsetof(transpositioncluster, debugHash);
#endif
    U64 maxsize = ((U64)sizeMb << 20) / clustersize;
    if (!maxsize)
    {
        size = 0ULL;
        return 0;
    }
    GETMSB(msb, maxsize);
    size = (1ULL << msb);
    restMb = (int)(((maxsize ^ size) >> 20) * clustersize);  // return rest for pawnhash
    size_t allocsize = (size_t)(size * sizeof(transpositioncluster));

#if defined(__linux__) && !defined(__ANDROID__) // Many thanks to Sami Kiminki for advise on the huge page theory and for this patch
    // Round up hashSize to the next 2M for alignment
    constexpr size_t HashAlignBytes = 2ull << 20;
    allocsize = ((allocsize + HashAlignBytes - 1u) / HashAlignBytes) * HashAlignBytes;

    table = (transpositioncluster*)aligned_alloc(HashAlignBytes, allocsize);

    // Linux-specific call to request huge pages, in case the aligned_alloc()
    // call above doesn't already trigger them (depends on transparent huge page
    // settings)
    madvise(table, allocsize, MADV_HUGEPAGE);
#else
    table = (transpositioncluster*)my_large_malloc(allocsize);
#endif
    if (!table) {
        // alloc failed, back to old size
        size = 0;
        sizeMb = (int)(((sizemask + 1) * clustersize) >> 20);
        cerr << "Keeping " << sizeMb << "MByte Hash.\n";
        return setSize(sizeMb);
    }

    sizemask = size - 1;
    clean();
    return restMb;
}

void transposition::clean()
{
    size_t totalsize = size * sizeof(transpositioncluster);
    size_t sizePerThread = totalsize / en.Threads;
    thread tthread[MAXTHREADS];
    for (int i = 0; i < en.Threads; i++)
    {
        void *start = (char*)table + i * sizePerThread;
        tthread[i] = thread(memset, start, 0, sizePerThread);
    }
    memset((char*)table + en.Threads * sizePerThread, 0, totalsize - en.Threads * sizePerThread);
    for (int i = 0; i < en.Threads; i++)
    {
        if (tthread[i].joinable())
            tthread[i].join();
    }
    numOfSearchShiftTwo = 0;
}


unsigned int transposition::getUsedinPermill()
{
    unsigned int used = 0;

    // Take 1000 samples
    for (int i = 0; i < 1000 / TTBUCKETNUM; i++)
        for (int j = 0; j < TTBUCKETNUM; j++)
            if ((table[i].entry[j].boundAndAge & AGEMASK) == numOfSearchShiftTwo)
                used++;

    return used;
}


void transposition::addHash(ttentry* entry, U64 hash, int val, int16_t staticeval, int bound, int depth, uint16_t movecode)
{
#ifdef EVALTUNE
    // don't use transposition table when tuning evaluation
    return;
#endif
    const hashupper_t hashupper = GETHASHUPPER(hash);
    const uint8_t ttdepth = depth - TTDEPTH_OFFSET;

    // Don't overwrite an entry from the same position, unless we have
    // an exact bound or depth that is nearly as good as the old one
    if (bound == HASHEXACT
        || entry->hashupper != hashupper
        || ttdepth + 3 >= entry->depth)
    {
        entry->hashupper = hashupper;
        entry->depth = (uint8_t)ttdepth;
        entry->boundAndAge = (uint8_t)(bound | numOfSearchShiftTwo);
        entry->movecode = movecode;
        entry->staticeval = staticeval;
        entry->value = (int16_t)val;
    }
}


void transposition::printHashentry(U64 hash)
{
    unsigned long long index = hash & sizemask;
    transpositioncluster *data = &table[index];
    printf("Hashentry for %llx\n", hash);
    for (int i = 0; i < TTBUCKETNUM; i++)
    {
        if ((data->entry[i].hashupper) == GETHASHUPPER(hash))
        {
            printf("Match in upper part: %x / %x\n", (unsigned int)data->entry[i].hashupper, (unsigned int)(hash >> 32));
            printf("Move code: %x\n", (unsigned int)data->entry[i].movecode);
            printf("Depth:     %d\n", data->entry[i].depth);
            printf("Value:     %d\n", data->entry[i].value);
            printf("Eval:      %d\n", data->entry[i].staticeval);
            printf("BoundAge:  %d\n", data->entry[i].boundAndAge);
            return;
        }
    }
    printf("No match found\n");
}


ttentry* transposition::probeHash(U64 hash, bool* bFound)
{
    transpositioncluster* cluster = &table[hash & sizemask];
    ttentry* e;
    const hashupper_t hashupper = GETHASHUPPER(hash);

    for (int i = 0; i < TTBUCKETNUM; i++)
    {
        // First try to find a free or matching entry
        e = &(cluster->entry[i]);
        if (e->hashupper == hashupper || !e->depth)
        {
            *bFound = (bool)e->depth;
            e->boundAndAge = (e->boundAndAge & BOUNDMASK) | numOfSearchShiftTwo;
            return e;
        }
    }

    *bFound = false;
    ttentry* leastValuableEntry = &(cluster->entry[0]);

    for (int i = 1; i < TTBUCKETNUM; i++)
    {
        e = &(cluster->entry[i]);
        if (e->depth - ((AGECYCLE + numOfSearchShiftTwo - e->boundAndAge) & AGEMASK) * 2
            < leastValuableEntry->depth - ((AGECYCLE + numOfSearchShiftTwo - leastValuableEntry->boundAndAge) & AGEMASK) * 2)
        {
            // found a new less valuable entry
            leastValuableEntry = e;
        }
    }
    return leastValuableEntry;
}


uint16_t transposition::getMoveCode(U64 hash)
{
    unsigned long long index = hash & sizemask;
    transpositioncluster *data = &table[index];
    for (int i = 0; i < TTBUCKETNUM; i++)
    {
        if ((data->entry[i].hashupper) == GETHASHUPPER(hash))
            return data->entry[i].movecode;
    }
    return 0;
}


void Pawnhash::setSize(int sizeMb)
{
    int msb = 0;
    sizeMb = max(sizeMb, 16);
    U64 size = ((U64)sizeMb << 20) / sizeof(S_PAWNHASHENTRY);
    if (!size) return;
    GETMSB(msb, size);
    size = (1ULL << msb);

    sizemask = size - 1;
    size_t tablesize = (size_t)size * sizeof(S_PAWNHASHENTRY);
    table = (S_PAWNHASHENTRY*)allocalign64(tablesize);
    memset(table, 0, tablesize);
}


void Pawnhash::remove()
{
    freealigned64(table);
}


bool Pawnhash::probeHash(U64 hash, pawnhashentry **entry)
{
    unsigned long long index = hash & sizemask;
    *entry = &table[index];
    if (((*entry)->hashupper) == (hash >> 32))
    {
#ifndef EVALTUNE
        // don't use pawn hash when tuning evaluation
        return true;
#endif
    }
    (*entry)->hashupper = (uint32_t)(hash >> 32);
    (*entry)->value = 0;
    (*entry)->semiopen[0] = (*entry)->semiopen[1] = 0xff;
    (*entry)->passedpawnbb[0] = (*entry)->passedpawnbb[1] = 0ULL;
    (*entry)->attacked[0] = (*entry)->attacked[1] = 0ULL;
    (*entry)->attackedBy2[0] = (*entry)->attackedBy2[1] = 0ULL;

    return false;
}


transposition tp;

} // namespace rubichess
