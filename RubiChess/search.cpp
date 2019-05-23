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


const int deltapruningmargin = 100;

int reductiontable[2][MAXDEPTH][64];

#define MAXLMPDEPTH 9
int lmptable[2][MAXLMPDEPTH];

// Shameless copy of Ethereal/Laser for now; may be improved/changed in the future
static const int SkipSize[16] = { 1, 1, 1, 2, 2, 2, 1, 3, 2, 2, 1, 3, 3, 2, 2, 1 };
static const int SkipDepths[16] = { 1, 2, 2, 4, 4, 3, 2, 5, 4, 3, 2, 6, 5, 4, 3, 2 };

void searchinit()
{
    for (int d = 0; d < MAXDEPTH; d++)
        for (int m = 0; m < 64; m++)
        {
            // reduction for not improving positions
            reductiontable[0][d][m] = (int)round(log(d) * log(m) / 1.2);
            // reduction for improving positions
            reductiontable[1][d][m] = (int)round(log(d) * log(m) / 2.1);
        }
    for (int d = 0; d < MAXLMPDEPTH; d++)
    {
        // lmp for not improving positions
        lmptable[0][d] = (int)(2.5 + 0.7 * round(pow(d, 1.85)));
        // lmp for improving positions
        lmptable[1][d] = (int)(4.0 + 1.3 * round(pow(d, 1.85)));
    }
}

void chessposition::getCmptr(int16_t **cmptr)
{
    for (int i = 0, j = mstop - 1; i < CMPLIES; i++, j--)
    {
        uint32_t c;
        if (j >= 0 && (c = movestack[j].movecode))
            cmptr[i] = (int16_t*)counterhistory[GETPIECE(c)][GETTO(c)];
        else
            cmptr[i] = NULL;
    }
}

inline int chessposition::getHistory(uint32_t code, int16_t **cmptr)
{
    int pc = GETPIECE(code);
    int s2m = pc & S2MMASK;
    int from = GETFROM(code);
    int to = GETTO(code);
    int value = history[s2m][from][to];
    for (int i = 0; i < CMPLIES; i++)
        if (cmptr[i])
            value += cmptr[i][pc * 64 + to];

    return value;
}


inline void chessposition::updateHistory(uint32_t code, int16_t **cmptr, int value)
{
    int pc = GETPIECE(code);
    int s2m = pc & S2MMASK;
    int from = GETFROM(code);
    int to = GETTO(code);
    value = max(-400, min(400, value));
    int delta = 32 * value - history[s2m][from][to] * abs(value) / 512;
    history[s2m][from][to] += delta;
    for (int i = 0; i < CMPLIES; i++)
        if (cmptr[i]) {
            int delta = 32 * value - cmptr[i][pc * 64 + to] * abs(value) / 512;
            cmptr[i][pc * 64 + to] += delta;
        }
}


int chessposition::getQuiescence(int alpha, int beta, int depth)
{
    int patscore, score;
    int bestscore = SHRT_MIN;
    bool myIsCheck = (bool)isCheckbb;
#ifdef EVALTUNE
    if (depth < 0) isQuiet = false;
    positiontuneset targetpts;
    evalparam ev[NUMOFEVALPARAMS];

    bool foundpts = false;
#endif

    // FIXME: Should quiescience nodes count for the statistics?
    //en.nodes++;

    // Reset pv
    pvtable[ply][0] = 0;

#ifdef SDEBUG
    chessmove debugMove;
    int debugInsert = ply - rootheight;
    bool isDebugPv = triggerDebug(&debugMove);
#endif

    int hashscore = NOSCORE;
    uint16_t hashmovecode = 0;
    int staticeval = NOSCORE;
    bool tpHit = tp.probeHash(hash, &hashscore, &staticeval, &hashmovecode, depth, alpha, beta, ply);
    if (tpHit)
    {
        SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from TT.", hashscore);
        return hashscore;
    }

    if (!myIsCheck)
    {
        bestscore = patscore = (staticeval != NOSCORE ? staticeval : S2MSIGN(state & S2MMASK) * getValue<NOTRACE>());
        if (patscore >= beta)
        {
            SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch (fail high by patscore).", patscore);
            return patscore;
        }
        if (patscore > alpha)
        {
#ifdef EVALTUNE
            getPositionTuneSet(&targetpts, &ev[0]);
            foundpts = true;
#endif
            alpha = patscore;
        }

        // Delta pruning
        int bestCapture = getBestPossibleCapture();
        if (patscore + deltapruningmargin + bestCapture < alpha)
        {
            SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch (delta pruning by patscore).", patscore);
            return patscore;
        }
    }

    prepareStack();

    MoveSelector ms = {};
    ms.SetPreferredMoves(this);

    chessmove *m;

    while ((m = ms.next()))
    {
        if (!myIsCheck && patscore + materialvalue[GETCAPTURE(m->code) >> 1] + deltapruningmargin <= alpha)
            // Leave out capture that is delta-pruned
            continue;

        if (playMove(m))
        {
            ms.legalmovenum++;
            score = -getQuiescence(-beta, -alpha, depth - 1);
            unplayMove(m);
            if (score > bestscore)
            {
                bestscore = score;
                if (score >= beta)
                {
                    SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch (fail high).", score);
                    return score;
                }
                if (score > alpha)
                {
                    updatePvTable(m->code, true);
                    alpha = score;
#ifdef EVALTUNE
                    foundpts = true;
                    copyPositionTuneSet(&this->pts, &this->ev[0], &targetpts, &ev[0]);
#endif
                }
            }
        }
    }
#ifdef EVALTUNE
    if (foundpts)
        copyPositionTuneSet(&targetpts, &ev[0], &this->pts, &this->ev[0]);
#endif

    if (myIsCheck && !ms.legalmovenum)
    {
        // It's a mate
        SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch (mate).", SCOREBLACKWINS + ply);
        return SCOREBLACKWINS + ply;
    }

    SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch.", bestscore);
    return bestscore;
}



int chessposition::alphabeta(int alpha, int beta, int depth)
{
    int score;
    int hashscore = NOSCORE;
    uint16_t hashmovecode = 0;
    int staticeval = NOSCORE;
    bool isLegal;
    int bestscore = NOSCORE;
    uint32_t bestcode = 0;
    int eval_type = HASHALPHA;
    chessmove *m;
    int extendall = 0;
    int effectiveDepth;
    bool PVNode = (alpha != beta - 1);

    nodes++;

    // Reset pv
    pvtable[ply][0] = 0;

#ifdef SDEBUG
    chessmove debugMove;
    string excludestr = "";
    int debugInsert = ply - rootheight;
    bool isDebugPv = triggerDebug(&debugMove);
#endif

    // test for remis via repetition
    if (testRepetiton() >= 2)
    {
        SDEBUGPRINT(isDebugPv, debugInsert, "Draw (repetition)");
        return SCOREDRAW;
    }

    // test for remis via 50 moves rule
    if (halfmovescounter > 100)
    {
        SDEBUGPRINT(isDebugPv, debugInsert, "Draw (50 moves)");
        return SCOREDRAW;
    }


    // Reached depth? Do a qsearch
    if (depth <= 0)
    {
        // update selective depth info
        if (seldepth < ply + 1)
            seldepth = ply + 1;

        return getQuiescence(alpha, beta, depth);
    }


    // Get move for singularity check and change hash to seperate partial searches from full searches
    uint16_t excludeMove = excludemovestack[mstop - 1];
    excludemovestack[mstop] = 0;

#ifdef SDEBUG
    if (isDebugPv)
    {
        chessmove cm, em;
        string s;
        for (int i = rootheight; i < mstop; i++)
        {
            cm.code = movestack[i].movecode;
            s = s + cm.toString() + " ";
        }
        if (excludeMove)
        {
            em.code = excludeMove;
            excludestr = " singular testing " + em.toString();
        }
        SDEBUGPRINT(true, debugInsert, "(depth=%2d%s) Entering debug pv: %s (%s)  [%3d,%3d] ", depth, excludestr.c_str(), s.c_str(), debugMove.code ? debugMove.toString().c_str() : "", alpha, beta);
    }
#endif

    U64 newhash = hash ^ excludeMove;

    bool tpHit = tp.probeHash(newhash, &hashscore, &staticeval, &hashmovecode, depth, alpha, beta, ply);
    if (tpHit && rp.getPositionCount(hash) <= 1)  //FIXME: This test on the repetition table works like a "is not PV"; should be fixed in the future)
    {
        uint32_t fullhashmove = shortMove2FullMove(hashmovecode);
        if (fullhashmove)
            updatePvTable(fullhashmove, false);
        SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from TT.", hashscore);
        return hashscore;
    }


    // TB
    // The test for rule50_count() == 0 is required to prevent probing in case
    // the root position is a TB position but only WDL tables are available.
    // In that case the search should not probe before a pawn move or capture
    // is made.
    if (POPCOUNT(occupied00[0] | occupied00[1]) <= useTb && halfmovescounter == 0)
    {
        int success;
        int v = probe_wdl(&success, this);
        if (success) {
            en.tbhits++;
            int bound;
            if (v < -1) {
                bound = HASHALPHA;
                score = -SCORETBWIN + ply;
            }
            else if (v > 1) {
                bound = HASHBETA;
                score = SCORETBWIN - ply;
            }
            else {
                bound = HASHEXACT;
                score = SCOREDRAW + v;
            }
            if (bound == HASHEXACT || (bound == HASHALPHA ? (score <= alpha) : (score >= beta)))
            {
                tp.addHash(hash, score, staticeval, bound, MAXDEPTH, 0);
                SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from TB.", score);
            }
            return score;
        }
    }

    // Check extension
    if (isCheckbb)
        extendall = 1;

    prepareStack();

    // get static evaluation of the position
    if (staticeval == NOSCORE)
        staticeval = S2MSIGN(state & S2MMASK) * getValue<NOTRACE>();
    staticevalstack[mstop] = staticeval;

    bool positionImproved = (mstop >= rootheight + 2
        && staticevalstack[mstop] > staticevalstack[mstop - 2]);

    // Razoring
    if (!PVNode && !isCheckbb && depth <= 2)
    {
        const int ralpha = alpha - 250 - depth * 50;
        if (staticeval < ralpha)
        {
            if (depth == 1 && ralpha < alpha)
                return getQuiescence(alpha, beta, depth);
            int value = getQuiescence(ralpha, ralpha + 1, depth);
            if (value <= ralpha)
                return value;
        }
    }

    // futility pruning
    bool futility = false;
    if (depth <= 6)
    {
        // reverse futility pruning
        if (!isCheckbb && staticeval - depth * (72 - 20 * positionImproved) > beta)
        {
            SDEBUGPRINT(isDebugPv, debugInsert, " Cutoff by reverse futility pruning: staticscore(%d) - revMargin(%d) > beta(%d)", staticeval, depth * (72 - 20 * positionImproved), beta);
            return staticeval;
        }
        futility = (staticeval < alpha - (100 + 80 * depth));
    }

    // Nullmove pruning with verification like SF does it
    int bestknownscore = (hashscore != NOSCORE ? hashscore : staticeval);
    if (!isCheckbb && depth >= 2 && bestknownscore >= beta && (ply  >= nullmoveply || ply % 2 != nullmoveside))
    {
        playNullMove();
        int R = 4 + (depth / 6) + (bestknownscore - beta) / 150 + !PVNode * 2;

        score = -alphabeta(-beta, -beta + 1, depth - R);
        unplayNullMove();

        if (score >= beta)
        {
            if (MATEFORME(score))
                score = beta;

            if (abs(beta) < 5000 && (depth < 12 || nullmoveply)) {
                SDEBUGPRINT(isDebugPv, debugInsert, "Low-depth-cutoff by null move: %d", score);
                return score;
            }
            // Verification search
            nullmoveply = ply + 3 * (depth - R) / 4;
            nullmoveside = ply % 2;
            int verificationscore = alphabeta(beta - 1, beta, depth - R);
            nullmoveside = nullmoveply = 0;
            if (verificationscore >= beta) {
                SDEBUGPRINT(isDebugPv, debugInsert, "Verified cutoff by null move: %d", score);
                return score;
            }
            else {
                SDEBUGPRINT(isDebugPv, debugInsert, "Verification refutes cutoff by null move: %d", score);
            }
        }
    }

    // ProbCut
    if (!PVNode && depth >= 5 && abs(beta) < SCOREWHITEWINS)
    {
        int rbeta = min(SCOREWHITEWINS, beta + 100);
        chessmovelist *movelist = new chessmovelist;
        movelist->length = getMoves(&movelist->move[0], TACTICAL);

        for (int i = 0; i < movelist->length; i++)
        {
            if (!see(movelist->move[i].code, rbeta - staticeval))
                continue;

            if (playMove(&movelist->move[i]))
            {
                int probcutscore = -alphabeta(-rbeta, -rbeta + 1, depth - 4);

                unplayMove(&movelist->move[i]);

                if (probcutscore >= rbeta)
                {
                    // ProbCut off
                    delete movelist;
                    return probcutscore;
                }
            }
        }
        delete movelist;
    }


    // Internal iterative deepening 
    const int iidmin = 3;
    const int iiddelta = 2;
    if (PVNode && !hashmovecode && depth >= iidmin)
    {
        SDEBUGPRINT(isDebugPv, debugInsert, " Entering iid...");
        alphabeta(alpha, beta, depth - iiddelta);
        hashmovecode = tp.getMoveCode(newhash);
    }

    MoveSelector ms = {};
    ms.SetPreferredMoves(this, hashmovecode, killer[0][ply], killer[1][ply], excludeMove);

    int  LegalMoves = 0;
    int quietsPlayed = 0;
    uint32_t quietMoves[MAXMOVELISTLENGTH];
    while ((m = ms.next()))
    {
#ifdef SDEBUG
        bool isDebugMove = ((debugMove.code & 0xeff) == (m->code & 0xeff));
#endif
        // Leave out the move to test for singularity
        if ((m->code & 0xffff) == excludeMove)
            continue;

        // Late move pruning
        if (depth < MAXLMPDEPTH && !ISTACTICAL(m->code) && bestscore > NOSCORE && quietsPlayed > lmptable[positionImproved][depth])
        {
            // Proceed to next moveselector state manually to save some time
            ms.state++;
            ms.capturemovenum = 0;
            continue;
        }

        // Check for futility pruning condition for this move and skip move if at least one legal move is already found
        bool futilityPrune = futility && !ISTACTICAL(m->code) && !isCheckbb && alpha <= 900 && !moveGivesCheck(m->code);
        if (futilityPrune)
        {
            if (LegalMoves)
            {
                SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s pruned by futility: staticeval(%d) < alpha(%d) - futilityMargin(%d)", debugMove.toString().c_str(), staticeval, alpha, 100 + 80 * depth);
                continue;
            }
            else if (staticeval > bestscore)
            {
                // Use the static score from futility test as a bestscore start value
                bestscore = staticeval;
            }
        }

        // Prune tactical moves with bad SEE
        if (!isCheckbb && depth < 8 && bestscore > NOSCORE && ms.state >= BADTACTICALSTATE && !see(m->code, -20 * depth * depth))
        {
            SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s pruned by bad SEE", debugMove.toString().c_str());
            continue;
        }

        int extendMove = 0;

        if ((m->code & 0xffff) == hashmovecode
            && depth > 7
            && !excludeMove
            && tp.probeHash(newhash, &hashscore, &staticeval, &hashmovecode, depth - 3, alpha, beta, ply)  // FIXME: maybe needs hashscore = FIXMATESCOREPROBE(hashscore, ply);
            && hashscore > alpha)
        {
            SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s tested for singularity", debugMove.toString().c_str());
            excludemovestack[mstop - 1] = hashmovecode;
            int sBeta = max(hashscore - 2 * depth, SCOREBLACKWINS);
            int redScore = alphabeta(sBeta - 1, sBeta, depth / 2);
            excludemovestack[mstop - 1] = 0;

            if (redScore < sBeta)
            {
                SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s is singular", debugMove.toString().c_str());
                extendMove = 1;
            }
        }

        isLegal = playMove(m);
        if (isLegal)
        {
            LegalMoves++;

            // Check again for futility pruning now that we found a valid move
            if (futilityPrune)
            {
                SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s pruned by futility: staticeval(%d) < alpha(%d) - futilityMargin(%d)", debugMove.toString().c_str(), staticeval, alpha, 100 + 80 * depth);
                unplayMove(m);
                continue;
            }

            int stats = getHistory(m->code, ms.cmptr);
            int reduction = 0;
            // Late move reduction
            if (!extendall && depth > 2 && !ISTACTICAL(m->code))
            {
                reduction = reductiontable[positionImproved][depth][min(63, LegalMoves)];
                SDEBUGPRINT(isDebugPv && isDebugMove && reduction, debugInsert, " PV move %s (value=%d) with depth reduced by %d", debugMove.toString().c_str(), m->value, reduction);

                // adjust reduction by stats value
                reduction -= stats / 10000;
                reduction = min(depth, max(0, reduction));
            }

            if (eval_type != HASHEXACT)
            {
                // First move ("PV-move"); do a normal search
                effectiveDepth = depth + extendall - reduction + extendMove;
                score = -alphabeta(-beta, -alpha, effectiveDepth - 1);
                if (reduction && score > alpha)
                {
                    // research without reduction
                    effectiveDepth += reduction;
                    score = -alphabeta(-beta, -alpha, effectiveDepth - 1);
                }
            }
            else {
                // try a PV-Search
                effectiveDepth = depth + extendall;
                score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1);
                if (score > alpha && score < beta)
                {
                    // reasearch with full window
                    score = -alphabeta(-beta, -alpha, effectiveDepth - 1);
                }
            }
            unplayMove(m);

            if (en.stopLevel == ENGINESTOPIMMEDIATELY)
            {
                // time is over; immediate stop requested
                return alpha;
            }

            SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s scored %d", debugMove.toString().c_str(), score);

            if (score > bestscore)
            {
                bestscore = score;
                bestcode = m->code;

                if (score >= beta)
                {
                    if (!ISCAPTURE(m->code))
                    {
                        updateHistory(m->code, ms.cmptr, depth * depth);
                        for (int i = 0; i < quietsPlayed; i++)
                        {
                            uint32_t qm = quietMoves[i];
                            updateHistory(qm, ms.cmptr, -(depth * depth));
                        }

                        // Killermove
                        if (killer[0][ply] != m->code)
                        {
                            killer[1][ply] = killer[0][ply];
                            killer[0][ply] = m->code;
                        }
                    }

                    SDEBUGPRINT(isDebugPv, debugInsert, " Beta-cutoff by move %s: %d  %s%s", m->toString().c_str(), score, excludestr.c_str(), excludeMove ? " : not singular" : "");
                    if (!excludeMove)
                    {
                        SDEBUGPRINT(isDebugPv, debugInsert, " ->Hash(%d) = %d(beta)", effectiveDepth, score);
                        tp.addHash(newhash, FIXMATESCOREADD(score, ply), staticeval, HASHBETA, effectiveDepth, (uint16_t)bestcode);
                    }
                    return score;   // fail soft beta-cutoff
                }

                if (score > alpha)
                {
                    SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s raising alpha to %d", debugMove.toString().c_str(), score);
                    alpha = score;
                    eval_type = HASHEXACT;
                    updatePvTable(bestcode, true);
                }
            }

            if (!ISTACTICAL(m->code))
                quietMoves[quietsPlayed++] = m->code;
        }
    }

    if (LegalMoves == 0)
    {
        if (excludeMove)
            return alpha;

        if (isCheckbb) {
            // It's a mate
            SDEBUGPRINT(isDebugPv, debugInsert, " Return score: %d  (mate)", SCOREBLACKWINS + ply);
            return SCOREBLACKWINS + ply;
        }
        else {
            // It's a stalemate
            SDEBUGPRINT(isDebugPv, debugInsert, " Return score: 0  (stalemate)");
            return SCOREDRAW;
        }
    }

    SDEBUGPRINT(isDebugPv, debugInsert, " Return score: %d  %s%s", bestscore, excludestr.c_str(), excludeMove ? " singular" : "");

    if (bestcode && !excludeMove)
    {
        SDEBUGPRINT(isDebugPv, debugInsert, " ->Hash(%d) = %d(%s)", depth, bestscore, eval_type == HASHEXACT ? "exact" : "alpha");
        tp.addHash(newhash, FIXMATESCOREADD(bestscore, ply), staticeval, eval_type, depth, (uint16_t)bestcode);
    }

    return bestscore;
}



template <RootsearchType RT>
int chessposition::rootsearch(int alpha, int beta, int depth)
{
    int score;
    uint16_t hashmovecode = 0;
    int bestscore = NOSCORE;
    int staticeval = NOSCORE;
    int eval_type = HASHALPHA;
    chessmove *m;
    int extendall = 0;
    int lastmoveindex;
    int maxmoveindex;

    const bool isMultiPV = (RT == MultiPVSearch);

    nodes++;

    // reset pv
    pvtable[0][0] = 0;

    if (isMultiPV)
    {
        lastmoveindex = 0;
        maxmoveindex = min(en.MultiPV, rootmovelist.length);
        for (int i = 0; i < maxmoveindex; i++)
            bestmovescore[i] = SHRT_MIN + 1;
    }

#ifdef SDEBUG
    chessmove debugMove;
    int debugInsert = ply - rootheight;
    bool isDebugPv = triggerDebug(&debugMove);
    SDEBUGPRINT(true, debugInsert, "(depth=%2d) Rootsearch Next pv debug move: %s  [%3d,%3d]", depth, debugMove.code ? debugMove.toString().c_str() : "", alpha, beta);
#endif

    if (!isMultiPV
        && !useRootmoveScore
        && tp.probeHash(hash, &score, &staticeval, &hashmovecode, depth, alpha, beta, 0)
        && rp.getPositionCount(hash) <= 1)  //FIXME: Is this really needed in rootsearch?
    {
        uint32_t fullhashmove = shortMove2FullMove(hashmovecode);
        if (fullhashmove)
        {
            if (bestmove[0].code != fullhashmove) {
                bestmove[0].code = fullhashmove;
                pondermove.code = 0;
            }
            if (score > alpha) bestmovescore[0] = score;
            updatePvTable(fullhashmove, false);
            return score;
        }
    }

    if (isCheckbb)
        extendall = 1;

    if (!tbPosition)
    {
        // Reset move values
        for (int i = 0; i < rootmovelist.length; i++)
        {
            m = &rootmovelist.move[i];

            //PV moves gets top score
            if (hashmovecode == (m->code & 0xffff))
            {
                m->value = PVVAL;
            }
            else if (bestFailingLow == m->code)
            {
                m->value = KILLERVAL2 - 1;
            }
            // killermoves gets score better than non-capture
            else if (killer[0][0] == m->code)
            {
                m->value = KILLERVAL1;
            }
            else if (killer[1][0] == m->code)
            {
                m->value = KILLERVAL2;
            }
            else if (GETCAPTURE(m->code) != BLANK)
            {
                m->value = (mvv[GETCAPTURE(m->code) >> 1] | lva[GETPIECE(m->code) >> 1]);
            }
            else {
                m->value = history[state & S2MMASK][GETFROM(m->code)][GETTO(m->code)];
            }
        }
    }

    // get static evaluation of the position
    if (staticeval == NOSCORE)
        staticeval = S2MSIGN(state & S2MMASK) * getValue<NOTRACE>();
    staticevalstack[mstop] = staticeval;

    int quietsPlayed = 0;
    uint32_t quietMoves[MAXMOVELISTLENGTH];

    // FIXME: Dummy move selector for now; only used to pass null cmptr to updateHistory
    MoveSelector ms = {};

    for (int i = 0; i < rootmovelist.length; i++)
    {
        for (int j = i + 1; j < rootmovelist.length; j++)
        {
            if (rootmovelist.move[i] < rootmovelist.move[j])
            {
                swap(rootmovelist.move[i], rootmovelist.move[j]);
            }
        }

        m = &rootmovelist.move[i];
#ifdef SDEBUG
        bool isDebugMove = (debugMove.code == (m->code & 0xeff));
#endif

        playMove(m);

        if (en.moveoutput && !threadindex)
        {
            char s[256];
            sprintf_s(s, "info depth %d currmove %s currmovenumber %d\n", depth, m->toString().c_str(), i + 1);
            cout << s;
        }

        int reduction = 0;

        // Late move reduction
        if (!extendall && depth > 2 && !ISTACTICAL(m->code))
        {
            reduction = reductiontable[0][depth][min(63, i + 1)];
        }

        int effectiveDepth;
        if (eval_type != HASHEXACT)
        {
            // First move ("PV-move"); do a normal search
            effectiveDepth = depth + extendall - reduction;
            score = -alphabeta(-beta, -alpha, effectiveDepth - 1);
            if (reduction && score > alpha)
            {
                // research without reduction
                effectiveDepth += reduction;
                score = -alphabeta(-beta, -alpha, effectiveDepth - 1);
            }
        }
        else {
            // try a PV-Search
            effectiveDepth = depth + extendall;
            score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1);
            if (score > alpha && score < beta)
            {
                // reasearch with full window
                score = -alphabeta(-beta, -alpha, effectiveDepth - 1);
            }
        }

        SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s scored %d", debugMove.toString().c_str(), score);

        unplayMove(m);
        if (useRootmoveScore)
            score = m->value;

        if (en.stopLevel == ENGINESTOPIMMEDIATELY)
        {
            // time over; immediate stop requested
            return bestscore;
        }

        if (!ISTACTICAL(m->code))
            quietMoves[quietsPlayed++] = m->code;

        if ((isMultiPV && score <= bestmovescore[lastmoveindex])
            || (!isMultiPV && score <= bestscore))
            continue;

        bestscore = score;
        bestFailingLow = m->code;

        if (isMultiPV)
        {
            if (score > bestmovescore[lastmoveindex])
            {
                int newindex = lastmoveindex;
                while (newindex > 0 && score > bestmovescore[newindex - 1])
                {
                    bestmovescore[newindex] = bestmovescore[newindex - 1];
                    bestmove[newindex] = bestmove[newindex - 1];
                    newindex--;
                }
                bestmovescore[newindex] = score;
                bestmove[newindex] = *m;
                if (lastmoveindex < maxmoveindex - 1)
                    lastmoveindex++;
                if (bestmovescore[maxmoveindex - 1] > alpha)
                {
                    alpha = bestmovescore[maxmoveindex - 1];
                }
                eval_type = HASHEXACT;
            }
        }

        // We have a new best move.
        // Now it gets a little tricky what to do with it
        // The move becomes the new bestmove[0] (take for UCI output) if
        // - it is the first one
        // - it raises alpha
        // If it fails low we don't change bestmove anymore but remember it in bestFailingLow for move ordering
        if (score > alpha)
        {
            if (!isMultiPV)
            {
                updatePvTable(m->code, true);
                if (bestmove[0].code != pvtable[0][0])
                {
                    bestmove[0].code = pvtable[0][0];
                    pondermove.code = pvtable[0][1];
                }
                alpha = score;
                bestmovescore[0] = score;
                eval_type = HASHEXACT;
            }
            if (score >= beta)
            {
                // Killermove
                if (!ISCAPTURE(m->code))
                {
                    updateHistory(m->code, ms.cmptr, depth * depth);
                    for (int i = 0; i < quietsPlayed - 1; i++)
                    {
                        uint32_t qm = quietMoves[i];
                        updateHistory(qm, ms.cmptr, -(depth * depth));
                    }

                    if (killer[0][0] != m->code)
                    {
                        killer[1][0] = killer[0][0];
                        killer[0][0] = m->code;
                    }
                }
                SDEBUGPRINT(isDebugPv, debugInsert, " Beta-cutoff by move %s: %d", m->toString().c_str(), score);
                tp.addHash(hash, beta, staticeval, HASHBETA, effectiveDepth, (uint16_t)m->code);
                return beta;   // fail hard beta-cutoff
            }
        }
        else if (!isMultiPV)
        {
            // at fail low don't overwrite an existing move
            if (!bestmove[0].code)
                bestmove[0] = *m;
        }
    }

    SDEBUGPRINT(true, 0, getPv().c_str());

    if (isMultiPV)
    {
        if (eval_type == HASHEXACT)
        {
            tp.addHash(hash, bestmovescore[0], staticeval, eval_type, depth, (uint16_t)bestmove[0].code);
            return bestmovescore[maxmoveindex - 1];
        }
        else {
            tp.addHash(hash, alpha, staticeval, eval_type, depth, (uint16_t)bestmove[0].code);
            return alpha;
        }
    }
    else {
        tp.addHash(hash, alpha, staticeval, eval_type, depth, (uint16_t)bestmove[0].code);
        return alpha;
    }
}


static void uciScore(searchthread *thr, int inWindow, U64 nowtime, int mpvIndex)
{
    int msRun = (int)((nowtime - en.starttime) * 1000 / en.frequency);
    if (inWindow != 1 && (msRun - en.lastReport) < 200)
        return;

    const char* boundscore[] = { "upperbound", "", "lowerbound" };
    char s[4096];
    chessposition *pos = &thr->pos;
    en.lastReport = msRun;
    string pvstring = pos->getPv();
    int score = pos->bestmovescore[mpvIndex];
    U64 nodes = en.getTotalNodes();

    if (!MATEDETECTED(score))
    {
        sprintf_s(s, "info depth %d seldepth %d multipv %d time %d score cp %d %s nodes %llu nps %llu tbhits %llu hashfull %d pv %s\n",
            thr->depth, pos->seldepth, mpvIndex + 1, msRun, score, boundscore[inWindow], nodes,
            (nowtime > en.starttime ? nodes * en.frequency / (nowtime - en.starttime) : 1),
            en.tbhits, tp.getUsedinPermill(), pvstring.c_str());
    }
    else
    {
        int matein = (score > 0 ? (SCOREWHITEWINS - score + 1) / 2 : (SCOREBLACKWINS - score) / 2);
        sprintf_s(s, "info depth %d seldepth %d multipv %d time %d score mate %d nodes %llu nps %llu tbhits %llu hashfull %d pv %s\n",
            thr->depth, pos->seldepth, mpvIndex + 1, msRun, matein, nodes,
            (nowtime > en.starttime ? nodes * en.frequency / (nowtime - en.starttime) : 1),
            en.tbhits, tp.getUsedinPermill(), pvstring.c_str());
    }
    cout << s;
}


template <RootsearchType RT>
static void search_gen1(searchthread *thr)
{
    int score;
    int alpha, beta;
    int deltaalpha = 8;
    int deltabeta = 8;
    int maxdepth;
    int inWindow;
    bool reportedThisDepth;

    const bool isMultiPV = (RT == MultiPVSearch);
    chessposition *pos = &thr->pos;

    if (en.mate > 0)  // FIXME: Not tested for a long time.
    {
        thr->depth = maxdepth = en.mate * 2;
    }
    else
    {
        thr->lastCompleteDepth = 0;
        thr->depth = 1;
        if (en.maxdepth > 0)
            maxdepth = en.maxdepth;
        else
            maxdepth = MAXDEPTH;
    }

    alpha = SHRT_MIN + 1;
    beta = SHRT_MAX;

    // increment generation counter for tt aging
    tp.nextSearch();

    uint32_t lastBestMove = 0;
    int constantRootMoves = 0;
    bool bExitIteration;
    en.lastReport = 0;
    U64 nowtime;
    do
    {
        inWindow = 1;
        pos->seldepth = thr->depth;

        if (pos->rootmovelist.length == 0)
        {
            // mate / stalemate
            pos->bestmove[0].code = 0;
            score = pos->bestmovescore[0] =  (pos->isCheckbb ? SCOREBLACKWINS : SCOREDRAW);
            en.stopLevel = ENGINESTOPPED;
        }
        else if (pos->testRepetiton() >= 2 || pos->halfmovescounter >= 100)
        {
            // remis via repetition or 50 moves rule
            pos->bestmove[0].code = 0;
            pos->pondermove.code = 0;
            score = pos->bestmovescore[0] = SCOREDRAW;
            en.stopLevel = ENGINESTOPPED;
        }
        else
        {
            score = pos->rootsearch<RT>(alpha, beta, thr->depth);

            // new aspiration window
            if (score == alpha)
            {
                // research with lower alpha and reduced beta
                beta = (alpha + beta) / 2;
                alpha = max(SHRT_MIN + 1, alpha - deltaalpha);
                deltaalpha += deltaalpha / 4 + 2;
                if (abs(alpha) > 1000)
                    deltaalpha = SHRT_MAX << 1;
                inWindow = 0;
                reportedThisDepth = false;
            }
            else if (score == beta)
            {
                // research with higher beta
                beta = min(SHRT_MAX, beta + deltabeta);
                deltabeta += deltabeta / 4 + 2;
                if (abs(beta) > 1000)
                    deltabeta = SHRT_MAX << 1;
                inWindow = 2;
                reportedThisDepth = false;
            }
            else
            {
                thr->lastCompleteDepth = thr->depth;
                if (score >= en.terminationscore)
                {
                    // bench mode reached needed score
                    en.stopLevel = ENGINEWANTSTOP;
                }
                else {
                    // next depth with new aspiration window
                    deltaalpha = 8;
                    deltabeta = 8;
                    if (isMultiPV)
                        alpha = pos->bestmovescore[en.MultiPV - 1] - deltaalpha;
                    else
                        alpha = score - deltaalpha;
                    beta = score + deltabeta;
                }
            }
        }
        if (score > NOSCORE && thr->index == 0)
        {
            nowtime = getTime();

            // search was successfull
            if (isMultiPV)
            {
                if (inWindow == 1)
                {
                    // MultiPV output only if in aspiration window
                    // FIXME: This is a bit ugly... code more consistent with SinglePV would be better
                    // but I had to fight against performance regression so I devided it this way
                    int i = 0;
                    int maxmoveindex = min(en.MultiPV, pos->rootmovelist.length);
                    do
                    {
                        // The only case that bestmove is not set can happen if rootsearch hit the TP table
                        // so get bestmovecode from there
                        if (!pos->bestmove[i].code)
                        {
                            uint16_t mc;
                            int dummystaticeval;
                            tp.probeHash(pos->hash, &pos->bestmovescore[i], &dummystaticeval, &mc, thr->depth, alpha, beta, 0);
                            pos->bestmove[i].code = pos->shortMove2FullMove(mc);
                        }

                        uciScore(thr, inWindow, nowtime, i);

                        i++;
                    } while (i < maxmoveindex
                        && (pos->bestmove[i].code || (pos->bestmove[i].code = tp.getMoveCode(pos->hash)))
                        && pos->bestmovescore[i] > SHRT_MIN + 1);
                }
            }
            else {
                // The only two cases that bestmove is not set can happen if alphabeta hit the TP table or we are in TB
                // so get bestmovecode from there or it was a TB hit so just get the first rootmove
                if (!pos->bestmove[0].code)
                {
                    uint16_t mc = 0;
                    int dummystaticeval;
                    tp.probeHash(pos->hash, &score, &dummystaticeval, &mc, MAXDEPTH, alpha, beta, 0);
                    pos->bestmove[0].code = pos->shortMove2FullMove(mc);
                    pos->pondermove.code = 0;
                }
                    
                // still no bestmove...
                if (!pos->bestmove[0].code && pos->rootmovelist.length > 0)
                    pos->bestmove[0].code = pos->rootmovelist.move[0].code;

                uciScore(thr, inWindow, nowtime, 0);
            }
        }
        if (inWindow == 1)
        {
            // Skip some depths depending on current depth and thread number using Laser's method
            int cycle = thr->index % 16;
            if (thr->index && (thr->depth + cycle) % SkipDepths[cycle] == 0)
                thr->depth += SkipSize[cycle];

            thr->depth++;
            reportedThisDepth = true;
            constantRootMoves++;
            if (lastBestMove != pos->bestmove[0].code)
            {
                lastBestMove = pos->bestmove[0].code;
                constantRootMoves = 0;
            }
            resetEndTime(constantRootMoves);
        }

        // early exit in playing mode as there is exactly one possible move
        bExitIteration = (pos->rootmovelist.length == 1 && thr->depth > 4 && en.endtime1 && !en.isPondering());

        // early exit in TB win/lose position
        bExitIteration = bExitIteration || (pos->tbPosition && abs(score) >= SCORETBWIN - 100);

        // exit if STOPSOON is requested and we're in aspiration window
        bExitIteration = bExitIteration || (en.stopLevel == ENGINESTOPSOON && inWindow == 1);

        // exit if STOPIMMEDIATELY
        bExitIteration = bExitIteration || (en.stopLevel == ENGINESTOPIMMEDIATELY);

        // exit if max depth is reached
        bExitIteration = bExitIteration || (thr->depth > maxdepth);

    } while (!bExitIteration);
    
    if (thr->index == 0)
    {
        // Output of best move
        searchthread *bestthr = thr;
        int bestscore = bestthr->pos.bestmovescore[0];
        for (int i = 1; i < en.Threads; i++)
        {
            // search for a better score in the other threads
            searchthread *hthr = &en.sthread[i];
            if (hthr->lastCompleteDepth >= bestthr->lastCompleteDepth
                && hthr->pos.bestmovescore[0] > bestscore)
            {
                bestscore = hthr->pos.bestmovescore[0];
                bestthr = hthr;
            }
        }
        if (bestthr->index)
        {
            // copy best moves and score from best thread to thread 0
            pos->bestmove[0] = bestthr->pos.bestmove[0];
            pos->pondermove = bestthr->pos.pondermove;
            pos->bestmovescore[0] = bestthr->pos.bestmovescore[0];
            inWindow = 1;
        }
        if (!reportedThisDepth || bestthr->index)
            uciScore(thr, inWindow, getTime(), 0);

        string strBestmove;
        string strPonder = "";

        if (!pos->bestmove[0].code)
        {
            // Not enough time to get any bestmove? Fall back to default move
            pos->bestmove[0] = pos->defaultmove;
            pos->pondermove.code = 0;
        }

        strBestmove = pos->bestmove[0].toString();

        if (en.ponder)
        {
            if (!pos->pondermove.code)
            {
                // Get the ponder move from TT
                pos->playMove(&pos->bestmove[0]);
                uint16_t pondershort = tp.getMoveCode(pos->hash);
                pos->pondermove.code = pos->shortMove2FullMove(pondershort);
                pos->unplayMove(&pos->bestmove[0]);
            }
            if (pos->pondermove.code)
                strPonder = " ponder " + pos->pondermove.toString();
        }

        cout << "bestmove " + strBestmove + strPonder + "\n";

        en.stopLevel = ENGINESTOPPED;

    }

    // Remember some exit values for benchmark output
    en.benchscore = score;
    en.benchdepth = thr->depth - 1;
}


void resetEndTime(int constantRootMoves, bool complete)
{
    int timetouse = (en.isWhite ? en.wtime : en.btime);
    int timeinc = (en.isWhite ? en.winc : en.binc);
    int overhead = en.moveOverhead * en.Threads;

    if (en.movestogo)
    {
        // should garantee timetouse > 0
        // stop soon at 0.5...1.5 x average movetime
        // stop immediately at 1.3...2.3 x average movetime
        int f1 = max(5, 15 - constantRootMoves * 2);
        int f2 = max(13, 23 - constantRootMoves * 2);
        if (complete)
            en.endtime1 = en.starttime + timetouse * en.frequency * f1 / (en.movestogo + 1) / 10000;
        en.endtime2 = en.starttime + min(max(0, timetouse - overhead * en.movestogo), f2 * timetouse / (en.movestogo + 1) / 10) * en.frequency / 1000;
        //printf("info string difftime1=%lld  difftime2=%lld\n", (endtime1 - en.starttime) * 1000 / en.frequency , (endtime2 - en.starttime) * 1000 / en.frequency);
    }
    else if (timetouse) {
        int ph = en.sthread[0].pos.phase();
        if (timeinc)
        {
            // sudden death with increment; split the remaining time in (256-phase) timeslots
            // stop soon after 6..10 timeslot
            // stop immediately after 10..18 timeslots
            int f1 = max(6, 10 - constantRootMoves);
            int f2 = max(10, 18 - constantRootMoves);
            if (complete)
                en.endtime1 = en.starttime + max(timeinc, f1 * (timetouse + timeinc) / (256 - ph)) * en.frequency / 1000;
            en.endtime2 = en.starttime + min(max(0, timetouse - overhead), max(timeinc, f2 * (timetouse + timeinc) / (256 - ph))) * en.frequency / 1000;
        }
        else {
            // sudden death without increment; play for another x;y moves
            // stop soon at 1/35...1/45 time slot
            // stop immediately at 1/20...1/30 time slot
            int f1 = min(45, 35 + constantRootMoves * 2);
            int f2 = min(30, 20 + constantRootMoves * 2);
            if (complete)
                en.endtime1 = en.starttime + timetouse / f1 * en.frequency / 1000;
            en.endtime2 = en.starttime + min(max(0, timetouse - overhead), timetouse / f2) * en.frequency / 1000;
        }
    }
    else {
        en.endtime1 = en.endtime2 = 0;
    }
}


void startSearchTime(bool complete = true)
{
    en.starttime = getTime();
    resetEndTime(complete, 0);
}


void searchguide()
{
    startSearchTime();

    en.moveoutput = false;
    en.tbhits = 0;
    en.fh = en.fhf = 0;

    for (int tnum = 0; tnum < en.Threads; tnum++)
    {
        if (en.MultiPV > 1)
            en.sthread[tnum].thr = thread(&search_gen1<MultiPVSearch>, &en.sthread[tnum]);
        else
            en.sthread[tnum].thr = thread(&search_gen1<SinglePVSearch>, &en.sthread[tnum]);
    }

    U64 nowtime;
    while (en.stopLevel != ENGINESTOPPED)
    {
        nowtime = getTime();

        if (nowtime - en.starttime > 3 * en.frequency)
            en.moveoutput = true;

        if (en.stopLevel < ENGINESTOPPED)
        {
            if (en.isPondering())
            {
                Sleep(10);
            }
            else if (en.testPonderHit())
            {
                startSearchTime(false);
                en.resetPonder();
            }
            else if (en.endtime2 && nowtime >= en.endtime2 && en.stopLevel < ENGINESTOPIMMEDIATELY)
            {
                en.stopLevel = ENGINESTOPIMMEDIATELY;
            }
            else if (en.maxnodes && en.maxnodes <= en.getTotalNodes() && en.stopLevel < ENGINESTOPIMMEDIATELY)
            {
                en.stopLevel = ENGINESTOPIMMEDIATELY;
            }
            else if (en.endtime1 && nowtime >= en.endtime2 && en.stopLevel < ENGINESTOPSOON)
            {
                en.stopLevel = ENGINESTOPSOON;
                Sleep(10);
            }
            else {
                Sleep(10);
            }
        }
    }

    // Make the other threads stop now
    en.stopLevel = ENGINESTOPIMMEDIATELY;
    for (int tnum = 0; tnum < en.Threads; tnum++)
        en.sthread[tnum].thr.join();
    en.stopLevel = ENGINETERMINATEDSEARCH;
}
