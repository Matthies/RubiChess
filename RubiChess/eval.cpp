
#include "RubiChess.h"

// Evaluation stuff
CONSTEVAL int tempo = 4;
CONSTEVAL int passedpawnbonus[8] = { 0,   14,   14,   38,   72,  114,  131,    0 };
CONSTEVAL int attackingpawnbonus[8] = { 0,  -15,   -7,  -12,    3,   38,    0,    0 };
CONSTEVAL int isolatedpawnpenalty = -16;
CONSTEVAL int doublepawnpenalty = -23;
CONSTEVAL int connectedbonus = 0;
CONSTEVAL int kingshieldbonus = 16;
CONSTEVAL int backwardpawnpenalty = -22;
CONSTEVAL int slideronfreefilebonus = 13;
CONSTEVAL int doublebishopbonus = 27;
CONSTEVAL int scalephaseshift = 6;
CONSTEVAL int materialvalue[7] = { 0,  100,  322,  329,  488,  986,32509 };
CONSTEVAL int PVBASE[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
       61,   55,   59,   56,   57,   57,   35,   82,
       29,   23,    9,   25,   18,   19,   14,   11,
        6,   -5,  -14,    8,   13,  -10,  -10,   -5,
        4,  -16,    0,   30,   18,  -10,  -18,   -9,
        1,   -3,    6,   -9,   -4,  -12,   -2,   10,
       -5,   -5,   -9,  -25,  -24,   -1,    9,  -12,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {  -143,  -29,  -25,   16,   14,  -49,  -11,  -63,
      -41,  -12,   -4,   36,   19,   20,  -44,  -37,
      -14,   14,   34,   55,   50,   34,   32,   -8,
       11,   10,   35,   33,   38,   36,   21,   50,
      -11,    1,   20,   22,   24,   18,    5,  -11,
      -28,   12,    9,   15,    8,   22,   34,  -24,
      -59,  -22,  -10,   13,   12,    7,  -37,    1,
      -69,  -24,  -24,  -33,  -19,  -21,  -16,  -46  },
  {   -12,    0,   13,   -1,  -24,  -37,  -12,   -3,
      -30,   11,    2,   12,    4,    4,   -2,  -42,
       12,   19,   27,   27,   29,   55,   35,   10,
       -7,    6,   18,   43,   37,   12,    7,    4,
        3,    9,   13,   35,   27,    5,   19,    0,
        4,   20,   11,   13,    8,   20,   21,   -1,
       16,    4,   20,   -1,   10,  -14,   27,  -22,
      -42,  -45,  -13,   -6,  -16,  -14,   10,  -20  },
  {     2,   28,   17,   16,   20,   35,   22,   23,
       29,   29,   29,   42,   34,   30,   24,   37,
       20,   12,   22,   37,   19,   22,    8,   22,
        9,   -2,    8,    8,    3,    7,   -4,    1,
       -7,  -15,   -8,   -1,  -16,   -3,   -3,  -32,
      -32,   -9,   -2,   -6,   -8,   -7,  -16,  -28,
      -35,  -16,  -11,   -2,   -8,   15,  -14,  -60,
      -12,   -6,   -3,    3,    8,    6,  -25,  -21  },
  {    -7,   10,    3,   -6,   41,    9,   63,   -3,
      -25,  -50,    8,   33,   29,   32,    1,   26,
      -32,   -5,  -12,    6,   50,   60,   57,   -2,
      -31,  -32,   -5,  -18,    9,    9,  -11,  -18,
      -29,  -30,  -31,  -23,  -17,   -9,   -4,  -31,
      -33,  -23,  -20,   -5,  -12,  -14,    1,   -9,
      -31,  -10,   -2,   -3,    3,   -8,   -4,  -62,
      -14,  -48,    0,   12,  -21,  -51,  -92,  -19  },
  {   -15,  -37,  -51,   11,  -21,  -21, -60/*-362*/,  -72,
      -30,  -18,  -23,    1,   18,  -50,  -67,  -32,
      -75,   13,    0,   10,    5,  -22,    2,  -19,
        0,   -9,  -13,   -9,  -14,  -17,   -1,   -7,
      -21,   -3,  -17,  -17,  -21,  -11,  -24,  -14,
      -17,  -15,  -30,  -18,  -14,  -24,  -11,  -21,
        5,  -14,  -24,  -34,  -28,  -20,  -11,   -9,
      -21,   10,   23,  -36,    4,  -37,   21,   19  }
 };
CONSTEVAL int PVPHASEDIFF[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
      -24,  -12,  -21,  -14,  -23,  -15,   -6,  -32,
        5,    3,    3,  -23,  -34,  -35,    0,   -6,
       11,   23,    6,  -12,  -23,   13,   19,   19,
       11,   29,    1,  -47,  -32,   10,   30,   11,
        3,    6,  -13,   11,    7,   13,    4,   -8,
       12,   -6,    1,   -1,   -3,   -2,  -11,    5,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {     0,    7,   -6,  -36,    1,    2,    5,    0,
        2,   14,    7,    0,   -1,    0,    8,    3,
        1,  -14,    0,  -10,    0,  -12,  -33,    0,
        2,    0,    0,    0,    0,    0,    0,  -75,
        1,   -1,    0,    7,    0,    0,    0,    0,
        0,   -3,  -12,    0,   -1,  -26,  -65,    0,
       29,   17,    7,  -24,  -35,  -17,   14,    1,
       31,    0,    1,    4,    1,    2,  -48,    1  },
  {    15,   15,   -7,   -1,    4,    7,    8,   39,
       23,    0,    7,   -3,    0,   -3,   -1,   35,
       -1,   -4,   -1,    0,   -4,  -17,  -41,   -3,
       -1,    0,    0,  -10,   -9,   -7,    0,  -14,
      -15,   -3,    0,   -5,   -9,    0,   -1,    0,
        1,  -22,    5,    0,    0,    5,   -3,    0,
      -62,    4,   -8,   11,   -7,   25,  -28,    1,
        9,    0,    0,    3,    9,    7,    0,    2  },
  {    -1,    0,   13,    0,    0,    0,    0,    0,
        0,    0,   -1,   -7,   -7,    0,   -6,  -15,
       -1,   -1,   -4,  -12,   -6,   -6,   -6,   -9,
       -1,   -1,   -1,    0,   -2,   -2,   -8,    0,
        0,    3,   11,    0,   10,    0,    0,   11,
        6,    0,    0,   -2,    2,    0,    1,   12,
       15,    0,    7,   -5,    0,  -22,    0,   18,
        3,  -11,   10,   -1,  -14,  -25,   19,    3  },
  {     0,  -16,   -1,   29,  -23,   -6,  -29,   13,
        0,   47,   -1,   -1,    0,    0,   48,  -15,
       32,    7,    5,   -3,  -15,   -2,   -8,    0,
       16,   36,   16,    0,   11,    6,   32,   30,
       12,    0,   33,   63,   32,   28,   15,   96,
        2,    2,    2,    5,    1,   60,    1,    7,
       32,    0,   -8,    0,   -6,  -29,    0,   34,
        1,   46,    1,  -64,    0,    3,    1,   -4  },
  {     7,    2,   39,   27,   24,   10,  -29,    8,
      -10,    7,   31,   34,   15,   24,   24,   13,
       22,   24,   25,   31,   27,   26,   16,   19,
       17,   19,   38,   48,   43,   27,   12,    9,
       15,   17,   27,   43,   43,   27,   19,   12,
       12,   15,   26,   27,   31,   34,   12,   16,
        3,   11,   22,   27,   23,   15,   10,    1,
      -36,  -39,  -51,   -3,  -36,    0,  -51,  -63  }
 };


//double kingdangerfactor = 0.15;
//double kingdangerexponent = 1.1;

int squaredistance[BOARDSIZE][BOARDSIZE];
int kingdanger[BOARDSIZE][BOARDSIZE][7];
int passedpawnbonusperside[2][8];
int attackingpawnbonusperside[2][8];


#ifdef EVALTUNE
void registeralltuners()
{
    int i, j;
#if 1
    // tuning other values
    registerTuner(&tempo, "tempo", tempo, 0, 0, 0, 0, NULL, false);
    for (i = 0; i < 8; i++)
        registerTuner(&passedpawnbonus[i], "passedpawnbonus", passedpawnbonus[i], i, 8, 0, 0, &CreatePositionvalueTable, i == 0 || i == 7);
    for (i = 0; i < 8; i++)
        registerTuner(&attackingpawnbonus[i], "attackingpawnbonus", attackingpawnbonus[i], i, 8, 0, 0 , &CreatePositionvalueTable, i == 0 || i == 7);
    registerTuner(&isolatedpawnpenalty, "isolatedpawnpenalty", isolatedpawnpenalty, 0, 0, 0, 0, NULL, false);
    registerTuner(&doublepawnpenalty, "doublepawnpenalty", doublepawnpenalty, 0, 0, 0, 0, NULL, false);
    registerTuner(&connectedbonus, "connectedbonus", connectedbonus, 0, 0, 0, 0, NULL, false);
    registerTuner(&kingshieldbonus, "kingshieldbonus", kingshieldbonus, 0, 0, 0, 0, NULL, false);
    registerTuner(&backwardpawnpenalty, "backwardpawnpenalty", backwardpawnpenalty, 0, 0, 0, 0, NULL, false);
    registerTuner(&slideronfreefilebonus, "slideronfreefilebonus", slideronfreefilebonus, 0, 0, 0, 0, NULL, false);
    registerTuner(&doublebishopbonus, "doublebishopbonus", doublebishopbonus, 0, 0, 0, 0, NULL, false);
    registerTuner(&scalephaseshift, "scalephaseshift", scalephaseshift, 0, 0, 0, 0, NULL, false);
#endif
#if 1
    // tuning material value
    for (i = BLANK; i <= KING; i++)
        registerTuner(&materialvalue[i], "materialvalue", materialvalue[i], i, 7, 0, 0, &CreatePositionvalueTable, i <= PAWN || i >= KING);
#endif
#if 1
    // tuning the psqt base at game start
    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 64; j++)
        {
            registerTuner(&PVBASE[i][j], "PVBASE", PVBASE[i][j], j, 64, i, 6, &CreatePositionvalueTable, PVBASE[i][j] <= -9999);
        }
    }
#endif
#if 1
    //tuning the psqt phase development
    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 64; j++)
        {
            registerTuner(&PVPHASEDIFF[i][j], "PVPHASEDIFF", PVPHASEDIFF[i][j], j, 64, i, 6, &CreatePositionvalueTable, PVPHASEDIFF[i][j] <= -9999);
        }
    }
#endif
}
#endif


void chessposition::init()
{
    int rs = 0;
    for (int b = BOARDSIZE; b ^ 0x8; b >>= 1)
        rs++;
    for (int i = 0; i < BOARDSIZE; i++)
    {
        int fi = i & 7;
        int ri = i >> rs;
        for (int j = 0; j < BOARDSIZE; j++)
        {
            int fj = j & 7;
            int rj = j >> rs;
            squaredistance[i][j] = max(abs(fi - fj), abs(ri - rj));
        }
    }
#ifdef EVALTUNE
    registeralltuners();
#endif

    positionvaluetable = NULL;
    CreatePositionvalueTable();
}



void CreatePositionvalueTable()
{
    for (int r = 0; r < 8; r++)
    {
        passedpawnbonusperside[0][r] = passedpawnbonus[r];
        passedpawnbonusperside[1][7 - r] = -passedpawnbonus[r];
        attackingpawnbonusperside[0][r] = attackingpawnbonus[r];
        attackingpawnbonusperside[1][7 - r] = -attackingpawnbonus[r];

    }

    if (!pos.positionvaluetable)
        pos.positionvaluetable = new int[2 * 8 * 256 * BOARDSIZE];  // color piecetype phase boardindex

    for (int i = 0; i < BOARDSIZE; i++)
    {
#ifdef BITBOARD
        int j1 = (i ^ 0x38);
        int j2 = i;
#else
        int j1 = (i & 0x7) + ((7 - ((i & 0x70) >> 4)) << 3);
        int j2 = (i & 0x7) + ((i & 0x70) >> 1);
#endif
        for (int p = PAWN; p <= KING; p++)
        {
            for (int ph = 0; ph < 256; ph++)
            {
#ifdef BITBOARD
                int index1 = i | (ph << 6) | (p << 14);
                int index2 = index1 | (1 << 17);
#else
                int index1 = i | (ph << 7) | (p << 15);
                int index2 = index1 | (1 << 18);
#endif
                pos.positionvaluetable[index1] = (PVBASE[(p - 1)][j1] * (255 - ph) + (PVBASE[(p - 1)][j1] + PVPHASEDIFF[(p - 1)][j1]) * ph) / 255;
                pos.positionvaluetable[index2] = -(PVBASE[(p - 1)][j2] * (255 - ph) + (PVBASE[(p - 1)][j2] + PVPHASEDIFF[(p - 1)][j2])* ph) / 255;
                pos.positionvaluetable[index1] += materialvalue[p];
                pos.positionvaluetable[index2] -= materialvalue[p];
            }
#if 0 // kingdanger disabled for now as it doesn't work this way
            for (int j = 0; j < BOARDSIZE; j++)
            {
                kingdanger[i][j][p] = (int)(pow((7 - squaredistance[i][j]), kingdangerexponent) * sqrt(materialvalue[p]) * kingdangerfactor);
            }
#endif
        }
    }
}


#ifdef BITBOARD

int chessposition::getPawnValue()
{
    int val = 0;
    int index;
    pawnhashentry *entry;

    if (!pwnhsh.probeHash(&entry))
    {
        entry->value = 0;
        for (int pc = WPAWN; pc <= BPAWN; pc++)
        {
            int s = pc & S2MMASK;
            entry->passedpawnbb[s] = 0ULL;
            entry->isolatedpawnbb[s] = 0ULL;
            entry->backwardpawnbb[s] = 0ULL;
            U64 pb = piece00[pc];
            while (LSB(index, pb))
            {
                if (!(passedPawnMask[index][s] & piece00[pc ^ S2MMASK]))
                {
                    // passed pawn
                    entry->passedpawnbb[s] |= BITSET(index);
                }

                if (!(piece00[pc] & neighbourfilesMask[index]))
                {
                    // isolated pawn
                    entry->isolatedpawnbb[s] |= BITSET(index);
                }
                else
                {
                    if (pawn_attacks_occupied[index][s] & piece00[pc ^ S2MMASK])
                    {
                        // pawn attacks opponent pawn
                        entry->value += attackingpawnbonusperside[s][RANK(index)];
#ifdef DEBUGEVAL
                        debugeval("Attacking Pawn Bonus(%d): %d\n", index, attackingpawnbonusperside[s][RANK(index)]);
#endif
                    }
                    if ((pawn_attacks_occupied[index][s ^ S2MMASK] & piece00[pc]) || (phalanxMask[index] & piece00[pc]))
                    {
                        // pawn is protected by other pawn
                        entry->value += S2MSIGN(s) * connectedbonus;
#ifdef DEBUGEVAL
                        debugeval("Connected Pawn Bonus(%d): %d\n", index, S2MSIGN(s) * connectedbonus);
#endif
                    }
                    if (!((passedPawnMask[index][1 - s] | phalanxMask[index]) & piece00[pc]))
                    {
                        // test for backward pawn
                        U64 opponentpawns = piece00[pc ^ S2MMASK] & passedPawnMask[index][s];
                        U64 mypawns = piece00[pc] & neighbourfilesMask[index];
                        U64 pawnstoreach = opponentpawns | mypawns;
                        int nextpawn;
                        if (s ? MSB(nextpawn, pawnstoreach) : LSB(nextpawn, pawnstoreach))
                        {
                            U64 nextpawnrank = rankMask[nextpawn];
                            U64 shiftneigbours = (s ? nextpawnrank >> 8 : nextpawnrank << 8);
                            if ((nextpawnrank | (shiftneigbours & neighbourfilesMask[index])) & opponentpawns)
                            {
                                // backward pawn detected
                                entry->backwardpawnbb[s] |= BITSET(index);
                            }
                        }
                    }
                }
                pb ^= BITSET(index);
            }
        }
    }

    for (int s = 0; s < 2; s++)
    {
        U64 bb;
        bb = entry->passedpawnbb[s];
        while (LSB(index, bb))
        {
            val += ph * passedpawnbonusperside[s][RANK(index)] / 256;
            bb ^= BITSET(index);
#ifdef DEBUGEVAL
            debugeval("Passed Pawn Bonus(%d): %d\n", index, passedpawnbonusperside[s][RANK(index)]);
#endif
        }

        // isolated pawns
        val += S2MSIGN(s) * POPCOUNT(entry->isolatedpawnbb[s]) * isolatedpawnpenalty;
#ifdef DEBUGEVAL
        debugeval("Isolated Pawn Penalty(%d): %d\n", s, S2MSIGN(s) * POPCOUNT(entry->isolatedpawnbb[s]) * isolatedpawnpenalty);
#endif

        // doubled pawns
        val += S2MSIGN(s) * doublepawnpenalty * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8));
#ifdef DEBUGEVAL
        debugeval("Double Pawn Penalty(%d): %d\n", s, S2MSIGN(s) * doublepawnpenalty * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8)));
#endif

        // backward pawns
        val += S2MSIGN(s) * POPCOUNT(entry->backwardpawnbb[s]) * backwardpawnpenalty * ph / 256;
#ifdef DEBUGEVAL
        debugeval("Backward Pawn Penalty(%d): %d\n", s, S2MSIGN(s) * POPCOUNT(entry->backwardpawnbb[s]) * backwardpawnpenalty * ph / 256);
#endif

    }

#ifdef DEBUGEVAL
    debugeval("Total Pawn value: %d\n", val + entry->value);
#endif
    return val + entry->value;
}



int chessposition::getValue()
{
    // Check for insufficient material using simnple heuristic from chessprogramming site
    if (!(piece00[WPAWN] | piece00[BPAWN]))
    {
        if (!(piece00[WQUEEN] | piece00[BQUEEN] | piece00[WROOK] | piece00[BROOK]))
        {
            if (POPCOUNT(piece00[WBISHOP] | piece00[WKNIGHT]) <= 2
                && POPCOUNT(piece00[BBISHOP] | piece00[BKNIGHT]) <= 2)
            {
                bool winpossible = false;
                // two bishop win if opponent has none
                if (abs(POPCOUNT(piece00[WBISHOP]) - POPCOUNT(piece00[BBISHOP])) == 2)
                    winpossible = true;
                // bishop and knight win against bare king
                if ((piece00[WBISHOP] && piece00[WKNIGHT] && !(piece00[BBISHOP] | piece00[BKNIGHT]))
                    || (piece00[BBISHOP] && piece00[BKNIGHT] && !(piece00[WBISHOP] | piece00[WKNIGHT])))
                    winpossible = true;

                if (!winpossible)
                    return SCOREDRAW;
            }
        }
    }
    ph = phase();
    return getPositionValue() + getPawnValue();
}


int chessposition::getPositionValue()
{
    int index;
    int scalephase = (~ph & 0xff) >> scalephaseshift; ;
    int result = S2MSIGN(state & S2MMASK) * tempo;
#ifdef DEBUGEVAL
    int positionvalue = 0;
    int kingdangervalue[2] = { 0, 0 };
#endif

    for (int pc = WPAWN; pc <= BKING; pc++)
    {
        int p = pc >> 1;
        int s = pc & S2MMASK;
        U64 pb = piece00[pc];

        while (LSB(index, pb))
        {
            int pvtindex = index | (ph << 6) | (p << 14) | (s << 17);
            result += *(positionvaluetable + pvtindex);
            // Kingdanger disabled for now; doesn't work this way, maybe just needs some tuning
            //result += S2MSIGN(s) * kingdanger[index][pos.kingpos[1 - s]][p];
#ifdef DEBUGEVAL
            positionvalue += *(positionvaluetable + pvtindex);
            kingdangervalue[s] += S2MSIGN(s) * kingdanger[index][pos.kingpos[1 - s]][p];
#endif
            pb ^= BITSET(index);
            if (shifting[p] & 0x2) // rook and queen
            {
#ifdef ROTATEDBITBOARD
                tobits |= (rank_attacks[from][mask] & opponentorfreebits);

                U64 mobility = ~occupied00[s]
                    & ((rank_attacks[index][((occupied00[0] | occupied00[1]) >> rotshift[index]) & 0x3f])
                        | (file_attacks[index][((occupied90[0] | occupied90[1]) >> rot90shift[index]) & 0x3f]));
#else
                U64 mobility = ~occupied00[s]
                    & (mRookAttacks[index][MAGICROOKINDEX((occupied00[0] | occupied00[1]), index)]);
#endif
                result += (S2MSIGN(s) * POPCOUNT(mobility) * scalephase);

                // extrabonus for shifter on open file
                if (!(filebarrierMask[index][s] & piece00[WPAWN | s]))
                {
                    // free file
                    result += S2MSIGN(s) * slideronfreefilebonus;
#ifdef DEBUGEVAL
                    debugeval("Slider on free file Bonus: %d\n", (S2MSIGN(s) * 15));
#endif
                }
            }

            if (shifting[p] & 0x1) // bishop and queen)
            {
#ifdef ROTATEDBITBOARD
                U64 mobility = ~occupied00[s]
                    & ((diaga1h8_attacks[index][((occupieda1h8[0] | occupieda1h8[1]) >> rota1h8shift[index]) & 0x3f])
                        | (diagh1a8_attacks[index][((occupiedh1a8[0] | occupiedh1a8[1]) >> roth1a8shift[index]) & 0x3f]));
#else
                U64 mobility = ~occupied00[s]
                    & (mBishopAttacks[index][MAGICBISHOPINDEX((occupied00[0] | occupied00[1]), index)]);
#endif
                result += (S2MSIGN(s) * POPCOUNT(mobility) * scalephase);
            }
        }
    }

    // bonus for double bishop
    if (POPCOUNT(piece00[WBISHOP]) >= 2)
        result += doublebishopbonus;
    if (POPCOUNT(piece00[BBISHOP]) >= 2)
        result -= doublebishopbonus;


    // some kind of king safety
	result += (256 - ph) * (POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1])) * kingshieldbonus / 256;

#ifdef DEBUGEVAL
    debugeval("(getPositionValue)  King safety: %d\n", (256 - ph) * (POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1])) * 15 / 256);
    debugeval("(getPositionValue)  Position value: %d\n", positionvalue);
    debugeval("(getPositionValue)  Kingdanger value: %d / %d\n", kingdangervalue[0], kingdangervalue[1]);
#endif
    return result;
}


#else //BITBOARD

int chessposition::getPawnValue()
{
    // not implemented for now
#if 0
    if (pwnhsh.probeHash(&val))
        return val;

    // calculate the pawn value

    pwnhsh.addHash(val);
    return val;
#endif
    return 0;
}


int chessposition::getValue()
{
    // Check for insufficient material using simnple heuristic from chessprogramming site
    if (piecenum[WPAWN] == 0 && piecenum[BPAWN] == 0)
    {
        if (piecenum[WQUEEN] == 0 && piecenum[BQUEEN] == 0 && piecenum[WROOK] == 0 && piecenum[BROOK] == 0)
        {
            if (piecenum[WBISHOP] + piecenum[WKNIGHT] <= 2
                && piecenum[BBISHOP] + piecenum[BKNIGHT] <= 2)
            {
                bool winpossible = false;
                // two bishop win if opponent has none
                if (abs(piecenum[WBISHOP] - piecenum[BBISHOP]) == 2)
                    winpossible = true;
                // bishop and knight win against bare king
                if (piecenum[WBISHOP] * piecenum[WKNIGHT] > piecenum[BBISHOP] + piecenum[BKNIGHT]
                    || piecenum[BBISHOP] * piecenum[BKNIGHT] > piecenum[WBISHOP] + piecenum[WKNIGHT])
                    winpossible = true;

                if (!winpossible)
                    return SCOREDRAW;
            }
        }
    }

    return getPositionValue();
}



/* Value of the position from whites pov */
int chessposition::getPositionValue()
{
    ph = phase();
    int result = S2MSIGN(state & S2MMASK) * tempo;
    int firstpawn[2][10] = { 0 };
    int lastpawn[2][10] = { 0 };
    int i;

    for (int f = 0; f < 8; f++)
    {
        for (int r = 0; r < 8; r++)
        {
            i = (r << 4) | f;
            if (Piece(i) == PAWN)
            {
                int col = mailbox[i] & S2MMASK;
                if (mailbox[i + S2MSIGN(col) * 16] == mailbox[i])
                    // double pawn penalty
                    result += S2MSIGN(col) * doublepawnpenalty;
                if (col == 1 || !lastpawn[col][f + 1])
                    lastpawn[col][f + 1] = r;
                if (col == 0 || !firstpawn[col][f + 1])
                    firstpawn[col][f + 1] = r;
            }
        }
        for (int r = 0; r < 8; r++)
        {
            i = (r << 4) | f;
            if (mailbox[i] != BLANK)
            {
                PieceType pt = Piece(i);
                int col = mailbox[i] & S2MMASK;
                int index = i | (ph << 7) | (pt << 15) | (col << 18);
                result += *(positionvaluetable + index);

                if ((pt == ROOK || pt == QUEEN) && (firstpawn[col][f + 1] == 0 || ((col && (firstpawn[col][f + 1] > r)) || (!col && (firstpawn[col][f + 1] < r)))))
                    // ROOK on free file
                    result += S2MSIGN(col) * slideronfreefilebonus;

            }
        }
    }
    for (int f = 0; f < 8; f++)
    {
        for (int col = 0; col < 2; col++)
        {
            int opcol = 1 - col;
            int pawnrank = firstpawn[col][f + 1];
            if (pawnrank)
            {
                // check for passed pawn
                if ((!lastpawn[opcol][f] || S2MSIGN(col) * lastpawn[opcol][f] <= S2MSIGN(col) * pawnrank)
                        && (!lastpawn[opcol][f + 1] || S2MSIGN(col) * lastpawn[opcol][f + 1] <= S2MSIGN(col) * pawnrank)
                        && (!lastpawn[opcol][f + 2] || S2MSIGN(col) * lastpawn[opcol][f + 2] <= S2MSIGN(col) * pawnrank))
                    //result += passedpawnbonus[col][pawnrank];
                    result += ph * passedpawnbonusperside[col][pawnrank] / 256;
            }
            if (pawnrank && !firstpawn[col][f] && !firstpawn[col][f + 2])
                // isolated pawn
                result += S2MSIGN(col) * isolatedpawnpenalty;
        }
    }

    return result;
}


void chessposition::countMaterial()
{
    for (int i = 0; i < 14; i++)
        piecenum[i] = 0;
    for (int r = 0; r < 8; r++)
    {
        for (int f = 0; f < 8; f++)
        {
            PieceCode pc = mailbox[(r << 4) | f];
            if (pc != BLANK)
            {
                piecenum[pc]++;
            }
        }
    }
}
#endif

