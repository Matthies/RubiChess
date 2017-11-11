
#include "RubiChess.h"

// Evaluation stuff

#ifdef BITBOARD
extern U64 mybitset[64];
extern U64 passedPawn[64][2];
extern U64 filebarrier[64][2];
extern U64 neighbourfiles[64];
extern U64 kingshield[64][2];
extern U64 filemask[64];
extern U64 mBishopAttacks[64][1 << BISHOPINDEXBITS];
extern U64 mRookAttacks[64][1 << ROOKINDEXBITS];
#endif

const int passedpawnbonus[2][8] = { { 0, 10, 20, 30, 40, 60, 80, 0 }, { 0, -80, -60, -40, -30, -20, -10, 0 } };
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
            }
        }
    }
}


int chessposition::getPawnValue()
{
    int val;
    return val;
}


#ifdef BITBOARD

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
#ifdef DEBUGEVAL
    debugeval("Material value: %d\n", countMaterial());
#endif
    return countMaterial() + getPositionValue();
}


int chessposition::getPositionValue()
{
    int index;
    int ph = phase();
    int scalephaseto4 = (~ph & 0xff) >> 6;
    int result = 0;
#ifdef DEBUGEVAL
    int positionvalue = 0;
#endif
    for (int s = 0; s < 2; s++)
    {
        for (int p = PAWN; p <= KING; p++)
        {
            PieceCode pc = (p << 1) | s;
            U64 pb = piece00[pc];

            while (LSB(index, pb))
            {
                int pvtindex = index | (ph << 6) | (p << 14) | (s << 17);
                result += *(positionvaluetable + pvtindex);
#ifdef DEBUGEVAL
                positionvalue += *(positionvaluetable + pvtindex);
#endif
                pb ^= BITSET(index);
                if (p == PAWN)
                {
                    if (!(passedPawn[index][s] & piece00[pc ^ S2MMASK]))
                    {
                        // passed pawn
                        result += passedpawnbonus[s][RANK(index)];
#ifdef DEBUGEVAL
                        debugeval("Passed Pawn Bonus: %d\n", (S2MSIGN(s) * 40));
#endif
                    }
                    if (!(piece00[pc] & neighbourfiles[index]))
                    {
                        // isolated pawn
                        result -= S2MSIGN(s) * 20;
#ifdef DEBUGEVAL
                        debugeval("Isolated Pawn Penalty: %d\n", -(S2MSIGN(s) * 20));
#endif
                    }
                    else if (POPCOUNT((piece00[pc] & filemask[index])) > 1)
                    {
                        // double pawn
                        result -= S2MSIGN(s) * 15;
#ifdef DEBUGEVAL
                        debugeval("Double Pawn Penalty: %d\n", -(S2MSIGN(s) * 15));
#endif
                    }
                }
                if (shifting[p] & 0x2) // rook and queen
                {
                    if (!(filebarrier[index][s] & piece00[WPAWN | s]))
                    {
                        // free file
                        result += S2MSIGN(s) * 15;
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
    }
	// some kind of king safety
	result += (255 - ph) * (POPCOUNT(piece00[WPAWN] & kingshield[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshield[kingpos[1]][1])) * 15 / 255;
#ifdef DEBUGEVAL
    debugeval("King safety: %d\n", (255 - ph) * (POPCOUNT(piece00[WPAWN] & kingshield[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshield[kingpos[1]][1])) * 15 / 255);
    debugeval("Positional value: %d\n", positionvalue);
#endif
    return result;
}


int chessposition::countMaterial()
{
    int value = 0;
    for (int p = PAWN; p < KING; p++)
    {
        value += (POPCOUNT(piece00[p << 1]) - POPCOUNT(piece00[(p << 1) | 1])) * materialvalue[p];
    }
	return value;
}


#else //BITBOARD

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

    int materialVal = value;
    int positionVal = getPositionValue();
    return materialVal + positionVal;
}



/* Value of the position from whites pov */
int chessposition::getPositionValue()
{
    int ph = phase();
    int result = 0;
    int firstpawn[2][10] = { 0 };
    int lastpawn[2][10] = { 0 };
    int i;

    for (int f = 0; f < 8; f++)
    {
        int pawnsonfile[2] = { 0,0 };
        for (int r = 0; r < 8; r++)
        {
            i = (r << 4) | f;
            if (Piece(i) == PAWN)
            {
                int col = board[i] & S2MMASK;
                if (++pawnsonfile[col] > 1)
                    // double pawn penalty
                    result += (col ? -15 : 15);
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
                    result += (col ? -15 : 15);

            }
        }
    }
    for (int f = 0; f < 8; f++)
    {
        for (int col = 0; col < 2; col++)
        {
            int opcol = 1 - col;
            int factor = (col ? -1 : 1);
            int pawnrank = firstpawn[col][f + 1];
            if (pawnrank)
            {
                // check for passed pawn
                if ((!lastpawn[opcol][f] || factor * lastpawn[opcol][f] <= factor * pawnrank)
                    && (!lastpawn[opcol][f + 1] || factor * lastpawn[opcol][f + 1] <= factor * pawnrank)
                    && (!lastpawn[opcol][f + 2] || factor * lastpawn[opcol][f + 2] <= factor * pawnrank))
                    //result += (factor * 20);
                    result += passedpawnbonus[col][pawnrank];
            }
            if (pawnrank && !firstpawn[col][f] && !firstpawn[col][f + 2])
                // isolated pawn
                result -= (factor * 20);
        }
    }

    return result;
}



void chessposition::countMaterial()
{
    value = 0;
    for (int i = 0; i < 14; i++)
        piecenum[i] = 0;
    for (int r = 0; r < 8; r++)
    {
        for (int f = 0; f < 8; f++)
        {
            PieceCode pc = board[(r << 4) | f];
            if (pc != BLANK)
            {
                int col = pc & S2MMASK;
                piecenum[pc]++;
                value += (col ? -materialvalue[pc >> 1] : materialvalue[pc >> 1]);
            }
        }
    }
}

#endif

