
#include "RubiChess.h"

// Evaluation stuff
CONSTEVAL int kingattackweight[7] = { 0,    0,    3,    3,    3,    4,    0 };
CONSTEVAL int KingSafetyFactor = 278;
CONSTEVAL int tempo = 4;
CONSTEVAL int passedpawnbonus[8] = { 0,   19,   15,   41,   74,  128,  143,    0 };
CONSTEVAL int attackingpawnbonus[8] = { 0,   -7,  -11,   -4,    3,   37,    0,    0 };
CONSTEVAL int isolatedpawnpenalty = -13;
CONSTEVAL int doublepawnpenalty = -21;
CONSTEVAL int connectedbonus = 2;
CONSTEVAL int kingshieldbonus = 13;
CONSTEVAL int backwardpawnpenalty = -25;
CONSTEVAL int doublebishopbonus = 42;
CONSTEVAL int shiftmobilitybonus = 2;
CONSTEVAL int slideronfreefilebonus[2] = { 14,   18 };
CONSTEVAL int materialvalue[7] = { 0,  100,  314,  314,  504, 1026,32509 };


CONSTEVAL int PVBASE[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
       60,   61,   97,   52,   47,   57,   50,   66,
       24,   27,   23,   22,   17,   24,   25,   29,
       11,    7,   -4,   17,   10,  -10,  -16,  -15,
        1,  -12,    2,   24,   20,   -4,  -20,  -14,
        2,   -3,    3,  -14,   -4,  -11,   -1,   -1,
      -11,   -9,  -15,  -22,  -23,    3,   11,  -17,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {  -178,    1,  -14,    6,   -2,  -17,   20,  -50,
      -17,  -41,   -9,   49,   22,   12,  -19,  -40,
      -20,   21,   23,   43,   24,   34,   41,  -10,
      -15,    8,   22,   34,   31,   39,   17,   38,
      -22,    1,   19,   17,   24,   16,    7,  -10,
      -29,    2,    6,   13,    9,   18,   35,  -18,
      -28,  -35,  -18,    8,   17,   17,  -40,  -14,
      -51,  -16,  -40,  -22,  -44,  -12,   -9,    1  },
  {    -1,   14,   -5,    8,   14,   -1,   29,  -33,
      -16,   11,   -7,   13,   22,   45,    7,  -25,
       11,   30,   29,   27,   40,   59,   32,   10,
       -4,   13,   22,   46,   36,   28,   13,    8,
      -18,   14,   22,   38,   30,    8,   19,   -5,
        2,   24,   12,   16,   10,   21,   16,   -7,
       55,    0,    9,   -2,   15,    2,   27,    1,
      -27,  -28,   -9,   -8,   -3,  -10,  -26,   -1  },
  {     6,   16,    7,   23,   20,   20,   21,   22,
       22,   26,   30,   39,   36,   24,   30,   41,
       16,   10,   20,   25,   19,   20,   25,   17,
        6,    0,   20,   16,    2,    7,   16,   -2,
      -32,  -12,  -11,   -6,  -10,   -6,   -3,  -14,
      -37,  -19,  -14,   -6,  -13,  -11,   -3,  -18,
      -35,  -24,   -6,   -6,  -12,    6,   -5,  -73,
       -9,   -5,    6,    1,    9,    4,  -43,  -19  },
  {    -8,    1,    0,  -12,   21,   52,   53,  -17,
      -19,  -42,   13,   21,   34,   35,  -16,    6,
      -28,  -11,    1,   12,   19,   25,   20,   -2,
      -31,  -24,  -12,  -17,  -12,  -24,  -31,  -20,
       -8,  -17,  -17,  -30,   -9,  -21,   -3,  -38,
       -3,    0,   -8,  -13,   -5,  -15,    7,  -19,
        3,    0,    0,   -3,    1,    6,   -4,  -30,
      -22,  -33,    0,   13,   -7,  -43,  -68,  -28  },
  {   -79,    2,    9,    5,  -13,   42,   73, -148,
       44,    4,   -8,   12,   26,   52,   47,  -11,
        1,    7,   16,   -7,    7,   19,   45,   11,
      -27,   15,  -14,  -20,  -21,   11,   15,    1,
      -23,    0,   -2,  -14,  -12,    2,   -6,  -22,
      -31,  -13,  -13,   -3,   -4,  -16,  -10,  -26,
      -17,   -9,  -14,  -26,  -26,  -12,   -9,   -9,
      -19,   17,   19,  -39,   -9,  -42,   17,   14  }
 };
CONSTEVAL int PVPHASEDIFF[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
      -24,  -12,  -36,  -15,  -24,  -28,   -5,  -36,
        7,    0,   -3,  -23,  -36,  -47,    0,  -21,
       14,    9,    5,  -31,  -22,    7,   20,   19,
       11,   29,    1,  -43,  -32,    0,   31,   11,
        3,    6,  -13,   16,    5,   13,   -5,   -7,
       12,    7,   12,    0,   11,    1,  -11,    5,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {    95,    3,   -1,  -34,  -12,    2,   -3,   -5,
        2,   17,    6,  -23,   -7,   -7,   15,    4,
        2,  -12,   -1,  -24,    0,  -11,  -48,    0,
        3,   -6,   -3,    0,    0,    0,    0,  -68,
        2,   -1,    0,    0,   -7,    0,    0,    0,
        0,  -18,  -12,   -7,   -3,  -27,  -58,    0,
       31,   19,  -15,  -24,  -39,  -17,    5,  -29,
      -48,    0,    2,    4,   -5,  -11,  -48,    1  },
  {    13,    8,   -1,   -1,    3,   11,    9,   39,
       23,    2,    7,   -2,   -3,  -23,    7,    3,
        6,   -4,   -7,   -3,   -7,  -36,  -25,   -3,
       -1,    0,    0,   -9,   -8,   -9,    0,  -12,
       19,   -1,    0,   -6,   -8,    0,    6,    0,
        1,  -15,    9,    0,    0,    1,   -6,   14,
      -79,  -12,   -6,   12,   -5,   22,  -28,  -54,
        8,    1,    0,   12,    9,   10,    0,    2  },
  {    -1,   -2,   13,    0,    3,    0,    0,    0,
       -1,   -3,   -6,  -11,   -9,    0,   -6,  -18,
       -1,    5,   -6,  -12,   -5,  -12,   -6,   -6,
       -1,   -1,   -5,   -1,   -1,   -2,  -10,   -1,
       30,   12,   12,    3,   11,    4,    0,   16,
       24,   24,    7,    5,    7,    3,    1,   12,
       23,    5,    7,    2,    0,  -24,    1,   48,
        2,  -10,   -8,   -1,  -15,  -24,   48,   11  },
  {     0,  -16,   -1,   29,  -15,   -5,  -29,   20,
        0,   47,   -1,   -1,   -1,    0,   64,  -10,
       31,    4,    4,   -1,   -8,   -1,   -2,    3,
       38,   58,   17,   29,   40,   24,   94,   31,
       13,   17,   34,   61,   34,   29,   15,   89,
        1,  -12,    2,    5,    5,   56,    1,    7,
        1,    0,   -8,    0,   -7,  -29,  -13,    4,
        1,   46,    1,  -65,    0,    5,   16,  -19  },
  {    23,   18,   47,   30,   86,   14,  -59,   13,
       -5,   17,   38,   31,   15,   31,   24,   13,
       23,   27,   31,   35,   27,   26,   19,   19,
       24,   19,   43,   56,   43,   27,   19,    7,
       14,   19,   27,   43,   50,   27,   23,   12,
       14,   15,   27,   27,   33,   37,   13,   18,
        3,   11,   23,   28,   31,   18,   10,   -3,
      -36,  -38,  -54,    0,  -36,   12,  -51,  -63  }
 };

// Shameless copy of chessprogramming wiki
CONSTEVAL int KingSafetyTable[100] = {
    0,   0,   0,   1,   1,   2,   3,   4,   5,   6,
    8,  10,  13,  16,  20,  25,  30,  36,  42,  48,
    55,  62,  70,  80,  90, 100, 110, 120, 130, 140,
    150, 160, 170, 180, 190, 200, 210, 220, 230, 240,
    250, 260, 270, 280, 290, 300, 310, 320, 330, 340,
    350, 360, 370, 380, 390, 400, 410, 420, 430, 440,
    450, 460, 470, 480, 490, 500, 510, 520, 530, 540,
    550, 560, 570, 580, 590, 600, 610, 620, 630, 640,
    650, 650, 650, 650, 650, 650, 650, 650, 650, 650,
    650, 650, 650, 650, 650, 650, 650, 650, 650, 650
};

int passedpawnbonusperside[2][8];
int attackingpawnbonusperside[2][8];


#ifdef EVALTUNE
void registeralltuners()
{
    int i, j;

#if 0 // averaging psqt (...OLD[][]) to a symmetric 4x8 map
    for (i = 0; i < 6; i++)
    {
        for (int y = 0; y < 8; y++)
        {
            for (int x = 0; x < 4; x++)
            {
                PVBASE[i][y * 4 + x] = (PVBASEOLD[i][y * 8 + x] + PVBASEOLD[i][y * 8 + 7 - x]) / 2;
                PVPHASEDIFF[i][y * 4 + x] = (PVPHASEDIFFOLD[i][y * 8 + x] + PVPHASEDIFFOLD[i][y * 8 + 7 - x]) / 2;
            }
        }
    }
#endif

#if 0
    // tuning other values
    for (i = 0; i < 7; i++)
        registerTuner(&kingattackweight[i], "kingattackweight", kingattackweight[i], i, 7, 0, 0, NULL, i < 2 || i > 5);
    registerTuner(&KingSafetyFactor, "KingSafetyFactor", KingSafetyFactor, 0, 0, 0, 0, NULL, false);
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
    registerTuner(&doublebishopbonus, "doublebishopbonus", doublebishopbonus, 0, 0, 0, 0, NULL, false);
    registerTuner(&shiftmobilitybonus, "shiftmobilitybonus", shiftmobilitybonus, 0, 0, 0, 0, NULL, false);
    for (i = 0; i < 2; i++)
        registerTuner(&slideronfreefilebonus[i], "slideronfreefilebonus", slideronfreefilebonus[i], i, 2, 0, 0, NULL, false);
#endif
#if 0
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
        int j1 = (i ^ 0x38);
        int j2 = i;

        for (int p = PAWN; p <= KING; p++)
        {
            for (int ph = 0; ph < 256; ph++)
            {
                int index1 = i | (ph << 6) | (p << 14);
                int index2 = index1 | (1 << 17);
                pos.positionvaluetable[index1] = (PVBASE[(p - 1)][j1] * (255 - ph) + (PVBASE[(p - 1)][j1] + PVPHASEDIFF[(p - 1)][j1]) * ph) / 255;
                pos.positionvaluetable[index2] = -(PVBASE[(p - 1)][j2] * (255 - ph) + (PVBASE[(p - 1)][j2] + PVPHASEDIFF[(p - 1)][j2])* ph) / 255;
                pos.positionvaluetable[index1] += materialvalue[p];
                pos.positionvaluetable[index2] -= materialvalue[p];
            }
        }
    }
}


static void printEvalTrace(int level, string s, int v[])
{
    printf("%*s %32s %*s : %5d | %5d | %5d\n", level * 2 + 1, "-", s.c_str(), 20 - level * 2, " ", v[0], -v[1], v[0] + v[1]);
}

static void printEvalTrace(int level, string s, int v)
{
    printf("%*s %32s %*s :       |       | %5d\n", level * 2 + 1, "-", s.c_str(), 20 - level * 2, " ", v);
}


template <EvalTrace Et>
int chessposition::getPawnValue(pawnhashentry **entry)
{
    int val = 0;
    int index;
    int attackingpawnval[2] = { 0 };
    int connectedpawnval[2] = { 0 };
    int passedpawnval[2] = { 0 };
    int isolatedpawnval[2] = { 0 };
    int doubledpawnval[2] = { 0 };
    int backwardpawnval[2] = { 0 };

    bool hashexist = pwnhsh.probeHash(entry);
    pawnhashentry *entryptr = *entry;
    if (!hashexist)
    {
        entryptr->value = 0;
        for (int pc = WPAWN; pc <= BPAWN; pc++)
        {
            int s = pc & S2MMASK;
            entryptr->semiopen[s] = 0xff; 
            entryptr->passedpawnbb[s] = 0ULL;
            entryptr->isolatedpawnbb[s] = 0ULL;
            entryptr->backwardpawnbb[s] = 0ULL;
            U64 pb = piece00[pc];
            while (LSB(index, pb))
            {
                entryptr->semiopen[s] &= ~BITSET(FILE(index)); 
                if (!(passedPawnMask[index][s] & piece00[pc ^ S2MMASK]))
                {
                    // passed pawn
                    entryptr->passedpawnbb[s] |= BITSET(index);
                }

                if (!(piece00[pc] & neighbourfilesMask[index]))
                {
                    // isolated pawn
                    entryptr->isolatedpawnbb[s] |= BITSET(index);
                }
                else
                {
                    if (pawn_attacks_occupied[index][s] & piece00[pc ^ S2MMASK])
                    {
                        // pawn attacks opponent pawn
                        entryptr->value += attackingpawnbonusperside[s][RANK(index)];
                        if (Et == TRACE)
                            attackingpawnval[s] += attackingpawnbonusperside[s][RANK(index)];
                    }
                    if ((pawn_attacks_occupied[index][s ^ S2MMASK] & piece00[pc]) || (phalanxMask[index] & piece00[pc]))
                    {
                        // pawn is protected by other pawn
                        entryptr->value += S2MSIGN(s) * connectedbonus;
                        if (Et == TRACE)
                            connectedpawnval[s] += S2MSIGN(s) * connectedbonus;
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
                                entryptr->backwardpawnbb[s] |= BITSET(index);
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
        bb = entryptr->passedpawnbb[s];
        while (LSB(index, bb))
        {
            val += ph * passedpawnbonusperside[s][RANK(index)] / 256;
            bb ^= BITSET(index);
            if (Et == TRACE)
                passedpawnval[s] += ph * passedpawnbonusperside[s][RANK(index)] / 256;
        }

        // isolated pawns
        val += S2MSIGN(s) * POPCOUNT(entryptr->isolatedpawnbb[s]) * isolatedpawnpenalty;
        if (Et == TRACE)
            isolatedpawnval[s] += S2MSIGN(s) * POPCOUNT(entryptr->isolatedpawnbb[s]) * isolatedpawnpenalty;

        // doubled pawns
        val += S2MSIGN(s) * doublepawnpenalty * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8));
        if (Et == TRACE)
            doubledpawnval[s] += S2MSIGN(s) * doublepawnpenalty * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8));

        // backward pawns
        val += S2MSIGN(s) * POPCOUNT(entryptr->backwardpawnbb[s]) * backwardpawnpenalty * ph / 256;
        if (Et == TRACE)
            backwardpawnval[s] += S2MSIGN(s) * POPCOUNT(entryptr->backwardpawnbb[s]) * backwardpawnpenalty * ph / 256;
    }

    if (Et == TRACE)
    {
        printEvalTrace(2, "connected pawn", connectedpawnval);
        printEvalTrace(2, "attacking pawn", attackingpawnval);
        printEvalTrace(2, "passed pawn", passedpawnval);
        printEvalTrace(2, "isolated pawn", isolatedpawnval);
        printEvalTrace(2, "doubled pawn", doubledpawnval);
        printEvalTrace(2, "backward pawn", backwardpawnval);
        printEvalTrace(1, "total pawn", val + entryptr->value);
    }

    return val + entryptr->value;
}


template <EvalTrace Et>
int chessposition::getPositionValue()
{
    pawnhashentry *phentry;
    int index;
    int result = S2MSIGN(state & S2MMASK) * tempo;
    int kingattackweightsum[2] = { 0 };
    int psqteval[2] = { 0 };
    int mobilityeval[2] = { 0 };
    int freeslidereval[2] = { 0 };
    int doublebishopeval = 0;
    int kingshieldeval[2] = { 0 };
    int kingdangereval[2] = { 0 };

    if (Et == TRACE)
        printEvalTrace(1, "tempo", result);

    result += getPawnValue<Et>(&phentry);

    for (int pc = WPAWN; pc <= BKING; pc++)
    {
        int p = pc >> 1;
        int s = pc & S2MMASK;
        U64 pb = piece00[pc];
        while (LSB(index, pb))
        {
            int pvtindex = index | (ph << 6) | (p << 14) | (s << 17);

            result += *(positionvaluetable + pvtindex);
            if (Et == TRACE)
                psqteval[s] += *(positionvaluetable + pvtindex);

            pb ^= BITSET(index);

            U64 mobility = 0ULL;
            if (shifting[p] & 0x2) // rook and queen
            {
                mobility = ~occupied00[s]
                    & (mRookAttacks[index][MAGICROOKINDEX((occupied00[0] | occupied00[1]), index)]);

                // extrabonus for rook on (semi-)open file  
                if (p == ROOK && (phentry->semiopen[s] & BITSET(FILE(index))))
                {
                    result += S2MSIGN(s) * slideronfreefilebonus[bool(phentry->semiopen[1 - s] & BITSET(FILE(index)))];
                    if (Et == TRACE)
                        freeslidereval[s] += S2MSIGN(s) * slideronfreefilebonus[bool(phentry->semiopen[1 - s] & BITSET(FILE(index)))];
                }
            }

            if (shifting[p] & 0x1) // bishop and queen)
            {
                mobility |= ~occupied00[s]
                    & (mBishopAttacks[index][MAGICBISHOPINDEX((occupied00[0] | occupied00[1]), index)]);
            }

            if (p == KNIGHT)
            {
                mobility = knight_attacks[index] & ~occupied00[s];
            }
            // mobility bonus
            result += S2MSIGN(s) * POPCOUNT(mobility) * shiftmobilitybonus;
            if (Et == TRACE)
                mobilityeval[s] += S2MSIGN(s) * POPCOUNT(mobility) * shiftmobilitybonus;

            // king danger
            U64 kingdangerarea = kingdangerMask[kingpos[1 - s]][1 - s];
            if (mobility & kingdangerarea)
            {
                kingattackweightsum[s] += POPCOUNT(mobility & kingdangerarea) * kingattackweight[p];
            }

        }
    }

    // bonus for double bishop
    if (POPCOUNT(piece00[WBISHOP]) >= 2)
    {
        result += doublebishopbonus;
        if (Et == TRACE)
            doublebishopeval += doublebishopbonus;
    }
    if (POPCOUNT(piece00[BBISHOP]) >= 2)
    {
        result -= doublebishopbonus;
        if (Et == TRACE)
            doublebishopeval -= doublebishopbonus;
    }

    // some kind of king safety
    result += (256 - ph) * (POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1])) * kingshieldbonus / 256;
    if (Et == TRACE)
    {
        kingshieldeval[0] = (256 - ph) * POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) * kingshieldbonus / 256;
        kingshieldeval[1] = -(256 - ph) * POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1]) * kingshieldbonus / 256;
    }

    for (int s = 0; s < 2; s++)
    {
        result += S2MSIGN(s) * KingSafetyFactor * KingSafetyTable[kingattackweightsum[s]] * (256 - ph) / 0x10000;
        if (Et == TRACE)
            kingdangereval[s] = S2MSIGN(s) * KingSafetyFactor * KingSafetyTable[kingattackweightsum[s]] * (256 - ph) / 0x10000;
    }

    if (Et == TRACE)
    {
        printEvalTrace(1, "PSQT", psqteval);
        printEvalTrace(1, "Mobility", mobilityeval);
        printEvalTrace(1, "Slider on free file", freeslidereval);
        printEvalTrace(1, "double bishop", doublebishopeval);
        printEvalTrace(1, "kingshield", kingshieldeval);
        printEvalTrace(1, "kingdanger", kingdangereval);

        printEvalTrace(0, "total value", result);
    }

    return result;
}


template <EvalTrace Et>
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
                {
                    if (Et == TRACE)
                        printEvalTrace(0, "missing material", SCOREDRAW);
                    return SCOREDRAW;
                }
            }
        }
    }
    ph = phase();
    return getPositionValue<Et>();
}


int getValueNoTrace(chessposition *p)
{
    return p->getValue<NOTRACE>();
}

int getValueTrace(chessposition *p)
{
    return p->getValue<TRACE>();
}



