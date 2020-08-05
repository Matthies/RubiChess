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

#ifdef STATISTICS
statistic statistics;
#endif

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
            reductiontable[0][d][m] = 1 + (int)round(log(d * 1.5) * log(m) * 0.60);
            // reduction for improving positions
            reductiontable[1][d][m] = (int)round(log(d * 1.5) * log(m * 2) * 0.43);
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
            cmptr[i] = (int16_t*)counterhistory[GETPIECE(c)][GETCORRECTTO(c)];
        else
            cmptr[i] = NULL;
    }
}

inline int chessposition::getHistory(uint32_t code, int16_t **cmptr)
{
    int pc = GETPIECE(code);
    int s2m = pc & S2MMASK;
    int from = GETFROM(code);
    int to = GETCORRECTTO(code);
    int value = history[s2m][from][to];
    for (int i = 0; i < CMPLIES; i++)
        if (cmptr[i])
            value += cmptr[i][pc * 64 + to];

    return value;
}


#define HISTORYMAXDEPTH 20
#define HISTORYAGESHIFT 8
#define HISTORYNEWSHIFT 5

inline void chessposition::updateHistory(uint32_t code, int16_t **cmptr, int value)
{
    int pc = GETPIECE(code);
    int s2m = pc & S2MMASK;
    int from = GETFROM(code);
    int to = GETCORRECTTO(code);
    value = max(-HISTORYMAXDEPTH * HISTORYMAXDEPTH, min(HISTORYMAXDEPTH * HISTORYMAXDEPTH, value));

    int delta = value * (1 << HISTORYNEWSHIFT) - history[s2m][from][to] * abs(value) / (1 << HISTORYAGESHIFT);
    myassert(history[s2m][from][to] + delta < MAXINT16 && history[s2m][from][to] + delta > MININT16, this, 2, history[s2m][from][to], delta);

    history[s2m][from][to] += delta;

    for (int i = 0; i < CMPLIES; i++)
        if (cmptr[i]) {
            delta = value * (1 << HISTORYNEWSHIFT) - cmptr[i][pc * 64 + to] * abs(value) / (1 << HISTORYAGESHIFT);
            cmptr[i][pc * 64 + to] += delta;
        }
}


int chessposition::getQuiescence(int alpha, int beta, int depth)
{
    int score;
    int bestscore = NOSCORE;
    bool myIsCheck = (bool)isCheckbb;
#ifdef EVALTUNE
    if (depth < 0) isQuiet = false;
    positiontuneset targetpts;
    evalparam myev[NUMOFEVALPARAMS];
    if (noQs)
    {
        // just evaluate and return (for tuning sets with just quiet positions)
        score = S2MSIGN(state & S2MMASK) * getEval<NOTRACE>();
        getPositionTuneSet(&targetpts, &myev[0]);
        copyPositionTuneSet(&targetpts, &myev[0], &this->pts, &this->ev[0]);
        return score;
    }

    bool foundpts = false;
#endif

    // FIXME: Should quiescience nodes count for the statistics?
    //en.nodes++;

    // Reset pv
    pvtable[ply][0] = 0;

#ifdef SDEBUG
    chessmove debugMove;
    bool isDebugPv = triggerDebug(&debugMove);
    SDEBUGDO(isDebugPv, pvaborttype[ply + 1] = PVA_UNKNOWN; pvdepth[ply] = depth; pvmovenum[ply] = -1;);
#endif

    STATISTICSINC(qs_n[myIsCheck]);

    int hashscore = NOSCORE;
    uint16_t hashmovecode = 0;
    int staticeval = NOSCORE;
    bool tpHit = tp.probeHash(hash, &hashscore, &staticeval, &hashmovecode, depth, alpha, beta, ply);
    if (tpHit)
    {
        STATISTICSINC(qs_tt);
        return hashscore;
    }

    if (!myIsCheck)
    {
#ifdef EVALTUNE
        staticeval = S2MSIGN(state & S2MMASK) * getEval<NOTRACE>();
#else
        // get static evaluation of the position
        if (staticeval == NOSCORE)
        {
            if (movestack[mstop - 1].movecode == 0)
                staticeval = -staticevalstack[mstop - 1] + CEVAL(eps.eTempo, 2);
            else
                staticeval = S2MSIGN(state & S2MMASK) * getEval<NOTRACE>();
        }
#endif

        bestscore = staticeval;
        if (staticeval >= beta)
        {
            STATISTICSINC(qs_pat);
            tp.addHash(hash, staticeval, staticeval, HASHBETA, 0, 0);

            return staticeval;
        }
        if (staticeval > alpha)
        {
#ifdef EVALTUNE
            getPositionTuneSet(&targetpts, &myev[0]);
            foundpts = true;
#endif
            alpha = staticeval;
        }

        // Delta pruning
        int bestExpectableScore = staticeval + deltapruningmargin + getBestPossibleCapture();
        if (bestExpectableScore < alpha)
        {
            STATISTICSINC(qs_delta);
            tp.addHash(hash, bestExpectableScore, staticeval, HASHALPHA, 0, 0);
            return staticeval;
        }
    }

    prepareStack();

    MoveSelector ms = {};
    ms.SetPreferredMoves(this);
    STATISTICSINC(qs_loop_n);

    uint32_t bestcode = 0;
    int eval_type = HASHALPHA;
    chessmove *m;

    while ((m = ms.next()))
    {
        if (!myIsCheck && staticeval + materialvalue[GETCAPTURE(m->code) >> 1] + deltapruningmargin <= alpha)
        {
            // Leave out capture that is delta-pruned
            STATISTICSINC(qs_move_delta);
            continue;
        }

        if (!playMove(m))
            continue;

        STATISTICSINC(qs_moves);
        ms.legalmovenum++;
        score = -getQuiescence(-beta, -alpha, depth - 1);
        unplayMove(m);
        if (score > bestscore)
        {
            bestscore = score;
            bestcode = m->code;
            if (score >= beta)
            {
                STATISTICSINC(qs_moves_fh);
                tp.addHash(hash, score, staticeval, HASHBETA, 0, (uint16_t)bestcode);
                return score;
            }
            if (score > alpha)
            {
                updatePvTable(m->code, true);
                eval_type = HASHEXACT;
                alpha = score;
#ifdef EVALTUNE
                foundpts = true;
                copyPositionTuneSet(&this->pts, &this->ev[0], &targetpts, &myev[0]);
#endif
            }
        }
    }
#ifdef EVALTUNE
    if (foundpts)
        copyPositionTuneSet(&targetpts, &myev[0], &this->pts, &this->ev[0]);
#endif

    if (myIsCheck && !ms.legalmovenum)
        // It's a mate
        return SCOREBLACKWINS + ply;

    tp.addHash(hash, alpha, staticeval, eval_type, 0, (uint16_t)bestcode);
    return bestscore;
}



int chessposition::alphabeta(int alpha, int beta, int depth)
{
    int score;
    int hashscore = NOSCORE;
    uint16_t hashmovecode = 0;
    int staticeval = NOSCORE;
    int bestscore = NOSCORE;
    uint32_t bestcode = 0;
    int eval_type = HASHALPHA;
    chessmove *m;
    int extendall = 0;
    int effectiveDepth;
    const bool PVNode = (alpha != beta - 1);

    nodes++;
    CheckForImmediateStop();

    // Reset pv
    pvtable[ply][0] = 0;

    STATISTICSINC(ab_n);
    STATISTICSADD(ab_pv, PVNode);

    // test for remis via repetition
    int rep = testRepetiton();
    if (rep >= 2)
    {
        STATISTICSINC(ab_draw_or_win);
        return SCOREDRAW;
    }

    // test for remis via 50 moves rule
    if (halfmovescounter >= 100)
    {
        STATISTICSINC(ab_draw_or_win);
        if (!isCheckbb)
        {
            return SCOREDRAW;
        } else {
            // special case: test for checkmate
            chessmovelist evasions;
            if (CreateEvasionMovelist(this, &evasions.move[0]) > 0)
                return SCOREDRAW;
            else
                return SCOREBLACKWINS + ply;
        }
    }

    if (en.stopLevel == ENGINESTOPIMMEDIATELY)
    {
        // time is over; immediate stop requested
        return beta;
    }

    // Reached depth? Do a qsearch
    if (depth <= 0)
    {
        // update selective depth info
        if (seldepth < ply + 1)
            seldepth = ply + 1;

        STATISTICSINC(ab_qs);
        return getQuiescence(alpha, beta, depth);
    }

    // Maximum depth
    if (mstop >= MAXDEPTH - MOVESTACKRESERVE)
        return getQuiescence(alpha, beta, depth);


    // Get move for singularity check and change hash to seperate partial searches from full searches
    uint16_t excludeMove = excludemovestack[mstop - 1];
    excludemovestack[mstop] = 0;
    U64 newhash = hash ^ excludeMove;

#ifdef SDEBUG
    chessmove debugMove;
    bool isDebugPv = !excludeMove && triggerDebug(&debugMove);
    bool debugMovePlayed = false;
    int isDebugPosition = tp.isDebugPosition(newhash);
    bool debugTransposition = (isDebugPosition >= 0 && !isDebugPv);
    SDEBUGDO(isDebugPv, pvaborttype[ply + 1] = PVA_UNKNOWN; pvdepth[ply] = depth; pvalpha[ply] = alpha; pvbeta[ply] = beta; pvmovenum[ply] = 0;);
#endif

    bool tpHit = tp.probeHash(newhash, &hashscore, &staticeval, &hashmovecode, depth, alpha, beta, ply);
    if (tpHit)
    {
        if (!rep)
        {
            // not a single repetition; we can (almost) safely trust the hash value
            uint32_t fullhashmove = shortMove2FullMove(hashmovecode);
            if (fullhashmove)
                updatePvTable(fullhashmove, false);

            if (!PVNode)
            {
                STATISTICSINC(ab_tt);
                SDEBUGDO(isDebugPv, pvabortval[ply] = hashscore; if (debugMove.code == fullhashmove) pvaborttype[ply] = PVA_FROMTT; else pvaborttype[ply] =  PVA_DIFFERENTFROMTT; );
                SDEBUGDO(isDebugPv, pvadditionalinfo[ply] = "PV = " + getPv(pvtable[ply]) + "  " + tp.debugGetPv(newhash); );
                return hashscore;
            }
        }
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
            if (v <= -1 - en.Syzygy50MoveRule) {
                bound = HASHALPHA;
                score = -SCORETBWIN + ply;
            }
            else if (v >= 1 + en.Syzygy50MoveRule) {
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
            }
            STATISTICSINC(ab_tb);
            return score;
        }
    }

    // Check extension
    if (isCheckbb)
        extendall = 1;

    prepareStack();

    // get static evaluation of the position
    if (staticeval == NOSCORE)
    {
        if (movestack[mstop - 1].movecode == 0)
            // just reverse the staticeval before the null move respecting the tempo
            staticeval = -staticevalstack[mstop - 1] + CEVAL(eps.eTempo, 2);
        else
            staticeval = S2MSIGN(state & S2MMASK) * getEval<NOTRACE>();
    }
    staticevalstack[mstop] = staticeval;

    bool positionImproved = (mstop >= rootheight + 2
        && staticevalstack[mstop] > staticevalstack[mstop - 2]);

    // Razoring
    if (!PVNode && !isCheckbb && depth <= 2)
    {
        const int ralpha = alpha - 250 - depth * 50;
        if (staticeval < ralpha)
        {
            int qscore;
            if (depth == 1 && ralpha < alpha)
            {
                qscore = getQuiescence(alpha, beta, depth);
                SDEBUGDO(isDebugPv, pvabortval[ply] = qscore; pvaborttype[ply] = PVA_RAZORPRUNED;);
                return qscore;
            }
            qscore = getQuiescence(ralpha, ralpha + 1, depth);
            if (qscore <= ralpha)
            {
                SDEBUGDO(isDebugPv, pvabortval[ply] = qscore; pvaborttype[ply] = PVA_RAZORPRUNED;);
                return qscore;
            }
        }
    }

    // futility pruning
    bool futility = false;
    if (depth <= 8)
    {
        // reverse futility pruning
        if (!isCheckbb && staticeval - depth * (70 - 20 * positionImproved) > beta)
        {
            STATISTICSINC(prune_futility);
            SDEBUGDO(isDebugPv, pvabortval[ply] = staticeval; pvaborttype[ply] = PVA_REVFUTILITYPRUNED;);
            return staticeval;
        }
        futility = (staticeval < alpha - (100 + 80 * depth));
    }

    // Nullmove pruning with verification like SF does it
    int bestknownscore = (hashscore != NOSCORE ? hashscore : staticeval);
    if (!isCheckbb && depth >= 2 && bestknownscore >= beta && (ply  >= nullmoveply || ply % 2 != nullmoveside) && ph < 255)
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
                STATISTICSINC(prune_nm);
                SDEBUGDO(isDebugPv, pvabortval[ply] = score; pvaborttype[ply] = PVA_NMPRUNED;);
                return score;
            }
            // Verification search
            nullmoveply = ply + 3 * (depth - R) / 4;
            nullmoveside = ply % 2;
            int verificationscore = alphabeta(beta - 1, beta, depth - R);
            nullmoveside = nullmoveply = 0;
            if (verificationscore >= beta) {
                STATISTICSINC(prune_nm);
                SDEBUGDO(isDebugPv, pvabortval[ply] = score; pvaborttype[ply] = PVA_NMPRUNED;);
                return score;
            }
        }
    }

    // ProbCut
    const int probcutmargin = 100;
    if (!PVNode && depth >= 5 && abs(beta) < SCOREWHITEWINS)
    {
        int rbeta = min(SCOREWHITEWINS, beta + probcutmargin);
        chessmovelist *movelist = new chessmovelist;
        movelist->length = CreateMovelist<TACTICAL>(this, &movelist->move[0]);

        for (int i = 0; i < movelist->length; i++)
        {
            chessmove* cm = &movelist->move[i];

            if (!see(cm->code, rbeta - staticeval))
                continue;

            if (playMove(cm))
            {
                int probcutscore = -getQuiescence(-rbeta, -rbeta + 1, 0);
                if (probcutscore >= rbeta)
                    probcutscore = -alphabeta(-rbeta, -rbeta + 1, depth - 4);

                unplayMove(cm);

                if (probcutscore >= rbeta)
                {
                    // ProbCut off
                    delete movelist;
                    STATISTICSINC(prune_probcut);
                    SDEBUGDO(isDebugPv, pvabortval[ply] = probcutscore; pvaborttype[ply] = PVA_PROBCUTPRUNED; pvadditionalinfo[ply] = "pruned by " + movelist->move[i].toString(););
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
        alphabeta(alpha, beta, depth - iiddelta);
        hashmovecode = tp.getMoveCode(newhash);
    }

    // Get possible countermove from table
    uint32_t lastmove = movestack[mstop - 1].movecode;
    uint32_t counter = 0;
    if (lastmove)
        counter = countermove[GETPIECE(lastmove)][GETCORRECTTO(lastmove)];

    // Reset killers for child ply
    killer[ply + 1][0] = killer[ply + 1][1] = 0;

    MoveSelector ms = {};
    ms.SetPreferredMoves(this, hashmovecode, killer[ply][0], killer[ply][1], counter, excludeMove);
    STATISTICSINC(moves_loop_n);

    int legalMoves = 0;
    int quietsPlayed = 0;
    uint32_t quietMoves[MAXMOVELISTLENGTH];
    while ((m = ms.next()))
    {
        ms.legalmovenum++;
#ifdef SDEBUG
        bool isDebugMove = (debugMove.code == m->code);
        SDEBUGDO(isDebugMove, pvmovenum[ply] = legalMoves + 1;);
        SDEBUGDO((isDebugPv && pvmovenum[ply] <= 0), pvmovenum[ply] = -(legalMoves + 1););
#endif
        STATISTICSINC(moves_n[(bool)ISTACTICAL(m->code)]);
        // Leave out the move to test for singularity
        if ((m->code & 0xffff) == excludeMove)
            continue;

        // Late move pruning
        if (depth < MAXLMPDEPTH && !ISTACTICAL(m->code) && bestscore > NOSCORE && quietsPlayed > lmptable[positionImproved][depth])
        {
            // Proceed to next moveselector state manually to save some time
            ms.state++;
            STATISTICSINC(moves_pruned_lmp);
            SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_LMPRUNED;);
            continue;
        }

        // Check for futility pruning condition for this move and skip move if at least one legal move is already found
        bool futilityPrune = futility && !ISTACTICAL(m->code) && !isCheckbb && alpha <= 900 && !moveGivesCheck(m->code);
        if (futilityPrune)
        {
            if (legalMoves)
            {
                STATISTICSINC(moves_pruned_futility);
                SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_FUTILITYPRUNED;);
                continue;
            }
            else if (staticeval > bestscore)
            {
                // Use the static score from futility test as a bestscore start value
                bestscore = staticeval;
            }
        }

        // Prune moves with bad SEE
        if (!isCheckbb && depth < 9 && bestscore > NOSCORE && ms.state >= QUIETSTATE && !see(m->code, -20 * depth * (ISTACTICAL(m->code) ? depth : 4)))
        {
            STATISTICSINC(moves_pruned_badsee);
            SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_SEEPRUNED;);
            continue;
        }

        int stats = getHistory(m->code, ms.cmptr);
        int extendMove = 0;
        int pc = GETPIECE(m->code);
        int to = GETCORRECTTO(m->code);

        // Singular extension
        if ((m->code & 0xffff) == hashmovecode
            && depth > 7
            && !excludeMove
            && tp.probeHash(newhash, &hashscore, &staticeval, &hashmovecode, depth - 3, alpha, beta, ply)  // FIXME: maybe needs hashscore = FIXMATESCOREPROBE(hashscore, ply);
            && hashscore > alpha)
        {
            excludemovestack[mstop - 1] = hashmovecode;
            int sBeta = max(hashscore - 2 * depth, SCOREBLACKWINS);
            int redScore = alphabeta(sBeta - 1, sBeta, depth / 2);
            excludemovestack[mstop - 1] = 0;

            if (redScore < sBeta)
            {
                // Move is singular
                STATISTICSINC(extend_singular);
                extendMove = 1;
            }
            else if (bestknownscore >= beta && sBeta >= beta)
            {
                // Hashscore for lower depth and static eval cut and we have at least a second good move => lets cut here
                STATISTICSINC(prune_multicut);
                SDEBUGDO(isDebugPv, pvabortval[ply] = sBeta; pvaborttype[ply] = PVA_MULTICUT;);
                return sBeta;
            }
        }
        // Extend captures that lead into endgame
        else if (ph > 200 && GETCAPTURE(m->code) >= WKNIGHT)
        {
            STATISTICSINC(extend_endgame);
            extendMove = 1;
        }
        else if(!ISTACTICAL(m->code) && ms.cmptr[0] && ms.cmptr[1])
        {
            if (ms.cmptr[0][pc * 64 + to] > he_threshold && ms.cmptr[1][pc * 64 + to] > he_threshold)
            {
                STATISTICSINC(extend_history);
                extendMove = 1;
                he_yes++;
            }
            if ((++he_all & 0x3fffff) == 0)
            {
                // adjust history extension threshold
                if (he_all / 512 < he_yes)
                {
                    // 1/512 ~ extension ratio > 0.1953% ==> increase threshold
                    he_threshold = he_threshold * 257 / 256;
                    he_all = he_yes = 0ULL;
                } else if (he_all / 32768 > he_yes)
                {
                    // 1/32768 ~ extension ratio < 0.0030% ==> decrease threshold
                    he_threshold = he_threshold * 255 / 256;
                    he_all = he_yes = 0ULL;
                }
            }
        }

        // Late move reduction
        int reduction = 0;
        if (depth > 2 && !ISTACTICAL(m->code))
        {
            reduction = reductiontable[positionImproved][depth][min(63, legalMoves + 1)];

            // adjust reduction by stats value
            reduction -= stats / 5000;

            // adjust reduction at PV nodes
            reduction -= PVNode;

            // adjust reduction with opponents move number
            reduction -= (LegalMoves[ply] > 15);

            STATISTICSINC(red_pi[positionImproved]);
            STATISTICSADD(red_lmr[positionImproved], reductiontable[positionImproved][depth][min(63, legalMoves + 1)]);
            STATISTICSADD(red_history, -stats / 5000);
            STATISTICSADD(red_pv, -(int)PVNode);
            STATISTICSDO(int red0 = reduction);

            reduction = min(depth, max(0, reduction));

            STATISTICSDO(int red1 = reduction);
            STATISTICSADD(red_correction, red1 - red0);
            STATISTICSADD(red_total, reduction);
        }

        effectiveDepth = depth + extendall - reduction + extendMove;

        // Prune moves with bad counter move history
        if (!ISTACTICAL(m->code) && effectiveDepth < 4
            && ms.cmptr[0] && ms.cmptr[0][pc * 64 + to] < 0
            && ms.cmptr[1] && ms.cmptr[1][pc * 64 + to] < 0)
        {
            SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_BADHISTORYPRUNED;);
            continue;
        }

        if (!playMove(m))
            continue;

        legalMoves++;
        SDEBUGDO(isDebugMove, debugMovePlayed = true;)

        // Check again for futility pruning now that we found a valid move
        if (futilityPrune)
        {
            unplayMove(m);
            SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_FUTILITYPRUNED;);
            continue;
        }

        STATISTICSINC(moves_played[(bool)ISTACTICAL(m->code)]);

        LegalMoves[ply] = ms.legalmovenum;
        SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] = ""; );

        if (reduction)
        {
            // LMR search; test against alpha
            score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            if (score > alpha)
            {
                // research without reduction
                effectiveDepth += reduction;
                score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1);
                SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            }
        }
        else if (!PVNode || legalMoves > 1)
        {
            // Np PV node or not the first move; test against alpha
            score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
        }
        // (re)search with full window at PV nodes if necessary
        if (PVNode && (legalMoves == 1 || score > alpha)) {
            score = -alphabeta(-beta, -alpha, effectiveDepth - 1);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha)+ ",beta=" +to_string(beta) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
        }
        SDEBUGDO(isDebugMove, pvadditionalinfo[ply - 1] += "score=" + to_string(score) + "  "; );
        unplayMove(m);

        if (en.stopLevel == ENGINESTOPIMMEDIATELY)
        {
            // time is over; immediate stop requested
            return beta;
        }

        SDEBUGDO(isDebugMove, pvabortval[ply] = score;);
        SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_BELOWALPHA; pvadditionalinfo[ply] += "Alpha=" + to_string(alpha) + " at this point."; );
        if (score > bestscore)
        {
            bestscore = score;
            bestcode = m->code;

            if (score >= beta)
            {
                if (!ISTACTICAL(m->code))
                {
                    updateHistory(m->code, ms.cmptr, depth * depth);
                    for (int i = 0; i < quietsPlayed; i++)
                    {
                        uint32_t qm = quietMoves[i];
                        updateHistory(qm, ms.cmptr, -(depth * depth));
                    }

                    // Killermove
                    if (killer[ply][0] != m->code)
                    {
                        killer[ply][1] = killer[ply][0];
                        killer[ply][0] = m->code;
                    }

                    // save countermove
                    if (lastmove)
                        countermove[GETPIECE(lastmove)][GETCORRECTTO(lastmove)] = m->code;
                }

                STATISTICSINC(moves_fail_high);

                if (!excludeMove)
                    tp.addHash(newhash, FIXMATESCOREADD(score, ply), staticeval, HASHBETA, effectiveDepth, (uint16_t)bestcode);

                SDEBUGDO(isDebugPv, pvaborttype[ply] = isDebugMove ? PVA_BETACUT : debugMovePlayed ? PVA_NOTBESTMOVE : PVA_OMITTED;);
                SDEBUGDO(isDebugPv || debugTransposition, tp.debugSetPv(newhash, movesOnStack() + " " + (debugTransposition ? "(transposition)" : "") + " effectiveDepth=" + to_string(effectiveDepth)););
                return score;   // fail soft beta-cutoff
            }

            if (score > alpha)
            {
                SDEBUGDO(isDebugPv, pvaborttype[ply] = isDebugMove ? PVA_BESTMOVE : debugMovePlayed ? PVA_NOTBESTMOVE : PVA_OMITTED;);
                alpha = score;
                eval_type = HASHEXACT;
                updatePvTable(bestcode, true);
            }
        }

        if (!ISTACTICAL(m->code))
            quietMoves[quietsPlayed++] = m->code;
    }

    if (legalMoves == 0)
    {
        if (excludeMove)
            return alpha;

        STATISTICSINC(ab_draw_or_win);
        if (isCheckbb) {
            // It's a mate
            return SCOREBLACKWINS + ply;
        }
        else {
            // It's a stalemate
            return SCOREDRAW;
        }
    }

    if (bestcode && !excludeMove)
    {
        tp.addHash(newhash, FIXMATESCOREADD(bestscore, ply), staticeval, eval_type, depth, (uint16_t)bestcode);
        SDEBUGDO(isDebugPv || debugTransposition, tp.debugSetPv(newhash, movesOnStack() + " " + (debugTransposition ? "(transposition)" : "") + " depth=" + to_string(depth)););
    }

    return bestscore;
}



template <RootsearchType RT>
int chessposition::rootsearch(int alpha, int beta, int depth, int inWindowLast)
{
    int score;
    uint16_t hashmovecode = 0;
    int bestscore = NOSCORE;
    int staticeval = NOSCORE;
    int eval_type = HASHALPHA;
    chessmove *m;
    int lastmoveindex;
    int maxmoveindex;

    const bool isMultiPV = (RT == MultiPVSearch);

    nodes++;
    CheckForImmediateStop();

    // reset pv
    pvtable[0][0] = 0;

    if (isMultiPV)
    {
        lastmoveindex = 0;
        maxmoveindex = min(en.MultiPV, rootmovelist.length);
        for (int i = 0; i < maxmoveindex; i++)
        {
            multipvtable[i][0] = 0;
            bestmovescore[i] = NOSCORE;
        }
    }

#ifdef SDEBUG
    chessmove debugMove;
    bool isDebugPv = triggerDebug(&debugMove);
    bool debugMovePlayed = false;
    SDEBUGDO(isDebugPv, pvaborttype[1] = PVA_UNKNOWN; pvdepth[0] = depth; pvalpha[0] = alpha; pvbeta[0] = beta; pvmovenum[ply] = 0;);
#endif

    if (!isMultiPV
        && !useRootmoveScore
        && tp.probeHash(hash, &score, &staticeval, &hashmovecode, depth, alpha, beta, 0))
    {
        // Hash is fixed regarding scores that don't see actual 3folds so we can trust the entry
        uint32_t fullhashmove = shortMove2FullMove(hashmovecode);
        if (fullhashmove)
        {
            if (bestmove.code != fullhashmove) {
                bestmove.code = fullhashmove;
                pondermove.code = 0;
            }
            updatePvTable(fullhashmove, false);
            if (score > alpha) bestmovescore[0] = score;
            if (score > NOSCORE)
            {
                SDEBUGDO(isDebugPv, pvabortval[ply] = score; if (debugMove.code == fullhashmove) pvaborttype[ply] = PVA_FROMTT; else pvaborttype[ply] = PVA_DIFFERENTFROMTT; );
                SDEBUGDO(isDebugPv, pvadditionalinfo[ply] = "PV = " + getPv(pvtable[ply]) + "  " + tp.debugGetPv(hash); );
                return score;
            }
        }
    }

    if (!tbPosition)
    {
        // Reset move values
        for (int i = 0; i < rootmovelist.length; i++)
        {
            m = &rootmovelist.move[i];

            //PV moves gets top score
            if (hashmovecode == (m->code & 0xffff))
                m->value = PVVAL;
            else if (bestFailingLow == m->code)
                m->value = KILLERVAL2 - 1;
            // killermoves gets score better than non-capture
            else if (killer[0][0] == m->code)
                m->value = KILLERVAL1;
            else if (killer[0][1] == m->code)
                m->value = KILLERVAL2;
            else if (GETCAPTURE(m->code) != BLANK)
                m->value = (mvv[GETCAPTURE(m->code) >> 1] | lva[GETPIECE(m->code) >> 1]);
            else 
                m->value = history[state & S2MMASK][GETFROM(m->code)][GETCORRECTTO(m->code)];
        }
    }

    // get static evaluation of the position
    if (staticeval == NOSCORE)
        staticeval = S2MSIGN(state & S2MMASK) * getEval<NOTRACE>();
    staticevalstack[mstop] = staticeval;

    int quietsPlayed = 0;
    uint32_t quietMoves[MAXMOVELISTLENGTH];

    // FIXME: Dummy move selector for now; only used to pass null cmptr to updateHistory
    MoveSelector ms = {};

    for (int i = 0; i < rootmovelist.length; i++)
    {
        for (int j = i + 1; j < rootmovelist.length; j++)
            if (rootmovelist.move[i] < rootmovelist.move[j])
                swap(rootmovelist.move[i], rootmovelist.move[j]);

        m = &rootmovelist.move[i];
#ifdef SDEBUG
        bool isDebugMove = (debugMove.code == m->code);
        SDEBUGDO(isDebugMove, pvmovenum[0] = i + 1; debugMovePlayed = true;)
        SDEBUGDO(pvmovenum[0] <= 0, pvmovenum[0] = -(i + 1););
#endif
        playMove(m);

#ifndef SDEBUG
        if (en.moveoutput && !threadindex && (en.pondersearch != PONDERING || depth < MAXDEPTH - 1))
        {
            char s[256];
            sprintf_s(s, "info depth %d currmove %s currmovenumber %d\n", depth, m->toString().c_str(), i + 1);
            cout << s;
        }
#endif
        int reduction = 0;

        // Late move reduction
        if (!ISTACTICAL(m->code))
        {
            reduction = reductiontable[1][depth][min(63, i + 1)] + inWindowLast - 1;
            STATISTICSINC(red_pi[1]);
            STATISTICSADD(red_lmr[1], reductiontable[1][depth][min(63, i + 1)] + inWindowLast - 1);
        }

        int effectiveDepth = depth - reduction;
        SDEBUGDO(isDebugMove, pvadditionalinfo[ply - 1] = ""; );

        if (i > 0)
        {
            // LMR search; test against alpha
            score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply - 1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            if (reduction && score > alpha)
            {
                // research without reduction
                effectiveDepth += reduction;
                score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1);
                SDEBUGDO(isDebugMove, pvadditionalinfo[ply - 1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            }
        }
        // (re)search with full window if necessary
        if (i == 0 || score > alpha) {
            score = -alphabeta(-beta, -alpha, effectiveDepth - 1);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply - 1] += "PVS(alpha=" + to_string(alpha) + ",beta=" + to_string(beta) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
        }

        SDEBUGDO(isDebugMove, pvabortval[0] = score;)

        unplayMove(m);

        if (en.stopLevel == ENGINESTOPIMMEDIATELY)
        {
            // time over; immediate stop requested
            return bestscore;
        }

        if (!ISTACTICAL(m->code))
            quietMoves[quietsPlayed++] = m->code;

        if ((isMultiPV && score <= bestmovescore[lastmoveindex])
            || (!isMultiPV && score <= bestscore))
        {
            SDEBUGDO(isDebugMove, pvaborttype[0] = PVA_NOTBESTMOVE;);
            continue;
        }

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
                    uint32_t *srctable = (newindex - 1 ? multipvtable[newindex - 1] : pvtable[0]);
                    memcpy(multipvtable[newindex], srctable, sizeof(multipvtable[newindex]));
                    newindex--;
                }
                updateMultiPvTable(newindex, m->code);

                bestmovescore[newindex] = score;
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
        // The move becomes the new bestmove (take for UCI output) if
        // - it is the first one
        // - it raises alpha
        // If it fails low we don't change bestmove anymore but remember it in bestFailingLow for move ordering
        if (score > alpha)
        {
            if (!isMultiPV)
            {
                SDEBUGDO(isDebugPv, pvaborttype[0] = isDebugMove ? PVA_BESTMOVE : debugMovePlayed ? PVA_NOTBESTMOVE : PVA_OMITTED;);
                updatePvTable(m->code, true);
                if (bestmove.code != pvtable[0][0])
                {
                    bestmove.code = pvtable[0][0];
                    pondermove.code = pvtable[0][1];
                }
                else if (pvtable[0][1]) {
                    // use new ponder move
                    pondermove.code = pvtable[0][1];
                }
                alpha = score;
                bestmovescore[0] = score;
                eval_type = HASHEXACT;
            }
            if (score >= beta)
            {
                // Killermove
                if (!ISTACTICAL(m->code))
                {
                    updateHistory(m->code, ms.cmptr, depth * depth);
                    for (int q = 0; q < quietsPlayed - 1; q++)
                    {
                        uint32_t qm = quietMoves[q];
                        updateHistory(qm, ms.cmptr, -(depth * depth));
                    }

                    if (killer[0][0] != m->code)
                    {
                        killer[0][1] = killer[0][0];
                        killer[0][0] = m->code;
                    }
                }
                tp.addHash(hash, beta, staticeval, HASHBETA, effectiveDepth, (uint16_t)m->code);
                SDEBUGDO(isDebugPv, pvaborttype[0] = isDebugMove ? PVA_BETACUT : debugMovePlayed ? PVA_NOTBESTMOVE : PVA_OMITTED;);
                SDEBUGDO(isDebugPv, tp.debugSetPv(hash, movesOnStack() + " effectiveDepth=" + to_string(effectiveDepth)););
                return beta;   // fail hard beta-cutoff
            }
        }
        else if (!isMultiPV)
        {
            // at fail low don't overwrite an existing move
            if (!bestmove.code)
                bestmove = *m;
        }
    }

    if (isMultiPV)
    {
        if (eval_type == HASHEXACT)
            return bestmovescore[maxmoveindex - 1];
        else
            return alpha;
    }
    else {
        tp.addHash(hash, alpha, staticeval, eval_type, depth, (uint16_t)bestmove.code);
        SDEBUGDO(isDebugPv, tp.debugSetPv(hash, movesOnStack() + " depth=" + to_string(depth)););
        return alpha;
    }
}


static void uciScore(searchthread *thr, int inWindow, U64 nowtime, int score, int mpvIndex = 0)
{
    int msRun = (int)((nowtime - en.starttime) * 1000 / en.frequency);
#ifndef SDEBUG
    if (inWindow != 1 && (msRun - en.lastReport) < 200)
        return;
#endif
    const char* boundscore[] = { "upperbound ", " ", "lowerbound " };
    char s[4096];
    chessposition *pos = &thr->pos;
    en.lastReport = msRun;
    string pvstring = pos->getPv(mpvIndex ? pos->multipvtable[mpvIndex] : pos->lastpv);
    U64 nodes = en.getTotalNodes();
    U64 nps = (nowtime == en.starttime) ? 1 : nodes / 1024 * en.frequency / (nowtime - en.starttime) * 1024;  // lower resolution to avoid overflow under Linux in high performance systems

    if (!MATEDETECTED(score))
    {
        sprintf_s(s, "info depth %d seldepth %d multipv %d time %d score cp %d %snodes %llu nps %llu tbhits %llu hashfull %d pv %s\n",
            thr->depth, pos->seldepth, mpvIndex + 1, msRun, score, boundscore[inWindow], nodes, nps,
            en.tbhits, tp.getUsedinPermill(), pvstring.c_str());
    }
    else
    {
        int matein = (score > 0 ? (SCOREWHITEWINS - score + 1) / 2 : (SCOREBLACKWINS - score) / 2);
        sprintf_s(s, "info depth %d seldepth %d multipv %d time %d score mate %d %snodes %llu nps %llu tbhits %llu hashfull %d pv %s\n",
            thr->depth, pos->seldepth, mpvIndex + 1, msRun, matein, boundscore[inWindow], nodes, nps,
            en.tbhits, tp.getUsedinPermill(), pvstring.c_str());
    }
    cout << s;
#ifdef SDEBUG
    pos->pvdebugout();
#endif
}


template <RootsearchType RT>
static void search_gen1(searchthread *thr)
{
    int score;
    int alpha, beta;
    int delta = 8;
    int maxdepth;
    int inWindow = 1;
    bool reportedThisDepth;

#ifdef TDEBUG
    en.bStopCount = false;
#endif

    const bool isMultiPV = (RT == MultiPVSearch);
    const bool isMainThread = (thr->index == 0);

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
            maxdepth = MAXDEPTH - 1;
    }

    alpha = SCOREBLACKWINS;
    beta = SCOREWHITEWINS;

    uint32_t lastBestMove = 0;
    int constantRootMoves = 0;
    int lastiterationscore = NOSCORE;
    en.lastReport = 0;
    U64 nowtime;
    pos->lastpv[0] = 0;
    bool isDraw = (pos->testRepetiton() >= 2) || (pos->halfmovescounter >= 100);
    do
    {
        pos->seldepth = thr->depth;
        if (pos->rootmovelist.length == 0)
        {
            // mate / stalemate
            pos->bestmove.code = 0;
            score = pos->bestmovescore[0] =  (pos->isCheckbb ? SCOREBLACKWINS : SCOREDRAW);
        }
        else if (isDraw)
        {
            // remis via repetition or 50 moves rule
            pos->bestmove.code = 0;
            pos->pondermove.code = 0;
            score = pos->bestmovescore[0] = SCOREDRAW;
        }
        else
        {
            score = pos->rootsearch<RT>(alpha, beta, thr->depth, inWindow);
#ifdef TDEBUG
            if (en.stopLevel == ENGINESTOPIMMEDIATELY && isMainThread)
            {
                en.t2stop++;
                en.bStopCount = true;
            }
#endif

            // new aspiration window
            if (score == alpha)
            {
                // research with lower alpha and reduced beta
                beta = (alpha + beta) / 2;
                alpha = max(SCOREBLACKWINS, alpha - delta);
                if (abs(alpha) > 5000)
                    delta = SCOREWHITEWINS;
                else
                    delta += delta / 4 + 2;
                inWindow = 0;
                reportedThisDepth = false;
            }
            else if (score == beta)
            {
                // research with higher beta
                beta = min(SCOREWHITEWINS, beta + delta);
                if (abs(beta) > 2000)
                    delta = SCOREWHITEWINS;
                else
                    delta += delta / 4 + 2;
                inWindow = 2;
                reportedThisDepth = false;
            }
            else
            {
                inWindow = 1;
                thr->lastCompleteDepth = thr->depth;
                if (score >= en.terminationscore)
                {
                    // bench mode reached needed score
                    en.stopLevel = ENGINEWANTSTOP;
                }
                else if (thr->depth > 4) {
                    // next depth with new aspiration window
                    delta = 8;
                    if (isMultiPV)
                        alpha = pos->bestmovescore[en.MultiPV - 1] - delta;
                    else
                        alpha = score - delta;
                    beta = score + delta;
                }
            }
        }

        // copy new pv to lastpv; preserve identical and longer lastpv
        int i = 0;
        int bDiffers = false;
        while (pos->pvtable[0][i])
        {
            bDiffers = bDiffers || (pos->lastpv[i] != pos->pvtable[0][i]);
            pos->lastpv[i] = pos->pvtable[0][i];
            i++;
            if (i == MAXDEPTH - 1) break;
        }
        if (bDiffers)
            pos->lastpv[i] = 0;

        nowtime = getTime();

        if (score > NOSCORE && isMainThread)
        {
            // Enable currentmove output after 3 seconds
            if (!en.moveoutput && nowtime - en.starttime > 3 * en.frequency)
                en.moveoutput = true;

            // search was successfull
            if (isMultiPV)
            {
                if (inWindow == 1)
                {
                    // MultiPV output only if in aspiration window
                    i = 0;
                    int maxmoveindex = min(en.MultiPV, pos->rootmovelist.length);
                    do
                    {
                        uciScore(thr, inWindow, nowtime, pos->bestmovescore[i], i);
                        i++;
                    } while (i < maxmoveindex);
                }
            }
            else {
                // The only two cases that bestmove is not set can happen if alphabeta hit the TP table or we are in TB
                // so get bestmovecode from there or it was a TB hit so just get the first rootmove
                if (!pos->bestmove.code)
                {
                    uint16_t mc = 0;
                    int dummystaticeval;
                    tp.probeHash(pos->hash, &score, &dummystaticeval, &mc, MAXDEPTH, alpha, beta, 0);
                    pos->bestmove.code = pos->shortMove2FullMove(mc);
                    pos->pondermove.code = 0;
                }
                    
                // still no bestmove...
                if (!pos->bestmove.code && pos->rootmovelist.length > 0 && !isDraw)
                    pos->bestmove.code = pos->rootmovelist.move[0].code;

                if (pos->rootmovelist.length == 1 && !pos->tbPosition && en.endtime1 && en.pondersearch != PONDERING && pos->lastbestmovescore != NOSCORE)
                    // Don't report score of instamove; use the score of last position instead
                    pos->bestmovescore[0] = pos->lastbestmovescore;

                if (pos->useRootmoveScore)
                {
                    // We have a tablebase score so report this and adjust the search window
                    int tbScore = pos->rootmovelist.move[0].value;
                    if ((tbScore > 0 && score > tbScore) || (tbScore < 0 && score < tbScore))
                        // TB win/loss but we even found a mate; use the correct score
                        pos->bestmovescore[0] = score;
                    else
                        // otherwise use and report the tablebase score
                        score = pos->bestmovescore[0] = tbScore;
                }

                if (en.pondersearch != PONDERING || thr->depth < maxdepth)
                    uciScore(thr, inWindow, nowtime, inWindow == 1 ? pos->bestmovescore[0] : score);
            }
        }
        if (inWindow == 1)
        {
            if (lastiterationscore > pos->bestmovescore[0] + 10)
            {
                // Score decreases; use more thinking time
                constantRootMoves /= 2;
#ifdef TDEBUG
                printf("info string Score descreases... more thinking time at depth %d...\n", thr->depth);
#endif
            }
            lastiterationscore = pos->bestmovescore[0];

            // Skip some depths depending on current depth and thread number using Laser's method
            int cycle = thr->index % 16;
            if (thr->index && (thr->depth + cycle) % SkipDepths[cycle] == 0)
                thr->depth += SkipSize[cycle];

            thr->depth++;
            if (en.pondersearch == PONDERING && thr->depth > maxdepth) thr->depth--;  // stay on maxdepth when pondering
            reportedThisDepth = true;
            constantRootMoves++;
        }

        if (lastBestMove != pos->bestmove.code)
        {
            // New best move is found; reset thinking time
            lastBestMove = pos->bestmove.code;
            constantRootMoves = 0;
        }

        // Reset remaining time if depth is finished or new best move is found
        if (isMainThread)
        {
            if (inWindow == 1 || !constantRootMoves)
                resetEndTime(constantRootMoves);
        }

        // exit if STOPIMMEDIATELY
        if (en.stopLevel == ENGINESTOPIMMEDIATELY)
            break;

        // Pondering; just continue next iteration
        if (en.pondersearch == PONDERING)
            continue;

        // early exit in playing mode as there is exactly one possible move
        if (pos->rootmovelist.length == 1 && en.endtime1 && !pos->useRootmoveScore)
            break;

        // exit if STOPSOON is requested and we're in aspiration window
        if (en.endtime1 && nowtime >= en.endtime1 && inWindow == 1 && constantRootMoves && isMainThread)
            break;

        // exit if max depth is reached
        if (thr->depth > maxdepth)
            break;

    } while (1);
    
    if (isMainThread)
    {
#ifdef TDEBUG
        if (!en.bStopCount)
            en.t1stop++;
        printf("info string stop info last movetime: %4.3f    full-it. / immediate:  %4d /%4d\n", (nowtime - en.starttime) / (double)en.frequency, en.t1stop, en.t2stop);
#endif
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
        if (pos->bestmove.code != bestthr->pos.bestmove.code)
        {
            // copy best moves and score from best thread to thread 0
            int i = 0;
            while (bestthr->pos.lastpv[i])
            {
                pos->lastpv[i] = bestthr->pos.lastpv[i];
                i++;
                if (i == MAXDEPTH - 1) break;
            }
            pos->lastpv[i] = 0;
            pos->bestmove = bestthr->pos.bestmove;
            pos->pondermove = bestthr->pos.pondermove;
            pos->bestmovescore[0] = bestthr->pos.bestmovescore[0];
            inWindow = 1;
        }

        // remember score for next search in case of an instamove
        en.rootposition.lastbestmovescore = pos->bestmovescore[0];

        if (!reportedThisDepth || bestthr->index)
            uciScore(thr, inWindow, getTime(), inWindow == 1 ? pos->bestmovescore[0] : score);

        string strBestmove;
        string strPonder = "";

        if (!pos->bestmove.code && !isDraw)
        {
            // Not enough time to get any bestmove? Fall back to default move
            pos->bestmove = pos->defaultmove;
            pos->pondermove.code = 0;
        }

        strBestmove = pos->bestmove.toString();

        if (!pos->pondermove.code)
        {
            // Get the ponder move from TT
            pos->playMove(&pos->bestmove);
            uint16_t pondershort = tp.getMoveCode(pos->hash);
            pos->pondermove.code = pos->shortMove2FullMove(pondershort);
            pos->unplayMove(&pos->bestmove);
        }

        // Save pondermove in rootposition for time management of following search
        en.rootposition.pondermove = pos->pondermove;

        if (pos->pondermove.code)
            strPonder = " ponder " + pos->pondermove.toString();

        cout << "bestmove " + strBestmove + strPonder + "\n";

        en.stopLevel = ENGINESTOPIMMEDIATELY;
        en.benchmove = strBestmove;

        // Remember depth for benchmark output
        en.benchdepth = thr->depth - 1;

#ifdef STATISTICS
        search_statistics();
#endif
    }
}


void resetEndTime(int constantRootMoves, bool complete)
{
    int timetouse = (en.isWhite ? en.wtime : en.btime);
    int timeinc = (en.isWhite ? en.winc : en.binc);
    int overhead = en.moveOverhead + 8 * en.Threads;
    int constance = constantRootMoves * 2 + en.ponderhit * 4;

    // main goal is to let the search stop at endtime1 (full iterations) most times and get only few stops at endtime2 (interrupted iteration)
    // constance: ponder hit and/or onstance of best move in the last iteration lower the time within a given interval
    if (en.movestogo)
    {
        // should garantee timetouse > 0
        // f1: stop soon after current iteration at 1.0...2.2 x average movetime
        // f2: stop immediately at 1.9...3.1 x average movetime
        // movevariation: many moves to go decrease f1 (stop soon)
        int movevariation = min(32, en.movestogo) * 3 / 32;
        int f1 = max(10 - movevariation, 22 - movevariation - constance);
        int f2 = max(19, 31 - constance);
        if (complete)
            en.endtime1 = en.starttime + timetouse * en.frequency * f1 / (en.movestogo + 1) / 10000;
        en.endtime2 = en.starttime + min(max(0, timetouse - overhead * en.movestogo), f2 * timetouse / (en.movestogo + 1) / 10) * en.frequency / 1000;
    }
    else if (timetouse) {
        int ph = en.sthread[0].pos.phase();
        if (timeinc)
        {
            // sudden death with increment; split the remaining time in (256-phase) timeslots
            // f1: stop soon after 5..17 timeslot
            // f2: stop immediately after 15..27 timeslots
            int f1 = max(5, 17 - constance);
            int f2 = max(15, 27 - constance);
            if (complete)
                en.endtime1 = en.starttime + max(timeinc, f1 * (timetouse + timeinc) / (256 - ph)) * en.frequency / 1000;
            en.endtime2 = en.starttime + min(max(0, timetouse - overhead), max(timeinc, f2 * (timetouse + timeinc) / (256 - ph))) * en.frequency / 1000;
        }
        else {
            // sudden death without increment; play for another x;y moves
            // f1: stop soon at 1/30...1/42 time slot
            // f2: stop immediately at 1/10...1/22 time slot
            int f1 = min(42, 30 + constance);
            int f2 = min(22, 10 + constance);
            if (complete)
                en.endtime1 = en.starttime + timetouse / f1 * en.frequency / 1000;
            en.endtime2 = en.starttime + min(max(0, timetouse - overhead), timetouse / f2) * en.frequency / 1000;
        }
    }
    else if (timeinc)
    {
        // timetouse = 0 => movetime mode: Use exactly timeinc without overhead or early stop
        en.endtime1 = en.endtime2 = en.starttime + timeinc * en.frequency / 1000;
    }
    else {
        en.endtime1 = en.endtime2 = 0;
    }

#ifdef TDEBUG
    printf("info string Time for this move: %4.3f  /  %4.3f\n", (en.endtime1 - en.starttime) / (double)en.frequency, (en.endtime2 - en.starttime) / (double)en.frequency);
#endif
}


void startSearchTime(bool complete = true)
{
    en.starttime = getTime();
    resetEndTime(0, complete);
}


void searchStart()
{
    startSearchTime();

    en.moveoutput = false;
    en.tbhits = en.sthread[0].pos.tbPosition;  // Rootpos in TB => report at least one tbhit

    // increment generation counter for tt aging
    tp.nextSearch();

    if (en.MultiPV == 1)
        for (int tnum = 0; tnum < en.Threads; tnum++)
            en.sthread[tnum].thr = thread(&search_gen1<SinglePVSearch>, &en.sthread[tnum]);
    else
        for (int tnum = 0; tnum < en.Threads; tnum++)
            en.sthread[tnum].thr = thread(&search_gen1<MultiPVSearch>, &en.sthread[tnum]);
}


void searchWaitStop(bool forceStop)
{
    if (en.stopLevel == ENGINETERMINATEDSEARCH)
        return;

    // Make the other threads stop now
    if (forceStop)
        en.stopLevel = ENGINESTOPIMMEDIATELY;
    for (int tnum = 0; tnum < en.Threads; tnum++)
        if (en.sthread[tnum].thr.joinable())
            en.sthread[tnum].thr.join();
    en.stopLevel = ENGINETERMINATEDSEARCH;
}


inline void chessposition::CheckForImmediateStop()
{
    if (threadindex || (nodes & NODESPERCHECK))
        return;

    if (en.pondersearch == PONDERING)
        // pondering... just continue searching
        return;

    if (en.pondersearch == HITPONDER)
    {
        // ponderhit
        startSearchTime(false);
        en.pondersearch = NO;
        return;
    }

    U64 nowtime = getTime();

    if (en.endtime2 && nowtime >= en.endtime2 && en.stopLevel < ENGINESTOPIMMEDIATELY)
    {
        en.stopLevel = ENGINESTOPIMMEDIATELY;
        return;
    }

    if (en.maxnodes && en.maxnodes <= en.getTotalNodes() && en.stopLevel < ENGINESTOPIMMEDIATELY)
    {
        en.stopLevel = ENGINESTOPIMMEDIATELY;
        return;
    }
}



#ifdef STATISTICS
void search_statistics()
{
    U64 n, i1, i2, i3;
    double f0, f1, f2, f3, f4, f5, f6, f7, f10, f11;

    printf("(ST)====Statistics====================================================================================================================================\n");

    // quiescense search statistics
    i1 = statistics.qs_n[0];
    i2 = statistics.qs_n[1];
    n = i1 + i2;
    f0 = 100.0 * i2 / (double)n;
    f1 = 100.0 * statistics.qs_tt / (double)n;
    f2 = 100.0 * statistics.qs_pat / (double)n;
    f3 = 100.0 * statistics.qs_delta / (double)n;
    i3 = statistics.qs_move_delta + statistics.qs_moves;
    f4 =  i3 / (double)statistics.qs_loop_n;
    f5 = 100.0 * statistics.qs_move_delta / (double)i3;
    f6 = 100.0 * statistics.qs_moves_fh / (double)statistics.qs_moves;
    printf("(ST) QSearch: %12lld   %%InCheck:  %5.2f   %%TT-Hits:  %5.2f   %%Std.Pat: %5.2f   %%DeltaPr: %5.2f   Mvs/Lp: %5.2f   %%DlPrM: %5.2f   %%FailHi: %5.2f\n", n, f0, f1, f2, f3, f4, f5, f6);

    // general aplhabeta statistics
    n = statistics.ab_n;
    f0 = 100.0 * statistics.ab_pv / (double)n;
    f1 = 100.0 * statistics.ab_tt / (double)n;
    f2 = 100.0 * statistics.ab_tb / (double)n;
    f3 = 100.0 * statistics.ab_qs / (double)n;
    f4 = 100.0 * statistics.ab_draw_or_win / (double)n;
    printf("(ST) Total AB:%12lld   %%PV-Nodes: %5.2f   %%TT-Hits:  %5.2f   %%TB-Hits: %5.2f   %%QSCalls: %5.2f   %%Draw/Mates: %5.2f\n", n, f0, f1, f2, f3, f4);

    // node pruning
    f0 = 100.0 * statistics.prune_futility / (double)n;
    f1 = 100.0 * statistics.prune_nm / (double)n;
    f2 = 100.0 * statistics.prune_probcut / (double)n;
    f3 = 100.0 * statistics.prune_multicut / (double)n;
    f4 = 100.0 * (statistics.prune_futility + statistics.prune_nm + statistics.prune_probcut + statistics.prune_multicut) / (double)n;
    printf("(ST) Node pruning            %%Futility: %5.2f   %%NullMove: %5.2f   %%ProbeC.: %5.2f   %%MultiC.: %7.5f Total:  %5.2f\n", f0, f1, f2, f3, f4);

    // move statistics
    i1 = statistics.moves_n[0]; // quiet moves
    i2 = statistics.moves_n[1]; // tactical moves
    n = i1 + i2;
    f0 = 100.0 * i1 / (double)n;
    f1 = 100.0 * i2 / (double)n;
    f2 = 100.0 * statistics.moves_pruned_lmp / (double)n;
    f3 = 100.0 * statistics.moves_pruned_futility / (double)n;
    f4 = 100.0 * statistics.moves_pruned_badsee / (double)n;
    f5 = n / (double)statistics.moves_loop_n;
    i3 = statistics.moves_played[0] + statistics.moves_played[1];
    f6 = 100.0 * statistics.moves_fail_high / (double)i3;
    f7 = 100.0 * statistics.moves_bad_hash / i2;
    printf("(ST) Moves:   %12lld   %%Quiet-M.: %5.2f   %%Tact.-M.: %5.2f   %%BadHshM: %5.2f   %%LMP-M.:  %5.2f   %%FutilM.: %5.2f   %%BadSEE: %5.2f  Mvs/Lp: %5.2f   %%FailHi: %5.2f\n", n, f0, f1, f7, f2, f3, f4, f5, f6);

    // late move reduction statistics
    U64 red_n = statistics.red_pi[0] + statistics.red_pi[1];
    f10 = statistics.red_lmr[0] / (double)statistics.red_pi[0];
    f11 = statistics.red_lmr[1] / (double)statistics.red_pi[1];
    f1 = (statistics.red_lmr[0] + statistics.red_lmr[1]) / (double)red_n;
    f2 = statistics.red_history / (double)red_n;
    f3 = statistics.red_pv / (double)red_n;
    f4 = statistics.red_correction / (double)red_n;
    f5 = statistics.red_total / (double)red_n;
    printf("(ST) Reduct.  %12lld   lmr[0]: %4.2f   lmr[1]: %4.2f   lmr: %4.2f   hist: %4.2f   pv: %4.2f   corr: %4.2f   total: %4.2f\n", red_n, f10, f11, f1, f2, f3, f4, f5);

    f0 = 100.0 * statistics.extend_singular / (double)n;
    f1 = 100.0 * statistics.extend_endgame / (double)n;
    f2 = 100.0 * statistics.extend_history / (double)n;
    printf("(ST) Extensions: %%singular: %7.4f   %%endgame: %7.4f   %%history: %7.4f\n", f0, f1, f2);
    printf("(ST)==================================================================================================================================================\n");
}
#endif