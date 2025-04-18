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

using namespace rubichess;

namespace rubichess {

// Evaluation stuff

// static values for the search/pruning/material stuff
const int materialvalue[7] = { 0,  100,  314,  314,  483,  913, 32509 };  // some evaluation depends on bishop value >= knight value!!!
eval NnuePsqThreshold = 760;
eval FrcCorneredBishopPenalty = 25;

void initPsqtable()
{
    // initialize psqtable for faster evaluation
    for (int from = 0; from < 64; from++)
    {
        for (int pc = 0; pc <= BKING; pc++)
        {
            int p = pc >> 1;
            int s2m = pc & S2MMASK;
            psqtable[pc][from] = S2MSIGN(s2m) * (eps.eMaterialvalue[p] + eps.ePsqt[p][PSQTINDEX(from, s2m)]);
        }
    }
}



#ifdef EVALTUNE


static void registerfortuning(chessposition *pos, eval *e, string name, int index1, int bound1, int index2, int bound2, bool tune)
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
#endif

#ifdef EVALOPTIONS

static void registerforoptions(eval *e, string name, int index1, int bound1, int index2, int bound2)
{
    ostringstream osName;
    size_t maxdig1 = bound1 > 0 ? to_string(bound1 - 1).length() : 0;
    size_t maxdig2 = bound2 > 0 ? to_string(bound2 - 1).length() : 0;
    osName << name;
    if (bound2 > 0)
    {
        osName << "_" << setw(maxdig2) << setfill('0') << to_string(index2) << "_" << setw(maxdig1) << setfill('0') << to_string(index1);
    }
    else if (bound1 > 0)
    {
        osName << "_" << setw(maxdig1) << setfill('0') << to_string(index1);
    }

    string sDef =  to_string(GETMGVAL(*e));
    en.ucioptions.Register((void*)e, osName.str() + "_mg", ucieval, sDef, 0, 0, initPsqtable);
    sDef = to_string(GETEGVAL(*e));
    en.ucioptions.Register((void*)e, osName.str() + "_eg", ucieval, sDef, 0, 0, initPsqtable);
    en.ucioptions.Register(&FrcCorneredBishopPenalty, "FrcCorneredBishopPenalty", ucispin, to_string(FrcCorneredBishopPenalty), 0, SCOREWHITEWINS, nullptr);
    en.ucioptions.Register(&NnuePsqThreshold, "NnuePsqThreshold", ucinnuebias, to_string(NnuePsqThreshold), 0, SCOREWHITEWINS, nullptr);
}
#endif

#if defined(EVALTUNE) || defined(EVALOPTIONS)

static void registertuner(chessposition* pos, eval* e, string name, int index1, int bound1, int index2, int bound2, bool tune)
{
#ifdef EVALTUNE
    if (pos)
        registerfortuning(pos, e, name, index1, bound1, index2, bound2, tune);
#endif

#ifdef EVALOPTIONS
    (void) pos;
    (void) tune;
    registerforoptions(e, name, index1, bound1, index2, bound2);
#endif
}


const int maxmobility[4] = { 9, 14, 15, 28 }; // indexed by piece - 2

void registerallevals(chessposition *pos)
{
    int i, j;
    bool tuneIt;


    tuneIt = false;  // the complex parameters needed to be tuned manually
    registertuner(pos, &eps.eComplexpawnsbonus, "eComplexpawnsbonus", 0, 0, 0, 0, tuneIt);
    registertuner(pos, &eps.eComplexpawnflanksbonus, "eComplexpawnflanksbonus", 0, 0, 0, 0, tuneIt);
    registertuner(pos, &eps.eComplexonlypawnsbonus, "eComplexonlypawnsbonus", 0, 0, 0, 0, tuneIt);
    registertuner(pos, &eps.eComplexadjust, "eComplexadjust", 0, 0, 0, 0, tuneIt);

    tuneIt = false;
    registertuner(pos, &eps.eTempo, "eTempo", 0, 0, 0, 0, tuneIt);
    tuneIt = false;
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
    for (i = 0; i < 4; i++)
        for (j = 0; j < 8; j++)
            registertuner(pos, &eps.ePotentialpassedpawnbonus[i][j], "ePotentialpassedpawnbonus", j, 8, i, 4, tuneIt && (j > 0 && j < 7));
    tuneIt = false;
    for (i = 0; i < 8; i++)
        registertuner(pos, &eps.eAttackingpawnbonus[i], "eAttackingpawnbonus", i, 8, 0, 0, tuneIt && (i > 0 && i < 7));
    tuneIt = false;
    for (i = 0; i < 2; i++)
        for (j = 0; j < 8; j++)
            registertuner(pos, &eps.eIsolatedpawnpenalty[i][j], "eIsolatedpawnpenalty", j, 8, i, 2, tuneIt);
    tuneIt = false;
    registertuner(pos, &eps.eDoublepawnpenalty, "eDoublepawnpenalty", 0, 0, 0, 0, tuneIt);
    tuneIt = false;
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
            registertuner(pos, &eps.eConnectedbonus[i][j], "eConnectedbonus", j, 6, i, 6, tuneIt);

    tuneIt = false;
    for (i = 0; i < 2; i++)
        for (j = 0; j < 8; j++)
            registertuner(pos, &eps.eBackwardpawnpenalty[i][j], "eBackwardpawnpenalty", j, 8, i, 2, tuneIt);
    tuneIt = false;
    registertuner(pos, &eps.eDoublebishopbonus, "eDoublebishopbonus", 0, 0, 0, 0, tuneIt);
    tuneIt = false;
    registertuner(pos, &eps.ePawnblocksbishoppenalty, "ePawnblocksbishoppenalty", 0, 0, 0, 0, tuneIt);
    registertuner(pos, &eps.eBishopcentercontrolbonus, "eBishopcentercontrolbonus", 0, 0, 0, 0, tuneIt);
    tuneIt = false;
    registertuner(pos, &eps.eKnightOutpost, "eKnightOutpost", 0, 0, 0, 0, tuneIt);

    tuneIt = false;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 28; j++)
            registertuner(pos, &eps.eMobilitybonus[i][j], "eMobilitybonus", j, 28, i, 4, tuneIt && (j < maxmobility[i]));

    tuneIt = false;
    registertuner(pos, &eps.eNocastlepenalty, "eNocastlepenalty", 0, 0, 0, 0, tuneIt);

    tuneIt = false;
    registertuner(pos, &eps.eRookon7thbonus, "eRookon7thbonus", 0, 0, 0, 0, tuneIt);

    tuneIt = false;
    registertuner(pos, &eps.eRookonkingarea, "eRookonkingarea", 0, 0, 0, 0, tuneIt);
    registertuner(pos, &eps.eBishoponkingarea, "eBishoponkingarea", 0, 0, 0, 0, tuneIt);

    tuneIt = false;
    registertuner(pos, &eps.eQueenattackedbysliderpenalty, "eQueenattackedbysliderpenalty", 0, 0, 0, 0, tuneIt);

    tuneIt = false;
    for (i = 0; i < 6; i++)
        registertuner(pos, &eps.eMinorbehindpawn[i], "eMinorbehindpawn", i, 6, 0, 0, tuneIt);

    tuneIt = false;
    for (i = 0; i < 2; i++)
        registertuner(pos, &eps.eSlideronfreefilebonus[i], "eSlideronfreefilebonus", i, 2, 0, 0, tuneIt);

    for (i = 0; i < 7; i++)
        registertuner(pos, &eps.eMaterialvalue[i], "eMaterialvalue", i, 7, 0, 0, false);

    tuneIt = false;
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

    tuneIt = false;
    for (i = 0; i < 7; i++)
        for (j = 0; j < 64; j++)
            registertuner(pos, &eps.ePsqt[i][j], "ePsqt", j, 64, i, 7, tuneIt && (i >= KNIGHT || (i == PAWN && j >= 8 && j < 56)));
}

#endif

struct traceeval {
    int rooks[2];
    int minors[2];
    int material[2];
    int pawns[2];
    int mobility[2];
    int kingattackpower[2];
    int threats[2];
    int ppawns[2];
    int complexity[2];
    int tempo[2];
    int sc;
    int ph;
    int total;
    int score;
    int endgame;
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


void traceEvalOut()
{
    stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2)
    << "              |    White    |    Black    |    Total   \n"
    << "              |   MG    EG  |   MG    EG  |   MG    EG \n"
    << " -------------+-------------+-------------+------------\n"
    << "     Material | " << splitvaluestring(te.material)
    << "       Minors | " << splitvaluestring(te.minors)
    << "        Rooks | " << splitvaluestring(te.rooks)
    << "        Pawns | " << splitvaluestring(te.pawns)
    << "      Passers | " << splitvaluestring(te.ppawns)
    << "     Mobility | " << splitvaluestring(te.mobility)
    << "      Threats | " << splitvaluestring(te.threats)
    << " King attacks | " << splitvaluestring(te.kingattackpower)
    << "   Complexity | " << splitvaluestring(te.complexity)
    << " -------------+-------------+-------------+------------\n"
    << "        Total |  Ph=" << setw(3) << te.ph << "/256 |  Sc=" << setw(3) << te.sc << "/128 | " << splitvaluestring(te.total)
    << " => " << cp(TAPEREDANDSCALEDEVAL(te.total, te.ph, te.sc)) << "\n"
    << "        Tempo | " << splitvaluestring(te.tempo)
    << "      Endgame | " << setw(5) << cp(te.endgame) << "\n"
    << "    Resulting | " << setw(5) << cp(te.score) << "\n";
    
    cout << ss.str();
    
}

#if 0  // this could be useful if endgames are preregistered
void register_endgame(string gamesignature, int(*endgame)(chessposition*))
{
    int pcs[16];
    getPcsFromStr(gamesignature.c_str(), pcs);
    for (int col = WHITE; col <= BLACK; col++)
    {
        U64 mathash = calc_key_from_pcs(pcs, col);
        Materialhashentry* e;
        mh.probeHash(mathash, &e);
    }
}
#endif


// some common endgames that need help of special evaluation
inline int KBNvK(chessposition *p)
{
    int strongside = p->piece00[WBISHOP] ? WHITE : BLACK;
    int ks = p->kingpos[strongside];
    int kw = p->kingpos[strongside ^ S2MMASK];
    int bishopcol = p->piece00[WBISHOP + strongside] & WHITEBB ? WHITE : BLACK;
    int c1 = bishopcol == WHITE ? 7 : 0;  // lower corner of same color as bishop
    int c2 = bishopcol == WHITE ? 56 : 63;  // upper corner of same color as bishop
    
    const double pw = 0.7;
    int kwcornerdistance = (int)(10.0 * min(pow(abs(FILE(c1) - FILE(kw)), pw) + pow(abs(RANK(c1) - RANK(kw)), pw),
                                            pow(abs(FILE(c2) - FILE(kw)), pw) + pow(abs(RANK(c2) - RANK(kw)), pw)));
    
    return (1000 - kwcornerdistance * 10 - squareDistance[ks][kw] - p->testRepetition() * 50 - p->halfmovescounter) * S2MSIGN(strongside);
}


// get psqt for eval tracing and tuning
int chessposition::getpsqval(bool showDetails)
{
    if (showDetails) printf("psq:\n====");
    te.material[0] = te.material[1] = 0;
    for (int j = 0; j < 64; j++)
    {
        // Use the index from whites position for better display
        int i = PSQTINDEX(j, 0);
        if (showDetails && (i & 7) == 0) printf("\n");
        PieceCode pc = mailbox[i];
        if (pc)
        {
            PieceType p = pc >> 1;
            int s2m = pc & S2MMASK;
            int mv = EVAL(eps.eMaterialvalue[p], S2MSIGN(s2m));
            int pv = EVAL(eps.ePsqt[p][PSQTINDEX(i, s2m)], S2MSIGN(s2m));
            te.material[s2m] += mv + pv;
            if (showDetails) printf("%4d ", TAPEREDANDSCALEDEVAL(pv, getPhase(), SCALE_NORMAL));
        }
        else
        {
            if (showDetails) printf("____ ");
        }
    }
    if (showDetails) printf("\n\n");
    return te.material[0] + te.material[1];
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
        U64 yourOpponents = yourStoppers & fileMask[index];
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
            if (yourAttackers || yourPushattackers)
            {
                // Lets see if we can get rid of the remaining stoppers
                if (POPCOUNT(myPushsupporters) >= POPCOUNT(yourPushattackers))
                {
                    // exchange is possible; bonus defferentiates between pawn majority and waiting enemy pawns behind
                    int severity = (POPCOUNT(mySupporters) >= POPCOUNT(yourAttackers)) + 2 * (bool)(!(yourStoppers ^ yourAttackers ^ yourPushattackers));
                    entryptr->value += EVAL(eps.ePotentialpassedpawnbonus[severity][RRANK(index, Me)], S2MSIGN(Me));
                    if (bTrace) te.pawns[Me] += EVAL(eps.ePotentialpassedpawnbonus[severity][RRANK(index, Me)], S2MSIGN(Me));
                }
            }
        }

        if (!(myPawns & neighbourfilesMask[index]))
        {
            // isolated pawn penalty per file
            int f = FILE(index);
            bool opposed = (bool)yourOpponents;
            entryptr->value += EVAL(eps.eIsolatedpawnpenalty[opposed][f], S2MSIGN(Me));
            if (bTrace) te.pawns[Me] += EVAL(eps.eIsolatedpawnpenalty[opposed][f], S2MSIGN(Me));
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
                        // backward pawn penalty per file
                        bool opposed = (bool)yourOpponents;
                        int f = FILE(index);
                        entryptr->value += EVAL(eps.eBackwardpawnpenalty[opposed][f], S2MSIGN(Me));
                        if (bTrace) te.pawns[Me] += EVAL(eps.eBackwardpawnpenalty[opposed][f], S2MSIGN(Me));
                    }
                }
            }
        }
    }

    // doubled pawns
    entryptr->value += EVAL(eps.eDoublepawnpenalty, S2MSIGN(Me) * POPCOUNT(piece00[WPAWN | Me] & (Me ? piece00[WPAWN | Me] >> 8 : piece00[WPAWN | Me] << 8)));
    if (bTrace) te.pawns[Me] += EVAL(eps.eDoublepawnpenalty, S2MSIGN(Me) * POPCOUNT(piece00[WPAWN | Me] & (Me ? piece00[WPAWN | Me] >> 8 : piece00[WPAWN | Me] << 8)));

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

        bool isBlocked = (myDist == yourDist - 1);
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
    const U64 myRammedPawns = piece00[WPAWN | Me] & PAWNPUSH(You, piece00[WPAWN | You]);
    U64 occupied = occupied00[0] | occupied00[1];
    U64 kingdangerarea = kingdangerMask[kingpos[You]][You];
    U64 xrayrookoccupied = occupied ^ (piece00[WROOK + Me] | piece00[WQUEEN + Me]);
    U64 xraybishopoccupied = occupied ^ (piece00[WBISHOP + Me] | piece00[WQUEEN + Me]);
    U64 goodMobility = ~((piece00[WPAWN + Me] & (RANK2(Me) | RANK3(Me))) | attackedBy[You][PAWN] | piece00[WKING + Me]);

    while (pb)
    {
        index = pullLsb(&pb);
        U64 attack = 0ULL;
        if (Pt == ROOK || Pt == QUEEN)
        {
            attack = ROOKATTACKS(xrayrookoccupied, index);

            // extrabonus for rook on (semi-)open file
            if (Pt == ROOK) {
                if (pe->phentry->semiopen[Me] & BITSET(FILE(index))) {
                    result += EVAL(eps.eSlideronfreefilebonus[bool(pe->phentry->semiopen[You] & BITSET(FILE(index)))], S2MSIGN(Me));
                    if (bTrace) te.rooks[Me] += EVAL(eps.eSlideronfreefilebonus[bool(pe->phentry->semiopen[You] & BITSET(FILE(index)))], S2MSIGN(Me));
                }
                else if (!GETCASTLERIGHTS(Me, state))
                {
                    result += EVAL(eps.eNocastlepenalty, S2MSIGN(Me));
                    if (bTrace) te.rooks[Me] += EVAL(eps.eNocastlepenalty, S2MSIGN(Me));
                }
            }

            // extrabonus for rook targeting the kingdanger area
            if (Pt == ROOK && (fileMask[index] & kingdangerarea)) {
                result += EVAL(eps.eRookonkingarea, S2MSIGN(Me));
                if (bTrace) te.rooks[Me] += EVAL(eps.eRookonkingarea, S2MSIGN(Me));
            }
        }

        if (Pt == BISHOP || Pt == QUEEN)
        {
            attack |= BISHOPATTACKS(xraybishopoccupied, index);

            if (Pt == BISHOP)
            {
                U64 blockingpawns = myRammedPawns & (BITSET(index) & WHITEBB ? WHITEBB : BLACKBB);
                result += EVAL(eps.ePawnblocksbishoppenalty, S2MSIGN(Me) * POPCOUNT(blockingpawns));
                if (bTrace) te.minors[Me] += EVAL(eps.ePawnblocksbishoppenalty, S2MSIGN(Me) * POPCOUNT(blockingpawns));

                if (MORETHANONE(BISHOPATTACKS(piece00[WPAWN] | piece00[BPAWN], index) & CENTER))
                {
                    result += EVAL(eps.eBishopcentercontrolbonus, S2MSIGN(Me));
                    if (bTrace) te.minors[Me] += EVAL(eps.eBishopcentercontrolbonus, S2MSIGN(Me));
                }
                // extrabonus for rook targeting the kingdanger area
                if ((BISHOPATTACKS(piece00[WPAWN] | piece00[BPAWN], index) & kingdangerarea)) {
                    result += EVAL(eps.eBishoponkingarea, S2MSIGN(Me));
                    if (bTrace) te.minors[Me] += EVAL(eps.eBishoponkingarea, S2MSIGN(Me));
                }
            }
        }

        if (Pt == QUEEN && sliderAttacked<Me>(index, occupied))
        {
            result += EVAL(eps.eQueenattackedbysliderpenalty, S2MSIGN(Me));
            if (bTrace) te.mobility[Me] += EVAL(eps.eQueenattackedbysliderpenalty, S2MSIGN(Me));
        }

        if (Pt == KNIGHT)
        {
            attack = knight_attacks[index];
        }

        if (Pt == KNIGHT || Pt == BISHOP)
        {
            U64 minorbb = BITSET(index);
            // bonus depending on rank if minor is shielded by pawn
            bool isBehindPawn = (minorbb & PAWNPUSH(You, piece00[WPAWN + Me]));
            if (isBehindPawn)
            {
                int r = RRANK(index, Me);
                result += EVAL(eps.eMinorbehindpawn[r], S2MSIGN(Me));
                if (bTrace) te.minors[Me] += EVAL(eps.eMinorbehindpawn[r], S2MSIGN(Me));
            }
        }

        // update attack bitboard
        attackedBy[Me][Pt] |= attack;
        attackedBy2[Me] |= (attackedBy[Me][0] & attack);
        attackedBy[Me][0] |= attack;

        // mobility bonus
        U64 mobility = attack & goodMobility;

        // Penalty for a piece pinned in front of the king
        if (kingPinned & (BITSET(index)))
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
    int kingdanger = SQEVAL(eps.eKingringattack[POPCOUNT(kingdangerMask[kingpos[Me]][Me]) - 4], pe->kingringattacks[You], You);

    // My attacked and poorly defended squares
    U64 myweaksquares = attackedBy[You][0]
        & ~attackedBy2[Me]
        & (~attackedBy[Me][0] | attackedBy[Me][KING] | attackedBy[Me][QUEEN]);

    // Your safe target squares
    U64 yoursafetargets = (~attackedBy[Me][0] | (myweaksquares & attackedBy2[You])) & ~occupied00[You];

    // penalty for weak squares in our king ring
    kingdanger += SQEVAL(eps.eWeakkingringpenalty, POPCOUNT(myweaksquares & kingdangerMask[kingpos[Me]][Me]), You);

    // Safe checks and attacks to king area
    kingdanger += SQEVAL(eps.eKingattackweight[KNIGHT], pe->kingattackpiececount[You][KNIGHT] * pe->kingattackers[You], You);
    if (pieceMovesTo<KNIGHT>(kingpos[Me]) & attackedBy[You][KNIGHT] & yoursafetargets)
        kingdanger += SQEVAL(eps.eSafecheckbonus[KNIGHT], 1, You);
    kingdanger += SQEVAL(eps.eKingattackweight[BISHOP], pe->kingattackpiececount[You][BISHOP] * pe->kingattackers[You], You);
    if (pieceMovesTo<BISHOP>(kingpos[Me]) & attackedBy[You][BISHOP] & yoursafetargets)
        kingdanger += SQEVAL(eps.eSafecheckbonus[BISHOP], 1, You);
    kingdanger += SQEVAL(eps.eKingattackweight[ROOK], pe->kingattackpiececount[You][ROOK] * pe->kingattackers[You], You);
    if (pieceMovesTo<ROOK>(kingpos[Me]) & attackedBy[You][ROOK] & yoursafetargets)
        kingdanger += SQEVAL(eps.eSafecheckbonus[ROOK], 1, You);
    kingdanger += SQEVAL(eps.eKingattackweight[QUEEN], pe->kingattackpiececount[You][QUEEN] * pe->kingattackers[You], You);
    if (pieceMovesTo<QUEEN>(kingpos[Me]) & attackedBy[You][QUEEN] & yoursafetargets)
        kingdanger += SQEVAL(eps.eSafecheckbonus[QUEEN], 1, You);

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

    // Knight outposts for 'You' cause we already calculated possible pawnpushed for 'Me' that might be important to defend against outpost
    U64 outpost = piece00[WKNIGHT | You] & OUTPOSTAREA(You) & attackedBy[You][PAWN] & ~(attackedPieces | attackedBy[Me][PAWN]);
    if (outpost)
    {
        result += EVAL(eps.eKnightOutpost, S2MSIGN(You) * POPCOUNT(outpost));
        if (bTrace) te.minors[You] += EVAL(eps.eKnightOutpost, S2MSIGN(You) * POPCOUNT(outpost));
    }


    return result;
}


template <EvalType Et, int Me>
int chessposition::getGeneralEval(positioneval *pe)
{
    const bool bTrace = (Et == TRACE);
    const int You = Me ^ S2MMASK;

    attackedBy[Me][KING] = king_attacks[kingpos[Me]];
    attackedBy2[Me] = pe->phentry->attackedBy2[Me] | (attackedBy[Me][KING] & pe->phentry->attacked[Me]);
    attackedBy[Me][0] = attackedBy[Me][KING] | pe->phentry->attacked[Me];

    attackedBy[Me][PAWN] = pe->phentry->attacked[Me];

    pe->kingattackers[Me] = POPCOUNT(attackedBy[Me][PAWN] & kingdangerMask[kingpos[You]][You]);

    // bonus for double bishop
    int result = EVAL(eps.eDoublebishopbonus, S2MSIGN(Me) * (POPCOUNT(piece00[WBISHOP | Me]) >= 2));
    if (bTrace) te.minors[Me] += EVAL(eps.eDoublebishopbonus, S2MSIGN(Me) * (POPCOUNT(piece00[WBISHOP | Me]) >= 2));

    // bonus for rook on 7th pressing against the king
    if ((piece00[WROOK | Me] & RANK7(Me)) && (piece00[WKING | You] & (RANK7(Me) | RANK8(Me))))
    {
        result += EVAL(eps.eRookon7thbonus, S2MSIGN(Me));
        if (bTrace) te.rooks[Me] += EVAL(eps.eRookon7thbonus, S2MSIGN(Me));
    }

    return result;
}



int chessposition::getFrcCorrection()
{
    if (!((piece00[WBISHOP] | piece00[BBISHOP]) & CORNERS))
        return 0;

    int correction = 0;

    if (mailbox[0]  == WBISHOP && mailbox[9] == WPAWN)
        correction += mailbox[17] ? -FrcCorneredBishopPenalty * 4 : -FrcCorneredBishopPenalty * 3;
    if (mailbox[7] == WBISHOP && mailbox[14] == WPAWN)
        correction += mailbox[22] ? -FrcCorneredBishopPenalty * 4 : -FrcCorneredBishopPenalty * 3;
    if (mailbox[56] == BBISHOP && mailbox[49] == BPAWN)
        correction += mailbox[41] ? FrcCorneredBishopPenalty * 4 : FrcCorneredBishopPenalty * 3;
    if (mailbox[63] == BBISHOP && mailbox[54] == BPAWN)
        correction += mailbox[46] ? FrcCorneredBishopPenalty * 4 : FrcCorneredBishopPenalty * 3;

    return S2MSIGN(state & S2MMASK) * correction;
}


//
// printf("constexpr U64 MH_Kk = 0x%016llX\n", calc_key_from_str("KvK"));
//
constexpr U64 MH_Kk = 0x3679A3A322768AB5;
constexpr U64 MH_KNk = 0x83F6D94BCF81A7CE;
constexpr U64 MH_KBk = 0x6E56427061F09750;
constexpr U64 MH_KNNk = 0xA6D01BADBCD3304C;
constexpr U64 MH_Kkn = 0x764E7792D2A7E7A2;
constexpr U64 MH_Kkb = 0x8D7565C35C201DD8;
constexpr U64 MH_Kknn = 0x0BDEC92B4D5E8455;
constexpr U64 MH_KNkn = 0xC3C10D7A3F50CAD9;
constexpr U64 MH_KBkb = 0xD55A84101FA6003D;
constexpr U64 MH_KNkb = 0x38FA1F2BB1D730A3;
constexpr U64 MH_KBkn = 0x2E6196419121FA47;
constexpr U64 MH_KBPk = 0x5E375EA73CF2C39E;
constexpr U64 MH_KBPPk = 0x6B50E60679111E9A;
constexpr U64 MH_Kkbp = 0xF2BAB13399668F63;
constexpr U64 MH_Kkbpp = 0xF843081EE1F63B4D;
constexpr U64 MH_KBNk = 0xDBD938988C07BA2B;
constexpr U64 MH_Kkbn = 0xCD42B1F2ACF170CF;

inline bool chessposition::isEndgame(int *score)
{
    U64 materialhash = zb.getMaterialHash(this);
    switch(materialhash) {
    case MH_Kk:
    case MH_KNk:
    case MH_KBk:
    case MH_KNNk:
    case MH_Kkn:
    case MH_Kkb:
    case MH_Kknn:
    case MH_KNkn:
    case MH_KBkb:
    case MH_KNkb:
    case MH_KBkn:
        return true;
    case MH_KBPk:
    case MH_KBPPk:
        if (piece00[WBISHOP] & WHITEBB)
            return !(piece00[WPAWN] & ~FILEHBB) && squareDistance[kingpos[BLACK]][63] <= 0;
        else
            return !(piece00[WPAWN] & ~FILEABB) && squareDistance[kingpos[BLACK]][56] <= 0;
    case MH_Kkbp:
    case MH_Kkbpp:
        if (piece00[BBISHOP] & WHITEBB)
            return !(piece00[BPAWN] & ~FILEABB) && squareDistance[kingpos[WHITE]][0] <= 0;
        else
            return !(piece00[BPAWN] & ~FILEHBB) && squareDistance[kingpos[WHITE]][7] <= 0;
    case MH_KBNk:
    case MH_Kkbn:
        *score = KBNvK(this);
        return true;
    default:
        return false;
    }
}

//
// getEval() is the general evaluation interface for search
// It returns the score of the position from the view of the side to move
//
template <EvalType Et>
int chessposition::getEval()
{
    const bool bTrace = (Et == TRACE);
    if (bTrace) te = { { 0 }, { 0 },{ 0 },{ 0 },{ 0 },{ 0 },{ 0 },{ 0 },{ 0 },{ 0 }, 0, 0, 0, 0, 0 };
#ifdef EVALTUNE
    resetTuner();
    getpsqval();
#endif

    int score = SCOREDRAW;

    if (piececount <= 5 && isEndgame(&score))
        return S2MSIGN(state & S2MMASK) * score;

    if (NnueReady && abs(GETEGVAL(psqval)) < NnuePsqThreshold)
    {
        int frcCorrection = (en.chess960 ? getFrcCorrection() : 0);
        score = NnueGetEval();
        score += S2MSIGN(state & S2MMASK) * contempt;
        int phscaled = score * (116 + phcount) / 128;

        if (bTrace) {
            cout << "Raw NNUE eval:  " << S2MSIGN(state & S2MMASK) * score << endl;
            cout << "Phased scaled:  " << S2MSIGN(state & S2MMASK) * phscaled << endl;
            if (frcCorrection)
                cout << "FRC correction: " << S2MSIGN(state & S2MMASK) * frcCorrection << endl;
            cout << "Tempo:          " << S2MSIGN(state & S2MMASK) * eps.eTempo << endl;
            cout << "Total:          " << S2MSIGN(state & S2MMASK) * phscaled + frcCorrection + eps.eTempo << endl;
        }

        return phscaled + frcCorrection + eps.eTempo;
    }

    // reset the attackedBy information
    memset(attackedBy, 0, sizeof(attackedBy));

    positioneval pe;
    bool hashexist = pwnhsh.probeHash(zb.getPawnKingHash(this), &pe.phentry);
    if (bTrace || !hashexist)
    {
        if (bTrace) pe.phentry->value = 0;
        getPawnAndKingEval<Et, 0>(pe.phentry);
        getPawnAndKingEval<Et, 1>(pe.phentry);
        U64 pawns = piece00[WPAWN] | piece00[BPAWN];
        pe.phentry->bothFlanks = ((pawns & FLANKLEFT) && (pawns & FLANKRIGHT));
    }

    int pawnEval = pe.phentry->value;
    int generalEval = getGeneralEval<Et, 0>(&pe) + getGeneralEval<Et, 1>(&pe);
    int piecesEval = getPieceEval<Et, KNIGHT, 0>(&pe)   + getPieceEval<Et, KNIGHT, 1>(&pe)
                    + getPieceEval<Et, BISHOP, 0>(&pe) + getPieceEval<Et, BISHOP, 1>(&pe)
                    + getPieceEval<Et, ROOK, 0>(&pe)   + getPieceEval<Et, ROOK, 1>(&pe)
                    + getPieceEval<Et, QUEEN, 0>(&pe)  + getPieceEval<Et, QUEEN, 1>(&pe);
    int lateEval = getLateEval<Et, 0>(&pe) + getLateEval<Et, 1>(&pe);

    int totalEval = psqval + pawnEval + generalEval + piecesEval + lateEval;

    int sideToScale = GETEGVAL(totalEval) > SCOREDRAW ? WHITE : BLACK;
    sc = getScaling(sideToScale);
    if (!bTrace && sc == SCALE_DRAW)
        return SCOREDRAW;

    int complexity = getComplexity(totalEval, pe.phentry);
    totalEval += complexity;

    if (bTrace)
    {
        te.tempo[state & S2MMASK] += CEVAL(eps.eTempo, S2MSIGN(state & S2MMASK));
        te.complexity[complexity < 0] += complexity;
    }

    score = TAPEREDANDSCALEDEVAL(totalEval, getPhase(), sc) + CEVAL(eps.eTempo, S2MSIGN(state & S2MMASK));

    if (bTrace)
    {
        getpsqval(en.evaldetails);
        te.sc = sc;
        te.ph = getPhase();
        te.total = totalEval;
        te.score = score;
        traceEvalOut();
    }

    return S2MSIGN(state & S2MMASK) * score;
}


int chessposition::getComplexity(int val, pawnhashentry *phentry)
{
        int evaleg = GETEGVAL(val);
        int sign = (evaleg > 0) - (evaleg < 0);
        bool onlypawns = (((piece00[0] | piece00[1]) ^ (piece00[WPAWN] | piece00[BPAWN] | piece00[WKING] | piece00[BKING])) == 0);
        int complexity = EEVAL(eps.eComplexpawnsbonus, POPCOUNT(piece00[WPAWN] | piece00[BPAWN]));
        complexity += EEVAL(eps.eComplexpawnflanksbonus, phentry->bothFlanks);
        complexity += EEVAL(eps.eComplexonlypawnsbonus, onlypawns);
        complexity += EEVAL(eps.eComplexadjust, 1);

        return sign * max(complexity, -abs(evaleg));
}


int chessposition::getScaling(int me)
{
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

    // Default scaling
    int scale = SCALE_NORMAL;

    // Check for insufficient material using simnple heuristic from chessprogramming site
    int you = me ^ S2MMASK;

    if (pawns[me] == 0 && nonpawnvalue[me] - nonpawnvalue[you] <= materialvalue[BISHOP])
        scale = nonpawnvalue[me] < materialvalue[ROOK] ? SCALE_DRAW : SCALE_HARDTOWIN;

    if (pawns[me] == 1 && nonpawnvalue[me] - nonpawnvalue[you] <= materialvalue[BISHOP])
        scale = SCALE_ONEPAWN;

    U64 bishopsbb = (piece00[WBISHOP] | piece00[BBISHOP]);
    if (bishops[WHITE] == 1 && bishops[BLACK] == 1
        && (bishopsbb & WHITEBB) && (bishopsbb & BLACKBB)
        && nonpawnvalue[WHITE] <= materialvalue[BISHOP]
        && nonpawnvalue[BLACK] <= materialvalue[BISHOP])
        scale = SCALE_OCB;

    return scale;
}

// Explicit template instantiation
// This avoids putting these definitions in header file
template int chessposition::getEval<NOTRACE>();
template int chessposition::getEval<TRACE>();

} // namespace rubichess

