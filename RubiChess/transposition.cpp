
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
			if (i & (1 << j))
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

u8 zobrist::getHash(chessposition *p)
{
    u8 hash = 0;
    int i;
    int state = p->state;
    for (i = WPAWN; i <= BKING; i++)
    {
        U64 pmask = p->piece00[i];
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
	hash ^= ept[p->ept];

    return hash;
}

#else
u8 zobrist::getHash(chessposition *p)
{
    u8 hash = 0;
    int i;
    int state = p->state;
    for (i = 0; i < 120; i++)
    {
        if (!(i & 0x88) && p->board[i] != BLANK)
        {
            hash ^= boardtable[(i << 4)  | p->board[i]];
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

    if (p->ept)
        hash ^= ep[p->ept & 0x7];

    return hash;
}
#endif


transposition::transposition()
{
    size = 0;
}

transposition::~transposition()
{
    if (size > 0)
        delete table;
}

void transposition::setSize(int sizeMb)
{
    int msb = 0;
    if (size > 0)
        delete table;
    size = (sizeMb << 20) / sizeof(S_TRANSPOSITIONENTRY);
    if (MSB(msb, size))
        size = (1ULL << msb);

    sizemask = size - 1;
    table = (S_TRANSPOSITIONENTRY*)malloc((size_t)(size * sizeof(S_TRANSPOSITIONENTRY)));
    clean();
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


void transposition::addHash(int val, int valtype, int depth, unsigned long move)
{
    unsigned long long hash = pos->hash;
    unsigned long long index = hash & sizemask;
    S_TRANSPOSITIONENTRY *data = &table[index];

    if (data->hashupper == 0)
		used++;
    data->hashupper = (unsigned long)(hash >> 32);
    data->depth = (unsigned char)depth;
    if (MATEFORME(val))
        val += pos->ply;
    else if (MATEFOROPPONENT(val))
        val -= pos->ply;
    data->value = (short)val;
    data->flag = (char)valtype;
    data->movecode = move;
}


bool transposition::probeHash(int *val, unsigned long *movecode, int depth, int alpha, int beta)
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

zobrist zb;
