
#include "RubiChess.h"


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


u8 zobrist::modHash(int i)
{
    return 0;
}


u8 zobrist::getHash()
{
    u8 hash = 0;
    int i;
    int state = pos.state;
    for (i = WPAWN; i <= BKING; i++)
    {
        U64 pmask = pos.piece00[i];
        unsigned int index;
        while (LSB(index, pmask))
        {
            hash ^= boardtable[(index << 4) | i];
            pmask ^= (1ULL << index);
        }
    }

    if (state & S2MMASK)
        hash ^= s2m;

    hash ^= cstl[state & CASTLEMASK];
    hash ^= ept[pos.ept];

    return hash;
}

u8 zobrist::getPawnHash()
{
    u8 hash = 0;
    for (int i = WPAWN; i <= BPAWN; i++)
    {
        U64 pmask = pos.piece00[i];
        unsigned int index;
        while (LSB(index, pmask))
        {
            hash ^= boardtable[(index << 4) | i];
            pmask ^= (1ULL << index);
        }
    }
    // Store also kings position in pawn hash
    hash ^= boardtable[(pos.kingpos[0] << 4) | WKING] ^ boardtable[(pos.kingpos[1] << 4) | BKING];
    return hash;
}


u8 zobrist::getMaterialHash()
{
    u8 hash = 0;
    for (PieceCode pc = WPAWN; pc <= BKING; pc++)
    {
        int count = 0;
        for (int j = 0; j < BOARDSIZE; j++)
        {
                if (pos.mailbox[j] == pc)
                    hash ^= zb.boardtable[(count++ << 4) | pc];
        }
    }
    return hash;
}


transposition::~transposition()
{
    if (size > 0)
        delete table;
}

int transposition::setSize(int sizeMb)
{
    int restMb = 0;
    int msb = 0;
    if (size > 0)
        delete table;
    U64 maxsize = ((U64)sizeMb << 20) / sizeof(transpositioncluster);
    if (MSB(msb, maxsize))
    {
        size = (1ULL << msb);
        restMb = (int)(((maxsize ^ size) >> 20) * sizeof(transpositioncluster));  // return rest for pawnhash
    }

    sizemask = size - 1;
    table = (transpositioncluster*)malloc((size_t)(size * sizeof(transpositioncluster)));
    clean();
    return restMb;
}

void transposition::clean()
{
    memset(table, 0, (size_t)(size * sizeof(transpositioncluster)));
    used = 0;
    numOfSearchShiftTwo = 0xfc; // 0x3f << 2; will be incremented / reset to 0 before first search 
}

#if 0
bool transposition::testHash()
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    return ((table[index].hashupper)   ==  (hash >> 32));
}
#endif

unsigned int transposition::getUsedinPermill()
{
    if (size > 0)
        return (unsigned int) (used * 1000 / size / TTBUCKETNUM);
    else
        return 0;
}


void transposition::addHash(int val, int bound, int depth, uint16_t movecode)
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    transpositioncluster *cluster = &table[index];
    transpositionentry *e;
    transpositionentry *leastValuableEntry;

    for (int i = 0; i < TTBUCKETNUM; i++)
    {
        // First try to find a free or matching entry
        e = &(cluster->entry[i]);
        if (cluster->entry[i].hashupper == (uint32_t)(hash >> 32) || !cluster->entry[i].hashupper)
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

#if 1
    // Don't overwrite an entry from the same position, unless we have
    // an exact bound or depth that is nearly as good as the old one
    if (bound != HASHEXACT
        &&  leastValuableEntry->hashupper == (uint32_t)(hash >> 32)
        &&  depth < leastValuableEntry->depth - 3)
        return;
#endif

    if (leastValuableEntry->hashupper == 0)
        used++;
    leastValuableEntry->hashupper = (uint32_t)(hash >> 32);
    leastValuableEntry->depth = (uint8_t)depth;
    if (MATEFORME(val))
        val += pos->ply;
    else if (MATEFOROPPONENT(val))
        val -= pos->ply;
    leastValuableEntry->value = (short)val;
    leastValuableEntry->boundAndAge = (uint8_t)(bound | numOfSearchShiftTwo);
    leastValuableEntry->movecode = movecode;
}


void transposition::printHashentry()
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    transpositioncluster data = table[index];
    printf("Hashentry for %llx\n", hash);
    for (int i = 0; i < TTBUCKETNUM; i++)
    {
        if ((data.entry[i].hashupper) == (hash >> 32))
        {
            printf("Match in upper part: %x / %x\n", (unsigned int)data.entry[i].hashupper, (unsigned int)(hash >> 32));
            printf("Move code: %x\n", (unsigned int)data.entry[i].movecode);
            printf("Depth:     %d\n", data.entry[i].depth);
            printf("Value:     %d\n", data.entry[i].value);
            printf("BoundAge:  %d\n", data.entry[i].boundAndAge);
            return;
        }
    }
    printf("No match found\n");
}


bool transposition::probeHash(int *val, uint16_t *movecode, int depth, int alpha, int beta)
{
#ifdef EVALTUNE
    // don't use transposition table when tuning evaluation
    return false;
#endif
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    transpositioncluster data = table[index];
    for (int i = 0; i < TTBUCKETNUM; i++)
    {
        transpositionentry *e = &(data.entry[i]);
        if (e->hashupper == (hash >> 32))
        {
            *movecode = e->movecode;
            if (e->depth >= depth)
            {
                *val = e->value;
                if (MATEFORME(*val))
                    *val -= pos->ply;
                else if (MATEFOROPPONENT(*val))
                    *val += pos->ply;
                int bound = (e->boundAndAge & BOUNDMASK);
                if (bound == HASHEXACT)
                {
                    return true;
                }
                if (bound == HASHALPHA && *val <= alpha)
                {
                    *val = alpha;
                    return true;
                }
                if (bound == HASHBETA && *val >= beta)
                {
                    *val = beta;
                    return true;
                }
            }
            // found but depth too low
            return false;
        }
    }
    // not found
    return false;
}

#if 0
short transposition::getValue()
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    return table[index].value;
}

int transposition::getValtype()
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    return table[index].flag;
}

int transposition::getDepth()
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    return table[index].depth;
}
#endif
// FIXME: Is this really needed?
uint16_t transposition::getMoveCode()
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    transpositioncluster data = table[index];
    for (int i = 0; i < TTBUCKETNUM; i++)
    {
        if ((data.entry[i].hashupper) == (hash >> 32))
            return data.entry[i].movecode;
    }
    return 0;
}


pawnhash::~pawnhash()
{
    if (size > 0)
        delete table;
}

void pawnhash::setSize(int sizeMb)
{
    int msb = 0;
    if (size > 0)
        delete table;
    size = ((U64)sizeMb << 20) / sizeof(S_PAWNHASHENTRY);
    if (MSB(msb, size))
        size = (1ULL << msb);

    sizemask = size - 1;
    table = (S_PAWNHASHENTRY*)malloc((size_t)(size * sizeof(S_PAWNHASHENTRY)));
    clean();
}

void pawnhash::clean()
{
    memset(table, 0, (size_t)(size * sizeof(S_PAWNHASHENTRY)));
#ifdef DEBUG
    used = 0;
    hit = 0;
    query = 0;
#endif
}

bool pawnhash::probeHash(pawnhashentry **entry)
{
    unsigned long long hash = pos->pawnhash;
    unsigned long long index = hash & sizemask;
    *entry = &table[index];
#ifdef DEBUG
    query++;
#endif
    if (((*entry)->hashupper) == (hash >> 32))
    {
#ifdef DEBUG
        hit++;
#endif
#ifndef EVALTUNE
        // don't use pawn hash when tuning evaluation
        return true;
#endif
    }
    (*entry)->hashupper = (uint32_t)(hash >> 32);
    (*entry)->value = 0;
    (*entry)->attacked[0] = (*entry)->attacked[1] = 0ULL;
    (*entry)->attackedBy2[0] = (*entry)->attackedBy2[1] = 0ULL;

    return false;
}

unsigned int pawnhash::getUsedinPermill()
{
#ifdef DEBUG
    if (size > 0)
        return (unsigned int)(used * 1000 / size);
    else
        return 0;
#else
    return 0;
#endif
}


void repetition::clean()
{
    memset(table, 0, 0x10000);
}

void repetition::addPosition(unsigned long long hash)
{
    table[hash & 0xffff]++;
}

void repetition::removePosition(unsigned long long hash)
{
    table[hash & 0xffff]--;
}

int repetition::getPositionCount(unsigned long long hash)
{
    return table[hash & 0xffff];
}

repetition rp;
transposition tp;
pawnhash pwnhsh;
