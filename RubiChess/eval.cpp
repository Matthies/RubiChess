
#include "RubiChess.h"

// Evaluation stuff
CONSTEVAL int kingattackweight[7] = { 0,    0,    3,    4,    3,    4,    0 };
CONSTEVAL int KingSafetyFactor = 278;
CONSTEVAL int tempo = 5;
CONSTEVAL int passedpawnbonus[8] = { 0,   15,   21,   41,   74,  121,  140,    0 };
CONSTEVAL int attackingpawnbonus[8] = { 0,  -22,  -12,   -9,    1,   37,    0,    0 };
CONSTEVAL int isolatedpawnpenalty = -14;
CONSTEVAL int doublepawnpenalty = -20;
CONSTEVAL int connectedbonus = 3;
CONSTEVAL int kingshieldbonus = 14;
CONSTEVAL int backwardpawnpenalty = -21;
CONSTEVAL int doublebishopbonus = 39;
CONSTEVAL int shiftmobilitybonus = 2;
CONSTEVAL int safepawnattackbonus = 37;
CONSTEVAL int slideronfreefilebonus[2] = { 15,   17 };
CONSTEVAL int materialvalue[7] = { 0,  100,  314,  314,  492,  930,32509 };

CONSTEVAL int PVBASE[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
       65,   75,  102,   48,   48,   74,   51,   72,
       29,   30,   21,   31,   25,   35,   28,   28,
        7,    8,   -6,   19,   17,   -7,   -7,  -15,
        0,  -13,    5,   23,   19,   -2,  -20,  -14,
        3,   -8,    4,  -11,   -3,  -16,    6,   -4,
       -9,  -13,  -18,  -23,  -21,    4,    9,  -19,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {  -149,  -27,  -20,   15,   -2,  -36,    4,  -59,
      -32,   -2,   -2,   23,    7,    7,   -5,  -21,
      -20,   16,   22,   49,   32,   50,   39,  -17,
      -16,    9,   30,   35,   20,   33,   19,   31,
      -11,    0,   15,   12,   20,   12,   10,   -7,
      -25,   -1,    3,   11,   19,   17,   24,  -17,
      -56,  -21,  -24,    5,   14,   11,   -2,  -12,
      -60,  -24,  -36,  -22,  -24,  -16,  -10,  -77  },
  {    -1,    2,    1,    9,   14,    9,    1,  -30,
      -23,   10,    1,   16,   16,   25,    1,   -6,
        5,   21,   18,   28,   26,   62,   34,   23,
        2,   15,   15,   37,   27,   27,    5,    5,
      -14,   10,   21,   29,   26,    2,   11,  -10,
       -1,   28,    8,   17,    8,   21,   18,    4,
       19,   11,   16,   -2,   16,    1,   33,    2,
      -17,   -9,  -12,  -18,   -1,  -11,  -23,  -26  },
  {     2,   17,    5,    6,   18,   36,   34,   20,
       25,   22,   33,   37,   37,   32,   29,   40,
       13,    8,   19,   26,   26,   21,   21,   17,
        4,   13,   13,    7,    3,    3,    2,   -3,
      -26,  -17,   -8,   -7,  -10,  -14,   -6,  -14,
      -39,  -26,  -14,  -14,  -14,  -21,  -13,  -29,
      -29,  -21,  -12,  -11,  -20,   -5,  -21,  -70,
      -11,   -5,    5,    0,    4,    5,  -46,  -20  },
  {   -13,   -7,    0,    2,   13,   17,   27,    1,
      -17,  -37,   12,   18,   19,   25,  -15,   34,
      -21,  -14,  -18,   13,   12,   27,   21,   13,
      -31,  -30,   -2,  -15,   -6,  -27,  -25,  -16,
      -14,  -21,  -15,  -31,   -9,  -13,   -3,  -26,
      -12,    1,   -8,  -11,   -5,  -10,    5,   -9,
      -11,  -11,    0,   -3,    6,    4,  -20,  -48,
      -29,  -21,    0,   13,   -8,  -48,  -99,  -37  },
  {   -91,  -46, -103,  -48, -109,   11,   43, -118,
      -18,    1,   -2,  -32,  -10,  -60,  -44,  -95,
      -14,    9,  -11,  -10,  -17,   -2,   -8,  -20,
      -13,   -4,  -15,  -28,  -21,   -9,   -8,   -6,
      -22,  -15,   -4,  -17,  -21,   -8,  -13,  -19,
      -25,   -2,   -8,  -11,   -9,  -17,   -7,  -23,
      -17,   -2,  -18,  -31,  -24,   -9,   -2,   -6,
      -15,    8,   14,  -43,   -7,  -37,   19,    8  }
 };
CONSTEVAL int PVPHASEDIFF[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
      -24,  -12,  -36,  -15,  -24,  -20,   -5,  -36,
        7,    0,   -9,  -33,  -36,  -48,   -5,  -24,
       15,   11,    5,  -31,  -22,    7,   19,   13,
       11,   23,    1,  -43,  -24,    0,   26,   11,
        4,    6,  -13,   16,    1,   19,  -12,   -7,
       12,   11,   20,    0,   12,    1,  -11,    6,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {   110,    3,   -1,  -34,  -14,    3,   -1,   -5,
        9,  -11,   -2,  -24,  -19,  -23,  -14,   -3,
        2,  -12,   -7,  -23,  -23,  -11,  -47,    1,
       -4,   -9,  -17,  -17,    0,  -33,  -15,  -68,
        9,   -1,   -4,    0,  -10,    0,   -2,    0,
        0,  -18,  -12,   -9,  -18,  -33,  -58,    0,
       18,   18,  -14,  -24,  -38,  -33,    4,  -41,
      -47,    0,    3,    6,  -20,  -21,  -48,   -3  },
  {    14,    8,    8,   -1,   -2,   11,   10,   39,
       24,    2,    7,    2,   -4,  -24,    7,    9,
        9,   -4,   -9,  -11,   -4,  -36,  -27,   -3,
        1,    0,   -5,  -17,   -6,   -9,    0,  -15,
       18,   -2,    0,   -6,   -9,    0,    4,   22,
        3,  -15,    4,    0,    0,    1,  -12,   14,
      -63,  -12,   -6,   13,  -10,    8,  -28,  -53,
        8,   -7,    0,   11,   -4,   10,    0,    1  },
  {    -5,   -4,   10,   -4,    3,   -7,   -7,   -9,
       -5,   -4,  -12,  -20,  -15,  -12,  -12,  -26,
       -1,    6,   -6,  -22,  -29,  -12,   -6,  -12,
       -1,   -1,  -10,   -6,   -8,   -1,   -9,   -1,
       30,   11,   15,    6,    8,   11,    0,   16,
       33,   24,    7,    4,    5,    3,    1,   27,
        6,    3,    2,   -5,    0,  -27,    2,   48,
        3,  -11,   -7,   -2,  -15,  -14,   48,   10  },
  {     0,  -13,   -1,   31,  -15,   -6,  -29,   20,
        7,   62,   15,    5,   -1,   -3,   61,   -8,
       31,    5,    5,    6,   -8,   -2,   -2,    7,
       40,   58,   18,   44,   40,   31,   94,   32,
        1,   20,   34,   74,   34,   31,    8,   25,
        0,  -11,   19,    5,    4,   41,    1,   -7,
       -7,    0,   -8,    0,  -24,  -53,  -35,    5,
        1,   31,    1,  -63,    0,    4,  -15,  -19  },
  {    13,    1,   40,   17,   85,   13,  -75,   13,
      -20,    1,   23,   32,    7,   16,   24,   13,
       14,   19,   34,   31,   25,   20,   20,   19,
       25,   19,   43,   55,   43,   27,   19,    7,
       15,   19,   26,   44,   51,   27,   24,   12,
       13,   11,   27,   30,   33,   38,   13,   18,
       -2,   12,   24,   35,   31,   18,   10,   -3,
      -36,  -38,  -39,   15,  -37,   12,  -51,  -63  }
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

    registerTuner(&safepawnattackbonus, "safepawnattackbonus", safepawnattackbonus, 0, 0, 0, 0, NULL, false);
#if 0
    // tuning other values
    for (i = 0; i < 7; i++)
        registerTuner(&kingattackweight[i], "kingattackweight", kingattackweight[i], i, 7, 0, 0, NULL, i < 2 || i > 5);
    registerTuner(&KingSafetyFactor, "KingSafetyFactor", KingSafetyFactor, 0, 0, 0, 0, NULL, false);
    registerTuner(&safepawnattackbonus, "safepawnattackbonus", safepawnattackbonus, 0, 0, 0, 0, NULL, false);
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
#if 0
    // tuning the psqt base at game start
    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 64; j++)
        {
            registerTuner(&PVBASE[i][j], "PVBASE", PVBASE[i][j], j, 64, i, 6, &CreatePositionvalueTable, PVBASE[i][j] <= -9999);
        }
    }
#endif
#if 0
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
        for (int pc = WPAWN; pc <= BPAWN; pc++)
        {
            int me = pc & S2MMASK;
            int you = me ^ S2MMASK;
            entryptr->semiopen[me] = 0xff; 
            entryptr->passedpawnbb[me] = 0ULL;
            entryptr->isolatedpawnbb[me] = 0ULL;
            entryptr->backwardpawnbb[me] = 0ULL;
            U64 pb = piece00[pc];
            while (LSB(index, pb))
            {
                entryptr->attackedBy2[you] |= (entryptr->attacked[you] & pawn_attacks_occupied[index][me]);
                entryptr->attacked[you] |= pawn_attacks_occupied[index][me];
                entryptr->semiopen[me] &= ~BITSET(FILE(index)); 
                if (!(passedPawnMask[index][me] & piece00[pc ^ S2MMASK]))
                {
                    // passed pawn
                    entryptr->passedpawnbb[me] |= BITSET(index);
                }

                if (!(piece00[pc] & neighbourfilesMask[index]))
                {
                    // isolated pawn
                    entryptr->isolatedpawnbb[me] |= BITSET(index);
                }
                else
                {
                    if (pawn_attacks_occupied[index][me] & piece00[pc ^ S2MMASK])
                    {
                        // pawn attacks opponent pawn
                        entryptr->value += attackingpawnbonusperside[me][RANK(index)];
                        if (Et == TRACE)
                            attackingpawnval[me] += attackingpawnbonusperside[me][RANK(index)];
                    }
                    if ((pawn_attacks_occupied[index][you] & piece00[pc]) || (phalanxMask[index] & piece00[pc]))
                    {
                        // pawn is protected by other pawn
                        entryptr->value += S2MSIGN(me) * connectedbonus;
                        if (Et == TRACE)
                            connectedpawnval[me] += S2MSIGN(me) * connectedbonus;
                    }
                    if (!((passedPawnMask[index][you] | phalanxMask[index]) & piece00[pc]))
                    {
                        // test for backward pawn
                        U64 opponentpawns = piece00[pc ^ S2MMASK] & passedPawnMask[index][me];
                        U64 mypawns = piece00[pc] & neighbourfilesMask[index];
                        U64 pawnstoreach = opponentpawns | mypawns;
                        int nextpawn;
                        if (me ? MSB(nextpawn, pawnstoreach) : LSB(nextpawn, pawnstoreach))
                        {
                            U64 nextpawnrank = rankMask[nextpawn];
                            U64 shiftneigbours = (me ? nextpawnrank >> 8 : nextpawnrank << 8);
                            if ((nextpawnrank | (shiftneigbours & neighbourfilesMask[index])) & opponentpawns)
                            {
                                // backward pawn detected
                                entryptr->backwardpawnbb[me] |= BITSET(index);
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

    memset(attackedBy, 0, sizeof(attackedBy));

    if (Et == TRACE)
        printEvalTrace(1, "tempo", result);

    result += getPawnValue<Et>(&phentry);

    attackedBy[0][KING] = king_attacks[kingpos[1]];
    attackedBy2[0] = phentry->attackedBy2[0] | (attackedBy[0][KING] & phentry->attacked[0]);
    attackedBy[0][0] = attackedBy[0][KING] | phentry->attacked[0];
    attackedBy[1][KING] = king_attacks[kingpos[0]];
    attackedBy2[1] = phentry->attackedBy2[1] | (attackedBy[1][KING] & phentry->attacked[1]);
    attackedBy[1][0] = attackedBy[1][KING] | phentry->attacked[1];
 
    // king specials
    int psqking0 = *(positionvaluetable + (kingpos[0] | (ph << 6) | (KING << 14) | (0 << 17)));
    int psqking1 = *(positionvaluetable + (kingpos[1] | (ph << 6) | (KING << 14) | (1 << 17)));
    result += psqking0 + psqking1;
    if (Et == TRACE)
    {
        psqteval[0] += psqking0;
        psqteval[0] += psqking1;
    }

    for (int pc = WPAWN; pc <= BQUEEN; pc++)
    {
        int p = pc >> 1;
        int me = pc & S2MMASK;
        int you = me ^ S2MMASK;
        U64 pb = piece00[pc];
        while (LSB(index, pb))
        {
            int pvtindex = index | (ph << 6) | (p << 14) | (me << 17);
            result += *(positionvaluetable + pvtindex);
            if (Et == TRACE)
                psqteval[me] += *(positionvaluetable + pvtindex);

            pb ^= BITSET(index);

            U64 attack = 0ULL;;
            U64 mobility = 0ULL;
            if (shifting[p] & 0x2) // rook and queen
            {
                attack = mRookAttacks[index][MAGICROOKINDEX((occupied00[0] | occupied00[1]), index)];
                mobility = attack & ~occupied00[me];

                // extrabonus for rook on (semi-)open file  
                if (p == ROOK && (phentry->semiopen[me] & BITSET(FILE(index))))
                {
                    result += S2MSIGN(me) * slideronfreefilebonus[bool(phentry->semiopen[you] & BITSET(FILE(index)))];
                    if (Et == TRACE)
                        freeslidereval[me] += S2MSIGN(me) * slideronfreefilebonus[bool(phentry->semiopen[you] & BITSET(FILE(index)))];
                }
            }

            if (shifting[p] & 0x1) // bishop and queen)
            {
                attack |= mBishopAttacks[index][MAGICBISHOPINDEX((occupied00[0] | occupied00[1]), index)];
                mobility |= ~occupied00[me]
                    & (mBishopAttacks[index][MAGICBISHOPINDEX((occupied00[0] | occupied00[1]), index)]);
            }

            if (p == KNIGHT)
            {
                attack = knight_attacks[index];
                mobility = attack & ~occupied00[me];
            }

            if (p != PAWN)
            {
                // update attack bitboard
                attackedBy[you][p] |= attack;
                attackedBy2[you] |= (attackedBy[you][0] & attack);
                attackedBy[you][0] |= attack;

                // mobility bonus
                result += S2MSIGN(me) * POPCOUNT(mobility) * shiftmobilitybonus;
                if (Et == TRACE)
                    mobilityeval[me] += S2MSIGN(me) * POPCOUNT(mobility) * shiftmobilitybonus;

                // king danger
                U64 kingdangerarea = kingdangerMask[kingpos[you]][you];
                if (mobility & kingdangerarea)
                {
                    kingattackweightsum[me] += POPCOUNT(mobility & kingdangerarea) * kingattackweight[p];
                }
            }
        }
    }
    attackedBy[0][PAWN] = phentry->attacked[0];
    attackedBy[1][PAWN] = phentry->attacked[1];

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
        // King safety
        result += S2MSIGN(s) * KingSafetyFactor * KingSafetyTable[kingattackweightsum[s]] * (256 - ph) / 0x10000;
        if (Et == TRACE)
            kingdangereval[s] = S2MSIGN(s) * KingSafetyFactor * KingSafetyTable[kingattackweightsum[s]] * (256 - ph) / 0x10000;

        // Threats
        int t = s ^ S2MMASK;
        U64 nonPawns = occupied00[t] ^ piece00[WPAWN | t];
        U64 attackedNonPawns = (nonPawns & attackedBy[s][PAWN]);
        if (attackedNonPawns)
        {
            // Our safe or protected pawns
            U64 b = piece00[WPAWN | s] & (~attackedBy[t][0] | attackedBy[s][0]);

            U64 safeThreats = pawnAttack(s, b) & attackedNonPawns;
            result += S2MSIGN(s) * safepawnattackbonus * POPCOUNT(safeThreats);
        }

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



