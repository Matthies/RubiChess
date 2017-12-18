
#include "RubiChess.h"

// Evaluation stuff

#if 0

int materialvalue[] = { 0, 100, 320, 330, 500, 900, SCOREWHITEWINS };

int passedpawnbonus[8] = { 0, 20, 30, 50, 70, 100, 150, 0 };
int isolatedpawnpenalty = -20;
int doublepawnpenalty = -15;
int connectedbonus = 0;  // tried 10 but that seems not to work very well; best results without connected bonus due to a bug
int attackingpawnbonus[8] = { 0, 0, 0, 5, 10, 15, 0, 0 };
int kingshieldbonus = 15;
int backwardpawnpenalty = -20;
int doublebishopbonus = 20;   // not yet used
int slideronfreefilebonus = 15;
#else
// tuned values, first try
int materialvalue[] = { 0, 100, 315, 328, 517, 1033, SCOREWHITEWINS };
int passedpawnbonus[8] = { 0, 16, 5, 33, 72, 125, 212, 0 };
int isolatedpawnpenalty = -18;
int doublepawnpenalty = -23;
int connectedbonus = -4;  // tried 10 but that seems not to work very well; best results without connected bonus due to a bug
int attackingpawnbonus[8] = { 0, 0, 0, 5, 10, 15, 0, 0 };
int kingshieldbonus = 8;
int backwardpawnpenalty = -34;
int doublebishopbonus = 20;   // not yet used
int slideronfreefilebonus = 24;

#endif
double kingdangerfactor = 0.15;
double kingdangerexponent = 1.1;

int squaredistance[BOARDSIZE][BOARDSIZE];
int kingdanger[BOARDSIZE][BOARDSIZE][7];
int passedpawnbonusperside[2][8];
int attackingpawnbonusperside[2][8];


int PVBASE[][64] = {
#if 1
      { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
       89,   48,   53,   41,    3,   42,   42,   44,
       49,   35,   31,   13,   10,   18,   34,   29,
       13,    2,    4,   14,   13,   -4,    0,    4,
        4,   -7,    8,   25,   23,  -16,  -15,    2,
        0,   -1,    7,    6,   -2,   -9,   -2,   12,
       -1,    5,    0,  -27,  -18,    5,    9,   -1,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {  -118,  -25,  -20,    7,  -32,  -14,  -53,  -88,
      -20,  -25,   14,   16,   -8,    0,   -2,  -56,
      -18,   14,   40,   33,   29,   41,   34,   -7,
       -6,   12,   28,   37,   34,   29,    8,   29,
       -4,   12,   21,   26,   31,   24,    9,  -13,
      -18,   12,   16,   19,   19,   20,    6,   -9,
      -13,  -12,   -8,    5,    5,    6,  -24,  -13,
      -34,  -17,  -36,  -23,   -3,  -31,  -30,  -18  },
  {     9,  -17,   13,   10,   -7,  -11,   -5,  -23,
      -10,  -11,   -1,    5,   10,   13,   25,  -31,
       15,   14,    9,   21,   30,   39,   23,   22,
        7,   18,   21,   27,   32,   32,    8,    1,
       19,    7,   20,   29,   23,    7,    4,   -8,
       10,   16,   22,   14,   13,   22,    4,  -11,
      -16,   -1,    9,    7,   12,    6,    4,  -15,
      -55,  -27,  -11,   -9,  -19,   -9,  -16,   -6  },
  {    17,   14,   -4,   17,   11,   13,    4,   12,
       25,   21,   27,   29,   38,   20,   13,   10,
       12,   13,   13,   15,   14,   14,    9,    1,
        4,    7,   13,   15,   15,    6,    2,   -2,
       -8,    0,   -2,    1,   -3,    3,  -15,  -20,
      -17,  -12,   -3,   -8,   -1,  -12,  -12,   -7,
      -25,    0,    4,   -4,   -4,    6,   -7,  -23,
      -13,   -7,   -2,    4,    1,   14,  -13,  -26  },
  {   -21,    4,   -5,  -21,   12,   26,  -33,  -12,
      -14,  -30,    6,    6,    5,    7,  -14,   29,
      -16,  -12,   -4,   23,   36,   60,   34,   22,
      -24,  -12,   -8,   15,   27,   17,   -6,  -18,
      -16,   -5,  -17,   -9,   -2,   -9,  -18,  -32,
      -23,  -15,  -11,   -5,    0,  -15,  -14,  -14,
      -11,  -20,    2,   -6,    3,  -11,   -4,   -6,
      -26,  -21,   -4,    6,  -19,  -34,  -77,  -66  },
  {  -109,  -44,   13,  -40,   31,   13,   -6,  -99,
      -55,    8,  -14,   37,   10,   24,   17,  -32,
        0,   28,   11,    7,    4,   17,   30,    1,
        6,    3,   -7,  -29,  -23,  -15,   -9,   -6,
      -39,   -8,  -15,  -32,  -37,  -15,   -9,  -26,
      -44,  -17,  -34,  -21,  -25,  -28,  -12,  -23,
      -24,  -19,  -30,  -28,  -27,  -19,  -15,   -5,
        1,   18,   22,  -12,    7,  -13,   18,   10  }
#else
    // values before tuning up to version 0.6
    //PAWN
    {
        -9999, -9999, -9999, -9999, -9999, -9999, -9999, -9999,
        50,  50,  50,  50,  50,  50,  50,  50,
        20,  20,  20,  20,  20,  20,  20,  20,
        0,   0,   0,   10,  10,  0,   0,   0,
        0,   -20, 10,  20,  20,  -20, -20, 0,
        0,   -10, 10,  10,  10,  -10, -10, 0,
        0,  10,  0,  -20, -20,  0,   10,  10,
        -9999, -9999, -9999, -9999, -9999, -9999, -9999, -9999
    },
    //KNIGHT
    {
        -25, -15, -10,    0,  0, -10, -15,  -25,
        -15, -10,   0,  10,  10,   0, -10,  -15,
        -10,   0,  10,  15,  15,  10,  0,   -10,
        0,   5,  15,  25,  25,  15,  5,    0,
        0,   5,  15,  25,  25,  15,  5,    0,
        -10,   0,  10,  15,  15,  10,  0,   -10,
        -15, -10,   0,  10,  10,   0, -10,  -15,
        -25, -15, -10,    0,  0, -10, -15,  -25,
    },
    //BISHOP
    {
        -25, -15, -10,    0,  0, -10, -15,  -25,
        -15, -10,   0,  10,  10,   0, -10,  -15,
        -10,   0,  10,  15,  15,  10,  0,   -10,
        0,   5,  15,  25,  25,  15,  5,    0,
        0,   5,  15,  25,  25,  15,  5,    0,
        -10,   0,  10,  15,  15,  10,  0,   -10,
        -15, -10,   0,  10,  10,   0, -10,  -15,
        -25, -15, -10,    0,  0, -10, -15,  -25,
    },
    //ROOK
    {
        -10, -10, -10, -10, -10, -10, -10, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10, -10, -10, -10, -10, -10, -10, -10
    },
    //QUEEN
    {
        -10, -10, -10, -10, -10, -10, -10, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10,  0,   0,   0,   0,   0,   0, -10,
        -10, -10, -10, -10, -10, -10, -10, -10
    },
    //KING
    {
        -30, -30, -30, -30, -30, -30, -30, -30,
        -30, -30, -30, -30, -30, -30, -30, -30,
        -30, -30, -30, -30, -30, -30, -30, -30,
        -30, -30, -30, -30, -30, -30, -30, -30,
        -30, -30, -30, -30, -30, -30, -30, -30,
        -30, -30, -30, -30, -30, -30, -30, -30,
        -30, -30, -40, -40, -40, -30, -30, -30,
        10,  10,  20,   0,   0,   0,   20,  10
    }
#endif
};

int PVPHASEDIFF[][64] = {
#if 1
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
      -23,  -12,  -21,  -17,  -23,  -15,  -12,  -18,
        3,    1,    7,  -13,  -10,    0,    6,   -6,
       11,   22,    0,   -9,   -6,   13,   15,   19,
        7,   26,    1,  -31,  -27,   29,   30,    5,
        3,   12,  -12,   -3,    2,   18,   16,  -11,
        6,   -6,    3,   -9,  -10,   -3,  -11,   -9,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {     0,    7,    0,   -6,    1,    1,    5,    0,
        0,   14,    0,    0,    1,    0,    0,    1,
        0,    0,    0,  -10,    0,    0,   -3,    0,
        3,    0,    0,    0,    0,    0,    0,  -15,
        0,    0,    0,   -1,    0,    0,    0,    0,
        0,   -1,    0,    0,    0,  -15,   -1,    0,
        1,    1,    8,    0,   -7,    0,    0,    0,
        0,    0,    0,    4,    0,    2,   16,    0  },
  {     0,    1,    0,   -1,    4,    3,    9,   31,
        6,    0,    7,    0,    0,    0,    0,   34,
        0,   -1,   -1,    0,    0,    0,  -14,    0,
       -1,    0,    0,    0,    0,   -7,    0,  -15,
      -15,   -4,    0,   -3,    0,    0,    0,    0,
        0,    0,   -7,    0,    0,  -15,   -3,    0,
        1,    4,   -7,   -7,   -7,   -6,  -15,    1,
        7,    0,    0,    0,    2,   15,    0,    2  },
  {     0,    0,   15,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
       -1,   -1,    0,    0,   -2,   -1,   -1,    0,
        0,    0,    9,   -1,    7,   -1,    0,    0,
        0,    0,    0,    1,    1,    0,    0,    0,
        0,    0,    0,    1,    0,    0,    0,    4,
        3,    1,   10,    0,    0,  -20,    0,    0  },
  {     0,  -17,    0,   31,    0,    0,    2,   11,
        0,   31,   -2,   -1,    0,    0,   48,    0,
       30,    0,    5,    0,  -15,   -2,   -9,    0,
        0,    0,    0,    0,   -2,   -1,   62,   30,
        0,    0,    0,   63,   32,   31,   17,    3,
        0,    2,    2,    0,    0,   61,    2,    7,
       31,    0,    0,    1,    0,    3,    0,    0,
        1,   31,    1,  -32,    0,    2,    2,    2  },
  {     2,    8,   38,   27,   24,   10,    2,    7,
        3,   14,   24,   34,   13,   23,   25,    2,
       11,   24,   27,   32,   27,   26,   16,   17,
       17,   18,   34,   44,   40,   26,   10,    9,
       18,   17,   27,   41,   36,   27,   18,   12,
       14,   14,   26,   27,   31,   29,   12,   13,
        1,   14,   23,   24,   23,   11,   10,    1,
      -39,  -39,  -43,  -10,  -36,  -13,  -51,  -70  }
 };
#else
    // values before tuning up to version 0.6
    //PAWN
    {
        -9999, -9999, -9999, -9999, -9999, -9999, -9999, -9999,
        -15, -15, -15 , -15, -15, -15, -15, -15,
        -5,   5,   5,   5,   5,   5,   5,  -5,
        10,  15,  15,   5,   5,  15,  15,  10,
        5,   30,   0, -10, -10,  30,  30,  5,
        2,  15,  -5,  -5,  -5,  15,  15,   2,
        0,  -10,  0,  20,  20,   0, -10, -10,
        -9999, -9999, -9999, -9999, -9999, -9999, -9999, -9999
    },
    //KNIGHT
    {
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0
    },
    //BISHOP
    {
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0
    },
    //ROOK
    {
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0
    },
    //QUEEN
    {
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0
    },
    //KING
    {
          0,   0,  10,  10,  10,  10,   0,   0,
          0,  10,  10,  10,  10,  10,  10,   0,
         10,  10,  20,  20,  20,  20,  10,  10,
         10,  10,  20,  30,  30,  20,  10,  10,
         10,  10,  20,  30,  30,  20,  10,  10,
         10,  10,  20,  20,  20,  20,  10,  10,
          0,  10,  20,  20,  20,  10,  10,   0,
        -40, -40, -40, -20, -20, -20, -50, -40
    }
};
#endif

#ifdef EVALTUNE
void registeralltuners()
{
    int i, j;
#if 0
    // tuning material value
    for (i = KNIGHT; i < KING; i++)
        registerTuner(&materialvalue[i], "materialvalue[" + to_string(i) + "]", materialvalue[i], -100, 100, &CreatePositionvalueTable);
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
#if 0
    //tuning the psqt development
    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 64; j++)
        {
            registerTuner(&PVPHASEDIFF[i][j], "PVPHASEDIFF", PVPHASEDIFF[i][j], j, 64, i, 6, &CreatePositionvalueTable, PVPHASEDIFF[i][j] <= -9999);
        }
    }
#endif
#if 0
    // tuning other values
    for (i = 0; i < 8; i++)
    {
        registerTuner(&passedpawnbonus[i], "passedpawnbonus", passedpawnbonus[i], i, 8, 0, 0, &CreatePositionvalueTable, i == 0 || i == 7);
        //registerTuner(&attackingpawnbonus[i], "attackingpawnbonus[" + to_string(i) + "]", attackingpawnbonus[i], -100, 100, &CreatePositionvalueTable);

    }
    registerTuner(&isolatedpawnpenalty, "isolatedpawnpenalty", isolatedpawnpenalty, 0, 0, 0, 0, NULL, false);
    registerTuner(&doublepawnpenalty, "doublepawnpenalty", doublepawnpenalty, 0, 0, 0, 0, NULL, false);
    registerTuner(&connectedbonus, "connectedbonus", connectedbonus, 0, 0, 0, 0, NULL, false);
    registerTuner(&kingshieldbonus, "kingshieldbonus", kingshieldbonus, 0, 0, 0, 0, NULL, false);
    registerTuner(&backwardpawnpenalty, "backwardpawnpenalty", backwardpawnpenalty, 0, 0, 0, 0, NULL, false);
    registerTuner(&slideronfreefilebonus, "slideronfreefilebonus", slideronfreefilebonus, 0, 0, 0, 0, NULL, false);
    //registerTuner(&doublepawnpenalty, "doublepawnpenalty", doublepawnpenalty, -100, 100, NULL);
#endif

//    const double kingdangerfactor = 0.15;
//    const double kingdangerexponent = 1.1;

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
            for (int j = 0; j < BOARDSIZE; j++)
            {
                kingdanger[i][j][p] = (int)(pow((7 - squaredistance[i][j]), kingdangerexponent) * sqrt(materialvalue[p]) * kingdangerfactor);
            }
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
                        debugeval("Attacking Pawn Bonus(%d): %d\n", index, attackingpawnbonus[s][RANK(index)]);
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
            debugeval("Passed Pawn Bonus(%d): %d\n", index, passedpawnbonus[s][RANK(index)]);
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
    int scalephaseto4 = (~ph & 0xff) >> 6;
    int result = 0;
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
                U64 diagmobility = ~occupied00[s]
                    & ((diaga1h8_attacks[index][((occupieda1h8[0] | occupieda1h8[1]) >> rota1h8shift[index]) & 0x3f])
                        | (diagh1a8_attacks[index][((occupiedh1a8[0] | occupiedh1a8[1]) >> roth1a8shift[index]) & 0x3f]));
#else
                U64 diagmobility = ~occupied00[s]
                    & (mBishopAttacks[index][MAGICBISHOPINDEX((occupied00[0] | occupied00[1]), index)]);
#endif
                result += (S2MSIGN(s) * POPCOUNT(diagmobility) * scalephaseto4);
            }
        }
    }

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
    int result = 0;
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

