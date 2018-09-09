
#include "RubiChess.h"

// Evaluation stuff

// static values for the search/pruning stuff
const int prunematerialvalue[7] = { 0,  100,  314,  314,  483,  913, 32509 };

// bench von master: 11712498
eval ePawnpushthreatbonus = VALUE(14, 14);
eval eSafepawnattackbonus = VALUE(36, 36);
eval eKingshieldbonus = VALUE(14, 0);
eval eTempo = VALUE(5, 5);
eval ePassedpawnbonus[8] = { VALUE(0, 0), VALUE(0, 11), VALUE(0, 19), VALUE(0, 39), VALUE(0, 72), VALUE(0, 117), VALUE(0, 140), VALUE(0, 0) };
// neuer Bench wegen summieren und rechnen statt rechnen/runden - summieren: 12127303
eval eAttackingpawnbonus[8] = { VALUE(0, 0), VALUE(-17, -17), VALUE(-15, -15), VALUE(-5, -5), VALUE(-4, -4), VALUE(34, 34), VALUE(0, 0), VALUE(0, 0) };
eval eIsolatedpawnpenalty = VALUE(-14, -14);
eval eDoublepawnpenalty = VALUE(-19, -19);
eval eConnectedbonus = VALUE(3, 3);
eval eBackwardpawnpenalty = VALUE(0, -21);  // FIXME: beide Seiten separat gezählt, um identisch zum master zu bleiben
eval eDoublebishopbonus = VALUE(36, 36);
eval eShiftmobilitybonus = VALUE(2, 2);
eval eSlideronfreefilebonus[2] = { VALUE(15, 15),   VALUE(17, 17) };
eval eMaterialvalue[7] = { VALUE(0, 0),  VALUE(100, 100),  VALUE(314, 314),  VALUE(314, 314),  VALUE(483, 483),  VALUE(913, 913), VALUE(32509, 32509) };

eval ePsqt[7][64] = {
    {0},
    {
        VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), 
        VALUE(  60,  36), VALUE(  71,  59), VALUE(  97,  61), VALUE(  70,  22), VALUE(  46,  22), VALUE(  73,  49), VALUE(  41,  37), VALUE(  67,  31), 
        VALUE(  27,  38), VALUE(  27,  27), VALUE(  20,  11), VALUE(  34,  -2), VALUE(  24, -19), VALUE(  35, -13), VALUE(  30,  25), VALUE(  29,   5), 
        VALUE(   7,  22), VALUE(   6,  17), VALUE(  -7,  -2), VALUE(  16, -15), VALUE(  17, -11), VALUE(  -8,  -1), VALUE(  -5,  10), VALUE( -16,  -3), 
        VALUE(   0,  11), VALUE( -16,   8), VALUE(   2,   3), VALUE(  21, -22), VALUE(  16,  -8), VALUE(  -4,  -4), VALUE( -19,   6), VALUE( -19,  -2), 
        VALUE(   2,   6), VALUE( -10,  -4), VALUE(   2, -11), VALUE( -13,   3), VALUE(  -5,  -4), VALUE( -16,   2), VALUE(   4,  -9), VALUE(  -8, -11), 
        VALUE( -11,  11), VALUE( -14,  -3), VALUE( -19,   0), VALUE( -24, -24), VALUE( -22, -10), VALUE(   2,   3), VALUE(   7,  -4), VALUE( -22, -11), 
        VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), 
    },
    {
        VALUE(-164, -44), VALUE( -38, -35), VALUE( -23, -31), VALUE(  -8, -33), VALUE(  -9, -23), VALUE( -44, -40), VALUE(   1,   0), VALUE( -70, -75), 
        VALUE( -43, -34), VALUE(  -2, -23), VALUE(  -7, -17), VALUE(   9, -15), VALUE(   0, -19), VALUE(   5, -31), VALUE(   3, -30), VALUE( -12, -54), 
        VALUE( -32, -30), VALUE(   3,  -9), VALUE(  11,   4), VALUE(  35,  12), VALUE(  58,  -5), VALUE(  37,  28), VALUE(  31, -23), VALUE( -28, -30), 
        VALUE( -24, -31), VALUE(   4,  -8), VALUE(  34,   1), VALUE(  28,   4), VALUE(  10,   7), VALUE(  27,  -6), VALUE(  13, -10), VALUE(  21, -45), 
        VALUE( -19, -10), VALUE(  -9, -10), VALUE(  12,   3), VALUE(   5,   5), VALUE(  12,   3), VALUE(   9,   9), VALUE(  12,   4), VALUE( -10, -10), 
        VALUE( -36, -36), VALUE( -11, -29), VALUE(  -7, -19), VALUE(   6, -11), VALUE(  12,  -6), VALUE(  10, -26), VALUE(  16, -44), VALUE( -27, -27), 
        VALUE( -64, -46), VALUE( -32, -14), VALUE( -35, -49), VALUE(  -5, -29), VALUE(   2, -36), VALUE(   1, -32), VALUE(  -8, -11), VALUE( -17, -65), 
        VALUE( -68,-115), VALUE( -35, -35), VALUE( -50, -45), VALUE( -34, -28), VALUE( -29, -56), VALUE( -27, -48), VALUE( -20, -70), VALUE( -85, -88), 
    },
    {
        VALUE(  -6,   8), VALUE(  -5,   3), VALUE( -13,  -1), VALUE(   7,   6), VALUE(   9,   7), VALUE(   6,   9), VALUE(  -6,   4), VALUE( -36,   3), 
        VALUE( -31,  -7), VALUE(   0,   2), VALUE( -11,  -5), VALUE(   8,   7), VALUE(   5,   2), VALUE(  14, -10), VALUE(  -6,   1), VALUE( -19,  -8), 
        VALUE(  -7,   5), VALUE(  10,   6), VALUE(  13,   3), VALUE(  23,   6), VALUE(  16,  12), VALUE(  56,  20), VALUE(  22,  -5), VALUE(  18,  11), 
        VALUE(  -8,  -4), VALUE(  13,  13), VALUE(  10,   4), VALUE(  40,  12), VALUE(  17,  14), VALUE(  24,  12), VALUE(   3,   3), VALUE(   3, -16), 
        VALUE( -19,  -1), VALUE(   3,   0), VALUE(  16,  16), VALUE(  25,  13), VALUE(  29,   7), VALUE(  -4,   8), VALUE(   6,  10), VALUE( -17,   9), 
        VALUE(  -9,  -6), VALUE(  20,   5), VALUE(   2,   3), VALUE(   8,   8), VALUE(  -1,  -1), VALUE(  13,  14), VALUE(  12,  -6), VALUE(  -7,   7), 
        VALUE(  11, -52), VALUE(   1, -11), VALUE(   7,   1), VALUE(  -9,  -7), VALUE(   8,  -2), VALUE(   0,   1), VALUE(  27, -11), VALUE(  -6, -65), 
        VALUE( -19, -21), VALUE( -17, -24), VALUE( -22, -22), VALUE( -26, -15), VALUE(  -6, -13), VALUE( -20, -10), VALUE( -32, -32), VALUE( -34, -33), 
    },
    {
        VALUE(   5,   0), VALUE(  18,  12), VALUE(   6,  14), VALUE(   9,   2), VALUE(  17,  20), VALUE(  34,  27), VALUE(  32,  25), VALUE(  21,  12), 
        VALUE(  24,  19), VALUE(  21,  17), VALUE(  31,  19), VALUE(  39,  16), VALUE(  35,  20), VALUE(  28,  16), VALUE(  29,  17), VALUE(  40,  11), 
        VALUE(  12,  11), VALUE(   5,  12), VALUE(  17,  11), VALUE(  27,   4), VALUE(  34,   1), VALUE(  19,   7), VALUE(  21,  15), VALUE(  17,   5), 
        VALUE(   0,   3), VALUE(  11,  14), VALUE(  13,   3), VALUE(   8,   2), VALUE(   5,  -3), VALUE(   2,   5), VALUE(   4,  -5), VALUE(  -3,  -4), 
        VALUE( -26,   5), VALUE( -17,  -2), VALUE(  -6,   9), VALUE(  -9,   0), VALUE(  -9,  -1), VALUE( -13,  -2), VALUE(   0,   1), VALUE( -14,   2), 
        VALUE( -43, -10), VALUE( -26,  -2), VALUE( -19,  -8), VALUE( -15, -11), VALUE( -14,  -9), VALUE( -19, -16), VALUE( -13, -12), VALUE( -40,  -4), 
        VALUE( -28, -22), VALUE( -23, -21), VALUE( -12, -12), VALUE( -11, -17), VALUE( -21, -21), VALUE(  -7, -28), VALUE( -20, -21), VALUE( -73, -20), 
        VALUE( -14, -11), VALUE(  -8, -17), VALUE(   3,  -5), VALUE(   0,  -5), VALUE(   1, -14), VALUE(   2, -12), VALUE( -47,   1), VALUE( -22, -12), 
    },
    {
        VALUE( -22, -16), VALUE(  -6, -19), VALUE(   0,  -1), VALUE(   0,  31), VALUE(  13,  -2), VALUE(  17,  11), VALUE(  25,  -4), VALUE(   0,  20), 
        VALUE( -18, -11), VALUE( -47,  38), VALUE(   5,  20), VALUE(  12,  29), VALUE(  21,  20), VALUE(  26,  23), VALUE( -20,  41), VALUE(  30,  24), 
        VALUE( -21,  10), VALUE( -14,  -9), VALUE( -24,  -5), VALUE(  14,  20), VALUE(  12,  11), VALUE(  38,  23), VALUE(  25,  23), VALUE(  11,  18), 
        VALUE( -30,  10), VALUE( -28,  30), VALUE(  -2,  15), VALUE( -19,  26), VALUE(  -4,  36), VALUE( -20,  11), VALUE( -24,  70), VALUE( -12,  35), 
        VALUE(  -9, -23), VALUE( -19,   0), VALUE( -15,  19), VALUE( -26,  48), VALUE(  -8,  26), VALUE( -11,  21), VALUE(   3,  12), VALUE(  -3, -20), 
        VALUE( -12, -12), VALUE(   1,  -9), VALUE(  -9,  12), VALUE( -12,  -7), VALUE(  -5,  -1), VALUE(  -7,  30), VALUE(   7,   8), VALUE(  -9, -16), 
        VALUE( -10, -17), VALUE( -10, -10), VALUE(   0,  -8), VALUE(  -4,  -4), VALUE(   6, -18), VALUE(  19, -66), VALUE( -19, -54), VALUE( -50, -49), 
        VALUE( -25, -36), VALUE( -21,  10), VALUE(   1,   2), VALUE(  12, -51), VALUE(  -7,  -7), VALUE( -46, -42), VALUE( -83, -98), VALUE( -38, -57), 
    },
    {
        VALUE( -92, -79), VALUE( -44, -43), VALUE(-104, -63), VALUE( -41, -24), VALUE(-112, -24), VALUE(  10,  23), VALUE(  46, -29), VALUE(-113,-100), 
        VALUE( -18, -38), VALUE(   0,   4), VALUE(   0,  22), VALUE( -33,  -1), VALUE( -10,  -3), VALUE( -62, -46), VALUE( -46, -22), VALUE( -92, -79), 
        VALUE( -12,   2), VALUE(   8,  27), VALUE( -15,  20), VALUE(  -9,  22), VALUE( -17,   8), VALUE(  -3,  17), VALUE( -12,   9), VALUE( -19,   0), 
        VALUE( -16,  10), VALUE(  -3,  16), VALUE( -15,  28), VALUE( -28,  27), VALUE( -21,  22), VALUE(  -8,  19), VALUE(  -8,  11), VALUE(  -6,   1), 
        VALUE( -22,  -7), VALUE( -15,   4), VALUE(  -4,  22), VALUE( -18,  27), VALUE( -20,  31), VALUE(  -6,  20), VALUE( -15,   9), VALUE( -16,  -4), 
        VALUE( -26, -13), VALUE(   6,   9), VALUE(  -8,  19), VALUE( -16,  20), VALUE(  -8,  25), VALUE( -16,  22), VALUE(  -7,   6), VALUE( -24,  -6), 
        VALUE( -11, -20), VALUE(  -2,  10), VALUE( -18,   6), VALUE( -38,   9), VALUE( -25,   8), VALUE(  -8,  10), VALUE(  -2,   8), VALUE(  -6,  -9), 
        VALUE( -16, -52), VALUE(   3, -29), VALUE(  11, -26), VALUE( -51, -36), VALUE(  -7, -44), VALUE( -37, -25), VALUE(  19, -32), VALUE(   8, -55), 
    }
};

CONSTEVAL int KingSafetyFactor = 278;
CONSTEVAL int kingattackweight[7] = { 0,    0,    4,    4,    3,    4,    0 };
CONSTEVAL int PVBASEold[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
       60,   71,   97,   70,   46,   73,   41,   67,
       27,   27,   20,   34,   24,   35,   30,   29,
        7,    6,   -7,   16,   17,   -8,   -5,  -16,
        0,  -16,    2,   21,   16,   -4,  -19,  -19,
        2,  -10,    2,  -13,   -5,  -16,    4,   -8,
      -11,  -14,  -19,  -24,  -22,    2,    7,  -22,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {  -164,  -38,  -23,   -8,   -9,  -44,    1,  -70,
      -43,   -2,   -7,    9,    0,    5,    3,  -12,
      -32,    3,   11,   35,   58,   37,   31,  -28,
      -24,    4,   34,   28,   10,   27,   13,   21,
      -19,   -9,   12,    5,   12,    9,   12,  -10,
      -36,  -11,   -7,    6,   12,   10,   16,  -27,
      -64,  -32,  -35,   -5,    2,    1,   -8,  -17,
      -68,  -35,  -50,  -34,  -29,  -27,  -20,  -85  },
  {    -6,   -5,  -13,    7,    9,    6,   -6,  -36,
      -31,    0,  -11,    8,    5,   14,   -6,  -19,
       -7,   10,   13,   23,   16,   56,   22,   18,
       -8,   13,   10,   40,   17,   24,    3,    3,
      -19,    3,   16,   25,   29,   -4,    6,  -17,
       -9,   20,    2,    8,   -1,   13,   12,   -7,
       11,    1,    7,   -9,    8,    0,   27,   -6,
      -19,  -17,  -22,  -26,   -6,  -20,  -32,  -34  },
  {     5,   18,    6,    9,   17,   34,   32,   21,
       24,   21,   31,   39,   35,   28,   29,   40,
       12,    5,   17,   27,   34,   19,   21,   17,
        0,   11,   13,    8,    5,    2,    4,   -3,
      -26,  -17,   -6,   -9,   -9,  -13,    0,  -14,
      -43,  -26,  -19,  -15,  -14,  -19,  -13,  -40,
      -28,  -23,  -12,  -11,  -21,   -7,  -20,  -73,
      -14,   -8,    3,    0,    1,    2,  -47,  -22  },
  {   -22,   -6,    0,    0,   13,   17,   25,    0,
      -18,  -47,    5,   12,   21,   26,  -20,   30,
      -21,  -14,  -24,   14,   12,   38,   25,   11,
      -30,  -28,   -2,  -19,   -4,  -20,  -24,  -12,
       -9,  -19,  -15,  -26,   -8,  -11,    3,   -3,
      -12,    1,   -9,  -12,   -5,   -7,    7,   -9,
      -10,  -10,    0,   -4,    6,   19,  -19,  -50,
      -25,  -21,    1,   12,   -7,  -46,  -83,  -38  },
  {   -92,  -44, -104,  -41, -112,   10,   46, -113,
      -18,    0,    0,  -33,  -10,  -62,  -46,  -92,
      -12,    8,  -15,   -9,  -17,   -3,  -12,  -19,
      -16,   -3,  -15,  -28,  -21,   -8,   -8,   -6,
      -22,  -15,   -4,  -18,  -20,   -6,  -15,  -16,
      -26,    6,   -8,  -16,   -8,  -16,   -7,  -24,
      -11,   -2,  -18,  -38,  -25,   -8,   -2,   -6,
      -16,    3,   11,  -51,   -7,  -37,   19,    8  }
 };
CONSTEVAL int PVPHASEDIFFold[6][64] = {
  { -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999,
      -24,  -12,  -36,  -48,  -24,  -24,   -4,  -36,
       11,    0,   -9,  -36,  -43,  -48,   -5,  -24,
       15,   11,    5,  -31,  -28,    7,   15,   13,
       11,   24,    1,  -43,  -24,    0,   25,   17,
        4,    6,  -13,   16,    1,   18,  -13,   -3,
       22,   11,   19,    0,   12,    1,  -11,   11,
    -9999,-9999,-9999,-9999,-9999,-9999,-9999,-9999  },
  {   120,    3,   -8,  -25,  -14,    4,   -1,   -5,
        9,  -21,  -10,  -24,  -19,  -36,  -33,  -42,
        2,  -12,   -7,  -23,  -63,   -9,  -54,   -2,
       -7,  -12,  -33,  -24,   -3,  -33,  -23,  -66,
        9,   -1,   -9,    0,   -9,    0,   -8,    0,
        0,  -18,  -12,  -17,  -18,  -36,  -60,    0,
       18,   18,  -14,  -24,  -38,  -33,   -3,  -48,
      -47,    0,    5,    6,  -27,  -21,  -50,   -3  },
  {    14,    8,   12,   -1,   -2,    3,   10,   39,
       24,    2,    6,   -1,   -3,  -24,    7,   11,
       12,   -4,  -10,  -17,   -4,  -36,  -27,   -7,
        4,    0,   -6,  -28,   -3,  -12,    0,  -19,
       18,   -3,    0,  -12,  -22,   12,    4,   26,
        3,  -15,    1,    0,    0,    1,  -18,   14,
      -63,  -12,   -6,    2,  -10,    1,  -38,  -59,
       -2,   -7,    0,   11,   -7,   10,    0,    1  },
  {    -5,   -6,    8,   -7,    3,   -7,   -7,   -9,
       -5,   -4,  -12,  -23,  -15,  -12,  -12,  -29,
       -1,    7,   -6,  -23,  -33,  -12,   -6,  -12,
        3,    3,  -10,   -6,   -8,    3,   -9,   -1,
       31,   15,   15,    9,    8,   11,    1,   16,
       33,   24,   11,    4,    5,    3,    1,   36,
        6,    2,    0,   -6,    0,  -21,   -1,   53,
        3,   -9,   -8,   -5,  -15,  -14,   48,   10  },
  {     6,  -13,   -1,   31,  -15,   -6,  -29,   20,
        7,   85,   15,   17,   -1,   -3,   61,   -6,
       31,    5,   19,    6,   -1,  -15,   -2,    7,
       40,   58,   17,   45,   40,   31,   94,   47,
      -14,   19,   34,   74,   34,   32,    9,  -17,
        0,  -10,   21,    5,    4,   37,    1,   -7,
       -7,    0,   -8,    0,  -24,  -85,  -35,    1,
      -11,   31,    1,  -63,    0,    4,  -15,  -19  },
  {    13,    1,   41,   17,   88,   13,  -75,   13,
      -20,    4,   22,   32,    7,   16,   24,   13,
       14,   19,   35,   31,   25,   20,   21,   19,
       26,   19,   43,   55,   43,   27,   19,    7,
       15,   19,   26,   45,   51,   26,   24,   12,
       13,    3,   27,   36,   33,   38,   13,   18,
       -9,   12,   24,   47,   33,   18,   10,   -3,
      -36,  -32,  -37,   15,  -37,   12,  -51,  -63  }
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

//int passedpawnbonusperside[2][8];
//int attackingpawnbonusperside[2][8];


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
    registerTuner(&pawnpushthreatbonus, "pawnpushthreatbonus", pawnpushthreatbonus, 0, 0, 0, 0, NULL, false);
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
#if 1
    // tuning the psqt base at game start
    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 64; j++)
        {
            registerTuner(&PVBASE[i][j], "PVBASE", PVBASE[i][j], j, 64, i, 6, &CreatePositionvalueTable, PVBASE[i][j] <= -9999 );
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

    //positionvaluetable = NULL;
    CreatePositionvalueTable();//FIXME probably not needed when eval rewrite is finished
}


void CreatePositionvalueTable()
{

    for (int p = 0; p < 6; p++)
    {
        int egsum = 0, mgsum = 0;
        int num = 0;
        for (int i = 0; i < 8; i++)
        {
            printf("        ");
            for (int j = 0; j < 8; j++)
            {
                int mg = max(-999, PVBASEold[p][i * 8 + j]);
                int eg = max(-999, PVBASEold[p][i * 8 + j] + PVPHASEDIFFold[p][i * 8 + j]);
                printf("VALUE(%4d,%4d), ", max(-999, PVBASEold[p][i * 8 + j]), max(-999, PVBASEold[p][i * 8 + j] + PVPHASEDIFFold[p][i * 8 + j]));
                if (mg > -999)
                {
                    egsum += eg;
                    mgsum += mg;
                    num++;
                }
            }
            printf("\n");
        }
        printf("average mgval: %2.2f  average egval: %2.2f\n", mgsum / (float)num, egsum / (float)num);
        printf("\n");
        
    }
    for (int r = 0; r < 8; r++)
    {
        //passedpawnbonusperside[0][r] = ePassedpawnbonus[r];
        //passedpawnbonusperside[1][7 - r] = -ePassedpawnbonus[r];
        //attackingpawnbonusperside[0][r] = attackingpawnbonus[r];
        //attackingpawnbonusperside[1][7 - r] = -attackingpawnbonus[r];

    }

#if 1
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
                pos.positionvaluetable[index1] = (PVBASEold[(p - 1)][j1] * (256 - ph) + (PVBASEold[(p - 1)][j1] + PVPHASEDIFFold[(p - 1)][j1]) * ph) / 256;
                pos.positionvaluetable[index2] = -(PVBASEold[(p - 1)][j2] * (256 - ph) + (PVBASEold[(p - 1)][j2] + PVPHASEDIFFold[(p - 1)][j2])* ph) / 256;
                pos.positionvaluetable[index1] += prunematerialvalue[p];
                pos.positionvaluetable[index2] -= prunematerialvalue[p];
            }
        }
    }
#endif
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
        grad gAttackingpawnbonus[8] = { 0 };
        grad gConnectedbonus = 0;
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
                entryptr->attackedBy2[me] |= (entryptr->attacked[me] & pawn_attacks_occupied[index][me]);
                entryptr->attacked[me] |= pawn_attacks_occupied[index][me];
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
                        gAttackingpawnbonus[RRANK(index, me)] += S2MSIGN(me);
                        //entryptr->value += attackingpawnbonusperside[me][RANK(index)];
                        if (Et == TRACE)
                            ;// attackingpawnval[me] += attackingpawnbonusperside[me][RANK(index)];
                    }
                    if ((pawn_attacks_occupied[index][you] & piece00[pc]) || (phalanxMask[index] & piece00[pc]))
                    {
                        // pawn is protected by other pawn
                        gConnectedbonus += S2MSIGN(me);
                        //entryptr->value += S2MSIGN(me) * connectedbonus;
                        if (Et == TRACE)
                            ;// connectedpawnval[me] += S2MSIGN(me) * connectedbonus;
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

        entryptr->value += TAPEREDEVAL(eConnectedbonus * gConnectedbonus, ph);
        for (int r = 0; r < 8; r++)
        {
            entryptr->value += TAPEREDEVAL(eAttackingpawnbonus[r] * gAttackingpawnbonus[r], ph);
        }

    }

    grad gPassedpawnbonus[8] = { 0 };
    grad gIsolatedpawnpenalty = 0;
    grad gDoublepawnpenalty = 0;
    grad gBackwardpawnpenalty[2] = { 0 };
    int temp1 = 0;
    for (int s = 0; s < 2; s++)
    {
        U64 bb;
        bb = entryptr->passedpawnbb[s];
        while (LSB(index, bb))
        {
            gPassedpawnbonus[RRANK(index, s)] += S2MSIGN(s);
            //val += ph * passedpawnbonusperside[s][RANK(index)] / 256;
            bb ^= BITSET(index);
            if (Et == TRACE)
                ;// passedpawnval[s] += ph * passedpawnbonusperside[s][RANK(index)] / 256;
        }

        // isolated pawns
        gIsolatedpawnpenalty += S2MSIGN(s) * POPCOUNT(entryptr->isolatedpawnbb[s]);
        //val += S2MSIGN(s) * POPCOUNT(entryptr->isolatedpawnbb[s]) * isolatedpawnpenalty;
        if (Et == TRACE)
            ;// isolatedpawnval[s] += S2MSIGN(s) * POPCOUNT(entryptr->isolatedpawnbb[s]) * isolatedpawnpenalty;

        // doubled pawns
        gDoublepawnpenalty += S2MSIGN(s) * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8));
        //val += S2MSIGN(s) * doublepawnpenalty * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8));
        if (Et == TRACE)
            ;// doubledpawnval[s] += S2MSIGN(s) * doublepawnpenalty * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8));

        // backward pawns
        gBackwardpawnpenalty[s] += S2MSIGN(s) * POPCOUNT(entryptr->backwardpawnbb[s]);
        temp1 += S2MSIGN(s) * POPCOUNT(entryptr->backwardpawnbb[s]) * GETEGVAL(eBackwardpawnpenalty) * ph / 256;
        if (Et == TRACE)
            ;// backwardpawnval[s] += S2MSIGN(s) * POPCOUNT(entryptr->backwardpawnbb[s]) * backwardpawnpenalty * ph / 256;
    }

    val += TAPEREDEVAL(eIsolatedpawnpenalty * gIsolatedpawnpenalty, ph);
    val += TAPEREDEVAL(eDoublepawnpenalty * gDoublepawnpenalty, ph);
    val += TAPEREDEVAL(eBackwardpawnpenalty * gBackwardpawnpenalty[0], ph) + TAPEREDEVAL(eBackwardpawnpenalty * gBackwardpawnpenalty[1], ph);
#if 0
    if (temp1 != TAPEREDEVAL(eBackwardpawnpenalty * gBackwardpawnpenalty, ph))
        pos.print();
#endif
    for (int r = 0; r < 8; r++)
    {
        val += TAPEREDEVAL(ePassedpawnbonus[r] * gPassedpawnbonus[r], ph);
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
    grad gTempo = S2MSIGN(state & S2MMASK);
    int result = TAPEREDEVAL(eTempo * gTempo, ph);
    int kingattackweightsum[2] = { 0 };
    int psqteval[2] = { 0 };
    int mobilityeval[2] = { 0 };
    int freeslidereval[2] = { 0 };
    int doublebishopeval = 0;
    int kingshieldeval[2] = { 0 };
    //int kingdangereval[2] = { 0 };
    U64 occupied = occupied00[0] | occupied00[1];
    grad gPsqt[7][64] = { 0 };
    grad gMaterialvalue[7] = { 0 };
    memset(attackedBy, 0, sizeof(attackedBy));

    if (Et == TRACE)
        ;// printEvalTrace(1, "tempo", result);

    result += getPawnValue<Et>(&phentry);

    attackedBy[0][KING] = king_attacks[kingpos[0]];
    attackedBy2[0] = phentry->attackedBy2[0] | (attackedBy[0][KING] & phentry->attacked[0]);
    attackedBy[0][0] = attackedBy[0][KING] | phentry->attacked[0];
    attackedBy[1][KING] = king_attacks[kingpos[1]];
    attackedBy2[1] = phentry->attackedBy2[1] | (attackedBy[1][KING] & phentry->attacked[1]);
    attackedBy[1][0] = attackedBy[1][KING] | phentry->attacked[1];
 
    // king specials
    //int psqking0 = *(positionvaluetable + (kingpos[0] | (ph << 6) | (KING << 14) | (0 << 17)));
    //int psqking1 = *(positionvaluetable + (kingpos[1] | (ph << 6) | (KING << 14) | (1 << 17)));
    //result += psqking0 + psqking1;
    gPsqt[KING][PSQTINDEX(kingpos[0], 0)]++;
    gPsqt[KING][PSQTINDEX(kingpos[1], 1)]--;
    result += TAPEREDEVAL(ePsqt[KING][PSQTINDEX(kingpos[0], 0)], ph);
    result -= TAPEREDEVAL(ePsqt[KING][PSQTINDEX(kingpos[1], 1)], ph);
    if (Et == TRACE)
    {
        ;// psqteval[0] += psqking0;
        ;// psqteval[0] += psqking1;
    }

    grad gShiftmobilitybonus = 0;
    grad gSlideronfreefilebonus[2] = { 0 };

    for (int p = PAWN; p <= QUEEN; p++)
    {
        for (int me = 0; me < 2; me++)
        {
            //int pc = p + me >> 1;
            //int me = pc & S2MMASK;
            int you = me ^ S2MMASK;
            U64 pb = piece00[(p << 1) | me];
            while (LSB(index, pb))
            {
                //int pvtindex = index | (ph << 6) | (p << 14) | (me << 17);
                //result += *(positionvaluetable + pvtindex);
                int psqtindex = PSQTINDEX(index, me);
                gPsqt[p][psqtindex] += S2MSIGN(me);
                gMaterialvalue[p] += S2MSIGN(me);
                result += TAPEREDEVAL(ePsqt[p][psqtindex] * S2MSIGN(me), ph);
                if (Et == TRACE)
                    ;// psqteval[me] += *(positionvaluetable + pvtindex);

                pb ^= BITSET(index);

                U64 attack = 0ULL;;
                U64 mobility = 0ULL;
                if (shifting[p] & 0x2) // rook and queen
                {
                    attack = mRookAttacks[index][MAGICROOKINDEX(occupied, index)];
                    mobility = attack & ~occupied00[me];

                    // extrabonus for rook on (semi-)open file  
                    if (p == ROOK && (phentry->semiopen[me] & BITSET(FILE(index))))
                    {
                        gSlideronfreefilebonus[bool(phentry->semiopen[you] & BITSET(FILE(index)))] += S2MSIGN(me);
                        //result += S2MSIGN(me) * slideronfreefilebonus[bool(phentry->semiopen[you] & BITSET(FILE(index)))];
                        if (Et == TRACE)
                            ;// freeslidereval[me] += S2MSIGN(me) * slideronfreefilebonus[bool(phentry->semiopen[you] & BITSET(FILE(index)))];
                    }
                }

                if (shifting[p] & 0x1) // bishop and queen)
                {
                    attack |= mBishopAttacks[index][MAGICBISHOPINDEX(occupied, index)];
                    mobility |= ~occupied00[me]
                        & (mBishopAttacks[index][MAGICBISHOPINDEX(occupied, index)]);
                }

                if (p == KNIGHT)
                {
                    attack = knight_attacks[index];
                    mobility = attack & ~occupied00[me];
                }

                if (p != PAWN)
                {
                    // update attack bitboard
                    attackedBy[me][p] |= attack;
                    attackedBy2[me] |= (attackedBy[me][0] & attack);
                    attackedBy[me][0] |= attack;

                    // mobility bonus
                    //result += S2MSIGN(me) * POPCOUNT(mobility) * shiftmobilitybonus;
                    gShiftmobilitybonus += S2MSIGN(me) * POPCOUNT(mobility);
                    if (Et == TRACE)
                        ;// mobilityeval[me] += S2MSIGN(me) * POPCOUNT(mobility) * shiftmobilitybonus;

                    // king danger
                    U64 kingdangerarea = kingdangerMask[kingpos[you]][you];
                    if (mobility & kingdangerarea)
                    {
                        kingattackweightsum[me] += POPCOUNT(mobility & kingdangerarea) * kingattackweight[p];
                    }
                }
            }
        }
        result += TAPEREDEVAL(eMaterialvalue[p] * gMaterialvalue[p], ph);
    }
    attackedBy[0][PAWN] = phentry->attacked[0];
    attackedBy[1][PAWN] = phentry->attacked[1];

    // mobility bonus
    result += TAPEREDEVAL(eShiftmobilitybonus * gShiftmobilitybonus, ph);

    // slider on free file bonus
    for (int i = 0; i < 2; i++)
        result += TAPEREDEVAL(eSlideronfreefilebonus[i] * gSlideronfreefilebonus[i], ph);

    // bonus for double bishop
    grad gDoublebishopbonus = (POPCOUNT(piece00[WBISHOP]) >= 2) - (POPCOUNT(piece00[BBISHOP]) >= 2);
    result += TAPEREDEVAL(eDoublebishopbonus * gDoublebishopbonus, ph);
#if 0
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
#endif
    // some kind of king safety
    //result += (256 - ph) * (POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1])) * kingshieldbonus / 256;
    //result += kingshieldbonus.val(POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1]), ph);
    grad gKingshieldbonus = POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1]);
    result += TAPEREDEVAL(eKingshieldbonus * gKingshieldbonus, ph);

    if (Et == TRACE)
    {
        //kingshieldeval[0] = (256 - ph) * POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) * kingshieldbonus / 256;
        //kingshieldeval[1] = -(256 - ph) * POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1]) * kingshieldbonus / 256;
    }

    grad gPawnpushthreatbonus = 0;
    grad gSafepawnattackbonus = 0;
    for (int s = 0; s < 2; s++)
    {
        // King safety
        result += S2MSIGN(s) * KingSafetyFactor * KingSafetyTable[kingattackweightsum[s]] * (256 - ph) / 0x10000;
        if (Et == TRACE)
            ;//kingdangereval[s] = S2MSIGN(s) * KingSafetyFactor * KingSafetyTable[kingattackweightsum[s]] * (256 - ph) / 0x10000;

        // Threats
        int t = s ^ S2MMASK;
        U64 hisNonPawns = occupied00[t] ^ piece00[WPAWN | t];
        U64 hisAttackedNonPawns = (hisNonPawns & attackedBy[s][PAWN]);
        if (hisAttackedNonPawns)
        {
            // Our safe or protected pawns
            U64 ourSafePawns = piece00[WPAWN | s] & (~attackedBy[t][0] | attackedBy[s][0]);
            U64 safeThreats = PAWNATTACK(s, ourSafePawns) & hisAttackedNonPawns;
            gSafepawnattackbonus += S2MSIGN(s) * POPCOUNT(safeThreats);
            //result += S2MSIGN(s) * safepawnattackbonus * POPCOUNT(safeThreats);
        }

        // Threat by pawn push
        // Get empty squares for pawn pushes
        U64 pawnPush = PAWNPUSH(s, piece00[WPAWN | s]) & ~occupied;
        pawnPush |= PAWNPUSH(s, pawnPush & RANK3(s)) & ~occupied;

        // Filter squares that are attacked by opponent pawn or by any piece and undefeated
        pawnPush &= ~attackedBy[t][PAWN] & (attackedBy[s][0] | ~attackedBy[t][0]);

        // Get opponents pieces that are attacked from these pawn pushes and not already attacked now
        U64 attackedPieces = PAWNATTACK(s, pawnPush) & occupied00[t] & ~attackedBy[s][PAWN];
        gPawnpushthreatbonus += S2MSIGN(s) * POPCOUNT(attackedPieces);
        //result += S2MSIGN(s) * pawnpushthreatbonus * POPCOUNT(attackedPieces);
    }

    result += TAPEREDEVAL(ePawnpushthreatbonus * gPawnpushthreatbonus, ph);
    result += TAPEREDEVAL(eSafepawnattackbonus * gSafepawnattackbonus, ph);

    if (Et == TRACE)
    {
        printEvalTrace(1, "PSQT", psqteval);
        printEvalTrace(1, "Mobility", mobilityeval);
        printEvalTrace(1, "Slider on free file", freeslidereval);
        printEvalTrace(1, "double bishop", doublebishopeval);
        printEvalTrace(1, "kingshield", kingshieldeval);
        //printEvalTrace(1, "kingdanger", kingdangereval);

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
