
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
#ifdef BITBOARD
        // make the boardtable and so the hash compatible with older 0x88-board representation
        if (!((i >> 4) & 0x88))
            boardtable[j++] = r;
#else
        boardtable[j++] = r;
#endif
    }
#ifdef BITBOARD
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

#else
    for (i = 0; i < 4; i++)
        castle[i] = getRnd();
    for (i = 0; i < 8; i++)
        ep[i] = getRnd();
#endif
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

#ifdef BITBOARD

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

    return hash;
}

#else
u8 zobrist::getHash()
{
    u8 hash = 0;
    int i;
    int state = pos.state;
    for (i = 0; i < 120; i++)
    {
        if (!(i & 0x88) && pos.board[i] != BLANK)
        {
            hash ^= boardtable[(i << 4)  | pos.board[i]];
        }
    }
    if (state & S2MMASK)
        hash ^= s2m;
    state >>= 1;

    for (i = 0; i < 4; i++)
    {
        if (state & 1)
            hash ^= castle[i];
        state >>= 1;
    }

    if (pos.ept)
        hash ^= ep[pos.ept & 0x7];

    return hash;
}


u8 zobrist::getPawnHash()
{
    u8 hash = 0;
    int i;
    for (i = 0; i < 120; i++)
    {
        if (!(i & 0x88) && (pos.board[i] >> 1) == PAWN)
        {
            hash ^= boardtable[(i << 4) | pos.board[i]];
        }
    }

    return hash;
}

#endif


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
    U64 maxsize = (sizeMb << 20) / sizeof(S_TRANSPOSITIONENTRY);
    if (MSB(msb, maxsize))
    {
        size = (1ULL << msb);
        restMb = (int)((maxsize ^ size) >> 20) * sizeof(S_TRANSPOSITIONENTRY);  // return rest for pawnhash
    }

    sizemask = size - 1;
    table = (S_TRANSPOSITIONENTRY*)malloc((size_t)(size * sizeof(S_TRANSPOSITIONENTRY)));
    clean();
    return restMb;
}

void transposition::clean()
{
	memset(table, 0, (size_t)(size * sizeof(S_TRANSPOSITIONENTRY)));
	used = 0;
}

bool transposition::testHash()
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    return ((table[index].hashupper)   ==  (hash >> 32));
}

unsigned int transposition::getUsedinPermill()
{
    if (size > 0)
		return (unsigned int) (used * 1000 / size);
	else
		return 0;
}


void transposition::addHash(int val, int valtype, int depth, uint32_t move)
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    S_TRANSPOSITIONENTRY *data = &table[index];

    if (data->hashupper == 0)
		used++;
    data->hashupper = (uint32_t)(hash >> 32);
    data->depth = (unsigned char)depth;
    if (MATEFORME(val))
        val += pos->ply;
    else if (MATEFOROPPONENT(val))
        val -= pos->ply;
    data->value = (short)val;
    data->flag = (char)valtype;
    data->movecode = move;
}


void transposition::printHashentry()
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    S_TRANSPOSITIONENTRY data = table[index];
    printf("Hashentry for %llx\n", hash);
    if ((data.hashupper) == (hash >> 32))
    {
        printf("Match in upper part: %x / %x\n", (unsigned int)data.hashupper, (unsigned int)(hash >> 32));
        printf("Move code: %x\n", (unsigned int)data.movecode);
        printf("Depth:     %d\n", data.depth);
        printf("Value:     %d\n", data.value);
        printf("Valuetype: %d\n", data.flag);
    }
    else {
        printf("No match in upper part: %x / %x", (unsigned int)data.hashupper, (unsigned int)(hash >> 32));
    }
}


bool transposition::probeHash(int *val, uint32_t *movecode, int depth, int alpha, int beta)
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    S_TRANSPOSITIONENTRY data = table[index];
    if ((data.hashupper) == (hash >> 32))
    {
        *movecode = data.movecode;
        if (data.depth >= depth)
        {
            *val = data.value;
            if (MATEFORME(*val))
                *val -= pos->ply;
            else if (MATEFOROPPONENT(*val))
                *val += pos->ply;
            char flag = data.flag;
            if (flag == HASHEXACT)
            {
                return true;
            }
            if (flag == HASHALPHA && *val <= alpha)
            {
                *val = alpha;
                return true;
            }
            if (flag == HASHBETA && *val >= beta)
            {
                *val = beta;
                return true;
            }
        }
    }

    return false;
}

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

#if 1  //FIXME: Hier muss die neue Struktur von classnmove noch movecode angepasst werden 
chessmove transposition::getMove()
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
#ifdef BITBOARD
    chessmove cm(table[index].movecode);
#else
    chessmove cm;
    cm.code = table[index].movecode;
#endif
    return cm;
}
#endif


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
    size = (sizeMb << 20) / sizeof(S_PAWNHASHENTRY);
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
        return true;
    }
    (*entry)->hashupper = (hash >> 32);
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
