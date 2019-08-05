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
    tuneIt = false;
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

    // kingdanger evals
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
    
    tuneIt = true;
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



// get psqt for eval tracing and tuning
void chessposition::getpsqval()
{
    te.material[0] = te.material[1] = 0;
    for (int i = 0; i < 64; i++)
    {
        PieceCode pc = mailbox[i];
        if (pc)
        {
            PieceType p = pc >> 1;
            int s2m = pc & S2MMASK;
            te.material[s2m] += EVAL(eps.eMaterialvalue[p], S2MSIGN(s2m));
            te.material[s2m] += EVAL(eps.ePsqt[p][PSQTINDEX(i, s2m)], S2MSIGN(s2m));
        }
    }
}

template <EvalType Et, int Me>
void chessposition::getPawnAndKingEval(pawnhashentry *entryptr)
{
    const bool bTrace = (Et == TRACE);
    const int You = Me ^ S2MMASK;
    const int pc = WPAWN | Me;
    int index;
    
    // kingshield safety
    entryptr->value += EVAL(eps.eKingshieldbonus, S2MSIGN(Me) * POPCOUNT(piece00[WPAWN | Me] & kingshieldMask[kingpos[Me]][Me]));
    if (bTrace) te.kingattackpower[You] += EVAL(eps.eKingshieldbonus, S2MSIGN(Me) * POPCOUNT(piece00[WPAWN | Me] & kingshieldMask[kingpos[Me]][Me]));

    entryptr->semiopen[Me] = 0xff;
    entryptr->passedpawnbb[Me] = 0ULL;
    entryptr->isolatedpawnbb[Me] = 0ULL;
    entryptr->backwardpawnbb[Me] = 0ULL;
    const U64 yourPawns = piece00[pc ^ S2MMASK];
    const U64 myPawns = piece00[pc];
    U64 pb = myPawns;
    while (pb)
    {
        index = pullLsb(&pb);
        entryptr->attackedBy2[Me] |= (entryptr->attacked[Me] & pawn_attacks_to[index][Me]);
        entryptr->attacked[Me] |= pawn_attacks_to[index][Me];
        entryptr->semiopen[Me] &= (int)(~BITSET(FILE(index)));

        U64 yourStoppers = passedPawnMask[index][Me] & yourPawns;
        if (!yourStoppers)
        {
            // passed pawn
            entryptr->passedpawnbb[Me] |= BITSET(index);
            int mykingdistance = squareDistance[index][kingpos[Me]];
            int yourkingdistance = squareDistance[index][kingpos[You]];
            entryptr->value += EVAL(eps.eKingsupportspasserbonus[mykingdistance][RRANK(index, Me)], S2MSIGN(Me));
            if (bTrace) te.pawns[Me] += EVAL(eps.eKingsupportspasserbonus[mykingdistance][RRANK(index, Me)], S2MSIGN(Me));
            entryptr->value += EVAL(eps.eKingdefendspasserpenalty[yourkingdistance][RRANK(index, Me)], S2MSIGN(Me));
            if (bTrace) te.pawns[Me] += EVAL(eps.eKingdefendspasserpenalty[yourkingdistance][RRANK(index, Me)], S2MSIGN(Me));
        }
        else {
            // test for potential passer
            U64 mySupporters = myPawns & pawn_attacks_to[index][You];
            U64 myPushsupporters = myPawns & pawn_attacks_to[PAWNPUSHINDEX(Me, index)][You];
            U64 yourAttackers = yourPawns & pawn_attacks_to[index][Me];
            U64 yourPushattackers = yourPawns & pawn_attacks_to[PAWNPUSHINDEX(Me, index)][Me];
            if ((!yourStoppers ^ yourAttackers ^ yourPushattackers))
            {
                // Lets see if we can get rid of the remaining stoppers
                if (POPCOUNT(myPushsupporters) >= POPCOUNT(yourPushattackers))
                {
                    // exchange is possible
                    entryptr->value += EVAL(eps.ePotentialpassedpawnbonus[POPCOUNT(mySupporters) >= POPCOUNT(yourAttackers)][RRANK(index, Me)], S2MSIGN(Me));
                    if (bTrace) te.pawns[Me] += EVAL(eps.ePotentialpassedpawnbonus[POPCOUNT(mySupporters) >= POPCOUNT(yourAttackers)][RRANK(index, Me)], S2MSIGN(Me));
                }
            }
        }

        if (!(myPawns & neighbourfilesMask[index]))
        {
            // isolated pawn
            entryptr->isolatedpawnbb[Me] |= BITSET(index);
        }
        else
        {
            if (pawn_attacks_to[index][Me] & yourPawns)
            {
                entryptr->value += EVAL(eps.eAttackingpawnbonus[RRANK(index, Me)], S2MSIGN(Me));
                if (bTrace) te.pawns[Me] += EVAL(eps.eAttackingpawnbonus[RRANK(index, Me)], S2MSIGN(Me));
            }
            U64 supporting = myPawns & pawn_attacks_to[index][You];
            U64 phalanx = myPawns & phalanxMask[index];
            if (supporting || phalanx)
            {
                entryptr->value += EVAL(eps.eConnectedbonus[RRANK(index, Me) - 1][POPCOUNT(supporting) * 2 + (bool)phalanx], S2MSIGN(Me));
                if (bTrace) te.pawns[Me] += EVAL(eps.eConnectedbonus[RRANK(index, Me) - 1][POPCOUNT(supporting) * 2 + (bool)phalanx], S2MSIGN(Me));
            }
            if (!((passedPawnMask[index][You] | phalanxMask[index]) & myPawns))
            {
                // test for backward pawn
                U64 mynextpawns = myPawns & neighbourfilesMask[index];
                U64 pawnstoreach = yourStoppers | mynextpawns;
                int nextpawn;
                if (Me ? GETMSB(nextpawn, pawnstoreach) : GETLSB(nextpawn, pawnstoreach))
                {
                    U64 nextpawnrank = rankMask[nextpawn];
                    U64 shiftneigbours = (Me ? nextpawnrank >> 8 : nextpawnrank << 8);
                    if ((nextpawnrank | (shiftneigbours & neighbourfilesMask[index])) & yourStoppers)
                    {
                        // backward pawn detected
                        entryptr->backwardpawnbb[Me] |= BITSET(index);
                    }
                }
            }
        }
    }

    // Pawn storm evaluation
    int ki = kingpos[Me];
    int kf = FILE(ki);
    int kr = RANK(ki);
    for (int f = max(0, kf - 1), j = ki + f - kf; f <= min(kf + 1, 7); f++, j++)
    {
        U64 mask = (filebarrierMask[j][Me] | BITSET(j)); // FIXME: Better mask would be useful
        U64 yourStormingPawn = piece00[WPAWN | You] & mask;
        int yourDist = 7;
        if (yourStormingPawn)
        {
            int nextPawn;
            if (Me)
                GETMSB(nextPawn, yourStormingPawn);
            else
                GETLSB(nextPawn, yourStormingPawn);
            yourDist = abs(RANK(nextPawn) - kr);
        }

        if (yourDist > 4)   // long way for the pawn so we don't care... maybe extend later
            continue;

        U64 myProtectingPawn = piece00[WPAWN | Me] & mask;
        int myDist = 7;
        if (myProtectingPawn)
        {
            int nextPawn;
            if (Me)
                GETMSB(nextPawn, myProtectingPawn);
            else
                GETLSB(nextPawn, myProtectingPawn);
            myDist = abs(RANK(nextPawn) - kr);
        }

        bool isBlocked = (myDist != 7 && (myDist == yourDist - 1));
        if (isBlocked) {
            entryptr->value += EVAL(eps.ePawnstormblocked[BORDERDIST(f)][yourDist], S2MSIGN(Me));
            if (bTrace) te.kingattackpower[Me] += EVAL(eps.ePawnstormblocked[BORDERDIST(f)][yourDist], S2MSIGN(Me));
        }
        else {
            entryptr->value += EVAL(eps.ePawnstormfree[BORDERDIST(f)][yourDist], S2MSIGN(Me));
            if (bTrace) te.kingattackpower[Me] += EVAL(eps.ePawnstormfree[BORDERDIST(f)][yourDist], S2MSIGN(Me));
        }
    }
}


template <EvalType Et, PieceType Pt, int Me>
int chessposition::getPieceEval(positioneval *pe)
{
    const bool bTrace = (Et == TRACE);
    const int You = Me ^ S2MMASK;
    int result = 0;
    const int pc = Pt * 2 + Me;
    U64 pb = piece00[pc];
    int index;

    while (pb)
    {
        index = pullLsb(&pb);
        U64 attack = 0ULL;
        if (Pt == ROOK || Pt == QUEEN)
        {
            U64 occupied = occupied00[0] | occupied00[1];
            U64 xrayrookoccupied = occupied ^ (piece00[WROOK + Me] | piece00[WQUEEN + Me]);
            attack = mRookAttacks[index][MAGICROOKINDEX(xrayrookoccupied, index)];

            // extrabonus for rook on (semi-)open file  
            if (Pt == ROOK && (pe->phentry->semiopen[Me] & BITSET(FILE(index)))) {
                result += EVAL(eps.eSlideronfreefilebonus[bool(pe->phentry->semiopen[You] & BITSET(FILE(index)))], S2MSIGN(Me));
                if (bTrace) te.rooks[Me] += EVAL(eps.eSlideronfreefilebonus[bool(pe->phentry->semiopen[You] & BITSET(FILE(index)))], S2MSIGN(Me));
            }
        }

        if (Pt == BISHOP || Pt == QUEEN)
        {
            U64 occupied = occupied00[0] | occupied00[1];
            U64 xraybishopoccupied = occupied ^ (piece00[WBISHOP + Me] | piece00[WQUEEN + Me]);
            attack |= mBishopAttacks[index][MAGICBISHOPINDEX(xraybishopoccupied, index)];
        }

        if (Pt == KNIGHT)
            attack = knight_attacks[index];

        // update attack bitboard
        attackedBy[Me][Pt] |= attack;
        attackedBy2[Me] |= (attackedBy[Me][0] & attack);
        attackedBy[Me][0] |= attack;

        // mobility bonus
        U64 goodMobility = ~((piece00[WPAWN + Me] & (RANK2(Me) | RANK3(Me))) | attackedBy[You][PAWN] | piece00[WKING + Me]);
        U64 mobility = attack & goodMobility;

        // Penalty for a piece pinned in front of the king
        if (kingPinned[Me] & (BITSET(index)))
        {
            result += EVAL(eps.eKingpinpenalty[Pt], S2MSIGN(Me));
            if (bTrace) te.mobility[Me] += EVAL(eps.eKingpinpenalty[Pt], S2MSIGN(Me));
        }
        else {
            // Piece is not pinned; give him some mobility bonus
            result += EVAL(eps.eMobilitybonus[Pt - 2][POPCOUNT(mobility)], S2MSIGN(Me));
            if (bTrace) te.mobility[Me] += EVAL(eps.eMobilitybonus[Pt - 2][POPCOUNT(mobility)], S2MSIGN(Me));
        }

        // king danger
        U64 kingdangerarea = kingdangerMask[kingpos[You]][You];
        if (mobility & kingdangerarea)
        {
            pe->kingringattacks[Me] += POPCOUNT(mobility & kingdangerarea);
            pe->kingattackpiececount[Me][Pt] += POPCOUNT(mobility & kingdangerarea);
            pe->kingattackers[Me]++;
        }
    }
    return result;
}


template <EvalType Et, int Me>
int chessposition::getLateEval(positioneval *pe)
{
    const bool bTrace = (Et == TRACE);
    const int You = Me ^ S2MMASK;
    int index;
    int result = 0;
    U64 occupied = occupied00[0] | occupied00[1];

    // Passed pawns
    U64 ppb;
    ppb = pe->phentry->passedpawnbb[Me];
    while (ppb)
    {
        index = pullLsb(&ppb);
        int target = index + (Me ? -8 : 8);
        bool targetoccupied = (occupied & BITSET(target));
        bool targetsafe = (~attackedBy[You][0] & BITSET(target));
        result += EVAL(eps.ePassedpawnbonus[(targetsafe << 1) + targetoccupied][RRANK(index, Me)], S2MSIGN(Me));
        if (bTrace) te.ppawns[Me] += EVAL(eps.ePassedpawnbonus[(targetsafe << 1) + targetoccupied][RRANK(index, Me)], S2MSIGN(Me));
    }

    // King safety; calculate the danger for my king
    int kingdanger = SQEVAL(eps.eKingdangeradjust, 1, You);
    kingdanger += SQEVAL(eps.eKingringattack[POPCOUNT(kingdangerMask[kingpos[Me]][Me]) - 4], pe->kingringattacks[You], You);

    // My attacked and poorly defended squares
    U64 myweaksquares = attackedBy[You][0]
        & ~attackedBy2[Me]
        & (~attackedBy[Me][0] | attackedBy[Me][KING] | attackedBy[Me][QUEEN]);

    // Your safe target squares
    U64 yoursafetargets = (~attackedBy[Me][0] | (myweaksquares & attackedBy2[You])) & ~occupied00[You];

    // penalty for weak squares in our king ring
    kingdanger += SQEVAL(eps.eWeakkingringpenalty, POPCOUNT(myweaksquares & kingdangerMask[kingpos[Me]][Me]), You);

    for (int p = KNIGHT; p <= QUEEN; p++) {
        // Attacks to our king ring
        if (pe->kingattackpiececount[You][p])
        {
            kingdanger += SQEVAL(eps.eKingattackweight[p], pe->kingattackpiececount[You][p] * pe->kingattackers[You], You);
        }

        if (movesTo(p << 1, kingpos[Me]) & attackedBy[You][p] & yoursafetargets)
        {
            // Bonus for safe checks
            kingdanger += SQEVAL(eps.eSafecheckbonus[p], 1, You);
        }

    }
    kingdanger += SQEVAL(eps.eKingdangerbyqueen, !piece00[WQUEEN | You], You);
    result += SQRESULT(kingdanger, You);
    if (bTrace) te.kingattackpower[You] += SQRESULT(kingdanger, You);

    // Threats
    U64 hisNonPawns = occupied00[You] ^ piece00[WPAWN + You];
    U64 hisAttackedNonPawns = (hisNonPawns & attackedBy[Me][PAWN]);
    if (hisAttackedNonPawns)
    {
        // Our safe or protected pawns
        U64 ourSafePawns = piece00[WPAWN | Me] & (~attackedBy[You][0] | attackedBy[Me][0]);
        U64 safeThreats = PAWNATTACK(Me, ourSafePawns) & hisAttackedNonPawns;
        result += EVAL(eps.eSafepawnattackbonus, S2MSIGN(Me) * POPCOUNT(safeThreats));
        if (bTrace) te.threats[Me] += EVAL(eps.eSafepawnattackbonus, S2MSIGN(Me) * POPCOUNT(safeThreats));
    }

    // Threat by pawn push
    // Get empty squares for pawn pushes
    U64 pawnPush = PAWNPUSH(Me, piece00[WPAWN | Me]) & ~occupied;
    pawnPush |= PAWNPUSH(Me, pawnPush & RANK3(Me)) & ~occupied;

    // Filter squares that are attacked by opponent pawn or by any piece and undefeated
    pawnPush &= ~attackedBy[You][PAWN] & (attackedBy[Me][0] | ~attackedBy[You][0]);

    // Get opponents pieces that are attacked from these pawn pushes and not already attacked now
    U64 attackedPieces = PAWNATTACK(Me, pawnPush) & occupied00[You] & ~attackedBy[Me][PAWN];
    result += EVAL(eps.ePawnpushthreatbonus, S2MSIGN(Me) * POPCOUNT(attackedPieces));
    if (bTrace) te.threats[Me] += EVAL(eps.ePawnpushthreatbonus, S2MSIGN(Me) * POPCOUNT(attackedPieces));

    // Hanging pieces
    U64 hanging = occupied00[Me] & ~attackedBy[Me][0] & attackedBy[You][0];
    result += EVAL(eps.eHangingpiecepenalty, S2MSIGN(Me) * POPCOUNT(hanging));
    if (bTrace) te.threats[You] += EVAL(eps.eHangingpiecepenalty, S2MSIGN(Me) * POPCOUNT(hanging));

    return result;
}


template <EvalType Et, int Me>
int chessposition::getGeneralEval(positioneval *pe)
{
    const bool bTrace = (Et == TRACE);
    const int You = Me ^ S2MMASK;

    // isolated pawns
    int result = EVAL(eps.eIsolatedpawnpenalty, S2MSIGN(Me) * POPCOUNT(pe->phentry->isolatedpawnbb[Me]));
    if (bTrace) te.pawns[Me] += EVAL(eps.eIsolatedpawnpenalty, S2MSIGN(Me) * POPCOUNT(pe->phentry->isolatedpawnbb[Me]));

    // doubled pawns
    result += EVAL(eps.eDoublepawnpenalty, S2MSIGN(Me) * POPCOUNT(piece00[WPAWN | Me] & (Me ? piece00[WPAWN | Me] >> 8 : piece00[WPAWN | Me] << 8)));
    if (bTrace) te.pawns[Me] += EVAL(eps.eDoublepawnpenalty, S2MSIGN(Me) * POPCOUNT(piece00[WPAWN | Me] & (Me ? piece00[WPAWN | Me] >> 8 : piece00[WPAWN | Me] << 8)));

    // backward pawns
    result += EVAL(eps.eBackwardpawnpenalty, S2MSIGN(Me) * POPCOUNT(pe->phentry->backwardpawnbb[Me]));
    if (bTrace) te.pawns[Me] += EVAL(eps.eBackwardpawnpenalty, S2MSIGN(Me) * POPCOUNT(pe->phentry->backwardpawnbb[Me]));

    attackedBy[Me][KING] = king_attacks[kingpos[Me]];
    attackedBy2[Me] = pe->phentry->attackedBy2[Me] | (attackedBy[Me][KING] & pe->phentry->attacked[Me]);
    attackedBy[Me][0] = attackedBy[Me][KING] | pe->phentry->attacked[Me];

    attackedBy[Me][PAWN] = pe->phentry->attacked[Me];

    pe->kingattackers[Me] = POPCOUNT(attackedBy[Me][PAWN] & kingdangerMask[kingpos[You]][You]);

    // bonus for double bishop
    result += EVAL(eps.eDoublebishopbonus, S2MSIGN(Me) * (POPCOUNT(piece00[WBISHOP | Me]) >= 2));
    if (bTrace) te.bishops[Me] += EVAL(eps.eDoublebishopbonus, S2MSIGN(Me) * (POPCOUNT(piece00[WBISHOP | Me]) >= 2));

    return result;
}


template <EvalType Et>
int chessposition::getEval()
{
    const bool bTrace = (Et == TRACE);
    if (bTrace) te = { 0 };
#ifdef EVALTUNE
    resetTuner();
    getpsqval();
#endif
    ph = phase();
    updatePins();
    positioneval pe;

    // reset the attackedBy information
    memset(attackedBy, 0, sizeof(attackedBy));

    bool hashexist = pwnhsh->probeHash(pawnhash, &pe.phentry);
    if (bTrace || !hashexist)
    {
        getPawnAndKingEval<Et, 0>(pe.phentry);
        getPawnAndKingEval<Et, 1>(pe.phentry);
    }

    int pawnEval = pe.phentry->value;
    int generalEval = getGeneralEval<Et, 0>(&pe) + getGeneralEval<Et, 1>(&pe);
    int piecesEval = getPieceEval<Et, KNIGHT, 0>(&pe)   + getPieceEval<Et, KNIGHT, 1>(&pe)
                    + getPieceEval<Et, BISHOP, 0>(&pe) + getPieceEval<Et, BISHOP, 1>(&pe)
                    + getPieceEval<Et, ROOK, 0>(&pe)   + getPieceEval<Et, ROOK, 1>(&pe)
                    + getPieceEval<Et, QUEEN, 0>(&pe)  + getPieceEval<Et, QUEEN, 1>(&pe);
    int lateEval = getLateEval<Et, 0>(&pe) + getLateEval<Et, 1>(&pe);

    int totalEval = psqval + pawnEval + generalEval + piecesEval + lateEval;
    totalEval += EVAL(eps.eTempo, S2MSIGN(state & S2MMASK));
    if (bTrace) te.tempo[state & S2MMASK] += EVAL(eps.eTempo, S2MSIGN(state & S2MMASK));

    int sideToScale = totalEval > SCOREDRAW ? WHITE : BLACK;
    sc = getScaling(sideToScale);

    if (!bTrace && sc == SCALE_DRAW)
        return SCOREDRAW;

    int score = TAPEREDANDSCALEDEVAL(totalEval, ph, sc);

    if (bTrace)
    {
        getpsqval();
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
            << "        Total |  Ph=" << setw(3) << ph << "/256 |  Sc=" << setw(3) << sc << "/128 | " << splitvaluestring(totalEval) << "\n"
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
template int chessposition::getEval<NOTRACE>();
template int chessposition::getEval<TRACE>();
