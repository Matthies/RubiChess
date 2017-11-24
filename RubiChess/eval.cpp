
#include "RubiChess.h"

// Evaluation stuff

const int passedpawnbonus[2][8] = { { 0, 20, 30, 50, 70, 100, 150, 0 }, { 0, -150, -100, -70, -50, -30, -20, 0 } };
const int isolatedpawnpenalty = -20;
const int doublepawnpenalty = -15;
const int connectedbonus = 0;  // tried 10 but that seems not to work very well; best results without connected bonus due to a bug
const int attackingpawnbonus[2][8] = { { 0, 0, 0, 5, 10, 15, 0, 0 },{ 0, 0, -15, -10, -5, 0, 0, 0 } };
const int kingshieldbonus = 15;
const int backwardpawnpenalty = -20;
const int doublebishopbonus = 20;   // not yet used
const int slideronfreefilebonus = 15;
const double kingdangerfactor = 0.15;
const double kingdangerexponent = 1.1;

int squaredistance[BOARDSIZE][BOARDSIZE];
int kingdanger[BOARDSIZE][BOARDSIZE][7];

const int PV[][64] = {
    //PAWN
    {
        0,   0,   0,   0,   0,   0,   0,   0,
        50,  50,  50,  50,  50,  50,  50,  50,
        20,  20,  20,  20,  20,  20,  20,  20,
        0,   0,   0,   10,  10,  0,   0,   0,
        0,   -20, 10,  20,  20,  -20, -20, 0,
        0,   -10, 10,  10,  10,  -10, -10, 0,
        0,  10,  0,  -20, -20,  0,   10,  10,
        0,   0,   0,   0,   0,   0,   0,   0
    },
    {
        0,   0,   0,   0,   0,   0,   0,   0,
        35,  35,  35,  35,  35,  35,  35,  35,
        15,  25,  25,  25,  25,  25,  25,  15,
        10,  15,  15,  15,  15,  15,  15,  10,
        5,   10,  10,  10,  10,  10,  10,  5,
        2,   5,   5,   5,   5,   5,   5,   2,
        0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0
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
    },
    {
        -30, -30, -20, -20, -20, -20, -30, -30,
        -30, -20, -20, -20, -20, -20, -20, -30,
        -20, -20, -10, -10, -10, -10, -20, -20,
        -20, -20, -10,   0,   0, -10, -20, -20,
        -20, -20, -10,   0,   0, -10, -20, -20,
        -20, -20, -10, -10, -10, -10, -20, -20,
        -30, -20, -20, -20, -20, -20, -20, -30,
        -30, -30, -20, -20, -20, -20, -30, -30
    }
};



void chessposition::CreatePositionvalueTable()
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

    positionvaluetable = new int[2 * 8 * 256 * BOARDSIZE];  // color piecetype phase boardindex

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
                positionvaluetable[index1] = (PV[(p - 1) << 1][j1] * (255 - ph) + PV[((p - 1) << 1) | 1][j1] * ph) / 255;
                positionvaluetable[index2] = -(PV[(p - 1) << 1][j2] * (255 - ph) + PV[((p - 1) << 1) | 1][j2] * ph) / 255;
                positionvaluetable[index1] += materialvalue[p];
                positionvaluetable[index2] -= materialvalue[p];
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
                        entry->value += attackingpawnbonus[s][RANK(index)];
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
            val += ph * passedpawnbonus[s][RANK(index)] / 256;
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
                int col = board[i] & S2MMASK;
                if (board[i + S2MSIGN(col) * 16] == board[i])
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
            if (board[i] != BLANK)
            {
                PieceType pt = Piece(i);
                int col = board[i] & S2MMASK;
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
                    result += ph * passedpawnbonus[col][pawnrank] / 256;
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
            PieceCode pc = board[(r << 4) | f];
            if (pc != BLANK)
            {
                piecenum[pc]++;
            }
        }
    }
}
#endif

