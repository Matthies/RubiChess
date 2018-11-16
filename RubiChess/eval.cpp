
#include "RubiChess.h"

// Evaluation stuff

// static values for the search/pruning stuff
const int prunematerialvalue[7] = { 0,  100,  314,  314,  483,  913, 32509 };
const int maxmobility[4] = { 9, 14, 15, 28 }; // indexed by piece - 2

evalparamset eps;


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
    bool tuneIt = false;

    for (i = 0; i < 4; i++)
        for (j = 0; j < 5; j++)
            registertuner(&eps.ePawnstormblocked[i][j], "ePawnstormblocked", j, 5, i, 4, tuneIt);
    for (i = 0; i < 4; i++)
        for (j = 0; j < 5; j++)
            registertuner(&eps.ePawnstormfree[i][j], "ePawnstormfree", j, 5, i, 4, tuneIt);

    registertuner(&eps.ePawnpushthreatbonus, "ePawnpushthreatbonus", 0, 0, 0, 0, tuneIt);
    registertuner(&eps.eSafepawnattackbonus, "eSafepawnattackbonus", 0, 0, 0, 0, tuneIt);
    registertuner(&eps.eKingshieldbonus, "eKingshieldbonus", 0, 0, 0, 0, tuneIt);
    registertuner(&eps.eTempo, "eTempo", 0, 0, 0, 0, tuneIt);
    tuneIt = true;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 8; j++)
            registertuner(&eps.ePassedpawnbonus[i][j], "ePassedpawnbonus", j, 8, i, 4, tuneIt && (j > 0 && j < 7));
    tuneIt = false;
    for (i = 0; i < 8; i++)
        registertuner(&eps.eAttackingpawnbonus[i], "eAttackingpawnbonus", i, 8, 0, 0, tuneIt && (i > 0 && i < 7));
    registertuner(&eps.eIsolatedpawnpenalty, "eIsolatedpawnpenalty", 0, 0, 0, 0, tuneIt);
    registertuner(&eps.eDoublepawnpenalty, "eDoublepawnpenalty", 0, 0, 0, 0, tuneIt);
    registertuner(&eps.eConnectedbonus, "eConnectedbonus", 0, 0, 0, 0, tuneIt);
    registertuner(&eps.eBackwardpawnpenalty, "eBackwardpawnpenalty", 0, 0, 0, 0, tuneIt);
    registertuner(&eps.eDoublebishopbonus, "eDoublebishopbonus", 0, 0, 0, 0, tuneIt);

    tuneIt = false;

    for (i = 0; i < 4; i++)
        for (j = 0; j < 28; j++)
            registertuner(&eps.eMobilitybonus[i][j], "eMobilitybonus", j, 28, i, 4, tuneIt && (j < maxmobility[i]));

    tuneIt = false;
    for (i = 0; i < 2; i++)
        registertuner(&eps.eSlideronfreefilebonus[i], "eSlideronfreefilebonus", i, 2, 0, 0, tuneIt);
    for (i = 0; i < 7; i++)
        registertuner(&eps.eMaterialvalue[i], "eMaterialvalue", i, 7, 0, 0, false);
    registertuner(&eps.eWeakkingringpenalty, "eWeakkingringpenalty", 0, 0, 0, 0, tuneIt);
    for (i = 0; i < 7; i++)
        registertuner(&eps.eKingattackweight[i], "eKingattackweight", i, 7, 0, 0, tuneIt && (i >= KNIGHT && i <= QUEEN));

    tuneIt = false;

    for (i = 0; i < 7; i++)
        for (j = 0; j < 64; j++)
        registertuner(&eps.ePsqt[i][j], "ePsqt", j, 64, i, 7, tuneIt && (i >= KNIGHT || (i == PAWN && j >= 8 && j < 56)));
}
#endif


void chessposition::init()
{
#ifdef EVALTUNE
    registeralltuners();
#endif
}



template <EvalTrace Et> //FIXME: TRACE mode was completely remove at eval rewrite; should probably to be implemented again
int chessposition::getPawnAndKingValue(pawnhashentry **entry)
{
    int val = 0;
    int index;

    bool hashexist = pwnhsh.probeHash(entry);
    pawnhashentry *entryptr = *entry;
    if (!hashexist)
    {
        entryptr->value += EVAL(eps.ePsqt[KING][PSQTINDEX(kingpos[0], 0)], 1);
        entryptr->value += EVAL(eps.ePsqt[KING][PSQTINDEX(kingpos[1], 1)], -1);
        entryptr->value += EVAL(eps.eMaterialvalue[PAWN], POPCOUNT(piece00[WPAWN]) - POPCOUNT(piece00[BPAWN]));
        // kingshield safety
        entryptr->value += EVAL(eps.eKingshieldbonus, (POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1])));

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
                entryptr->semiopen[me] &= (int)(~BITSET(FILE(index))); 
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

            // Pawn storm evaluation
            int ki = kingpos[me];
            int kf = FILE(ki);
            int kr = RANK(ki);
            for (int f = max(0, kf - 1), j = ki + f - kf; f <= min(kf + 1, 7); f++, j++)
            {
                U64 mask = (filebarrierMask[j][me] | BITSET(j)); // FIXME: Better mask would be useful
                U64 yourStormingPawn = piece00[WPAWN | you] & mask;
                int yourDist = 7;
                if (yourStormingPawn)
                {
                    int nextPawn;
                    if (me)
                        MSB(nextPawn, yourStormingPawn);
                    else
                        LSB(nextPawn, yourStormingPawn);
                    yourDist = abs(RANK(nextPawn) - kr);
                }

                if (yourDist > 4)   // long way for the pawn so we don't care... maybe extend later
                    continue;

                U64 myProtectingPawn = piece00[WPAWN | me] & mask;
                int myDist = 7;
                if (myProtectingPawn)
                {
                    int nextPawn;
                    if (me)
                        MSB(nextPawn, myProtectingPawn);
                    else
                        LSB(nextPawn, myProtectingPawn);
                    myDist = abs(RANK(nextPawn) - kr);
                }

                bool isBlocked = (myDist != 7 && (myDist == yourDist - 1));
                if (isBlocked)
                    entryptr->value += EVAL(eps.ePawnstormblocked[BORDERDIST(f)][yourDist], S2MSIGN(me));
                else
                    entryptr->value += EVAL(eps.ePawnstormfree[BORDERDIST(f)][yourDist], S2MSIGN(me));
            }
        }
    }

    for (int s = 0; s < 2; s++)
    {
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
    U64 occupied = occupied00[0] | occupied00[1];
    memset(attackedBy, 0, sizeof(attackedBy));

    int result = EVAL(eps.eTempo, S2MSIGN(state & S2MMASK));

    result += getPawnAndKingValue<Et>(&phentry);

    attackedBy[0][KING] = king_attacks[kingpos[0]];
    attackedBy2[0] = phentry->attackedBy2[0] | (attackedBy[0][KING] & phentry->attacked[0]);
    attackedBy[0][0] = attackedBy[0][KING] | phentry->attacked[0];
    attackedBy[1][KING] = king_attacks[kingpos[1]];
    attackedBy2[1] = phentry->attackedBy2[1] | (attackedBy[1][KING] & phentry->attacked[1]);
    attackedBy[1][0] = attackedBy[1][KING] | phentry->attacked[1];

    kingattackers[0] = POPCOUNT(attackedBy[0][PAWN] & kingdangerMask[kingpos[1]][1]);
    kingattackers[1] = POPCOUNT(attackedBy[1][PAWN] & kingdangerMask[kingpos[0]][0]);

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

            // update attack bitboard
            attackedBy[me][p] |= attack;
            attackedBy2[me] |= (attackedBy[me][0] & attack);
            attackedBy[me][0] |= attack;

            // mobility bonus
            result += EVAL(eps.eMobilitybonus[p - 2][POPCOUNT(mobility)], S2MSIGN(me));

            // king danger
            if (mobility & kingdangerarea)
            {
                kingattackpiececount[me][p] += POPCOUNT(mobility & kingdangerarea);
                kingattackers[me]++;
            }
        }
    }
    attackedBy[0][PAWN] = phentry->attacked[0];
    attackedBy[1][PAWN] = phentry->attacked[1];

    // bonus for double bishop
    result += EVAL(eps.eDoublebishopbonus, ((POPCOUNT(piece00[WBISHOP]) >= 2) - (POPCOUNT(piece00[BBISHOP]) >= 2)));

    for (int me = 0; me < 2; me++)
    {
        int you = me ^ S2MMASK;

        // Passed pawns
        U64 ppb;
        ppb = phentry->passedpawnbb[me];
        while (LSB(index, ppb))
        {
            int target = index + (me ? -8 : 8);
            bool targetoccupied = (occupied & BITSET(target));
            bool targetsafe = (~attackedBy[you][0] & BITSET(target));
            result += EVAL(eps.ePassedpawnbonus[(targetsafe << 1) + targetoccupied][RRANK(index, me)], S2MSIGN(me));
            ppb ^= BITSET(index);
        }


        // King safety
        if (kingattackers[me] > 1 - (bool)(piece00[WQUEEN | me]))
        {
            // Attacks to our king ring
            for (int p = KNIGHT; p <= QUEEN; p++)
                result += EVAL(eps.eKingattackweight[p], S2MSIGN(me) * kingattackpiececount[me][p] * kingattackers[me]);

            // Attacked and poorly defended squares in our king ring
            U64 krattacked = kingdangerMask[kingpos[me]][me]
                & attackedBy[you][0]
                & ~attackedBy2[me]
                & (~attackedBy[me][0] | attackedBy[me][KING] | attackedBy[me][QUEEN]);
            
            result += EVAL(eps.eWeakkingringpenalty, S2MSIGN(me) * POPCOUNT(krattacked));
        }

        // Threats
        U64 hisNonPawns = occupied00[you] ^ piece00[WPAWN | you];
        U64 hisAttackedNonPawns = (hisNonPawns & attackedBy[me][PAWN]);
        if (hisAttackedNonPawns)
        {
            // Our safe or protected pawns
            U64 ourSafePawns = piece00[WPAWN | me] & (~attackedBy[you][0] | attackedBy[me][0]);
            U64 safeThreats = PAWNATTACK(me, ourSafePawns) & hisAttackedNonPawns;
            result += EVAL(eps.eSafepawnattackbonus, S2MSIGN(me) * POPCOUNT(safeThreats));
        }

        // Threat by pawn push
        // Get empty squares for pawn pushes
        U64 pawnPush = PAWNPUSH(me, piece00[WPAWN | me]) & ~occupied;
        pawnPush |= PAWNPUSH(me, pawnPush & RANK3(me)) & ~occupied;

        // Filter squares that are attacked by opponent pawn or by any piece and undefeated
        pawnPush &= ~attackedBy[you][PAWN] & (attackedBy[me][0] | ~attackedBy[you][0]);

        // Get opponents pieces that are attacked from these pawn pushes and not already attacked now
        U64 attackedPieces = PAWNATTACK(me, pawnPush) & occupied00[you] & ~attackedBy[me][PAWN];
        result += EVAL(eps.ePawnpushthreatbonus, S2MSIGN(me) * POPCOUNT(attackedPieces));
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

