
#include "RubiChess.h"

// Evaluation stuff

// static values for the search/pruning stuff
const int prunematerialvalue[7] = { 0,  100,  314,  314,  483,  913, 32509 };

evalparamset eps;

#if 0

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
// neuer Bench nach Einbau des kingdanger rewrite und Übernahme der psqt-Werte aus psqt-1: 11086704
eval eWeakkingringpenalty = VALUE(-20, 0);
eval eKingattackweight[7] = { VALUE(0, 0), VALUE(0, 0), VALUE(4, 0), VALUE(2, 0), VALUE(1, 0), VALUE(4, 0), VALUE(0, 0) };
// neuer Bench nach endgültigem Umbau kingdanger und letzte psqtdiff-Werte: 12055951 (evalrw1)
// neuer Bench nach Korrektur von Vorzeichenfehler bei Kingdanger: 11621517 (evalrw2)



eval ePsqt[7][64] = {
    {},
    {
        VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), 
        VALUE(  60,  37), VALUE(  71,  60), VALUE(  97,  61), VALUE(  70,   9), VALUE(  46,  22), VALUE(  73,  50), VALUE(  41,  37), VALUE(  67,  32), 
        VALUE(  27,  38), VALUE(  27,  27), VALUE(  20,  11), VALUE(  34,  -9), VALUE(  24, -21), VALUE(  35, -22), VALUE(  30,  25), VALUE(  29,   6), 
        VALUE(   7,  24), VALUE(   6,  17), VALUE(  -7,  -2), VALUE(  16, -15), VALUE(  17, -13), VALUE(  -8,  -2), VALUE(  -5,  10), VALUE( -16,  -3), 
        VALUE(   0,  11), VALUE( -16,   8), VALUE(   2,   3), VALUE(  21, -21), VALUE(  16,  -6), VALUE(  -4,  -4), VALUE( -19,   5), VALUE( -19,   0), 
        VALUE(   2,   6), VALUE( -10,  -2), VALUE(   2,  -6), VALUE( -13,   1), VALUE(  -5,  -4), VALUE( -16,   2), VALUE(   4,  -9), VALUE(  -8, -10), 
        VALUE( -11,  13), VALUE( -14,  -3), VALUE( -19,   0), VALUE( -24, -24), VALUE( -22, -10), VALUE(   2,   3), VALUE(   7,  -4), VALUE( -22,  -8), 
        VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), VALUE(-999,-999), 
    },
    {
        VALUE(-164, -29), VALUE( -38, -35), VALUE( -23, -34), VALUE(  -8, -27), VALUE(  -9, -23), VALUE( -44, -40), VALUE(   1,   0), VALUE( -70, -85), 
        VALUE( -43, -35), VALUE(  -2, -25), VALUE(  -7, -17), VALUE(   9, -23), VALUE(   0, -18), VALUE(   5, -36), VALUE(   3, -30), VALUE( -12, -54), 
        VALUE( -32, -30), VALUE(   3, -16), VALUE(  11,  -5), VALUE(  35,  12), VALUE(  58, -21), VALUE(  37,  26), VALUE(  31, -25), VALUE( -28, -30), 
        VALUE( -24, -31), VALUE(   4, -14), VALUE(  34,   0), VALUE(  28,   5), VALUE(  10,   6), VALUE(  27, -37), VALUE(  13, -15), VALUE(  21, -45), 
        VALUE( -19, -10), VALUE(  -9, -10), VALUE(  12,  -7), VALUE(   5,   1), VALUE(  12,   3), VALUE(   9,   9), VALUE(  12,   2), VALUE( -10, -10), 
        VALUE( -36, -36), VALUE( -11, -35), VALUE(  -7, -19), VALUE(   6, -15), VALUE(  12, -12), VALUE(  10, -32), VALUE(  16, -44), VALUE( -27, -27), 
        VALUE( -64, -46), VALUE( -32, -13), VALUE( -35, -49), VALUE(  -5, -38), VALUE(   2, -35), VALUE(   1, -31), VALUE(  -8, -18), VALUE( -17, -65), 
        VALUE( -68,-115), VALUE( -35, -35), VALUE( -50, -45), VALUE( -34, -28), VALUE( -29, -60), VALUE( -27, -51), VALUE( -20, -70), VALUE( -85, -87), 
    },
    {
        VALUE(  -6,   6), VALUE(  -5,   3), VALUE( -13,   2), VALUE(   7,   3), VALUE(   9,   7), VALUE(   6,   5), VALUE(  -6,   4), VALUE( -36,   3), 
        VALUE( -31,  -3), VALUE(   0,   2), VALUE( -11,  -4), VALUE(   8,   6), VALUE(   5,  -1), VALUE(  14, -12), VALUE(  -6,   1), VALUE( -19,  -8), 
        VALUE(  -7,  13), VALUE(  10,   6), VALUE(  13,   4), VALUE(  23,   6), VALUE(  16,  13), VALUE(  56,  15), VALUE(  22,  -5), VALUE(  18,   3), 
        VALUE(  -8,  -7), VALUE(  13,  13), VALUE(  10,   4), VALUE(  40,  12), VALUE(  17,  13), VALUE(  24,  10), VALUE(   3,   3), VALUE(   3, -16), 
        VALUE( -19,  -1), VALUE(   3,   0), VALUE(  16,  16), VALUE(  25,   7), VALUE(  29,   7), VALUE(  -4,   8), VALUE(   6,  10), VALUE( -17,  10), 
        VALUE(  -9,  -9), VALUE(  20,   3), VALUE(   2,   4), VALUE(   8,   8), VALUE(  -1,  14), VALUE(  13,  14), VALUE(  12,  -9), VALUE(  -7,   5), 
        VALUE(  11, -52), VALUE(   1, -13), VALUE(   7,   2), VALUE(  -9,  -6), VALUE(   8,  -7), VALUE(   0,   1), VALUE(  27, -11), VALUE(  -6, -65), 
        VALUE( -19, -24), VALUE( -17, -24), VALUE( -22, -22), VALUE( -26, -17), VALUE(  -6, -13), VALUE( -20,  -9), VALUE( -32, -29), VALUE( -34, -57), 
    },
    {
        VALUE(   5,   0), VALUE(  18,  13), VALUE(   6,  14), VALUE(   9,   2), VALUE(  17,  20), VALUE(  34,  27), VALUE(  32,  25), VALUE(  21,  12), 
        VALUE(  24,  19), VALUE(  21,  17), VALUE(  31,  20), VALUE(  39,  10), VALUE(  35,  20), VALUE(  28,  17), VALUE(  29,  18), VALUE(  40,  11), 
        VALUE(  12,  11), VALUE(   5,  12), VALUE(  17,  12), VALUE(  27,   4), VALUE(  34,  -4), VALUE(  19,   5), VALUE(  21,  16), VALUE(  17,   5), 
        VALUE(   0,   3), VALUE(  11,  14), VALUE(  13,   3), VALUE(   8,   3), VALUE(   5,  -3), VALUE(   2,   5), VALUE(   4,  -5), VALUE(  -3,  -4), 
        VALUE( -26,   5), VALUE( -17,  -1), VALUE(  -6,   9), VALUE(  -9,   0), VALUE(  -9,   1), VALUE( -13,  -2), VALUE(   0,   1), VALUE( -14,   2), 
        VALUE( -43, -10), VALUE( -26,  -2), VALUE( -19,  -3), VALUE( -15, -11), VALUE( -14,  -9), VALUE( -19, -16), VALUE( -13, -12), VALUE( -40,  -4), 
        VALUE( -28, -24), VALUE( -23, -21), VALUE( -12, -11), VALUE( -11, -21), VALUE( -21, -21), VALUE(  -7, -27), VALUE( -20, -21), VALUE( -73, -20), 
        VALUE( -14, -11), VALUE(  -8, -15), VALUE(   3,  -5), VALUE(   0,  -5), VALUE(   1, -13), VALUE(   2, -12), VALUE( -47,   7), VALUE( -22, -19), 
    },
    {
        VALUE( -22,  -9), VALUE(  -6, -11), VALUE(   0,  -1), VALUE(   0,  31), VALUE(  13,  -2), VALUE(  17,  11), VALUE(  25,  -7), VALUE(   0,  20), 
        VALUE( -18, -11), VALUE( -47,  38), VALUE(   5,  20), VALUE(  12,  29), VALUE(  21,  23), VALUE(  26,  23), VALUE( -20,  34), VALUE(  30,  24), 
        VALUE( -21,  10), VALUE( -14,  -9), VALUE( -24,  11), VALUE(  14,  18), VALUE(  12,  11), VALUE(  38,  23), VALUE(  25,  23), VALUE(  11,  18), 
        VALUE( -30,   7), VALUE( -28,  30), VALUE(  -2,  15), VALUE( -19,  41), VALUE(  -4,  36), VALUE( -20,  11), VALUE( -24,  54), VALUE( -12,  36), 
        VALUE(  -9, -23), VALUE( -19,   1), VALUE( -15,  19), VALUE( -26,  49), VALUE(  -8,  24), VALUE( -11,  21), VALUE(   3,  11), VALUE(  -3, -21), 
        VALUE( -12, -12), VALUE(   1,  -8), VALUE(  -9,  19), VALUE( -12,  -7), VALUE(  -5,   0), VALUE(  -7,  29), VALUE(   7,   8), VALUE(  -9, -16), 
        VALUE( -10, -17), VALUE( -10, -10), VALUE(   0,  -8), VALUE(  -4,  -4), VALUE(   6, -18), VALUE(  19, -66), VALUE( -19, -54), VALUE( -50, -49), 
        VALUE( -25, -36), VALUE( -21,  10), VALUE(   1,   2), VALUE(  12, -52), VALUE(  -7,  -7), VALUE( -46, -42), VALUE( -83, -98), VALUE( -38, -57), 
    },
    {
        VALUE( -92, -79), VALUE( -44, -44), VALUE(-104, -64), VALUE( -41, -24), VALUE(-112, -23), VALUE(  10,  23), VALUE(  46, -31), VALUE(-113,-100), 
        VALUE( -18, -38), VALUE(   0,   4), VALUE(   0,  21), VALUE( -33,  -1), VALUE( -10,  -3), VALUE( -62, -45), VALUE( -46, -20), VALUE( -92, -79), 
        VALUE( -12,   2), VALUE(   8,  27), VALUE( -15,  23), VALUE(  -9,  22), VALUE( -17,   8), VALUE(  -3,  16), VALUE( -12,   8), VALUE( -19,  -1), 
        VALUE( -16,  11), VALUE(  -3,  15), VALUE( -15,  28), VALUE( -28,  29), VALUE( -21,  22), VALUE(  -8,  19), VALUE(  -8,  11), VALUE(  -6,   1), 
        VALUE( -22,  -7), VALUE( -15,   4), VALUE(  -4,  22), VALUE( -18,  27), VALUE( -20,  31), VALUE(  -6,  20), VALUE( -15,  11), VALUE( -16,   0), 
        VALUE( -26, -14), VALUE(   6,   8), VALUE(  -8,  19), VALUE( -16,  22), VALUE(  -8,  25), VALUE( -16,  22), VALUE(  -7,   6), VALUE( -24,  -7), 
        VALUE( -11, -20), VALUE(  -2,  10), VALUE( -18,   7), VALUE( -38,   9), VALUE( -25,   8), VALUE(  -8,   9), VALUE(  -2,   6), VALUE(  -6,  -9), 
        VALUE( -16, -38), VALUE(   3, -25), VALUE(  11, -16), VALUE( -51, -21), VALUE(  -7, -44), VALUE( -37, -20), VALUE(  19, -36), VALUE(   8, -55), 
    }
};
#endif



#ifdef EVALTUNE
void chessposition::resetTuner()
{
    for (int i = 0; i < tps.count; i++)
        tps.ev[i]->resetGrad();
}

void chessposition::getPositionTuneSet(positiontuneset *p)
{
    p->ph = ph;
    for (int i = 0; i < tps.count; i++)
        p->g[i] = tps.ev[i]->getGrad();
}

void chessposition::copyPositionTuneSet(positiontuneset *from, positiontuneset *to)
{
    to->ph = from->ph;
    for (int i = 0; i < tps.count; i++)
        to->g[i] = from->g[i];
}

string chessposition::getGradientString()
{
    string s = "";
    for (int i = 0; i < tps.count; i++)
    {
        if (pts.g[i])
            s = s + tps.name[i] + "(" + to_string(pts.g[i]) + ") ";
    }

    return s;
}

int chessposition::getGradientValue(positiontuneset *p)
{
    int v = 0;
    for (int i = 0; i < tps.count; i++)
        v += *tps.ev[i] * p->g[i];

    return v;
}


static void registertuner(eval *e, string name, int index1, int bound1, int index2, int bound2, bool tune)
{
    int i = pos.tps.count;
    pos.tps.ev[i] = e;
    pos.tps.name[i] = name;
    pos.tps.index1[i] = index1;
    pos.tps.bound1[i] = bound1;
    pos.tps.index2[i] = index2;
    pos.tps.bound2[i] = bound2;
    pos.tps.tune[i] = tune;
    pos.tps.count++;
}

void registeralltuners()
{
    int i, j;

    registertuner(&eps.ePawnpushthreatbonus, "ePawnpushthreatbonus", 0, 0, 0, 0, true);
    registertuner(&eps.eSafepawnattackbonus, "eSafepawnattackbonus", 0, 0, 0, 0, true);
    registertuner(&eps.eKingshieldbonus, "eKingshieldbonus", 0, 0, 0, 0, true);
    registertuner(&eps.eTempo, "eTempo", 0, 0, 0, 0, true);
    for (i = 0; i < 8; i++)
        registertuner(&eps.ePassedpawnbonus[i], "ePassedpawnbonus", i, 8, 0, 0, i > 0 && i < 7);
    for (i = 0; i < 8; i++)
        registertuner(&eps.eAttackingpawnbonus[i], "eAttackingpawnbonus", i, 8, 0, 0, i > 0 && i < 7);
    registertuner(&eps.eIsolatedpawnpenalty, "eIsolatedpawnpenalty", 0, 0, 0, 0, true);
    registertuner(&eps.eDoublepawnpenalty, "eDoublepawnpenalty", 0, 0, 0, 0, true);
    registertuner(&eps.eConnectedbonus, "eConnectedbonus", 0, 0, 0, 0, true);
    registertuner(&eps.eBackwardpawnpenalty, "eBackwardpawnpenalty", 0, 0, 0, 0, true);
    registertuner(&eps.eDoublebishopbonus, "eDoublebishopbonus", 0, 0, 0, 0, true);
    registertuner(&eps.eShiftmobilitybonus, "eShiftmobilitybonus", 0, 0, 0, 0, true);
    for (i = 0; i < 2; i++)
        registertuner(&eps.eSlideronfreefilebonus[i], "eSlideronfreefilebonus", i, 2, 0, 0, true);
    for (i = 0; i < 7; i++)
        registertuner(&eps.eMaterialvalue[i], "eMaterialvalue", i, 7, 0, 0, false);
    registertuner(&eps.eWeakkingringpenalty, "eWeakkingringpenalty", 0, 0, 0, 0, true);
    for (i = 0; i < 7; i++)
        registertuner(&eps.eKingattackweight[i], "eKingattackweight", i, 7, 0, 0, i >= KNIGHT && i <= QUEEN);
    for (i = 0; i < 7; i++)
        for (j = 0; j < 64; j++)
        registertuner(&eps.ePsqt[i][j], "ePsqt", j, 64, i, 7, i >= KNIGHT || (j >= 8 && j < 56));

}
#endif


void chessposition::init()
{
#ifdef EVALTUNE
    registeralltuners();
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


template <EvalTrace Et> //FIXME: TRACE mode was completely remove at eval rewrite; should probably to be implemented again
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
        entryptr->value += EVAL(eps.eMaterialvalue[PAWN], POPCOUNT(piece00[WPAWN]) - POPCOUNT(piece00[BPAWN]));

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
                int psqtindex = PSQTINDEX(index, me);
                entryptr->value += EVAL(eps.ePsqt[PAWN][psqtindex], S2MSIGN(me));

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
                        entryptr->value += EVAL(eps.eAttackingpawnbonus[RRANK(index, me)], S2MSIGN(me));
                    }
                    if ((pawn_attacks_occupied[index][you] & piece00[pc]) || (phalanxMask[index] & piece00[pc]))
                    {
                        entryptr->value += EVAL(eps.eConnectedbonus, S2MSIGN(me));
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
            val += EVAL(eps.ePassedpawnbonus[RRANK(index, s)], S2MSIGN(s));
            bb ^= BITSET(index);
        }

        // isolated pawns
        val += EVAL(eps.eIsolatedpawnpenalty, S2MSIGN(s) * POPCOUNT(entryptr->isolatedpawnbb[s]));

        // doubled pawns
        val += EVAL(eps.eDoublepawnpenalty, S2MSIGN(s) * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8)));

        // backward pawns
        val += EVAL(eps.eBackwardpawnpenalty, S2MSIGN(s) * POPCOUNT(entryptr->backwardpawnbb[s]));
    }

    return val + entryptr->value;
}


template <EvalTrace Et>
int chessposition::getPositionValue()
{
    pawnhashentry *phentry;
    int index;
    int kingattackpiececount[2][7] = { 0 };
    int kingattackers[2];
    int psqteval[2] = { 0 };
    int mobilityeval[2] = { 0 };
    int freeslidereval[2] = { 0 };
    int doublebishopeval = 0;
    int kingshieldeval[2] = { 0 };
    U64 occupied = occupied00[0] | occupied00[1];
    memset(attackedBy, 0, sizeof(attackedBy));

    int result = EVAL(eps.eTempo, S2MSIGN(state & S2MMASK));

    result += getPawnValue<Et>(&phentry);

    attackedBy[0][KING] = king_attacks[kingpos[0]];
    attackedBy2[0] = phentry->attackedBy2[0] | (attackedBy[0][KING] & phentry->attacked[0]);
    attackedBy[0][0] = attackedBy[0][KING] | phentry->attacked[0];
    attackedBy[1][KING] = king_attacks[kingpos[1]];
    attackedBy2[1] = phentry->attackedBy2[1] | (attackedBy[1][KING] & phentry->attacked[1]);
    attackedBy[1][0] = attackedBy[1][KING] | phentry->attacked[1];
 
    kingattackers[0] = POPCOUNT(attackedBy[0][PAWN] & kingdangerMask[kingpos[1]][1]);
    kingattackers[1] = POPCOUNT(attackedBy[1][PAWN] & kingdangerMask[kingpos[0]][0]);

    // king specials
    result += EVAL(eps.ePsqt[KING][PSQTINDEX(kingpos[0], 0)], 1);
    result += EVAL(eps.ePsqt[KING][PSQTINDEX(kingpos[1], 1)], -1);

    for (int pc = WKNIGHT; pc <= BQUEEN; pc++)
    {
        int p = (pc >> 1);
        int me = pc & S2MMASK;
        int you = me ^ S2MMASK;
        U64 pb = piece00[pc];
        U64 kingdangerarea = kingdangerMask[kingpos[you]][you];

        while (LSB(index, pb))
        {
            int psqtindex = PSQTINDEX(index, me);
            result += EVAL(eps.ePsqt[p][psqtindex], S2MSIGN(me));
            result += EVAL(eps.eMaterialvalue[p], S2MSIGN(me));

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
                    result += EVAL(eps.eSlideronfreefilebonus[bool(phentry->semiopen[you] & BITSET(FILE(index)))], S2MSIGN(me));
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
                result += EVAL(eps.eShiftmobilitybonus, S2MSIGN(me) * POPCOUNT(mobility));

                // king danger
                if (mobility & kingdangerarea)
                {
                    kingattackpiececount[me][p] += POPCOUNT(mobility & kingdangerarea);
                    kingattackers[me]++;
                }
            }
        }
    }
    attackedBy[0][PAWN] = phentry->attacked[0];
    attackedBy[1][PAWN] = phentry->attacked[1];

    // bonus for double bishop
    result += EVAL(eps.eDoublebishopbonus, ((POPCOUNT(piece00[WBISHOP]) >= 2) - (POPCOUNT(piece00[BBISHOP]) >= 2)));

    // some kind of king safety
    result += EVAL(eps.eKingshieldbonus, (POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1])));

    for (int s = 0; s < 2; s++)
    {
        int t = s ^ S2MMASK;

        // King safety
        if (kingattackers[s] > 1 - (bool)(piece00[WQUEEN | s]))
        {
            // Attacks to our king ring
            for (int p = KNIGHT; p <= QUEEN; p++)
                result += EVAL(eps.eKingattackweight[p], S2MSIGN(s) * kingattackpiececount[s][p] * kingattackers[s]);

            // Attacked and poorly defended squares in our king ring
            U64 krattacked = kingdangerMask[kingpos[s]][s]
                & attackedBy[t][0]
                & ~attackedBy2[s]
                & (~attackedBy[s][0] | attackedBy[s][KING] | attackedBy[s][QUEEN]);
            
            result += EVAL(eps.eWeakkingringpenalty, S2MSIGN(s) * POPCOUNT(krattacked));
        }

        // Threats
        U64 hisNonPawns = occupied00[t] ^ piece00[WPAWN | t];
        U64 hisAttackedNonPawns = (hisNonPawns & attackedBy[s][PAWN]);
        if (hisAttackedNonPawns)
        {
            // Our safe or protected pawns
            U64 ourSafePawns = piece00[WPAWN | s] & (~attackedBy[t][0] | attackedBy[s][0]);
            U64 safeThreats = PAWNATTACK(s, ourSafePawns) & hisAttackedNonPawns;
            result += EVAL(eps.eSafepawnattackbonus, S2MSIGN(s) * POPCOUNT(safeThreats));
        }

        // Threat by pawn push
        // Get empty squares for pawn pushes
        U64 pawnPush = PAWNPUSH(s, piece00[WPAWN | s]) & ~occupied;
        pawnPush |= PAWNPUSH(s, pawnPush & RANK3(s)) & ~occupied;

        // Filter squares that are attacked by opponent pawn or by any piece and undefeated
        pawnPush &= ~attackedBy[t][PAWN] & (attackedBy[s][0] | ~attackedBy[t][0]);

        // Get opponents pieces that are attacked from these pawn pushes and not already attacked now
        U64 attackedPieces = PAWNATTACK(s, pawnPush) & occupied00[t] & ~attackedBy[s][PAWN];
        result += EVAL(eps.ePawnpushthreatbonus, S2MSIGN(s) * POPCOUNT(attackedPieces));
    }

    return NEWTAPEREDEVAL(result, ph);
}


template <EvalTrace Et>
int chessposition::getValue()
{
#ifdef EVALTUNE
    resetTuner();
#endif
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

