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
static const size_t HashAlignBytes = 2ull << 20;
#include <sys/mman.h> // madvise
#endif

/* A small noncryptographic PRNG */
/* http://www.burtleburtle.net/bob/rand/smallprng.html */

u8 ranval(ranctx *x) {
    u8 e = x->a - rot(x->b, 7);
    x->a = x->b ^ rot(x->c, 13);
    x->b = x->c + rot(x->d, 37);
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

void raninit(ranctx *x, u8 seed) {
    u8 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i = 0; i<20; ++i) {
        (void)ranval(x);
    }
}

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


u8 zobrist::getHash(chessposition *pos)
{
    u8 hash = 0;
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
        }
    }

    if (state & S2MMASK)
        hash ^= s2m;

    hash ^= cstl[state & CASTLEMASK];
    hash ^= ept[pos->ept];

    return hash;
}

u8 zobrist::getPawnHash(chessposition *pos)
{
    u8 hash = 0;
    for (int i = WPAWN; i <= BPAWN; i++)
    {
        U64 pmask = pos->piece00[i];
        unsigned int index;
        while (pmask)
        {
            index = pullLsb(&pmask);
            hash ^= boardtable[(index << 4) | i];
        }
    }
    // Store also kings position in pawn hash
    hash ^= boardtable[(pos->kingpos[0] << 4) | WKING] ^ boardtable[(pos->kingpos[1] << 4) | BKING];
    return hash;
}


u8 zobrist::getMaterialHash(chessposition *pos)
{
    u8 hash = 0;
    for (PieceCode pc = WPAWN; pc <= BKING; pc++)
    {
        int count = 0;
        for (int j = 0; j < BOARDSIZE; j++)
        {
                if (pos->mailbox[j] == pc)
                    hash ^= zb.boardtable[(count++ << 4) | pc];
        }
    }
    return hash;
}


transposition::~transposition()
{
    if (size > 0)
        freealigned64(table);
}

int transposition::setSize(int sizeMb)
{
    int restMb = 0;
    int msb = 0;
    if (size > 0)
        freealigned64(table);
    size_t clustersize = sizeof(transpositioncluster);
#ifdef SDEBUG
    // Don't use the debugging part of the cluster for calculation of size to get consistent search with non SDEBUG
    clustersize = offsetof(transpositioncluster, debugHash);
#endif
    U64 maxsize = ((U64)sizeMb << 20) / clustersize;
    if (!maxsize) return 0;
    GETMSB(msb, maxsize);
    size = (1ULL << msb);
    restMb = (int)(((maxsize ^ size) >> 20) * clustersize);  // return rest for pawnhash
    sizemask = size - 1;
    size_t allocsize = (size_t)(size * sizeof(transpositioncluster));

#if defined(__linux__) && !defined(__ANDROID__) // Many thanks to Sami Kiminki for advise on the huge page theory and for this patch
    // Round up hashSize to the next 2M for alignment
    allocsize = ((allocsize + HashAlignBytes - 1u) / HashAlignBytes) * HashAlignBytes;

    table = (transpositioncluster*)aligned_alloc(HashAlignBytes, allocsize);

    // Linux-specific call to request huge pages, in case the aligned_alloc()
    // call above doesn't already trigger them (depends on transparent huge page
    // settings)
    madvise(table, allocsize, MADV_HUGEPAGE);
#else
    table = (transpositioncluster*)allocalign64(allocsize);
#endif

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
            if ((table[i].entry[j].boundAndAge & 0xfc) == numOfSearchShiftTwo)
                used++;

    return used;
}


void transposition::addHash(U64 hash, int val, int16_t staticeval, int bound, int depth, uint16_t movecode)
{
    unsigned long long index = hash & sizemask;
    transpositioncluster *cluster = &table[index];
    transpositionentry *e;
    transpositionentry *leastValuableEntry;

    for (int i = 0; i < TTBUCKETNUM; i++)
    {
        // First try to find a free or matching entry
        e = &(cluster->entry[i]);
        if (cluster->entry[i].hashupper == GETHASHUPPER(hash) || !cluster->entry[i].hashupper)
        {
            leastValuableEntry = e;
            break;
        }

        if (i == 0)
        {
            // initialize leastValuableEntry
            leastValuableEntry = e;
            continue;
        }

        if (e->depth - ((259 + numOfSearchShiftTwo - e->boundAndAge) & 0xfc) * 2
            < leastValuableEntry->depth - ((259 + numOfSearchShiftTwo - leastValuableEntry->boundAndAge) & 0xfc) * 2)
        {
            // found a new less valuable entry
            leastValuableEntry = e;
        }
    }

    // Don't overwrite an entry from the same position, unless we have
    // an exact bound or depth that is nearly as good as the old one
    if (bound != HASHEXACT
        &&  leastValuableEntry->hashupper == GETHASHUPPER(hash)
        &&  depth < leastValuableEntry->depth - 3)
        return;

    leastValuableEntry->hashupper = GETHASHUPPER(hash);
#ifdef SDEBUG
    if (cluster->debugHash && (uint32_t)(cluster->debugHash >> 32) == leastValuableEntry->hashupper)
        cluster->debugStoredBy = "";

#endif
    leastValuableEntry->depth = (uint8_t)depth;
    leastValuableEntry->value = (short)val;
    leastValuableEntry->boundAndAge = (uint8_t)(bound | numOfSearchShiftTwo);
    leastValuableEntry->movecode = movecode;
    leastValuableEntry->staticeval = staticeval;
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


bool transposition::probeHash(U64 hash, int *val, int *staticeval, uint16_t *movecode, int depth, int alpha, int beta, int ply)
{
#ifdef EVALTUNE
    // don't use transposition table when tuning evaluation
    return false;
#endif
    unsigned long long index = hash & sizemask;
    transpositioncluster* data = &table[index];
    for (int i = 0; i < TTBUCKETNUM; i++)
    {
        transpositionentry *e = &(data->entry[i]);
        if (e->hashupper == GETHASHUPPER(hash))
        {
            *movecode = e->movecode;
            *staticeval = e->staticeval;
            int bound = (e->boundAndAge & BOUNDMASK);
            int v = FIXMATESCOREPROBE(e->value, ply);
            if (bound == HASHEXACT)
            {
                *val = v;
                return (e->depth >= depth);
            }
            if (bound == HASHALPHA && v <= alpha)
            {
                *val = alpha;
                return (e->depth >= depth);
            }
            if (bound == HASHBETA && v >= beta)
            {
                *val = beta;
                return (e->depth >= depth);
            }
            // value outside boundary
            return false;
        }
    }
    // not found
    return false;
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


void Materialhash::init()
{
    size_t tablesize = (size_t)MATERIALHASHSIZE * sizeof(Materialhashentry);
    table = (Materialhashentry*)allocalign64(tablesize);
    memset(table, 0, tablesize);
}

void Materialhash::remove()
{
    freealigned64(table);
}

bool  Materialhash::probeHash(U64 hash, Materialhashentry **entry)
{
    *entry = &table[hash & MATERIALHASHMASK];
    if ((*entry)->hash == hash)
        return true;

    (*entry)->endgame = nullptr;
    (*entry)->hash = hash;
    return false;
}


transposition tp;
