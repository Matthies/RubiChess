/*
  RubiChess is a UCI chess playing engine by Andreas Matthies.

  RubiChess is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  RubiChess is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "RubiChess.h"

// Evaluation stuff

// static values for the search/pruning/material stuff
const int materialvalue[7] = { 0,  100,  314,  314,  483,  913, 32509 };  // some evaluation depends on bishop value >= knight value!!!
const int maxmobility[4] = { 9, 14, 15, 28 }; // indexed by piece - 2

evalparamset eps;

#ifdef EVALTUNE

sqevallist sqglobal;

void chessposition::resetTuner()
{
    for (int i = 0; i < tps.count; i++)
        tps.ev[i]->resetGrad();
}

void chessposition::getPositionTuneSet(positiontuneset *p, evalparam *e)
{
    p->ph = ph;
    p->sc = sc;
    p->num = 0;
    for (int i = 0; i < tps.count; i++)
        if (tps.ev[i]->getGrad() || (tps.ev[i]->type == 1 && tps.ev[i]->getGrad(1)))
        {
            e->index = i;
            e->g[0] = tps.ev[i]->getGrad(0);
            e->g[1] = tps.ev[i]->getGrad(1);
            p->num++;
            e++;
        }
}

void chessposition::copyPositionTuneSet(positiontuneset *from, evalparam *efrom, positiontuneset *to, evalparam *eto)
{
    to->ph = from->ph;
    to->sc = from->sc;
    to->num = from->num;
    for (int i = 0; i < from->num; i++)
    {
        *eto = *efrom;
        eto++;
        efrom++;
    }
}

string chessposition::getGradientString()
{
    string s = "";
    for (int i = 0; i < pts.num; i++)
    {
        if (tps.ev[i]->type == 0)
            s = s + tps.name[ev[i].index] + "(" + to_string(ev[i].g[0]) + ") ";
        else
            s = s + tps.name[ev[i].index] + "(" + to_string(ev[i].g[0]) + "/" + to_string(ev[i].g[1]) + ") ";
    }

    return s;
}


static void registertuner(chessposition *pos, eval *e, string name, int index1, int bound1, int index2, int bound2, bool tune)
{
    int i = pos->tps.count;
    pos->tps.ev[i] = e;
    pos->tps.name[i] = name;
    pos->tps.index1[i] = index1;
    pos->tps.bound1[i] = bound1;
    pos->tps.index2[i] = index2;
    pos->tps.bound2[i] = bound2;
    pos->tps.tune[i] = tune;
    pos->tps.used[i] = 0;
    pos->tps.count++;
}

void registeralltuners(chessposition *pos)
{
    int i, j;
    bool tuneIt = false;

    pos->tps.count = 0;

    for (i = 0; i < 6; i++)
        registertuner(pos, &eps.eKingpinpenalty[i], "eKingpinpenalty", i, 6, 0, 0, tuneIt && (i > PAWN));
    tuneIt = false;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 5; j++)
            registertuner(pos, &eps.ePawnstormblocked[i][j], "ePawnstormblocked", j, 5, i, 4, tuneIt);
    for (i = 0; i < 4; i++)
        for (j = 0; j < 5; j++)
            registertuner(pos, &eps.ePawnstormfree[i][j], "ePawnstormfree", j, 5, i, 4, tuneIt);

    registertuner(pos, &eps.ePawnpushthreatbonus, "ePawnpushthreatbonus", 0, 0, 0, 0, tuneIt);
    registertuner(pos, &eps.eSafepawnattackbonus, "eSafepawnattackbonus", 0, 0, 0, 0, tuneIt);
    tuneIt = false;
    registertuner(pos, &eps.eHangingpiecepenalty, "eHangingpiecepenalty", 0, 0, 0, 0, tuneIt);
    registertuner(pos, &eps.eTempo, "eTempo", 0, 0, 0, 0, tuneIt);
    tuneIt = false;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 8; j++)
            registertuner(pos, &eps.ePassedpawnbonus[i][j], "ePassedpawnbonus", j, 8, i, 4, tuneIt && (j > 0 && j < 7));
    tuneIt = true;
    for (i = 0; i < 7; i++)
        for (j = 0; j < 8; j++)
            registertuner(pos, &eps.eKingsupportspasserbonus[i][j], "eKingsupportspasserbonus", j, 8, i, 7, tuneIt && (j > 0 && j < 7));
    for (i = 0; i < 7; i++)
        for (j = 0; j < 8; j++)
            registertuner(pos, &eps.eKingdefendspasserpenalty[i][j], "eKingdefendspasserpenalty", j, 8, i, 7, tuneIt && (j > 0 && j < 7));

    tuneIt = false;
    for (i = 0; i < 2; i++)
        for (j = 0; j < 8; j++)
            registertuner(pos, &eps.ePotentialpassedpawnbonus[i][j], "ePotentialpassedpawnbonus", j, 8, i, 2, tuneIt && (j > 0 && j < 7));
    tuneIt = false;
    for (i = 0; i < 8; i++)
        registertuner(pos, &eps.eAttackingpawnbonus[i], "eAttackingpawnbonus", i, 8, 0, 0, tuneIt && (i > 0 && i < 7));
    registertuner(pos, &eps.eIsolatedpawnpenalty, "eIsolatedpawnpenalty", 0, 0, 0, 0, tuneIt);
    registertuner(pos, &eps.eDoublepawnpenalty, "eDoublepawnpenalty", 0, 0, 0, 0, tuneIt);

    tuneIt = false;
    for (i = 0; i < 6; i++)
        for (j = 0; j < 5; j++)
            registertuner(pos, &eps.eConnectedbonus[i][j], "eConnectedbonus", j, 5, i, 6, tuneIt);

    registertuner(pos, &eps.eBackwardpawnpenalty, "eBackwardpawnpenalty", 0, 0, 0, 0, tuneIt);
    tuneIt = false;
    registertuner(pos, &eps.eDoublebishopbonus, "eDoublebishopbonus", 0, 0, 0, 0, tuneIt);

    tuneIt = false;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 28; j++)
            registertuner(pos, &eps.eMobilitybonus[i][j], "eMobilitybonus", j, 28, i, 4, tuneIt && (j < maxmobility[i]));

    tuneIt = false;
    for (i = 0; i < 2; i++)
        registertuner(pos, &eps.eSlideronfreefilebonus[i], "eSlideronfreefilebonus", i, 2, 0, 0, tuneIt);
    for (i = 0; i < 7; i++)
        registertuner(pos, &eps.eMaterialvalue[i], "eMaterialvalue", i, 7, 0, 0, false);
    registertuner(pos, &eps.eKingshieldbonus, "eKingshieldbonus", 0, 0, 0, 0, tuneIt);
    tuneIt = false;
    registertuner(pos, &eps.eWeakkingringpenalty, "eWeakkingringpenalty", 0, 0, 0, 0, tuneIt);
    for (i = 0; i < 7; i++)
        registertuner(pos, &eps.eKingattackweight[i], "eKingattackweight", i, 7, 0, 0, tuneIt && (i >= KNIGHT && i <= QUEEN));

    tuneIt = false;
    for (i = 0; i < 6; i++)
        registertuner(pos, &eps.eSafecheckbonus[i], "eSafecheckbonus", i, 6, 0, 0, tuneIt && (i >= KNIGHT && i <= QUEEN));
    registertuner(pos, &eps.eKingdangerbyqueen, "eKingdangerbyqueen", 0, 0, 0, 0, tuneIt);
    for (i = 0; i < 6; i++)
        registertuner(pos, &eps.eKingringattack[i], "eKingringattack", i, 6, 0, 0, tuneIt);

    registertuner(pos, &eps.eKingdangeradjust, "eKingdangeradjust", 0, 0, 0, 0, tuneIt);
    
    tuneIt = false;
    for (i = 0; i < 7; i++)
        for (j = 0; j < 64; j++)
            registertuner(pos, &eps.ePsqt[i][j], "ePsqt", j, 64, i, 7, tuneIt && (i >= KNIGHT || (i == PAWN && j >= 8 && j < 56)));
}
#endif

struct traceeval {
    int rooks[2];
    int bishops[2];
    int material[2];
    int pawns[2];
    int mobility[2];
    int kingattackpower[2];
    int threats[2];
    int ppawns[2];
    int tempo[2];
};

traceeval te;

static double cp(int v) { return (v / 100.0); }

static string splitvaluestring(int v)
{
    stringstream ss;
    ss << showpoint << noshowpos << fixed << setprecision(2)
        << setw(5) << cp(GETMGVAL(v)) << " " << setw(5) << cp(GETEGVAL(v));
    return ss.str();
}

static string splitvaluestring(int v[])
{
    stringstream ss;
    ss << splitvaluestring(v[0]) << " | " << splitvaluestring(-v[1]) << " | " << splitvaluestring(v[0] + v[1]) << "\n";
    return ss.str();
}

template <EvalType Et>
int chessposition::getPawnAndKingValue(pawnhashentry **entry)
{
    const bool bTrace = (Et == TRACE);
    int val = 0;
    int index;

    bool hashexist = pwnhsh->probeHash(pawnhash, entry);
    pawnhashentry *entryptr = *entry;
    if (bTrace || !hashexist)
    {
        entryptr->value = EVAL(eps.ePsqt[KING][PSQTINDEX(kingpos[0], 0)], 1)
            + EVAL(eps.ePsqt[KING][PSQTINDEX(kingpos[1], 1)], -1)
            + EVAL(eps.eMaterialvalue[PAWN], POPCOUNT(piece00[WPAWN]) - POPCOUNT(piece00[BPAWN]));
        if (bTrace) {
            te.material[0] += EVAL(eps.ePsqt[KING][PSQTINDEX(kingpos[0], 0)], 1) + EVAL(eps.eMaterialvalue[PAWN], POPCOUNT(piece00[WPAWN]));
            te.material[1] += EVAL(eps.ePsqt[KING][PSQTINDEX(kingpos[1], 1)], -1) + EVAL(eps.eMaterialvalue[PAWN], -POPCOUNT(piece00[BPAWN]));
        }

        // kingshield safety
        entryptr->value += EVAL(eps.eKingshieldbonus, POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]) - POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1]));
        if (bTrace) {
            te.kingattackpower[0] += EVAL(eps.eKingshieldbonus, -POPCOUNT(piece00[BPAWN] & kingshieldMask[kingpos[1]][1]));
            te.kingattackpower[1] += EVAL(eps.eKingshieldbonus, POPCOUNT(piece00[WPAWN] & kingshieldMask[kingpos[0]][0]));
        }
        for (int pc = WPAWN; pc <= BPAWN; pc++)
        {
            int me = pc & S2MMASK;
            int you = me ^ S2MMASK;
            entryptr->semiopen[me] = 0xff;
            entryptr->passedpawnbb[me] = 0ULL;
            entryptr->isolatedpawnbb[me] = 0ULL;
            entryptr->backwardpawnbb[me] = 0ULL;
            U64 yourPawns = piece00[pc ^ S2MMASK];
            U64 myPawns = piece00[pc];
            U64 pb = myPawns;
            while (pb)
            {
                index = pullLsb(&pb);
                int psqtindex = PSQTINDEX(index, me);
                entryptr->value += EVAL(eps.ePsqt[PAWN][psqtindex], S2MSIGN(me));
                if (bTrace) te.material[me] += EVAL(eps.ePsqt[PAWN][psqtindex], S2MSIGN(me));

                entryptr->attackedBy2[me] |= (entryptr->attacked[me] & pawn_attacks_to[index][me]);
                entryptr->attacked[me] |= pawn_attacks_to[index][me];
                entryptr->semiopen[me] &= (int)(~BITSET(FILE(index)));

                U64 yourStoppers = passedPawnMask[index][me] & yourPawns;
                if (!yourStoppers)
                {
                    // passed pawn
                    entryptr->passedpawnbb[me] |= BITSET(index);
                    int mykingdistance = squareDistance[index][kingpos[me]];
                    int yourkingdistance = squareDistance[index][kingpos[you]];
                    entryptr->value += EVAL(eps.eKingsupportspasserbonus[mykingdistance][RRANK(index, me)], S2MSIGN(me));
                    if (bTrace) te.pawns[me] += EVAL(eps.eKingsupportspasserbonus[mykingdistance][RRANK(index, me)], S2MSIGN(me));
                    entryptr->value += EVAL(eps.eKingdefendspasserpenalty[yourkingdistance][RRANK(index, me)], S2MSIGN(me));
                    if (bTrace) te.pawns[me] += EVAL(eps.eKingdefendspasserpenalty[yourkingdistance][RRANK(index, me)], S2MSIGN(me));
                }
                else {
                    // test for potential passer
                    U64 mySupporters = myPawns & pawn_attacks_to[index][you];
                    U64 myPushsupporters = myPawns & pawn_attacks_to[PAWNPUSHINDEX(me, index)][you];
                    U64 yourAttackers = yourPawns & pawn_attacks_to[index][me];
                    U64 yourPushattackers = yourPawns & pawn_attacks_to[PAWNPUSHINDEX(me, index)][me];
                    if ((!yourStoppers ^ yourAttackers ^ yourPushattackers))
                    {
                        // Lets see if we can get rid of the remaining stoppers
                        if (POPCOUNT(myPushsupporters) >= POPCOUNT(yourPushattackers))
                        {
                            // exchange is possible
                            entryptr->value += EVAL(eps.ePotentialpassedpawnbonus[POPCOUNT(mySupporters) >= POPCOUNT(yourAttackers)][RRANK(index, me)], S2MSIGN(me));
                            if (bTrace) te.pawns[me] += EVAL(eps.ePotentialpassedpawnbonus[POPCOUNT(mySupporters) >= POPCOUNT(yourAttackers)][RRANK(index, me)], S2MSIGN(me));
                        }
                    }

                }

                if (!(myPawns & neighbourfilesMask[index]))
                {
                    // isolated pawn
                    entryptr->isolatedpawnbb[me] |= BITSET(index);
                }
                else
                {
                    if (pawn_attacks_to[index][me] & yourPawns)
                    {
                        entryptr->value += EVAL(eps.eAttackingpawnbonus[RRANK(index, me)], S2MSIGN(me));
                        if (bTrace) te.pawns[me] += EVAL(eps.eAttackingpawnbonus[RRANK(index, me)], S2MSIGN(me));
                    }
                    U64 supporting = myPawns & pawn_attacks_to[index][you];
                    U64 phalanx = myPawns & phalanxMask[index];
                    if (supporting || phalanx)
                    {
                        entryptr->value += EVAL(eps.eConnectedbonus[RRANK(index, me) - 1][POPCOUNT(supporting) * 2 + (bool)phalanx], S2MSIGN(me));
                        if (bTrace) te.pawns[me] += EVAL(eps.eConnectedbonus[RRANK(index, me) - 1][POPCOUNT(supporting) * 2 + (bool)phalanx], S2MSIGN(me));
                    }
                    if (!((passedPawnMask[index][you] | phalanxMask[index]) & myPawns))
                    {
                        // test for backward pawn
                        U64 mynextpawns = myPawns & neighbourfilesMask[index];
                        U64 pawnstoreach = yourStoppers | mynextpawns;
                        int nextpawn;
                        if (me ? GETMSB(nextpawn, pawnstoreach) : GETLSB(nextpawn, pawnstoreach))
                        {
                            U64 nextpawnrank = rankMask[nextpawn];
                            U64 shiftneigbours = (me ? nextpawnrank >> 8 : nextpawnrank << 8);
                            if ((nextpawnrank | (shiftneigbours & neighbourfilesMask[index])) & yourStoppers)
                            {
                                // backward pawn detected
                                entryptr->backwardpawnbb[me] |= BITSET(index);
                            }
                        }
                    }
                }
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
                        GETMSB(nextPawn, yourStormingPawn);
                    else
                        GETLSB(nextPawn, yourStormingPawn);
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
                        GETMSB(nextPawn, myProtectingPawn);
                    else
                        GETLSB(nextPawn, myProtectingPawn);
                    myDist = abs(RANK(nextPawn) - kr);
                }

                bool isBlocked = (myDist != 7 && (myDist == yourDist - 1));
                if (isBlocked) {
                    entryptr->value += EVAL(eps.ePawnstormblocked[BORDERDIST(f)][yourDist], S2MSIGN(me));
                    if (bTrace) te.kingattackpower[me] += EVAL(eps.ePawnstormblocked[BORDERDIST(f)][yourDist], S2MSIGN(me));
                }
                else {
                    entryptr->value += EVAL(eps.ePawnstormfree[BORDERDIST(f)][yourDist], S2MSIGN(me));
                    if (bTrace) te.kingattackpower[me] += EVAL(eps.ePawnstormfree[BORDERDIST(f)][yourDist], S2MSIGN(me));
                }
            }
        }
    }

    for (int s = 0; s < 2; s++)
    {
        // isolated pawns
        val += EVAL(eps.eIsolatedpawnpenalty, S2MSIGN(s) * POPCOUNT(entryptr->isolatedpawnbb[s]));
        if (bTrace) te.pawns[s] += EVAL(eps.eIsolatedpawnpenalty, S2MSIGN(s) * POPCOUNT(entryptr->isolatedpawnbb[s]));

        // doubled pawns
        val += EVAL(eps.eDoublepawnpenalty, S2MSIGN(s) * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8)));
        if (bTrace) te.pawns[s] += EVAL(eps.eDoublepawnpenalty, S2MSIGN(s) * POPCOUNT(piece00[WPAWN | s] & (s ? piece00[WPAWN | s] >> 8 : piece00[WPAWN | s] << 8)));

        // backward pawns
        val += EVAL(eps.eBackwardpawnpenalty, S2MSIGN(s) * POPCOUNT(entryptr->backwardpawnbb[s]));
        if (bTrace) te.pawns[s] += EVAL(eps.eBackwardpawnpenalty, S2MSIGN(s) * POPCOUNT(entryptr->backwardpawnbb[s]));
    }

    return val + entryptr->value;
}


template <EvalType Et>
int chessposition::getPositionValue()
{
    const bool bTrace = (Et == TRACE);
    pawnhashentry *phentry;
    int index;
    int kingattackpiececount[2][7] = { 0 };
    int kingattackers[2];
    int kingringattacks[2] = { 0 };
    U64 occupied = occupied00[0] | occupied00[1];
    memset(attackedBy, 0, sizeof(attackedBy));

    int result = EVAL(eps.eTempo, S2MSIGN(state & S2MMASK));
    if (bTrace) te.tempo[state & S2MMASK] += EVAL(eps.eTempo, S2MSIGN(state & S2MMASK));

    result += getPawnAndKingValue<Et>(&phentry);

    attackedBy[0][KING] = king_attacks[kingpos[0]];
    attackedBy2[0] = phentry->attackedBy2[0] | (attackedBy[0][KING] & phentry->attacked[0]);
    attackedBy[0][0] = attackedBy[0][KING] | phentry->attacked[0];
    attackedBy[1][KING] = king_attacks[kingpos[1]];
    attackedBy2[1] = phentry->attackedBy2[1] | (attackedBy[1][KING] & phentry->attacked[1]);
    attackedBy[1][0] = attackedBy[1][KING] | phentry->attacked[1];

    attackedBy[0][PAWN] = phentry->attacked[0];
    attackedBy[1][PAWN] = phentry->attacked[1];

    kingattackers[0] = POPCOUNT(attackedBy[0][PAWN] & kingdangerMask[kingpos[1]][1]);
    kingattackers[1] = POPCOUNT(attackedBy[1][PAWN] & kingdangerMask[kingpos[0]][0]);

    U64 goodMobility[2];
    goodMobility[0] = ~((piece00[WPAWN] & (RANK2(0) | RANK3(0))) | attackedBy[1][PAWN] | piece00[WKING]);
    goodMobility[1] = ~((piece00[BPAWN] & (RANK2(1) | RANK3(1))) | attackedBy[0][PAWN] | piece00[BKING]);

    U64 kingdangerarea[2] = { kingdangerMask[kingpos[0]][0], kingdangerMask[kingpos[1]][1] };

    U64 xrayrookoccupied[2] = { occupied ^ (piece00[WROOK] | piece00[WQUEEN]), occupied ^ (piece00[BROOK] | piece00[BQUEEN]) };
    U64 xraybishopoccupied[2] = { occupied ^ (piece00[WBISHOP] | piece00[WQUEEN]), occupied ^ (piece00[BBISHOP] | piece00[BQUEEN]) };

    for (int pc = WKNIGHT; pc <= BQUEEN; pc++)
    {
        int p = (pc >> 1);
        int me = pc & S2MMASK;
        int you = me ^ S2MMASK;
        U64 pb = piece00[pc];

        while (pb)
        {
            index = pullLsb(&pb);
            int psqtindex = PSQTINDEX(index, me);
            result += EVAL(eps.ePsqt[p][psqtindex], S2MSIGN(me));
            result += EVAL(eps.eMaterialvalue[p], S2MSIGN(me));
            if (bTrace) te.material[me] += EVAL(eps.ePsqt[p][psqtindex], S2MSIGN(me)) + EVAL(eps.eMaterialvalue[p], S2MSIGN(me));

            U64 attack = 0ULL;
            if (shifting[p] & 0x2) // rook and queen
            {
                attack = mRookAttacks[index][MAGICROOKINDEX(xrayrookoccupied[me], index)];

                // extrabonus for rook on (semi-)open file  
                if (p == ROOK && (phentry->semiopen[me] & BITSET(FILE(index)))) {
                    result += EVAL(eps.eSlideronfreefilebonus[bool(phentry->semiopen[you] & BITSET(FILE(index)))], S2MSIGN(me));
                    if (bTrace) te.rooks[me] += EVAL(eps.eSlideronfreefilebonus[bool(phentry->semiopen[you] & BITSET(FILE(index)))], S2MSIGN(me));
                }
            }

            if (shifting[p] & 0x1) // bishop and queen)
                attack |= mBishopAttacks[index][MAGICBISHOPINDEX(xraybishopoccupied[me], index)];

            if (p == KNIGHT)
                attack = knight_attacks[index];

            // update attack bitboard
            attackedBy[me][p] |= attack;
            attackedBy2[me] |= (attackedBy[me][0] & attack);
            attackedBy[me][0] |= attack;

            // mobility bonus
            U64 mobility = attack & goodMobility[me];

            // Penalty for a piece pinned in front of the king
            if (kingPinned[me] & (BITSET(index)))
            {
                result += EVAL(eps.eKingpinpenalty[p], S2MSIGN(me));
                if (bTrace) te.mobility[me] += EVAL(eps.eKingpinpenalty[p], S2MSIGN(me));
            }
            else {
                // Piece is not pinned; give him some mobility bonus
                result += EVAL(eps.eMobilitybonus[p - 2][POPCOUNT(mobility)], S2MSIGN(me));
                if (bTrace) te.mobility[me] += EVAL(eps.eMobilitybonus[p - 2][POPCOUNT(mobility)], S2MSIGN(me));
            }

            // king danger
            if (mobility & kingdangerarea[you])
            {
                kingringattacks[me] += POPCOUNT(mobility & kingdangerarea[you]);
                kingattackpiececount[me][p] += POPCOUNT(mobility & kingdangerarea[you]);
                kingattackers[me]++;
            }
        }
    }

    // bonus for double bishop
    result += EVAL(eps.eDoublebishopbonus, ((POPCOUNT(piece00[WBISHOP]) >= 2) - (POPCOUNT(piece00[BBISHOP]) >= 2)));
    if (bTrace) {
        te.bishops[0] += EVAL(eps.eDoublebishopbonus, (POPCOUNT(piece00[WBISHOP]) >= 2));
        te.bishops[1] += EVAL(eps.eDoublebishopbonus, -(POPCOUNT(piece00[BBISHOP]) >= 2));
    }

    for (int me = 0; me < 2; me++)
    {
        int you = me ^ S2MMASK;

        // Passed pawns
        U64 ppb;
        ppb = phentry->passedpawnbb[me];
        while (ppb)
        {
            index = pullLsb(&ppb);
            int target = index + (me ? -8 : 8);
            bool targetoccupied = (occupied & BITSET(target));
            bool targetsafe = (~attackedBy[you][0] & BITSET(target));
            result += EVAL(eps.ePassedpawnbonus[(targetsafe << 1) + targetoccupied][RRANK(index, me)], S2MSIGN(me));
            if (bTrace) te.ppawns[me] += EVAL(eps.ePassedpawnbonus[(targetsafe << 1) + targetoccupied][RRANK(index, me)], S2MSIGN(me));
        }

        // King safety; calculate the danger for my king
        int kingdanger = SQEVAL(eps.eKingdangeradjust, 1, you);
        kingdanger += SQEVAL(eps.eKingringattack[POPCOUNT(kingdangerarea[me]) - 4], kingringattacks[you], you);

        // My attacked and poorly defended squares
        U64 myweaksquares = attackedBy[you][0]
            & ~attackedBy2[me]
            & (~attackedBy[me][0] | attackedBy[me][KING] | attackedBy[me][QUEEN]);

        // Your safe target squares
        U64 yoursafetargets = (~attackedBy[me][0] | (myweaksquares & attackedBy2[you])) & ~occupied00[you];

        // penalty for weak squares in our king ring
        kingdanger += SQEVAL(eps.eWeakkingringpenalty, POPCOUNT(myweaksquares & kingdangerMask[kingpos[me]][me]), you);

        for (int p = KNIGHT; p <= QUEEN; p++) {
            // Attacks to our king ring
            if (kingattackpiececount[you][p])
            {
                kingdanger += SQEVAL(eps.eKingattackweight[p], kingattackpiececount[you][p] * kingattackers[you], you);
            }

            if (movesTo(p << 1, kingpos[me]) & attackedBy[you][p] & yoursafetargets)
            {
                // Bonus for safe checks
                kingdanger += SQEVAL(eps.eSafecheckbonus[p], 1, you);
            }

        }
        kingdanger += SQEVAL(eps.eKingdangerbyqueen, !piece00[WQUEEN | you], you);
        result += SQRESULT(kingdanger, you);
        if (bTrace) te.kingattackpower[you] += SQRESULT(kingdanger, you);

        // Threats
        U64 hisNonPawns = occupied00[you] ^ piece00[WPAWN | you];
        U64 hisAttackedNonPawns = (hisNonPawns & attackedBy[me][PAWN]);
        if (hisAttackedNonPawns)
        {
            // Our safe or protected pawns
            U64 ourSafePawns = piece00[WPAWN | me] & (~attackedBy[you][0] | attackedBy[me][0]);
            U64 safeThreats = PAWNATTACK(me, ourSafePawns) & hisAttackedNonPawns;
            result += EVAL(eps.eSafepawnattackbonus, S2MSIGN(me) * POPCOUNT(safeThreats));
            if (bTrace) te.threats[me] += EVAL(eps.eSafepawnattackbonus, S2MSIGN(me) * POPCOUNT(safeThreats));
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
        if (bTrace) te.threats[me] += EVAL(eps.ePawnpushthreatbonus, S2MSIGN(me) * POPCOUNT(attackedPieces));

        // Hanging pieces
        U64 hanging = occupied00[me] & ~attackedBy[me][0] & attackedBy[you][0];
        result += EVAL(eps.eHangingpiecepenalty, S2MSIGN(me) * POPCOUNT(hanging));
        if (bTrace) te.threats[you] += EVAL(eps.eHangingpiecepenalty, S2MSIGN(me) * POPCOUNT(hanging));
    }

    return result;
}


template <EvalType Et>
int chessposition::getValue()
{
    const bool bTrace = (Et == TRACE);
    if (bTrace) te = { 0 };
#ifdef EVALTUNE
    resetTuner();
#endif
    ph = phase();
    updatePins();
    int positionscore = getPositionValue<Et>();
    int sideToScale = positionscore > SCOREDRAW ? WHITE : BLACK;
    sc = getScaling(sideToScale);

    if (!bTrace && sc == SCALE_DRAW)
        return SCOREDRAW;

    int score = TAPEREDANDSCALEDEVAL(positionscore, ph, sc);

    if (bTrace)
    {
        stringstream ss;
        ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2)
            << "              |    White    |    Black    |    Total   \n"
            << "              |   MG    EG  |   MG    EG  |   MG    EG \n"
            << " -------------+-------------+-------------+------------\n"
            << "     Material | " << splitvaluestring(te.material)
            << "      Bishops | " << splitvaluestring(te.bishops)
            << "        Rooks | " << splitvaluestring(te.rooks)
            << "        Pawns | " << splitvaluestring(te.pawns)
            << "      Passers | " << splitvaluestring(te.ppawns)
            << "     Mobility | " << splitvaluestring(te.mobility)
            << "      Threats | " << splitvaluestring(te.threats)
            << " King attacks | " << splitvaluestring(te.kingattackpower)
            << "        Tempo | " << splitvaluestring(te.tempo)
            << " -------------+-------------+-------------+------------\n"
            << "        Total |  Ph=" << setw(3) << ph << "/256 |  Sc=" << setw(3) << sc << "/128 | " << splitvaluestring(positionscore) << "\n"
            << "\nResulting score: " << cp(score) << "\n";

        cout << ss.str();
    }

    return score;
}




int chessposition::getScaling(int col)
{
    Materialhashentry* e;
    if (mh.probeHash(materialhash, &e))
        return e->scale[col];

    // Calculate scaling for endgames with special material
    const int pawns[2] = { POPCOUNT(piece00[WPAWN]), POPCOUNT(piece00[BPAWN]) };
    const int knights[2] = { POPCOUNT(piece00[WKNIGHT]), POPCOUNT(piece00[BKNIGHT]) };
    const int bishops[2] = { POPCOUNT(piece00[WBISHOP]), POPCOUNT(piece00[BBISHOP]) };
    const int rooks[2] = { POPCOUNT(piece00[WROOK]), POPCOUNT(piece00[BROOK]) };
    const int queens[2] = { POPCOUNT(piece00[WQUEEN]), POPCOUNT(piece00[BQUEEN]) };
    const int nonpawnvalue[2] = {
        knights[WHITE] * materialvalue[KNIGHT]
        + bishops[WHITE] * materialvalue[BISHOP]
        + rooks[WHITE] * materialvalue[ROOK]
        + queens[WHITE] * materialvalue[QUEEN],
        knights[BLACK] * materialvalue[KNIGHT]
        + bishops[BLACK] * materialvalue[BISHOP]
        + rooks[BLACK] * materialvalue[ROOK]
        + queens[BLACK] * materialvalue[QUEEN]
    };

    // Check for insufficient material using simnple heuristic from chessprogramming site
    for (int me = WHITE; me <= BLACK; me++)
    {
        int you = me ^ S2MMASK;
        
        if (pawns[me] == 0 && nonpawnvalue[me] - nonpawnvalue[you] <= materialvalue[BISHOP])
            e->scale[me] = nonpawnvalue[me] < materialvalue[ROOK] ? SCALE_DRAW : SCALE_HARDTOWIN;

        if (pawns[me] == 1 && nonpawnvalue[me] - nonpawnvalue[you] <= materialvalue[BISHOP])
            e->scale[me] = SCALE_ONEPAWN;
    }

    return e->scale[col];
}

// Explicit template instantiation
// This avoids putting these definitions in header file
template int chessposition::getValue<NOTRACE>();
template int chessposition::getValue<TRACE>();
