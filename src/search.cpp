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


#define MAXPRUNINGDEPTH 8
int reductiontable[2][MAXDEPTH][64];
int lmptable[2][MAXPRUNINGDEPTH + 1];

// Shameless copy of Ethereal/Laser for now; may be improved/changed in the future
static const int SkipSize[16] = { 1, 1, 1, 2, 2, 2, 1, 3, 2, 2, 1, 3, 3, 2, 2, 1 };
static const int SkipDepths[16] = { 1, 2, 2, 4, 4, 3, 2, 5, 4, 3, 2, 6, 5, 4, 3, 2 };


void searchtableinit()
{
    for (int d = 0; d < MAXDEPTH; d++)
        for (int m = 0; m < 64; m++)
        {
            // reduction for not improving positions
            reductiontable[0][d][m] = 1 + (int)round(log(d * (sps.lmrlogf0 / 100.0)) * log(m) * (sps.lmrf0 / 100.0));
            // reduction for improving positions
            reductiontable[1][d][m] = (int)round(log(d * (sps.lmrlogf1 / 100.0)) * log(m * 2) * (sps.lmrf1 / 100.0));
        }
    for (int d = 0; d <= MAXPRUNINGDEPTH; d++)
    {
        // lmp for not improving positions
        lmptable[0][d] = (int)(2.5 + (sps.lmpf0 / 100.0) * round(pow(d, sps.lmppow0 / 100.0)));
        // lmp for improving positions
        lmptable[1][d] = (int)(4.0 + (sps.lmpf1 / 100.0) * round(pow(d, sps.lmppow1 / 100.0)));
    }
}


void searchinit()
{
    searchtableinit();
}


inline bool chessposition::CheckForImmediateStop()
{
    if (en.maxnodes) {
        if (!en.LimitNps)
            // go nodes
            return (nodes >= en.maxnodes);

        // Limit nps
        if (en.stopLevel == ENGINESTOPIMMEDIATELY)
            return true;
        nodesToNextCheck = 0;
        U64 now = getTime();
        int thinkingTimeMs = (int)((S64)(now - en.thinkstarttime) * 1000.0 / en.frequency);
        int AllowedTimeMs = (int)(nodes * 1000.0 / en.maxnodes);
        int waitMs = max(0, AllowedTimeMs - thinkingTimeMs);
        if (en.tmEnabled) {
            int remainingMs = (int)((S64)(en.endtime2 - now) * 1000.0 / en.frequency);
            waitMs = max(0, min(remainingMs, waitMs));
        }

        if (waitMs)
            Sleep(waitMs);
    }

    if (threadindex)
        return false;

    if (!en.tmEnabled)
        return false;

    if (en.pondersearch == PONDERING)
        // pondering... just continue searching
        return false;

    if (--nodesToNextCheck > 0)
        return false;

    S64 remainingticks = en.endtime2 - getTime();
    if (remainingticks <= 0)
    {
        en.stopLevel = ENGINESTOPIMMEDIATELY;
        return true;
    }

    U64 remainingMs = (U64)(remainingticks * 1000.0 / en.frequency);
    nodesToNextCheck = (remainingMs > 5000 ? 0x10000 : remainingMs > 500 ? 0x1000 : 0x100);

    return false;
}



#define HISTORYMAXDEPTH 20
#define HISTORYAGESHIFT 8
#define HISTORYNEWSHIFT 5


inline int chessposition::getHistory(uint32_t code)
{
    int pc = GETPIECE(code);
    int s2m = pc & S2MMASK;
    int from = GETFROM(code);
    int to = GETCORRECTTO(code);
    int value = history[s2m][threatSquare][from][to];
    int pieceTo = pc * 64 + to;
    value += (conthistptr[ply - 1][pieceTo] + conthistptr[ply - 2][pieceTo] + conthistptr[ply - 4][pieceTo]);

    return value;
}


inline void chessposition::updateHistory(uint32_t code, int value)
{
    int pc = GETPIECE(code);
    int s2m = pc & S2MMASK;
    int from = GETFROM(code);
    int to = GETCORRECTTO(code);
    value = max(-HISTORYMAXDEPTH * HISTORYMAXDEPTH, min(HISTORYMAXDEPTH * HISTORYMAXDEPTH, value));

    int delta = value * (1 << HISTORYNEWSHIFT) - history[s2m][threatSquare][from][to] * abs(value) / (1 << HISTORYAGESHIFT);
    myassert(history[s2m][threatSquare][from][to] + delta < MAXINT16 && history[s2m][threatSquare][from][to] + delta > MININT16, this, 2, history[s2m][from][to], delta);

    history[s2m][threatSquare][from][to] += delta;
    int pieceTo = pc * 64 + to;
    const int maxplies = min(4, ply);
    for (int i : {0, 1, 3}) {
        if (i >= maxplies)
            break;
        delta = value * (1 << HISTORYNEWSHIFT) - conthistptr[ply - 1 - i][pieceTo] * abs(value) / (1 << HISTORYAGESHIFT);
        conthistptr[ply - 1 - i][pieceTo] += delta;
    }
}


inline int chessposition::getTacticalHst(uint32_t code)
{
    int pt = GETPIECE(code) >> 1;
    int to = GETTO(code);
    int cp = GETCAPTURE(code) >> 1;

    return tacticalhst[pt][to][cp];
}


inline void chessposition::updateTacticalHst(uint32_t code, int value)
{
    int pt = GETPIECE(code) >> 1;
    int to = GETTO(code);
    int cp = GETCAPTURE(code) >> 1;

    value = max(-HISTORYMAXDEPTH * HISTORYMAXDEPTH, min(HISTORYMAXDEPTH * HISTORYMAXDEPTH, value));

    int delta = value * (1 << HISTORYNEWSHIFT) - tacticalhst[pt][to][cp] * abs(value) / (1 << HISTORYAGESHIFT);
    myassert(tacticalhst[pt][to][cp] + delta < MAXINT16 && tacticalhst[pt][to][cp] + delta > MININT16, this, 2, tacticalhst[pt][to][cp], delta);

    tacticalhst[pt][to][cp] += delta;
}


template <PruneType Pt>
int chessposition::getQuiescence(int alpha, int beta, int depth)
{
    const bool PVNode = (alpha != beta - 1);
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
        score = getEval<NOTRACE>();
        getPositionTuneSet(&targetpts, &myev[0]);
        copyPositionTuneSet(&targetpts, &myev[0], &this->pts, &this->ev[0]);
        return score;
    }

    bool foundpts = false;
#endif

    // Reset pv
    pvtable[ply][0] = 0;

#ifdef SDEBUG
    uint16_t debugMove;
    bool isDebugPv = triggerDebug(&debugMove);
    SDEBUGDO(isDebugPv, pvaborttype[ply + 1] = PVA_UNKNOWN; pvdepth[ply] = depth; pvmovenum[ply] = -1;);
#endif

    STATISTICSINC(qs_n[myIsCheck]);
    STATISTICSDO(if (depth < statistics.qs_mindepth) statistics.qs_mindepth = depth);

    bool tpHit;
    ttentry* tte = tp.probeHash(hash, &tpHit);
    int hashscore = tpHit ? FIXMATESCOREPROBE(tte->value, ply) : NOSCORE;
    uint16_t hashmovecode = tpHit ? tte->movecode : 0;

    if (tpHit && !PVNode && hashscore != NOSCORE && (tte->boundAndAge & (hashscore >= beta ? HASHBETA : HASHALPHA)))
    {
        STATISTICSINC(qs_tt);
        return hashscore;
    }

    int staticeval = tpHit ? tte->staticeval : NOSCORE;

    if (!myIsCheck)
    {
#ifdef EVALTUNE
        staticeval = getEval<NOTRACE>();
#else
        // get static evaluation of the position
        if (staticeval == NOSCORE)
        {
            if (movecode[ply - 1] == 0)
                staticeval = -staticevalstack[ply - 1] + CEVAL(eps.eTempo, 2);
            else
                staticeval = getEval<NOTRACE>();
        }
#endif

        bestscore = staticeval;
        if (staticeval >= beta)
        {
            STATISTICSINC(qs_pat);
            tp.addHash(tte, hash, staticeval, staticeval, HASHBETA, 0, hashmovecode);

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
        int bestExpectableScore = staticeval + sps.deltapruningmargin + getBestPossibleCapture();
        if (Pt != NoPrune && bestExpectableScore < alpha)
        {
            STATISTICSINC(qs_delta);
            tp.addHash(tte, hash, bestExpectableScore, staticeval, HASHALPHA, 0, hashmovecode);
            return staticeval;
        }
    }

    prepareStack();

    MoveSelector* ms = &moveSelector[ply];
    ms->SetPreferredMoves(this);
    STATISTICSINC(qs_loop_n);
    STATISTICSDO(ms->depth = 0);
    STATISTICSDO(ms->PvNode = (alpha != beta - 1));
    STATISTICSINC(ms_n[ms->PvNode][0]);

    uint32_t bestcode = 0;
    int legalMoves = 0;
    int eval_type = HASHALPHA;
    uint32_t mc;

    while ((mc = ms->next()))
    {
        if (Pt != NoPrune && !myIsCheck && staticeval + materialvalue[GETCAPTURE(mc) >> 1] + sps.deltapruningmargin <= alpha)
        {
            // Leave out capture that is delta-pruned
            STATISTICSINC(qs_move_delta);
            continue;
        }

        if (!playMove<false>(mc))
            continue;

        STATISTICSINC(qs_moves);
        legalMoves++;
        score = -getQuiescence<Pt>(-beta, -alpha, depth - 1);
        unplayMove<false>(mc);
        if (score > bestscore)
        {
            bestscore = score;
            bestcode = mc;
            if (score >= beta)
            {
                STATISTICSINC(qs_moves_fh);
                tp.addHash(tte, hash, score, staticeval, HASHBETA, 0, (uint16_t)bestcode);
                return score;
            }
            if (score > alpha)
            {
                updatePvTable(mc, true);
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

    if (myIsCheck && !legalMoves)
        // It's a mate
        return SCOREBLACKWINS + ply;

    tp.addHash(tte, hash, alpha, staticeval, eval_type, 0, (uint16_t)bestcode);
    return bestscore;
}


template <PruneType Pt>
int chessposition::alphabeta(int alpha, int beta, int depth, bool cutnode)
{
    int score;
    int bestscore = NOSCORE;
    uint32_t bestcode = 0;
    int eval_type = HASHALPHA;
    uint32_t mc;
    int extendall = 0;
    int effectiveDepth;
    const bool PVNode = (alpha != beta - 1);

    if (CheckForImmediateStop())
        return beta;

    // Reset pv
    pvtable[ply][0] = 0;

    STATISTICSINC(ab_n);
    STATISTICSADD(ab_pv, PVNode);

    // test for remis via repetition
    int rep = testRepetition();
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
            if (CreateEvasionMovelist(&evasions.move[0]) > 0)
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

    if (Pt == MatePrune)
    {
        // Mate distance pruning
        if (alpha < SCOREBLACKWINS + ply + 1)
            alpha = SCOREBLACKWINS + ply + 1;
        if (beta > SCOREWHITEWINS - ply)
            beta = SCOREWHITEWINS - ply;
        if (alpha >= beta)
            return alpha;
    }

    // Reached depth? Do a qsearch; searching for mate doesn't need qsearch
    if (Pt != MatePrune && depth <= 0)
    {
        // update selective depth info
        if (seldepth < ply + 1)
            seldepth = ply + 1;

        STATISTICSINC(ab_qs);
        return getQuiescence<Pt>(alpha, beta, 0);
    }

    // Maximum depth
    if (ply >= MAXDEPTH - MOVESTACKRESERVE)
        return getQuiescence<Pt>(alpha, beta, depth);

    // Get move for singularity check and change hash to seperate partial searches from full searches
    uint16_t excludeMove = excludemovestack[ply - 1];
    excludemovestack[ply] = 0;
    U64 newhash = hash ^ excludeMove;

#ifdef SDEBUG
    uint16_t debugMove = 0;
    bool isDebugPv = !excludeMove && triggerDebug(&debugMove);
    bool debugMovePlayed = false;
    int isDebugPosition = tp.isDebugPosition(newhash);
    bool debugTransposition = (isDebugPosition >= 0 && !isDebugPv);
    SDEBUGDO(isDebugPv, pvaborttype[ply + 1] = PVA_UNKNOWN; pvdepth[ply] = depth; pvalpha[ply] = alpha; pvbeta[ply] = beta; pvmovenum[ply] = 0; pvmovevalue[ply] = 0; pvadditionalinfo[ply] = "";);
#endif

    // TT lookup
    bool tpHit;
    ttentry* tte = tp.probeHash(newhash, &tpHit);
    int hashscore = tpHit ? FIXMATESCOREPROBE(tte->value, ply) : NOSCORE;
    uint16_t hashmovecode = tpHit ? tte->movecode : 0;
    int staticeval = tpHit ? tte->staticeval : NOSCORE;

    if (tpHit && !rep && !PVNode && FIXDEPTHFROMTT(tte->depth) >= depth && hashscore != NOSCORE && (tte->boundAndAge & (hashscore >= beta ? HASHBETA : HASHALPHA)))
    {
        if (hashscore >= beta && hashmovecode && !mailbox[GETTO(hashmovecode)])
        {
            int piece = mailbox[GETFROM(hashmovecode)];
            mc = (piece << 28) | hashmovecode;
            updateHistory(mc, depth * depth);
        }
        // not a single repetition; we can (almost) safely trust the hash value
        STATISTICSINC(ab_tt);
#ifdef SDEBUG
        uint32_t fullhashmove = shortMove2FullMove(hashmovecode);
        SDEBUGDO(isDebugPv, pvabortscore[ply] = hashscore; if (debugMove == hashmovecode) pvaborttype[ply] = PVA_FROMTT; else pvaborttype[ply] = PVA_DIFFERENTFROMTT; );
        SDEBUGDO(isDebugPv, pvadditionalinfo[ply] = "TT = " + chessmove(fullhashmove).toString() + "  " + tp.debugGetPv(newhash, ply); );
#endif
        return hashscore;
    }

    // TB
    // The test for rule50_count() == 0 is required to prevent probing in case
    // the root position is a TB position but only WDL tables are available.
    // In that case the search should not probe before a pawn move or capture
    // is made.
    if (piececount <= useTb && halfmovescounter == 0)
    {
        int success;
        int v = probe_wdl(&success);
        if (success) {
            tbhits++;
            nodesToNextCheck = 0;
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
                tp.addHash(tte, hash, score, staticeval, bound, MAXDEPTH - 1, 0);
            }
            STATISTICSINC(ab_tb);
            return score;
        }
    }

    // Check extension
    if (isCheckbb)
    {
        extendall = 1;
    }
    else {
        if (state & S2MMASK)
            updateThreats<BLACK>();
        else
            updateThreats<WHITE>();
    }

    prepareStack();

    // get static evaluation of the position
    if (staticeval == NOSCORE)
    {
        if (movecode[ply - 1] == 0)
            // just reverse the staticeval before the null move respecting the tempo
            staticeval = -staticevalstack[ply - 1] + CEVAL(eps.eTempo, 2);
        else
            staticeval = getEval<NOTRACE>();

        tp.addHash(tte, hash, staticeval, staticeval, HASHUNKNOWN, 0, hashmovecode);
    }
    staticevalstack[ply] = staticeval;

    if (Pt == MatePrune && depth <= 0)
        return staticeval;

    bool positionImproved = (ply >= 2  && staticevalstack[ply] > staticevalstack[ply - 2]);

    // Razoring
    if (!PVNode && !isCheckbb && depth <= 2)
    {
        const int ralpha = alpha - sps.razormargin - depth * sps.razordepthfactor;
        if (staticeval < ralpha)
        {
            int qscore;
            if (depth == 1 && ralpha < alpha)
            {
                qscore = getQuiescence<Pt>(alpha, beta, 0);
                SDEBUGDO(isDebugPv, pvabortscore[ply] = qscore; pvaborttype[ply] = PVA_RAZORPRUNED;);
                return qscore;
            }
            qscore = getQuiescence<Pt>(ralpha, ralpha + 1, 0);
            if (qscore <= ralpha)
            {
                SDEBUGDO(isDebugPv, pvabortscore[ply] = qscore; pvaborttype[ply] = PVA_RAZORPRUNED;);
                return qscore;
            }
        }
    }

    // Koivisto idea (no opponents) threat pruning
    if (Pt != MatePrune && !PVNode && !isCheckbb && depth == 1 && staticeval > beta + (positionImproved ? sps.threatprunemarginimprove : sps.threatprunemargin) && !threats)
    {
        STATISTICSINC(prune_threat);
        return beta;
    }

    // futility pruning
    bool futility = false;
    if (Pt != NoPrune && depth <= MAXPRUNINGDEPTH)
    {
        // reverse futility pruning
        if (!isCheckbb && POPCOUNT(threats) < 2 && staticeval - depth * (sps.futilityreversedepthfactor - sps.futilityreverseimproved * positionImproved) > beta)
        {
            STATISTICSINC(prune_futility);
            SDEBUGDO(isDebugPv, pvabortscore[ply] = staticeval; pvaborttype[ply] = PVA_REVFUTILITYPRUNED;);
            return staticeval;
        }
        futility = (staticeval < alpha - (sps.futilitymargin + sps.futilitymarginperdepth * depth));
    }

    // Nullmove pruning with verification like SF does it
    int bestknownscore = (hashscore != NOSCORE ? hashscore : staticeval);
    if (!isCheckbb && !threats && depth >= sps.nmmindepth && bestknownscore >= beta && (Pt != MatePrune || beta > -SCORETBWININMAXPLY) && (ply  >= nullmoveply || ply % 2 != nullmoveside) && phcount)
    {
        playNullMove();
        int nmreduction = min(depth, sps.nmmredbase + (depth / sps.nmmreddepthratio) + (bestknownscore - beta) / sps.nmmredevalratio + !PVNode * sps.nmmredpvfactor);

        score = -alphabeta<Pt>(-beta, -beta + 1, depth - nmreduction, !cutnode);
        unplayNullMove();

        if (score >= beta)
        {
            if (abs(beta) < 5000 && (depth < sps.nmverificationdepth || nullmoveply)) {
                STATISTICSINC(prune_nm);
                SDEBUGDO(isDebugPv, pvabortscore[ply] = score; pvaborttype[ply] = PVA_NMPRUNED;);
                SDEBUGDO(isDebugPv, pvadditionalinfo[ply] = "NM-Reduction:" +  to_string(depth) + "-" + to_string(nmreduction) + " (no verification)"; );
                return beta;
            }
            // Verification search
            nullmoveply = ply + 3 * (depth - nmreduction) / 4;
            nullmoveside = ply % 2;
            int verificationscore = alphabeta<Pt>(beta - 1, beta, depth - nmreduction, false);
            nullmoveside = nullmoveply = 0;
            if (verificationscore >= beta) {
                STATISTICSINC(prune_nm);
                SDEBUGDO(isDebugPv, pvabortscore[ply] = score; pvaborttype[ply] = PVA_NMPRUNED;);
                SDEBUGDO(isDebugPv, pvadditionalinfo[ply] = "NM-Reduction:" + to_string(depth) + "-" + to_string(nmreduction) + " (with verification)"; );
                return beta;
            }
        }
    }

    MoveSelector* ms = (excludeMove ? &extensionMoveSelector[ply] : &moveSelector[ply]);

    // ProbCut
    if (!PVNode && depth >= sps.probcutmindepth && abs(beta) < SCOREWHITEWINS)
    {
        int rbeta = min(SCOREWHITEWINS, beta + sps.probcutmargin);
        ms->SetPreferredMoves(this, rbeta - staticeval, excludeMove);
        STATISTICSDO(ms->depth = MAXSTATDEPTH - 1);
        STATISTICSDO(ms->PvNode = 0);
        STATISTICSINC(ms_n[0][ms->depth]);

        while ((mc = ms->next()))
        {
            if (playMove<false>(mc))
            {
                int probcutscore = -getQuiescence<Pt>(-rbeta, -rbeta + 1, 0);
                if (probcutscore >= rbeta)
                    probcutscore = -alphabeta<Pt>(-rbeta, -rbeta + 1, depth - 4, !cutnode);

                unplayMove<false>(mc);

                if (probcutscore >= rbeta)
                {
                    // ProbCut off
                    STATISTICSINC(prune_probcut);
                    SDEBUGDO(isDebugPv, pvabortscore[ply] = probcutscore; pvaborttype[ply] = PVA_PROBCUTPRUNED; pvadditionalinfo[ply] = "pruned by " + moveToString(mc););
                    tp.addHash(tte, hash, probcutscore, staticeval, HASHBETA, depth - 3, mc);
                    return probcutscore;
                }
            }
        }
    }

    // No hashmove reduction
    if (PVNode && !hashmovecode && depth >= sps.nohashreductionmindepth)
        // PV node and no best move from hash
        // Instead of iid the idea of Ed Schroeder to just decrease depth works well
        depth--;

    // Get possible countermove from table
    uint32_t lastmove = movecode[ply - 1];
    uint32_t counter = 0;
    if (lastmove)
        counter = countermove[GETPIECE(lastmove)][GETCORRECTTO(lastmove)];

    // Reset killers for child ply
    killer[ply + 1][0] = killer[ply + 1][1] = 0;

    ms->SetPreferredMoves(this, hashmovecode, killer[ply][0], killer[ply][1], counter, excludeMove);
    STATISTICSINC(moves_loop_n);
    STATISTICSDO(ms->depth = min(MAXSTATDEPTH - 2, depth));
    STATISTICSDO(ms->PvNode = PVNode);
    STATISTICSINC(ms_n[PVNode][ms->depth]);

    int legalMoves = 0;
    int quietsPlayed = 0;
    int tacticalPlayed = 0;
    while ((mc = ms->next()))
    {
#ifdef SDEBUG
        bool isDebugMove = (debugMove == (mc & 0xffff));
        SDEBUGDO(isDebugMove, pvmovenum[ply] = legalMoves + 1; pvmovevalue[ply] = ms->value; );
        SDEBUGDO((isDebugPv && pvmovenum[ply] <= 0), pvmovenum[ply] = -(legalMoves + 1););
#endif
        STATISTICSINC(moves_n[(bool)ISTACTICAL(mc)]);
        // Leave out the move to test for singularity
        if ((mc & 0xffff) == excludeMove)
            continue;

        if (Pt != NoPrune && depth <= MAXPRUNINGDEPTH && bestscore > -SCORETBWININMAXPLY)
        {
            // Late move pruning
            if (!ISTACTICAL(mc)
                && quietsPlayed > lmptable[positionImproved][depth])
            {
                // Proceed to next moveselector state manually to save some time
                ms->state++;
                STATISTICSINC(moves_pruned_lmp);
                SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_LMPRUNED;);
                continue;
            }

            // Check for futility pruning condition for this move and skip move if at least one legal move is already found
            bool futilityPrune = futility && !ISTACTICAL(mc) && !isCheckbb && !moveGivesCheck(mc);
            if (futilityPrune && legalMoves)
            {
                STATISTICSINC(moves_pruned_futility);
                SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_FUTILITYPRUNED;);
                continue;
            }

            // Prune moves with bad SEE
            if (!isCheckbb
                && ms->state >= QUIETSTATE
                && !see(mc, sps.seeprunemarginperdepth * depth * (ISTACTICAL(mc) ? depth : sps.seeprunequietfactor)))
            {
                STATISTICSINC(moves_pruned_badsee);
                SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_SEEPRUNED;);
                continue;
            }
        }

        // early prefetch of the next tt entry; valid for normal moves
        PREFETCH(&tp.table[nextHash(mc) & tp.sizemask]);

        int stats = !ISTACTICAL(mc) ? getHistory(mc) : getTacticalHst(mc);
        int extendMove = 0;
        int pc = GETPIECE(mc);
        int to = GETCORRECTTO(mc);

        if (Pt != MatePrune)
        {
            // Singular extension
            if ((mc & 0xffff) == hashmovecode
                && depth >= sps.singularmindepth
                && !excludeMove
                && (tte->boundAndAge & HASHBETA)
                && FIXDEPTHFROMTT(tte->depth) >= depth - 3
#ifdef NNUELEARN
                // No singular extension in root of gensfen
                && ply > 0
#endif
                )
            {
                excludemovestack[ply - 1] = hashmovecode;
                int sBeta = max(hashscore - sps.singularmarginperdepth * depth, SCOREBLACKWINS);
                int redScore = alphabeta<Pt>(sBeta - 1, sBeta, depth / 2, cutnode);
                excludemovestack[ply - 1] = 0;

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
                    SDEBUGDO(isDebugPv, pvabortscore[ply] = sBeta; pvaborttype[ply] = PVA_MULTICUT;);
                    return sBeta;
                }
            }
            // Extend captures that lead into endgame
            else if (phcount < 6 && GETCAPTURE(mc) >= WKNIGHT)
            {
                STATISTICSINC(extend_endgame);
                extendMove = 1;
            }
            else if (!ISTACTICAL(mc))
            {
                int pieceTo = pc * 64 + to;
                if (conthistptr[ply - 1][pieceTo] > he_threshold && conthistptr[ply - 2][pieceTo] > he_threshold)
                {
                    STATISTICSINC(extend_history);
                    extendMove = 1;
                    he_yes++;
                }
                if ((++he_all & 0x3fffff) == 0)
                {
                    // adjust history extension threshold
                    if (he_all / (1ULL << sps.histextminthreshold) < he_yes)
                    {
                        // 1/512 ~ extension ratio > 0.1953% ==> increase threshold
                        he_threshold = he_threshold * 257 / 256;
                        he_all = he_yes = 0ULL;
                    }
                    else if (he_all / (1ULL << sps.histextmaxthreshold) > he_yes)
                    {
                        // 1/32768 ~ extension ratio < 0.0030% ==> decrease threshold
                        he_threshold = he_threshold * 255 / 256;
                        he_all = he_yes = 0ULL;
                    }
                }
            }
        }

        if (!playMove<false>(mc))
            continue;

        // Late move reduction
        int reduction = 0;
        if (depth >= sps.lmrmindepth)
        {
            reduction = reductiontable[positionImproved][depth][min(63, legalMoves + 1)];

            // more reduction at cut nodes
            reduction += (cutnode && !ISTACTICAL(mc));

            // adjust reduction by stats value
            reduction -= stats / (sps.lmrstatsratio * 8);

            // less reduction at PV nodes
            reduction -= PVNode;

            // even lesser reduction at PV nodes for all but bad hash moves
            // FIXME: is this condition still valid with different tpHit logic now?
            reduction -= (PVNode && (!tpHit || hashmovecode != (uint16_t)mc || hashscore > alpha));

            // adjust reduction with opponents move number
            reduction -= (CurrentMoveNum[ply - 1] >= sps.lmropponentmovecount);

            STATISTICSINC(red_pi[positionImproved]);
            STATISTICSADD(red_lmr[positionImproved], reductiontable[positionImproved][depth][min(63, legalMoves + 1)]);
            STATISTICSADD(red_history, -stats / (sps.lmrstatsratio * 8));
            STATISTICSADD(red_historyabs, abs(-stats) / (sps.lmrstatsratio * 8));
            STATISTICSADD(red_pv, -(int)PVNode);
            STATISTICSADD(red_pv, -(int)(PVNode && (!tpHit || hashmovecode != (uint16_t)mc || hashscore > alpha)));
            STATISTICSDO(int red0 = reduction);

            reduction = min(depth, max(0, reduction));

            STATISTICSDO(int red1 = reduction);
            STATISTICSADD(red_correction, red1 - red0);
            STATISTICSADD(red_total, reduction);
        }
        effectiveDepth = depth + extendall - reduction + extendMove;

        SDEBUGDO(isDebugMove, debugMovePlayed = true;);
        STATISTICSINC(moves_played[(bool)ISTACTICAL(mc)]);

        CurrentMoveNum[ply] = ++legalMoves;

        if (reduction)
        {
            // LMR search; test against alpha
            score = -alphabeta<Pt>(-alpha - 1, -alpha, effectiveDepth - 1, true);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            if (score > alpha)
            {
                // research without reduction
                effectiveDepth += reduction;
                score = -alphabeta<Pt>(-alpha - 1, -alpha, effectiveDepth - 1, !cutnode);
                SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            }
        }
        else if (!PVNode || legalMoves > 1)
        {
            // Np PV node or not the first move; test against alpha
            score = -alphabeta<Pt>(-alpha - 1, -alpha, effectiveDepth - 1, !cutnode);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
        }
        // (re)search with full window at PV nodes if necessary
        if (PVNode && (legalMoves == 1 || score > alpha)) {
            score = -alphabeta<Pt>(-beta, -alpha, effectiveDepth - 1, false);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha)+ ",beta=" +to_string(beta) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
        }
        SDEBUGDO(isDebugMove, pvadditionalinfo[ply - 1] += "score=" + to_string(score) + "  "; );
        unplayMove<false>(mc);

        if (en.stopLevel == ENGINESTOPIMMEDIATELY)
        {
            // time is over; immediate stop requested
            return beta;
        }

        SDEBUGDO(isDebugMove, pvabortscore[ply] = score;);
        SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_BELOWALPHA; pvadditionalinfo[ply] += "Alpha=" + to_string(alpha) + " at this point."; );
        if (score > bestscore)
        {
            bestscore = score;
            bestcode = mc;

            if (score > alpha)
            {
                if (PVNode)
                    updatePvTable(bestcode, true);

                if (score >= beta)
                {
                    if (!ISTACTICAL(mc))
                    {
                        updateHistory(mc, depth * depth);
                        for (int i = 0; i < quietsPlayed; i++)
                            updateHistory(quietMoves[ply][i], -(depth * depth));

                        // Killermove
                        if (killer[ply][0] != mc)
                        {
                            killer[ply][1] = killer[ply][0];
                            killer[ply][0] = mc;
                        }

                        // save countermove
                        if (lastmove)
                            countermove[GETPIECE(lastmove)][GETCORRECTTO(lastmove)] = mc;
                    }
                    else
                    {
                        updateTacticalHst(mc, depth * depth);
                        for (int i = 0; i < tacticalPlayed; i++)
                            updateTacticalHst(tacticalMoves[ply][i], -(depth * depth));
                    }

                    STATISTICSINC(moves_fail_high);

                    if (!excludeMove)
                        tp.addHash(tte, newhash, FIXMATESCOREADD(score, ply), staticeval, HASHBETA, effectiveDepth, (uint16_t)bestcode);

                    SDEBUGDO(isDebugPv, pvaborttype[ply] = isDebugMove ? PVA_BETACUT : debugMovePlayed ? PVA_NOTBESTMOVE : PVA_OMITTED;);
                    SDEBUGDO(isDebugPv || debugTransposition, tp.debugSetPv(newhash, movesOnStack() + " " + (debugTransposition ? "(transposition)" : "") + " effectiveDepth=" + to_string(effectiveDepth)););
                    SDEBUGDO(isDebugPv && !isDebugMove, pvadditionalinfo[ply] += " Best move with betacutoff: " + moveToString(bestcode) + "(" + to_string(score) + ")";);
                    return score;   // fail soft beta-cutoff
                }
                SDEBUGDO(isDebugPv, pvaborttype[ply] = isDebugMove ? PVA_BESTMOVE : debugMovePlayed ? PVA_NOTBESTMOVE : PVA_OMITTED;);
                alpha = score;
                eval_type = HASHEXACT;
            }
        }

        if (!ISTACTICAL(mc))
            quietMoves[ply][quietsPlayed++] = mc;
        else
            tacticalMoves[ply][tacticalPlayed++] = mc;
    }

    if (legalMoves == 0)
    {
        if (excludeMove)
            return alpha;

        STATISTICSINC(ab_draw_or_win);
        if (isCheckbb) {
            // It's a mate
            SDEBUGDO(isDebugPv, pvaborttype[ply] = PVA_CHECHMATE;);
            return SCOREBLACKWINS + ply;
        }
        else {
            // It's a stalemate
            SDEBUGDO(isDebugPv, pvaborttype[ply] = PVA_STALEMATE;);
            return SCOREDRAW;
        }
    }

    if (bestcode && !excludeMove)
    {
        tp.addHash(tte, newhash, FIXMATESCOREADD(bestscore, ply), staticeval, eval_type, depth, (uint16_t)bestcode);
        SDEBUGDO(isDebugPv || debugTransposition, tp.debugSetPv(newhash, movesOnStack() + " " + (debugTransposition ? "(transposition)" : "") + " depth=" + to_string(depth)););
    }

    return bestscore;
}



template <RootsearchType RT>
int chessposition::rootsearch(int alpha, int beta, int *depthptr, int inWindowLast, int maxmoveindex)
{
    int depth = *depthptr;
    int bestscore = NOSCORE;
    int eval_type = HASHALPHA;
    chessmove *m;
    int lastmoveindex;

    const bool isMultiPV = (RT == MultiPVSearch);

    bool mateprune = (en.mate > 0 || alpha > SCORETBWININMAXPLY || beta < -SCORETBWININMAXPLY);

    // reset pv
    pvtable[0][0] = 0;

    if (isMultiPV)
    {
        lastmoveindex = 0;
        if (!maxmoveindex)
            maxmoveindex = min(en.MultiPV, rootmovelist.length);
    }

#ifdef SDEBUG
    uint16_t debugMove = 0;
    bool isDebugPv = triggerDebug(&debugMove);
    bool debugMovePlayed = false;
    SDEBUGDO(isDebugPv, pvaborttype[1] = PVA_UNKNOWN; pvdepth[0] = depth; pvalpha[0] = alpha; pvbeta[0] = beta; pvmovenum[0] = 0; pvadditionalinfo[0] = "";);
#endif

    bool tpHit;
    int newDepth;
    ttentry* tte = tp.probeHash(hash, &tpHit);
    int score = tpHit ? tte->value : NOSCORE;
    uint16_t hashmovecode = tpHit ? tte->movecode : 0;
    int staticeval = tpHit ? tte->staticeval : NOSCORE;

    if (!isMultiPV
        && !useRootmoveScore
        && tpHit
        && (newDepth = FIXDEPTHFROMTT(tte->depth)) >= depth
        && score != NOSCORE
        && (tte->boundAndAge & BOUNDMASK) == HASHEXACT)
    {
        // Hash is fixed regarding scores that don't see actual 3folds so we can trust the entry
        uint32_t fullhashmove = shortMove2FullMove(hashmovecode);
        if (fullhashmove)
        {
            if (bestmove != fullhashmove) {
                bestmove = fullhashmove;
                pondermove = 0;
            }
            updatePvTable(fullhashmove, false);
            if (score > alpha) {
                bestmovescore[0] = score;
                SDEBUGDO(isDebugPv, pvabortscore[0] = score; if (debugMove == hashmovecode) pvaborttype[0] = PVA_FROMTT; else pvaborttype[0] = PVA_DIFFERENTFROMTT; );
                SDEBUGDO(isDebugPv, pvadditionalinfo[0] = "PV = " + getPv(pvtable[0]) + "  " + tp.debugGetPv(hash, 0); );
                *depthptr = newDepth;
                return score;
            }
        }
    }

    if (!tbPosition)
    {
        if (state & S2MMASK)
            updateThreats<BLACK>();
        else
            updateThreats<WHITE>();
        // Reset move values
        for (int i = 0; i < rootmovelist.length; i++)
        {
            m = &rootmovelist.move[i];

            //PV moves gets top score
            if (hashmovecode == (m->code & 0xffff))
                m->value = PVVAL;
            else if (bestFailingLow == m->code)
                m->value = KILLERVAL2 - 1;
            // killermoves gets score better than other quiets
            else if (killer[0][0] == m->code)
                m->value = KILLERVAL1;
            else if (killer[0][1] == m->code)
                m->value = KILLERVAL2;
            else if (GETCAPTURE(m->code) != BLANK)
                m->value = (m->code & BADSEEFLAG ? -1 : 1) * (mvv[GETCAPTURE(m->code) >> 1] | lva[GETPIECE(m->code) >> 1]);
            else
                m->value = history[state & S2MMASK][threatSquare][GETFROM(m->code)][GETCORRECTTO(m->code)];
            if (isMultiPV) {
                if (multipvtable[0][0] == m->code)
                    m->value = PVVAL;
                if (multipvtable[1][0] == m->code)
                    m->value = PVVAL - 1;
            }
        }
    }

    if (isMultiPV)
    {
        for (int i = 0; i < maxmoveindex; i++)
        {
            multipvtable[i][0] = 0;
            bestmovescore[i] = NOSCORE;
        }
    }

    // get static evaluation of the position
    if (staticeval == NOSCORE)
        staticeval = getEval<NOTRACE>();
    staticevalstack[0] = staticeval;

    int quietsPlayed = 0;
    int tacticalPlayed = 0;

    for (int i = 0; i < rootmovelist.length; i++)
    {
        for (int j = i + 1; j < rootmovelist.length; j++)
            if (rootmovelist.move[i] < rootmovelist.move[j])
                swap(rootmovelist.move[i], rootmovelist.move[j]);

        U64 nodesbeforemove = nodes;

        m = &rootmovelist.move[i];
#ifdef SDEBUG
        bool isDebugMove = (debugMove == (m->code & 0xffff));
        SDEBUGDO(isDebugMove, pvmovenum[0] = i + 1; pvmovevalue[0] = rootmovelist.move[i].value; debugMovePlayed = true;)
        SDEBUGDO(pvmovenum[0] <= 0, pvmovenum[0] = -(i + 1););
#endif
        playMove<false>(m->code);

#ifndef SDEBUG
        if (en.moveoutput && !threadindex && (en.pondersearch != PONDERING || depth < MAXDEPTH - 1))
            guiCom << "info depth " + to_string(depth) + " currmove " + m->toString() + " currmovenumber " + to_string(i + 1) + "\n";
#endif
        int reduction = 0;
        CurrentMoveNum[1] = i + 1;

        // Late move reduction
        if (!ISTACTICAL(m->code))
        {
            reduction = reductiontable[1][depth][min(63, i + 1)] + inWindowLast - 1;
            STATISTICSINC(red_pi[1]);
            STATISTICSADD(red_lmr[1], reductiontable[1][depth][min(63, i + 1)] + inWindowLast - 1);
        }

        int effectiveDepth = depth - reduction;

        if (i > 0)
        {
            // LMR search; test against alpha
            score = -alphabeta<Prune>(-alpha - 1, -alpha, effectiveDepth - 1, true);
            SDEBUGDO(isDebugMove, pvadditionalinfo[0] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            if (reduction && score > alpha)
            {
                // research without reduction
                effectiveDepth += reduction;
                score = -alphabeta<Prune>(-alpha - 1, -alpha, effectiveDepth - 1, true);
                SDEBUGDO(isDebugMove, pvadditionalinfo[0] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            }
        }
        // (re)search with full window if necessary
        if (i == 0 || score > alpha) {
            score = (mateprune ? -alphabeta<MatePrune>(-beta, -alpha, effectiveDepth - 1, false) : -alphabeta<Prune>(-beta, -alpha, effectiveDepth - 1, false));
            SDEBUGDO(isDebugMove, pvadditionalinfo[0] += "PVS(alpha=" + to_string(alpha) + ",beta=" + to_string(beta) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
        }

        SDEBUGDO(isDebugMove, pvabortscore[0] = score;)

        unplayMove<false>(m->code);

        nodespermove[(uint16_t)m->code] += nodes - nodesbeforemove;

        if (CheckForImmediateStop())
            return bestscore;

        if (!ISTACTICAL(m->code))
            quietMoves[0][quietsPlayed++] = m->code;
        else
            tacticalMoves[0][tacticalPlayed++] = m->code;

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
                    int j = 0;
                    while (true)
                    {
                        multipvtable[newindex][j] = srctable[j];
                        if (!srctable[j])
                            break;
                        j++;
                    }
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
        else {
            updatePvTable(m->code, true);
        }

        // We have a new best move.
        // Now it gets a little tricky what to do with it
        // The move becomes the new bestmove (take for UCI output) if
        // - it is the first one
        // - it raises alpha
        // If it fails low we don't change bestmove anymore but remember it in bestFailingLow for move ordering
        if (score > alpha)
        {
            SDEBUGDO(isDebugPv, pvaborttype[0] = isDebugMove ? PVA_BESTMOVE : debugMovePlayed ? PVA_NOTBESTMOVE : PVA_OMITTED;);
            if (bestmove != pvtable[0][0])
            {
                bestmove = pvtable[0][0];
                pondermove = pvtable[0][1];
            }
            else if (pvtable[0][1]) {
                // use new ponder move
                pondermove = pvtable[0][1];
            }
            if (!isMultiPV)
            {
                alpha = score;
                bestmovescore[0] = score;
                eval_type = HASHEXACT;
            }
            if (score >= beta)
            {
                // Killermove
                if (!ISTACTICAL(m->code))
                {
                    updateHistory(m->code, depth * depth);
                    for (int q = 0; q < quietsPlayed - 1; q++)
                        updateHistory(quietMoves[0][q], -(depth * depth));

                    if (killer[0][0] != m->code)
                    {
                        killer[0][1] = killer[0][0];
                        killer[0][0] = m->code;
                    }
                }
                else
                {
                    updateTacticalHst(m->code, depth * depth);
                    for (int t = 0; t < tacticalPlayed - 1; t++)
                        updateTacticalHst(tacticalMoves[0][t], -(depth * depth));

                }
                tp.addHash(tte, hash, beta, staticeval, HASHBETA, effectiveDepth, (uint16_t)m->code);
                SDEBUGDO(isDebugPv, pvaborttype[0] = isDebugMove ? PVA_BETACUT : debugMovePlayed ? PVA_NOTBESTMOVE : PVA_OMITTED;);
                SDEBUGDO(isDebugPv, tp.debugSetPv(hash, movesOnStack() + " effectiveDepth=" + to_string(effectiveDepth)););
                return beta;   // fail hard beta-cutoff
            }
        }
        else
        {
            // at fail low don't overwrite an existing move
            if (!bestmove)
                bestmove = m->code;
        }
    }

    tp.addHash(tte, hash, alpha, staticeval, eval_type, depth, (uint16_t)bestmove);
    SDEBUGDO(isDebugPv, tp.debugSetPv(hash, movesOnStack() + " depth=" + to_string(depth)););
    return alpha;
}


inline bool uciScoreOutputNeeded(int inWindow, U64 thinktime)
{
    int msRun = (int)(thinktime * 1000 / en.frequency);
#ifndef SDEBUG
    if (msRun - en.lastReport < 1 + 128 * (inWindow != 1))
        return false;
#endif
    en.lastReport = msRun;
    return true;
}

static void uciScore(searchthread *thr, int inWindow, U64 thinktime, int score, int mpvIndex = 0)
{
    const string boundscore[] = { "upperbound ", "", "lowerbound " };
    chessposition *pos = &thr->pos;

    string pvstring = pos->getPv(mpvIndex ? pos->multipvtable[mpvIndex] : pos->lastpv);
    U64 nodes, tbhits;
    en.getNodesAndTbhits(&nodes, &tbhits);

    if (nodes)
        thr->nps = nodes * en.frequency / (thinktime + 1);

    if (!MATEDETECTED(score))
    {
        guiCom << "info depth " + to_string(thr->depth) + " seldepth " + to_string(pos->seldepth) + " multipv " + to_string(mpvIndex + 1) + " time " + to_string(thinktime * 1000 / en.frequency)
            + " score cp " + to_string(score) + " " + boundscore[inWindow] + "nodes " + to_string(nodes) + " nps " + to_string(thr->nps) + " tbhits " + to_string(tbhits)
            + " hashfull " + to_string(tp.getUsedinPermill()) + " pv " + pvstring + "\n";
    }
    else
    {
        int matein = MATEIN(score);
        guiCom << "info depth " + to_string(thr->depth) + " seldepth " + to_string(pos->seldepth) + " multipv " + to_string(mpvIndex + 1) + " time " + to_string(thinktime * 1000 / en.frequency)
            + " score mate " + to_string(matein) + " " + boundscore[inWindow] + "nodes " + to_string(nodes) + " nps " + to_string(thr->nps) + " tbhits " + to_string(tbhits)
            + " hashfull " + to_string(tp.getUsedinPermill()) + " pv " + pvstring + "\n";
    }
    SDEBUGDO(pos->pvmovecode[0], guiCom.log("[SDEBUG] Raw score: " + to_string(score) + "\n"););
    SDEBUGDO(pos->pvmovecode[0], pos->pvdebugout(););
}


template <RootsearchType RT>
void mainSearch(searchthread *thr)
{
    int score;
    int alpha, beta;
    int delta = 8;
    int maxdepth;
    int inWindow = 1;
    bool uciNeedsFinalReport = true;    // Flag to indicate that at immediate stop a last pv line should be written

#ifdef TDEBUG
    en.bStopCount = false;
#endif

    const bool isMultiPV = (RT == MultiPVSearch);
    const bool isMainThread = (thr->index == 0);

    chessposition *pos = &thr->pos;

    thr->lastCompleteDepth = 0;
    thr->depth = 1;
    if (en.maxdepth > 0)
        maxdepth = en.maxdepth;
    else
        maxdepth = MAXDEPTH - 1;

    alpha = SCOREBLACKWINS;
    beta = SCOREWHITEWINS;

    uint32_t lastBestMove = 0;
    int constantRootMoves = 0;
    int lastiterationscore = NOSCORE;
    en.lastReport = -1;
    U64 nowtime = 0;
    pos->lastpv[0] = 0;
    bool isDraw = (pos->testRepetition() >= 2) || (pos->halfmovescounter >= 100);
    do
    {
        pos->seldepth = thr->depth;
        if (pos->rootmovelist.length == 0)
        {
            // mate / stalemate
            pos->bestmove = 0;
            score = pos->bestmovescore[0] =  (pos->isCheckbb ? SCOREBLACKWINS : SCOREDRAW);
        }
        else if (isDraw)
        {
            // remis via repetition or 50 moves rule
            pos->bestmove = 0;
            pos->pondermove = 0;
            score = pos->bestmovescore[0] = SCOREDRAW;
        }
        else
        {
            score = pos->rootsearch<RT>(alpha, beta, &thr->depth, inWindow);
#ifdef TDEBUG
            if (en.stopLevel == ENGINESTOPIMMEDIATELY && isMainThread)
            {
                en.t2stop++;
                en.bStopCount = true;
            }
#endif
            // Open aspiration window for winning scores
            if (abs(score) > 5000)
                delta = SCOREWHITEWINS;

            // new aspiration window
            if (score == alpha)
            {
                // research with lower alpha and reduced beta
                beta = (alpha + beta) / 2;
                alpha = max(SCOREBLACKWINS, alpha - delta);
                delta = min(SCOREWHITEWINS, delta + delta / sps.aspincratio + sps.aspincbase);
                inWindow = 0;
            }
            else if (score == beta)
            {
                // research with higher beta
                beta = min(SCOREWHITEWINS, beta + delta);
                delta = min(SCOREWHITEWINS, delta + delta / sps.aspincratio + sps.aspincbase);
                inWindow = 2;
                uciNeedsFinalReport = true;
            }
            else
            {
                inWindow = 1;
                uciNeedsFinalReport = true;
                thr->lastCompleteDepth = thr->depth;
                if (thr->depth > 4 && !isMultiPV) {
                    // next depth with new aspiration window
                    delta = sps.aspinitialdelta;
                    alpha = score - delta;
                    beta = score + delta;
                }
            }
        }

        // exit if STOPIMMEDIATELY
        if (en.stopLevel == ENGINESTOPIMMEDIATELY)
            break;

        // exit when max nodes reached
        if (en.maxnodes && !en.LimitNps && pos->nodes >= en.maxnodes)
            break;

        if (pos->pvtable[0][0])
        {
            // copy new pv to lastpv
            int i = 0;
            while (pos->pvtable[0][i])
            {
                pos->lastpv[i] = pos->pvtable[0][i];
                i++;
                if (i == MAXDEPTH - 1) break;
            }
            pos->lastpv[i] = 0;
        }

        if (isMainThread)
            nowtime = getTime();

        if (score > NOSCORE && isMainThread)
        {
            // Enable currentmove output after 3 seconds
            if (!en.moveoutput && nowtime - en.thinkstarttime > 3 * en.frequency)
                en.moveoutput = true;

            // search was successfull
            if (isMultiPV)
            {
                // Avoid output of incomplete iterations
                if (en.stopLevel == ENGINESTOPIMMEDIATELY)
                    break;

                U64 thinkTime = nowtime - en.thinkstarttime;
                int maxmoveindex = min(en.MultiPV, pos->rootmovelist.length);
                if (!en.tmEnabled || uciScoreOutputNeeded(inWindow, thinkTime)) {
                    for (int i = 0; i < maxmoveindex; i++)
                        uciScore(thr, inWindow, thinkTime, pos->bestmovescore[i], i);
                    uciNeedsFinalReport = false;
                }
            }
            else {
                // The only two cases that bestmove is not set can happen if alphabeta hit the TP table or we are in TB
                // so get bestmovecode from there or it was a TB hit so just get the first rootmove
                if (!pos->bestmove)
                {
                    bool tpHit;
                    ttentry* tte = tp.probeHash(pos->hash, &tpHit);
                    if (tpHit)
                    {
                        pos->bestmove = pos->shortMove2FullMove(tte->movecode);
                        pos->pondermove = 0;
                    }
                }

                // still no bestmove...
                if (!pos->bestmove && pos->rootmovelist.length > 0 && !isDraw)
                    pos->bestmove = pos->rootmovelist.move[0].code;

                if (isMainThread && pos->rootmovelist.length == 1 && !pos->tbPosition && en.tmEnabled && en.pondersearch != PONDERING && en.lastbestmovescore != NOSCORE)
                    // Don't report score of instamove; use the score of last position instead
                    pos->bestmovescore[0] = en.lastbestmovescore;

                if (pos->useRootmoveScore)
                {
                    // We have a tablebase score so report this and adjust the search window
                    uciNeedsFinalReport = false;
                    int tbScore = pos->rootmovelist.move[0].value;
                    if ((tbScore > 0 && score > tbScore) || (tbScore < 0 && score < tbScore))
                        // TB win/loss but we even found a mate; use the correct score
                        pos->bestmovescore[0] = score;
                    else
                        // otherwise use and report the tablebase score
                        score = pos->bestmovescore[0] = tbScore;
                }

                if (en.pondersearch != PONDERING || thr->depth < maxdepth) {
                    U64 thinkTime = nowtime - en.thinkstarttime;
                    if (!en.tmEnabled || uciScoreOutputNeeded(inWindow, thinkTime)) {
                        uciScore(thr, inWindow, thinkTime, inWindow == 1 ? pos->bestmovescore[0] : score);
                        uciNeedsFinalReport = false;
                    }
                }
            }
        }
        if (inWindow == 1)
        {
            if (lastiterationscore > pos->bestmovescore[0] + 10)
            {
                // Score decreases; use more thinking time
                constantRootMoves /= 2;
#ifdef TDEBUG
                guiCom.log("[TDEBUG] Score decreases... more thinking time at depth " + to_string(thr->depth) + "\n");
#endif
            }
            lastiterationscore = pos->bestmovescore[0];

            // Skip some depths depending on current depth and thread number using Laser's method
            int cycle = thr->index % 16;
            if (thr->index && (thr->depth + cycle) % SkipDepths[cycle] == 0)
                thr->depth += SkipSize[cycle];

            thr->depth++;
            if (en.pondersearch == PONDERING && thr->depth > maxdepth) thr->depth--;  // stay on maxdepth when pondering
            constantRootMoves++;
        }

        if (isMainThread)
        {
            if (lastBestMove != pos->bestmove)
            {
                // New best move is found; reset thinking time
                lastBestMove = pos->bestmove;
                constantRootMoves = 0;
            }

            if (en.tmEnabled && (inWindow == 1 || !constantRootMoves))
            {
                // Recalculate remaining time for next depth
                int bestmovenodesratio = pos->nodes ? (int)(128 * (2.5 -  2 * (double)pos->nodespermove[(uint16_t)pos->bestmove] / pos->nodes)) : 128;
                en.resetEndTime(nowtime, constantRootMoves, bestmovenodesratio);
            }

            // Mate found; early exit
            if (!isMultiPV && inWindow == 1 && en.tmEnabled && thr->depth > SCOREWHITEWINS - abs(score) && en.pondersearch != PONDERING)
                break;

            // Exit when searching for mate and found it
            if (en.mate && score >= SCOREWHITEWINS - 2 * en.mate)
                break;
        }

        // Pondering; just continue next iteration
        if (en.pondersearch == PONDERING)
            continue;

        // early exit in playing mode as there is exactly one possible move
        if (pos->rootmovelist.length == 1 && en.tmEnabled)
            break;

        // exit if STOPSOON is requested and we're in aspiration window
        if (isMainThread && en.tmEnabled && (S64)(en.endtime1 - nowtime) < 0 && inWindow == 1 && constantRootMoves)
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
        stringstream ss;
        ss << "[TDEBUG] stop info last movetime: " << setprecision(3) << (nowtime - en.clockstarttime) / (double)en.frequency << "    full-it. / immediate:  " << en.t1stop << " / " << en.t2stop << "\n";
        guiCom.log(ss.str());
#endif
        if (en.maxnodes && !en.LimitNps)
        {
            // Wait for helper threads to finish their nodes
            for (int i = 1; i < en.Threads; i++)
            {
                while (en.sthread[i].pos.threadindex)
                    Sleep(1);
            }
        }

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
        if (pos->bestmove != bestthr->pos.bestmove)
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
        en.lastbestmovescore = pos->bestmovescore[0];

        if (uciNeedsFinalReport || bestthr->index)
            uciScore(thr, inWindow, getTime() - en.thinkstarttime, inWindow == 1 ? pos->bestmovescore[0] : score);

        string strBestmove;
        string strPonder = "";

        if (!pos->bestmove && !isDraw)
        {
            // Not enough time to get any bestmove? Fall back to default move
            pos->bestmove = pos->defaultmove;
            pos->pondermove = 0;
            // Enable next line to detect out-of-time situations via cutechess "illegal ponder move h8h8"
            // pos->pondermove = 0xfff;
#ifdef TDEBUG
            guiCom.log("[TDEBUG] Warning! Run out of time and using default move which may lose!\n");
#endif
        }

        strBestmove = moveToString(pos->bestmove);
        string guiStr = "bestmove " + strBestmove;
        if (!pos->pondermove)
        {
            // Get the ponder move from TT
            if (pos->playMove<true>(pos->bestmove)) {
                uint16_t pondershort = tp.getMoveCode(pos->hash);
                pos->pondermove = pos->shortMove2FullMove(pondershort);
                pos->unplayMove<true>(pos->bestmove);
            }
        }

        if (pos->pondermove)
        {
            strPonder = moveToString(pos->pondermove);
            guiStr += " ponder " + strPonder;
        }
        guiCom << guiStr + "\n";
        bool bStoppedImmediately = (en.stopLevel == ENGINESTOPIMMEDIATELY);
        en.stopLevel = ENGINESTOPIMMEDIATELY;
        en.clockstoptime = getTime();
        en.lastmovetime = en.clockstoptime - en.clockstarttime;
        if (bStoppedImmediately && en.tmEnabled)
        {
            int measuredOverhead = (int)((S64)(en.clockstoptime - en.endtime2) * 1000.0 / en.frequency);
            if (measuredOverhead > en.maxMeasuredEngineOverhead) {
                en.maxMeasuredEngineOverhead = measuredOverhead;
                int engineOverhead = en.moveOverhead;
                if (measuredOverhead > engineOverhead / 2)
                {
                    if (measuredOverhead > engineOverhead)
                        guiCom << "info string Measured engine overhead is " + to_string(measuredOverhead) + "ms and time forfeits are very likely. Please increase Move_Overhead option!\n";
                    else
                        guiCom << "info string Measured engine overhead is " + to_string(measuredOverhead) + "ms (> 50% of allowed via Move_Overhead option).\n";
                }
            }
        }

        // Save pondermove in rootposition for time management of following search
        en.rootposition.pondermove = pos->pondermove;

        // Remember moves and depth for benchmark output
        en.benchdepth = thr->depth - 1;
        en.benchmove = strBestmove;
        en.benchpondermove = strPonder;
    }

    pos->threadindex = 0; //reset index to signal termination of thread
}


// Explicit template instantiation
// This avoids putting these definitions in header file
template int chessposition::alphabeta<NoPrune>(int alpha, int beta, int depth, bool cutnode);
template int chessposition::rootsearch<MultiPVSearch>(int, int, int*, int, int);
template void mainSearch<SinglePVSearch>(searchthread*);
template void mainSearch<MultiPVSearch>(searchthread*);
