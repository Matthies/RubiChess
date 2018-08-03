
#include "RubiChess.h"

// Evaluation stuff
CONSTEVAL int kingattackweight[7] = { 0,    0,    3,    4,    3,    4,    0 };
CONSTEVAL int KingSafetyFactor = 278;
CONSTEVAL int tempo = 5;
CONSTEVAL int passedpawnbonus[8] = { 0,   16,   21,   41,   74,  121,  140,    0 };
CONSTEVAL int attackingpawnbonus[8] = { 0,  -23,  -12,   -3,    3,   37,    0,    0 };
CONSTEVAL int isolatedpawnpenalty = -14;
CONSTEVAL int doublepawnpenalty = -20;
CONSTEVAL int connectedbonus = 3;
CONSTEVAL int kingshieldbonus = 14;
CONSTEVAL int backwardpawnpenalty = -21;
CONSTEVAL int doublebishopbonus = 39;
CONSTEVAL int shiftmobilitybonus = 2;
CONSTEVAL int slideronfreefilebonus[2] = { 15,   17 };
CONSTEVAL int materialvalue[7] = { 0,  100,  314,  314,  496,  938,32509 };

CONSTEVAL int PVBASE[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
       65,   75,  102,   48,   48,   74,   51,   72,
       29,   30,   21,   31,   25,   35,   28,   28,
        7,    8,   -6,   19,   17,   -7,   -7,  -15,
        0,  -13,    5,   23,   19,   -2,  -20,  -14,
        2,   -6,    4,  -11,   -4,  -14,    6,   -3,
       -9,  -12,  -16,  -24,  -19,    4,    9,  -18,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {  -143,  -24,  -15,   21,    1,  -29,   10,  -63,
      -28,  -11,   -1,   24,    6,    7,  -11,  -25,
      -16,   19,   22,   49,   34,   60,   44,  -12,
      -16,   11,   31,   34,   22,   19,   15,   36,
       -7,    7,   16,   14,   21,   14,   14,   -5,
      -22,    1,    5,   13,   18,   18,   26,  -18,
      -57,  -20,  -22,    8,   16,   10,   -2,  -12,
      -58,  -21,  -33,  -22,  -27,  -19,   -9,  -75  },
  {     5,    3,    7,    8,   14,   13,    8,  -31,
      -23,   12,    1,   19,   22,   28,    8,    0,
        9,   22,   24,   30,   33,   60,   34,   25,
        4,   15,   15,   39,   33,   28,    7,    8,
      -15,   14,   22,   31,   28,    3,   13,   -2,
        1,   30,   10,   17,    9,   21,   17,    7,
       23,   11,   16,    0,   16,    1,   33,    2,
      -20,  -10,  -11,  -15,   -1,  -10,  -22,  -26  },
  {     2,   17,    5,    6,   21,   37,   34,   19,
       25,   23,   32,   36,   37,   30,   32,   39,
       15,   10,   20,   24,   19,   25,   25,   16,
        5,    0,   13,    6,    3,    5,    4,   -1,
      -26,  -15,   -6,   -5,  -10,  -11,   -3,  -14,
      -37,  -22,  -14,  -14,  -14,  -19,  -12,  -26,
      -32,  -22,  -12,  -13,  -19,   -4,  -21,  -70,
      -11,   -5,    6,    0,    2,    6,  -46,  -20  },
  {   -14,   -6,    0,    1,   13,   18,   25,    0,
      -14,  -37,   13,   20,   19,   25,  -15,   34,
      -21,  -15,  -18,   13,   13,   28,   20,   14,
      -32,  -30,   -2,  -15,   -6,  -24,  -24,  -16,
      -15,  -19,  -15,  -28,   -9,  -13,   -3,  -34,
      -11,    1,   -4,  -11,   -4,  -12,    4,  -12,
      -12,  -11,    0,   -3,    5,    4,  -25,  -49,
      -29,  -25,    1,   13,   -9,  -48, -101,  -33  },
  {   -91,  -46,  -99,  -50,  -94,   13,   43, -113,
      -18,    1,   -5,  -23,   -9,  -59,  -31,  -90,
      -14,    9,   -5,   -8,  -14,   -2,   -3,  -19,
       -8,    0,  -11,  -28,  -20,   -9,   -6,   -6,
      -21,  -15,   -4,  -14,  -19,   -8,  -10,  -18,
      -26,   -2,   -7,   -9,   -9,  -17,   -7,  -23,
      -19,   -2,  -18,  -26,  -25,   -9,   -5,   -6,
      -15,    9,   19,  -42,   -7,  -37,   19,    8  }
 };
CONSTEVAL int PVPHASEDIFF[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
      -24,  -12,  -36,  -15,  -24,  -20,   -5,  -36,
        7,    0,   -9,  -30,  -36,  -48,    0,  -24,
       15,   10,    5,  -31,  -22,    7,   22,   19,
       11,   22,    1,  -43,  -24,    0,   31,   11,
        4,    6,  -13,   16,    1,   18,  -12,   -7,
       12,   11,   18,    0,   12,    1,  -11,    7,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {   110,    3,   -1,  -34,  -14,    3,   -2,   -5,
        9,    4,    5,  -24,  -13,   -7,    1,   -4,
        2,  -12,   -7,  -23,    0,  -11,  -47,    0,
       -4,   -6,   -9,  -11,    0,  -33,  -15,  -68,
        9,   -1,   -4,    0,   -9,    0,   -2,    0,
        0,  -18,  -12,   -7,   -9,  -30,  -58,    0,
       26,   18,  -14,  -24,  -36,  -32,    4,  -28,
      -47,    0,    3,    6,  -16,  -21,  -48,    1  },
  {    14,    8,    5,   -1,   -2,   11,   10,   39,
       25,    2,    7,    2,   -4,  -24,    7,    9,
        9,   -4,   -9,  -11,   -6,  -36,  -27,   -3,
        2,    0,   -3,   -9,   -9,   -9,    0,  -15,
       19,   -1,    0,   -6,   -9,    0,    4,   17,
        3,  -15,   10,    0,    0,    1,  -12,   14,
      -63,  -12,   -6,   13,   -8,   15,  -28,  -53,
        9,   -7,    0,   11,    3,   10,    0,    1  },
  {    -1,   -4,   10,   -2,    3,   -7,   -7,   -7,
       -4,   -4,  -12,  -18,  -15,  -12,  -12,  -26,
       -1,    6,   -6,  -22,  -19,  -12,   -6,  -12,
       -1,   -1,   -8,   -5,   -9,   -2,   -9,   -1,
       30,   11,   15,    3,    8,    5,    0,   16,
       26,   24,    7,    3,    5,    3,    1,   12,
       13,    2,    2,   -5,    0,  -27,    2,   48,
        3,  -11,   -8,   -2,  -15,  -14,   48,   10  },
  {     0,  -13,   -1,   31,  -15,   -6,  -29,   20,
        7,   47,   -1,   -1,   -1,    0,   61,  -10,
       31,    5,    6,    6,   -8,    0,   -2,    4,
       39,   58,   17,   44,   40,   31,   94,   32,
        1,   20,   34,   74,   34,   32,    8,   56,
        0,  -12,   17,    5,    4,   41,    1,   -7,
       -7,    1,   -8,    0,  -24,  -29,  -35,    5,
        1,   31,    1,  -63,    0,    4,  -15,  -19  },
  {    13,    2,   40,   23,   85,   14,  -75,   13,
      -20,    4,   23,   31,    6,   16,   24,   13,
       17,   19,   31,   31,   25,   20,   20,   19,
       25,   19,   43,   55,   43,   27,   19,    7,
       15,   19,   26,   43,   50,   26,   21,   12,
       13,   14,   27,   29,   33,   38,   13,   18,
        1,   12,   24,   35,   31,   18,   10,   -3,
      -36,  -38,  -39,    0,  -36,   12,  -51,  -63  }
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

#if 1
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
        int j1, j2;
#ifdef SYMPSQT
        if (i & 4)
            j2 = (~i & 3) | ((i >> 1) & 0x1c);
        else
            j2 = (i & 3) | ((i >> 1) & 0x1c);

        j1 = (j2 ^ 0x1c);
#else
        j2 = i;
        j1 = (j2 ^ 0x38);
#endif
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



