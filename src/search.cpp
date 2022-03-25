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


#define MAXLMPDEPTH 9
int reductiontable[2][MAXDEPTH][64];
int lmptable[2][MAXLMPDEPTH];

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
    for (int d = 0; d < MAXLMPDEPTH; d++)
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



#define HISTORYMAXDEPTH 20
#define HISTORYAGESHIFT 8
#define HISTORYNEWSHIFT 5


void chessposition::getCmptr()
{
    for (int i = 0, j = ply - 1; i < CMPLIES; i++, j--)
    {
        uint32_t c;
        if (j >= -prerootmovenum && (c = (&movestack[0] + j)->movecode))
            cmptr[ply][i] = (int16_t*)counterhistory[GETPIECE(c)][GETCORRECTTO(c)];
        else
            cmptr[ply][i] = NULL;
    }
}


inline int chessposition::getHistory(uint32_t code)
{
    int pc = GETPIECE(code);
    int s2m = pc & S2MMASK;
    int from = GETFROM(code);
    int to = GETCORRECTTO(code);
    int value = history[s2m][threatSquare][from][to];
    for (int i = 0; i < CMPLIES; i++)
        if (cmptr[ply][i])
            value += cmptr[ply][i][pc * 64 + to];

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

    for (int i = 0; i < CMPLIES; i++)
        if (cmptr[ply][i]) {
            delta = value * (1 << HISTORYNEWSHIFT) - cmptr[ply][i][pc * 64 + to] * abs(value) / (1 << HISTORYAGESHIFT);
            cmptr[ply][i][pc * 64 + to] += delta;
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
    chessmove debugMove;
    bool isDebugPv = triggerDebug(&debugMove);
    SDEBUGDO(isDebugPv, pvaborttype[ply + 1] = PVA_UNKNOWN; pvdepth[ply] = depth; pvmovenum[ply] = -1;);
#endif

    STATISTICSINC(qs_n[myIsCheck]);
    STATISTICSDO(if (depth < statistics.qs_mindepth) statistics.qs_mindepth = depth);

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
        staticeval = getEval<NOTRACE>();
#else
        // get static evaluation of the position
        if (staticeval == NOSCORE)
        {
            if (movestack[ply - 1].movecode == 0)
                staticeval = -staticevalstack[ply - 1] + CEVAL(eps.eTempo, 2);
            else
                staticeval = getEval<NOTRACE>();
        }
#endif

        bestscore = staticeval;
        if (staticeval >= beta)
        {
            STATISTICSINC(qs_pat);
            tp.addHash(hash, staticeval, staticeval, HASHBETA, 0, hashmovecode);

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
            tp.addHash(hash, bestExpectableScore, staticeval, HASHALPHA, 0, hashmovecode);
            return staticeval;
        }
    }

    prepareStack();

    getCmptr();
    MoveSelector* ms = &moveSelector[ply];
    memset(ms, 0, sizeof(MoveSelector));
    ms->SetPreferredMoves(this);
    STATISTICSINC(qs_loop_n);

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

        if (!playMove(mc))
            continue;

        STATISTICSINC(qs_moves);
        legalMoves++;
        score = -getQuiescence<Pt>(-beta, -alpha, depth - 1);
        unplayMove(mc);
        if (score > bestscore)
        {
            bestscore = score;
            bestcode = mc;
            if (score >= beta)
            {
                STATISTICSINC(qs_moves_fh);
                tp.addHash(hash, score, staticeval, HASHBETA, 0, (uint16_t)bestcode);
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

    tp.addHash(hash, alpha, staticeval, eval_type, 0, (uint16_t)bestcode);
    return bestscore;
}


template <PruneType Pt>
int chessposition::alphabeta(int alpha, int beta, int depth)
{
    int score;
    int hashscore = NOSCORE;
    uint16_t hashmovecode = 0;
    int staticeval = NOSCORE;
    int bestscore = NOSCORE;
    uint32_t bestcode = 0;
    int eval_type = HASHALPHA;
    uint32_t mc;
    int extendall = 0;
    int effectiveDepth;
    const bool PVNode = (alpha != beta - 1);

    CheckForImmediateStop();

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

    // Reached depth? Do a qsearch
    if (depth <= 0)
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
    chessmove debugMove;
    bool isDebugPv = !excludeMove && triggerDebug(&debugMove);
    bool debugMovePlayed = false;
    int isDebugPosition = tp.isDebugPosition(newhash);
    bool debugTransposition = (isDebugPosition >= 0 && !isDebugPv);
    SDEBUGDO(isDebugPv, pvaborttype[ply + 1] = PVA_UNKNOWN; pvdepth[ply] = depth; pvalpha[ply] = alpha; pvbeta[ply] = beta; pvmovenum[ply] = 0; pvmovevalue[ply] = 0; pvadditionalinfo[ply] = "";);
#endif

    getCmptr();

    // TT lookup
    bool tpHit = tp.probeHash(newhash, &hashscore, &staticeval, &hashmovecode, depth, alpha, beta, ply);
    if (tpHit && !rep && !PVNode)
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
        SDEBUGDO(isDebugPv, pvabortscore[ply] = hashscore; if (debugMove.code == fullhashmove) pvaborttype[ply] = PVA_FROMTT; else pvaborttype[ply] = PVA_DIFFERENTFROMTT; );
        SDEBUGDO(isDebugPv, pvadditionalinfo[ply] = "TT = " + chessmove(fullhashmove).toString() + "  " + tp.debugGetPv(newhash, ply); );
#endif
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
        int v = probe_wdl(&success);
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
        if (movestack[ply - 1].movecode == 0)
            // just reverse the staticeval before the null move respecting the tempo
            staticeval = -staticevalstack[ply - 1] + CEVAL(eps.eTempo, 2);
        else
            staticeval = getEval<NOTRACE>();

        tp.addHash(hash, staticeval, staticeval, HASHUNKNOWN, 0, hashmovecode);
    }
    staticevalstack[ply] = staticeval;


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
    if (!PVNode && !isCheckbb && depth == 1 && staticeval > beta + (positionImproved ? sps.threatprunemarginimprove : sps.threatprunemargin) && !threats)
    {
        STATISTICSINC(prune_threat);
        return beta;
    }

    // futility pruning
    bool futility = false;
    if (Pt != NoPrune && depth <= sps.futilitymindepth)
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

        score = -alphabeta<Pt>(-beta, -beta + 1, depth - nmreduction);
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
            int verificationscore = alphabeta<Pt>(beta - 1, beta, depth - nmreduction);
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
        memset(ms, 0, sizeof(MoveSelector));
        ms->SetPreferredMoves(this, rbeta - staticeval, excludeMove);

        while ((mc = ms->next()))
        {
            if (playMove(mc))
            {
                int probcutscore = -getQuiescence<Pt>(-rbeta, -rbeta + 1, 0);
                if (probcutscore >= rbeta)
                    probcutscore = -alphabeta<Pt>(-rbeta, -rbeta + 1, depth - 4);

                unplayMove(mc);

                if (probcutscore >= rbeta)
                {
                    // ProbCut off
                    STATISTICSINC(prune_probcut);
                    SDEBUGDO(isDebugPv, pvabortscore[ply] = probcutscore; pvaborttype[ply] = PVA_PROBCUTPRUNED; pvadditionalinfo[ply] = "pruned by " + moveToString(mc););
                    tp.addHash(hash, probcutscore, staticeval, HASHBETA, depth - 3, mc);
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
    uint32_t lastmove = movestack[ply - 1].movecode;
    uint32_t counter = 0;
    if (lastmove)
        counter = countermove[GETPIECE(lastmove)][GETCORRECTTO(lastmove)];

    // Reset killers for child ply
    killer[ply + 1][0] = killer[ply + 1][1] = 0;

    memset(ms, 0, sizeof(MoveSelector));
    ms->SetPreferredMoves(this, hashmovecode, killer[ply][0], killer[ply][1], counter, excludeMove);
    STATISTICSINC(moves_loop_n);

    int legalMoves = 0;
    int quietsPlayed = 0;
    int tacticalPlayed = 0;
    while ((mc = ms->next()))
    {
#ifdef SDEBUG
        bool isDebugMove = (debugMove.code == mc);
        SDEBUGDO(isDebugMove, pvmovenum[ply] = legalMoves + 1; pvmovevalue[ply] = ms->value; );
        SDEBUGDO((isDebugPv && pvmovenum[ply] <= 0), pvmovenum[ply] = -(legalMoves + 1););
#endif
        STATISTICSINC(moves_n[(bool)ISTACTICAL(mc)]);
        // Leave out the move to test for singularity
        if ((mc & 0xffff) == excludeMove)
            continue;

        // Late move pruning
        if (Pt != NoPrune
            && depth < MAXLMPDEPTH
            && !ISTACTICAL(mc)
            && bestscore > -SCORETBWININMAXPLY
            && quietsPlayed > lmptable[positionImproved][depth])
        {
            // Proceed to next moveselector state manually to save some time
            ms->state++;
            STATISTICSINC(moves_pruned_lmp);
            SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_LMPRUNED;);
            continue;
        }

        // Check for futility pruning condition for this move and skip move if at least one legal move is already found
        bool futilityPrune = futility && !ISTACTICAL(mc) && !isCheckbb && alpha <= 900 && !moveGivesCheck(mc);
        if (futilityPrune && legalMoves)
        {
            STATISTICSINC(moves_pruned_futility);
            SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_FUTILITYPRUNED;);
            continue;
        }

        // Prune moves with bad SEE
        if (Pt != NoPrune
            && !isCheckbb
            && depth <= sps.seeprunemaxdepth
            && bestscore > -SCORETBWININMAXPLY
            && ms->state >= QUIETSTATE
            && !see(mc, sps.seeprunemarginperdepth * depth * (ISTACTICAL(mc) ? depth : sps.seeprunequietfactor)))
        {
            STATISTICSINC(moves_pruned_badsee);
            SDEBUGDO(isDebugMove, pvaborttype[ply] = PVA_SEEPRUNED;);
            continue;
        }

        // early prefetch of the next tt entry; valid for normal moves
        PREFETCH(&tp.table[nextHash(mc) & tp.sizemask]);

        int stats = !ISTACTICAL(mc) ? getHistory(mc) : getTacticalHst(mc);
        int extendMove = 0;
        int pc = GETPIECE(mc);
        int to = GETCORRECTTO(mc);

        // Singular extension
        if (Pt != MatePrune
            && (mc & 0xffff) == hashmovecode
            && depth >= sps.singularmindepth
            && !excludeMove
            && tp.probeHash(newhash, &hashscore, &staticeval, &hashmovecode, depth - 3, alpha, beta, ply)  // FIXME: maybe needs hashscore = FIXMATESCOREPROBE(hashscore, ply);
            && hashscore > alpha
#ifdef NNUELEARN
            // No singular extension in root of gensfen
            && ply > 0
#endif
            )
        {
            excludemovestack[ply - 1] = hashmovecode;
            int sBeta = max(hashscore - sps.singularmarginperdepth * depth, SCOREBLACKWINS);
            int redScore = alphabeta<Pt>(sBeta - 1, sBeta, depth / 2);
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
        else if(!ISTACTICAL(mc) && cmptr[ply][0] && cmptr[ply][1])
        {
            if (cmptr[ply][0][pc * 64 + to] > he_threshold && cmptr[ply][1][pc * 64 + to] > he_threshold)
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
                } else if (he_all / (1ULL << sps.histextmaxthreshold) > he_yes)
                {
                    // 1/32768 ~ extension ratio < 0.0030% ==> decrease threshold
                    he_threshold = he_threshold * 255 / 256;
                    he_all = he_yes = 0ULL;
                }
            }
        }

        if (!playMove(mc))
            continue;

        // Late move reduction
        int reduction = 0;
        if (depth >= sps.lmrmindepth)
        {
            reduction = reductiontable[positionImproved][depth][min(63, legalMoves + 1)];

            // adjust reduction by stats value
            reduction -= stats / (sps.lmrstatsratio * 8);

            // less reduction at PV nodes
            reduction -= PVNode;

            // even lesser reduction at PV nodes for all but bad hash moves
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
            score = -alphabeta<Pt>(-alpha - 1, -alpha, effectiveDepth - 1);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            if (score > alpha)
            {
                // research without reduction
                effectiveDepth += reduction;
                score = -alphabeta<Pt>(-alpha - 1, -alpha, effectiveDepth - 1);
                SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            }
        }
        else if (!PVNode || legalMoves > 1)
        {
            // Np PV node or not the first move; test against alpha
            score = -alphabeta<Pt>(-alpha - 1, -alpha, effectiveDepth - 1);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
        }
        // (re)search with full window at PV nodes if necessary
        if (PVNode && (legalMoves == 1 || score > alpha)) {
            score = -alphabeta<Pt>(-beta, -alpha, effectiveDepth - 1);
            SDEBUGDO(isDebugMove, pvadditionalinfo[ply-1] += "PVS(alpha=" + to_string(alpha)+ ",beta=" +to_string(beta) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
        }
        SDEBUGDO(isDebugMove, pvadditionalinfo[ply - 1] += "score=" + to_string(score) + "  "; );
        unplayMove(mc);

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
                        tp.addHash(newhash, FIXMATESCOREADD(score, ply), staticeval, HASHBETA, effectiveDepth, (uint16_t)bestcode);

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
        tp.addHash(newhash, FIXMATESCOREADD(bestscore, ply), staticeval, eval_type, depth, (uint16_t)bestcode);
        SDEBUGDO(isDebugPv || debugTransposition, tp.debugSetPv(newhash, movesOnStack() + " " + (debugTransposition ? "(transposition)" : "") + " depth=" + to_string(depth)););
    }

    return bestscore;
}



template <RootsearchType RT>
int chessposition::rootsearch(int alpha, int beta, int depth, int inWindowLast, int maxmoveindex)
{
    int score;
    uint16_t hashmovecode = 0;
    int bestscore = NOSCORE;
    int staticeval = NOSCORE;
    int eval_type = HASHALPHA;
    chessmove *m;
    int lastmoveindex;

    const bool isMultiPV = (RT == MultiPVSearch);
    bool mateprune = (alpha > SCORETBWININMAXPLY || beta < -SCORETBWININMAXPLY);

    CheckForImmediateStop();

    // reset pv
    pvtable[0][0] = 0;

    if (isMultiPV)
    {
        lastmoveindex = 0;
        if (!maxmoveindex)
            maxmoveindex = min(en.MultiPV, rootmovelist.length);
    }

#ifdef SDEBUG
    chessmove debugMove;
    bool isDebugPv = triggerDebug(&debugMove);
    bool debugMovePlayed = false;
    SDEBUGDO(isDebugPv, pvaborttype[1] = PVA_UNKNOWN; pvdepth[0] = depth; pvalpha[0] = alpha; pvbeta[0] = beta; pvmovenum[0] = 0; pvadditionalinfo[0] = "";);
#endif

    if (!isMultiPV
        && !useRootmoveScore
        && tp.probeHash(hash, &score, &staticeval, &hashmovecode, depth, alpha, beta, 0))
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
            if (score > alpha) bestmovescore[0] = score;
            if (score > NOSCORE)
            {
                SDEBUGDO(isDebugPv, pvabortscore[0] = score; if (debugMove.code == fullhashmove) pvaborttype[0] = PVA_FROMTT; else pvaborttype[0] = PVA_DIFFERENTFROMTT; );
                SDEBUGDO(isDebugPv, pvadditionalinfo[0] = "PV = " + getPv(pvtable[0]) + "  " + tp.debugGetPv(hash, 0); );
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
            // killermoves gets score better than non-capture
            else if (killer[0][0] == m->code)
                m->value = KILLERVAL1;
            else if (killer[0][1] == m->code)
                m->value = KILLERVAL2;
            else if (GETCAPTURE(m->code) != BLANK)
                m->value = (mvv[GETCAPTURE(m->code) >> 1] | lva[GETPIECE(m->code) >> 1]);
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
    staticevalstack[ply] = staticeval;

    int quietsPlayed = 0;
    int tacticalPlayed = 0;

    for (int i = 0; i < rootmovelist.length; i++)
    {
        for (int j = i + 1; j < rootmovelist.length; j++)
            if (rootmovelist.move[i] < rootmovelist.move[j])
                swap(rootmovelist.move[i], rootmovelist.move[j]);

        m = &rootmovelist.move[i];
#ifdef SDEBUG
        bool isDebugMove = (debugMove.code == m->code);
        SDEBUGDO(isDebugMove, pvmovenum[0] = i + 1; pvmovevalue[0] = rootmovelist.move[i].value; debugMovePlayed = true;)
        SDEBUGDO(pvmovenum[0] <= 0, pvmovenum[0] = -(i + 1););
#endif
        playMove(m->code);

#ifndef SDEBUG
        if (en.moveoutput && !threadindex && (en.pondersearch != PONDERING || depth < MAXDEPTH - 1))
            guiCom << "info depth " + to_string(depth) + " currmove " + m->toString() + " currmovenumber " + to_string(i + 1) + "\n";
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

        if (i > 0)
        {
            // LMR search; test against alpha
            score = -alphabeta<Prune>(-alpha - 1, -alpha, effectiveDepth - 1);
            SDEBUGDO(isDebugMove, pvadditionalinfo[0] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            if (reduction && score > alpha)
            {
                // research without reduction
                effectiveDepth += reduction;
                score = -alphabeta<Prune>(-alpha - 1, -alpha, effectiveDepth - 1);
                SDEBUGDO(isDebugMove, pvadditionalinfo[0] += "PVS(alpha=" + to_string(alpha) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
            }
        }
        // (re)search with full window if necessary
        if (i == 0 || score > alpha) {
            score = (mateprune ? -alphabeta<MatePrune>(-beta, -alpha, effectiveDepth - 1) : -alphabeta<Prune>(-beta, -alpha, effectiveDepth - 1));
            SDEBUGDO(isDebugMove, pvadditionalinfo[0] += "PVS(alpha=" + to_string(alpha) + ",beta=" + to_string(beta) + "/depth=" + to_string(effectiveDepth - 1) + ");score=" + to_string(score) + "..."; );
        }

        SDEBUGDO(isDebugMove, pvabortscore[0] = score;)

        unplayMove(m->code);

        if (en.stopLevel == ENGINESTOPIMMEDIATELY)
        {
            // time over; immediate stop requested
            return bestscore;
        }

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
                tp.addHash(hash, beta, staticeval, HASHBETA, effectiveDepth, (uint16_t)m->code);
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

    tp.addHash(hash, alpha, staticeval, eval_type, depth, (uint16_t)bestmove);
    SDEBUGDO(isDebugPv, tp.debugSetPv(hash, movesOnStack() + " depth=" + to_string(depth)););
    return alpha;
}


static void uciScore(searchthread *thr, int inWindow, U64 nowtime, int score, int mpvIndex = 0)
{
    int msRun = (int)((nowtime - en.starttime) * 1000 / en.frequency);
#ifndef SDEBUG
    if (inWindow != 1 && (msRun - en.lastReport) < 200)
        return;
#endif
    const string boundscore[] = { "upperbound ", "", "lowerbound " };
    chessposition *pos = &thr->pos;
    en.lastReport = msRun;
    string pvstring = pos->getPv(mpvIndex ? pos->multipvtable[mpvIndex] : pos->lastpv);
    U64 nodes = en.getTotalNodes();
    U64 nps = (nowtime == en.starttime) ? 1 : nodes / 1024 * en.frequency / (nowtime - en.starttime) * 1024;  // lower resolution to avoid overflow under Linux in high performance systems

    if (!MATEDETECTED(score))
    {
        guiCom << "info depth " + to_string(thr->depth) + " seldepth " + to_string(pos->seldepth) + " multipv " + to_string(mpvIndex + 1) + " time " + to_string(msRun)
            + " score cp " + to_string(score) + " " + boundscore[inWindow] + "nodes " + to_string(nodes) + " nps " + to_string(nps) + " tbhits " + to_string(en.tbhits)
            + " hashfull " + to_string(tp.getUsedinPermill()) + " pv " + pvstring + "\n";
    }
    else
    {
        int matein = (score > 0 ? (SCOREWHITEWINS - score + 1) / 2 : (SCOREBLACKWINS - score) / 2);
        guiCom << "info depth " + to_string(thr->depth) + " seldepth " + to_string(pos->seldepth) + " multipv " + to_string(mpvIndex + 1) + " time " + to_string(msRun)
            + " score mate " + to_string(matein) + " " + boundscore[inWindow] + "nodes " + to_string(nodes) + " nps " + to_string(nps) + " tbhits " + to_string(en.tbhits)
            + " hashfull " + to_string(tp.getUsedinPermill()) + " pv " + pvstring + "\n";
    }
    SDEBUGDO(pos->pvmovecode[0], guiCom.log("[SDEBUG] Raw score: " + to_string(score) + "\n"););
    SDEBUGDO(pos->pvmovecode[0], pos->pvdebugout(););
}


template <RootsearchType RT>
static void mainSearch(searchthread *thr)
{
    int score;
    int alpha, beta;
    int delta = 8;
    int maxdepth;
    int inWindow = 1;
    bool reportedThisDepth = false;

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
            score = pos->rootsearch<RT>(alpha, beta, thr->depth, inWindow);
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
                reportedThisDepth = false;
            }
            else if (score == beta)
            {
                // research with higher beta
                beta = min(SCOREWHITEWINS, beta + delta);
                delta = min(SCOREWHITEWINS, delta + delta / sps.aspincratio + sps.aspincbase);
                inWindow = 2;
                reportedThisDepth = false;
            }
            else
            {
                inWindow = 1;
                thr->lastCompleteDepth = thr->depth;
                if (thr->depth > 4 && !isMultiPV) {
                    // next depth with new aspiration window
                    delta = sps.aspinitialdelta;
                    alpha = score - delta;
                    beta = score + delta;
                }
            }
        }

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

        nowtime = getTime();

        if (score > NOSCORE && isMainThread)
        {
            // Enable currentmove output after 3 seconds
            if (!en.moveoutput && nowtime - en.starttime > 3 * en.frequency)
                en.moveoutput = true;

            // search was successfull
            if (isMultiPV)
            {
                // Avoid output of incomplete iterations
                if (en.stopLevel == ENGINESTOPIMMEDIATELY)
                    break;

                int maxmoveindex = min(en.MultiPV, pos->rootmovelist.length);
                for (int i = 0; i < maxmoveindex; i++)
                    uciScore(thr, inWindow, nowtime, pos->bestmovescore[i], i);
            }
            else {
                // The only two cases that bestmove is not set can happen if alphabeta hit the TP table or we are in TB
                // so get bestmovecode from there or it was a TB hit so just get the first rootmove
                if (!pos->bestmove)
                {
                    uint16_t mc = 0;
                    int dummystaticeval;
                    tp.probeHash(pos->hash, &score, &dummystaticeval, &mc, MAXDEPTH, alpha, beta, 0);
                    pos->bestmove = pos->shortMove2FullMove(mc);
                    pos->pondermove = 0;
                }

                // still no bestmove...
                if (!pos->bestmove && pos->rootmovelist.length > 0 && !isDraw)
                    pos->bestmove = pos->rootmovelist.move[0].code;

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
            reportedThisDepth = true;
            constantRootMoves++;
        }

        if (lastBestMove != pos->bestmove)
        {
            // New best move is found; reset thinking time
            lastBestMove = pos->bestmove;
            constantRootMoves = 0;
        }

        if (isMainThread)
        {
            if (inWindow == 1)
            {
                // Mate found; early exit
                if (!isMultiPV && en.endtime1 && thr->depth > SCOREWHITEWINS - abs(score))
                    break;

                // Recalculate remaining time for next depth
                resetEndTime(en.starttime, constantRootMoves);
            }
            else if (!constantRootMoves)
            {
                // New best move; also recalculate remaining time
                resetEndTime(en.starttime, constantRootMoves);
            }
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
        stringstream ss;
        ss << "[TDEBUG] stop info last movetime: " << setprecision(3) << (nowtime - en.starttime) / (double)en.frequency << "    full-it. / immediate:  " << en.t1stop << " / " << en.t2stop << "\n";
        guiCom.log(ss.str());
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
        en.rootposition.lastbestmovescore = pos->bestmovescore[0];

        if (!reportedThisDepth || bestthr->index)
            uciScore(thr, inWindow, getTime(), inWindow == 1 ? pos->bestmovescore[0] : score);

        string strBestmove;
        string strPonder = "";

        if (!pos->bestmove && !isDraw)
        {
            // Not enough time to get any bestmove? Fall back to default move
            pos->bestmove = pos->defaultmove;
            pos->pondermove = 0;
        }

        strBestmove = moveToString(pos->bestmove);
        string guiStr = "bestmove " + strBestmove;
        if (!pos->pondermove)
        {
            // Get the ponder move from TT
            pos->playMove(pos->bestmove);
            uint16_t pondershort = tp.getMoveCode(pos->hash);
            pos->pondermove = pos->shortMove2FullMove(pondershort);
            pos->unplayMove(pos->bestmove);
        }

        if (pos->pondermove)
        {
            strPonder = moveToString(pos->pondermove);
            guiStr += " ponder " + strPonder;
        }
        guiCom << guiStr + "\n";
        en.stopLevel = ENGINESTOPIMMEDIATELY;

        // Save pondermove in rootposition for time management of following search
        en.rootposition.pondermove = pos->pondermove;

        // Remember moves and depth for benchmark output
        en.benchdepth = thr->depth - 1;
        en.benchmove = strBestmove;
        en.benchpondermove = strPonder;

#ifdef STATISTICS
        search_statistics();
#endif
    }
}


void resetEndTime(U64 startTime, int constantRootMoves, bool complete)
{
    int timeinc = en.myinc;
    int timetouse = en.mytime;
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
        int f1 = max(9 - movevariation, 21 - movevariation - constance);
        int f2 = max(19, 31 - constance);
        int timeforallmoves = timetouse + en.movestogo * timeinc;
        if (complete)
            en.endtime1 = startTime + timeforallmoves * en.frequency * f1 / (en.movestogo + 1) / 10000;
        en.endtime2 = startTime + min(max(0, timetouse - overhead), f2 * timeforallmoves / (en.movestogo + 1) / (19 -  4 * movevariation)) * en.frequency / 1000;
    }
    else if (timetouse) {
        if (timeinc)
        {
            // sudden death with increment; split the remaining time in n timeslots depending on material phase and move number
            // ph: phase of the game averaging material and move number
            // f1: stop soon after 5..17 timeslot
            // f2: stop immediately after 15..27 timeslots
            int ph = (en.sthread[0].pos.getPhase() + min(255, en.sthread[0].pos.fullmovescounter * 6)) / 2;
            int f1 = max(5, 17 - constance);
            int f2 = max(15, 27 - constance);
            timetouse = max(timeinc, timetouse); // workaround for Arena bug
            if (complete)
                en.endtime1 = startTime + max(timeinc, f1 * (timetouse + timeinc) / (256 - ph)) * en.frequency / 1000;
            en.endtime2 = startTime + min(max(0, timetouse - overhead), max(timeinc, f2 * (timetouse + timeinc) / (256 - ph))) * en.frequency / 1000;
        }
        else {
            // sudden death without increment; play for another x;y moves
            // f1: stop soon at 1/30...1/42 time slot
            // f2: stop immediately at 1/10...1/22 time slot
            int f1 = min(42, 30 + constance);
            int f2 = min(22, 10 + constance);
            if (complete)
                en.endtime1 = startTime + timetouse / f1 * en.frequency / 1000;
            en.endtime2 = startTime + min(max(0, timetouse - overhead), timetouse / f2) * en.frequency / 1000;
        }
    }
    else if (timeinc)
    {
        // timetouse = 0 => movetime mode: Use exactly timeinc respecting overhead
        en.endtime1 = en.endtime2 = startTime + (timeinc - overhead) * en.frequency / 1000;
    }
    else {
        en.endtime1 = en.endtime2 = 0;
    }

#ifdef TDEBUG
    stringstream ss;
    guiCom.log("[TDEBUG] Time from UCI: time=" + to_string(timetouse) + "  inc=" + to_string(timeinc) + "  overhead=" + to_string(overhead) + "  constance=" + to_string(constance) + "\n");
    ss << "[TDEBUG] Time for this move: " << setprecision(3) << (en.endtime1 - en.starttime) / (double)en.frequency << " / " << (en.endtime2 - en.starttime) / (double)en.frequency << "\n";
    guiCom.log(ss.str());
    if (timeinc) guiCom.log("[TDEBUG] Timefactor (use/inc): " + to_string(timetouse / timeinc) + "\n");
#endif
}


void startSearchTime(bool complete = true)
{
    U64 nowTime = getTime();
    if (complete)
        en.starttime = nowTime;
    resetEndTime(nowTime, 0, complete);
}


void searchStart()
{
    uint32_t bm = pbook.GetMove(&en.sthread[0].pos);
    if (bm)
    {
        guiCom << "bestmove " + moveToString(bm) + "\n";
        return;
    }

    en.stopLevel = ENGINERUN;
    startSearchTime();

    en.moveoutput = false;
    en.tbhits = en.sthread[0].pos.tbPosition;  // Rootpos in TB => report at least one tbhit

    // increment generation counter for tt aging
    tp.nextSearch();

    if (en.MultiPV == 1)
        for (int tnum = 0; tnum < en.Threads; tnum++)
            en.sthread[tnum].thr = thread(&mainSearch<SinglePVSearch>, &en.sthread[tnum]);
    else
        for (int tnum = 0; tnum < en.Threads; tnum++)
            en.sthread[tnum].thr = thread(&mainSearch<MultiPVSearch>, &en.sthread[tnum]);
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
    U64 n, i1, i2, i3, i4;
    double f0, f1, f2, f3, f4, f5, f6, f7, f10, f11;
    char str[512];

    guiCom.log("[STATS] ==================================================================================================================================================================\n");

    // quiescense search statistics
    i1 = statistics.qs_n[0];
    i2 = statistics.qs_n[1];
    i4 = statistics.qs_mindepth;
    n = i1 + i2;
    f0 = 100.0 * i2 / (double)n;
    f1 = 100.0 * statistics.qs_tt / (double)n;
    f2 = 100.0 * statistics.qs_pat / (double)n;
    f3 = 100.0 * statistics.qs_delta / (double)n;
    i3 = statistics.qs_move_delta + statistics.qs_moves;
    f4 =  i3 / (double)statistics.qs_loop_n;
    f5 = 100.0 * statistics.qs_move_delta / (double)i3;
    f6 = 100.0 * statistics.qs_moves_fh / (double)statistics.qs_moves;
    sprintf(str, "[STATS] QSearch: %12lld   %%InCheck:  %5.2f   %%TT-Hits:  %5.2f   %%Std.Pat: %5.2f   %%DeltaPr: %5.2f   Mvs/Lp: %5.2f   %%DlPrM: %5.2f   %%FailHi: %5.2f   mindepth: %3lld\n", n, f0, f1, f2, f3, f4, f5, f6, i4);
    guiCom.log(str);

    // general aplhabeta statistics
    n = statistics.ab_n;
    f0 = 100.0 * statistics.ab_pv / (double)n;
    f1 = 100.0 * statistics.ab_tt / (double)n;
    f2 = 100.0 * statistics.ab_tb / (double)n;
    f3 = 100.0 * statistics.ab_qs / (double)n;
    f4 = 100.0 * statistics.ab_draw_or_win / (double)n;
    sprintf(str, "[STATS] Total AB:%12lld   %%PV-Nodes: %5.2f   %%TT-Hits:  %5.2f   %%TB-Hits: %5.2f   %%QSCalls: %5.2f   %%Draw/Mates: %5.2f\n", n, f0, f1, f2, f3, f4);
    guiCom.log(str);

    // node pruning
    f0 = 100.0 * statistics.prune_futility / (double)n;
    f1 = 100.0 * statistics.prune_nm / (double)n;
    f2 = 100.0 * statistics.prune_probcut / (double)n;
    f3 = 100.0 * statistics.prune_multicut / (double)n;
    f4 = 100.0 * statistics.prune_threat / (double)n;
    f5 = 100.0 * (statistics.prune_futility + statistics.prune_nm + statistics.prune_probcut + statistics.prune_multicut + statistics.prune_threat) / (double)n;
    sprintf(str, "[STATS] Node pruning            %%Futility: %5.2f   %%NullMove: %5.2f   %%ProbeC.: %5.2f   %%MultiC.: %7.5f   %%Threat.: %7.5f Total:  %5.2f\n", f0, f1, f2, f3, f4, f5);
    guiCom.log(str);

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
    sprintf(str, "[STATS] Moves:   %12lld   %%Quiet-M.: %5.2f   %%Tact.-M.: %5.2f   %%BadHshM: %5.2f   %%LMP-M.:  %5.2f   %%FutilM.: %5.2f   %%BadSEE: %5.2f  Mvs/Lp: %5.2f   %%FailHi: %5.2f\n", n, f0, f1, f7, f2, f3, f4, f5, f6);
    guiCom.log(str);

    // late move reduction statistics
    U64 red_n = statistics.red_pi[0] + statistics.red_pi[1];
    f10 = statistics.red_lmr[0] / (double)statistics.red_pi[0];
    f11 = statistics.red_lmr[1] / (double)statistics.red_pi[1];
    f1 = (statistics.red_lmr[0] + statistics.red_lmr[1]) / (double)red_n;
    f2 = statistics.red_history / (double)red_n;
    f6 = statistics.red_historyabs / (double)red_n;
    f3 = statistics.red_pv / (double)red_n;
    f4 = statistics.red_correction / (double)red_n;
    f5 = statistics.red_total / (double)red_n;
    sprintf(str, "[STATS] Reduct.  %12lld   lmr[0]: %4.2f   lmr[1]: %4.2f   lmr: %4.2f   hist: %4.2f   |hst|:%4.2f   pv: %4.2f   corr: %4.2f   total: %4.2f\n", red_n, f10, f11, f1, f2, f6, f3, f4, f5);
    guiCom.log(str);

    f0 = 100.0 * statistics.extend_singular / (double)n;
    f1 = 100.0 * statistics.extend_endgame / (double)n;
    f2 = 100.0 * statistics.extend_history / (double)n;
    sprintf(str, "[STATS] Extensions: %%singular: %7.4f   %%endgame: %7.4f   %%history: %7.4f\n", f0, f1, f2);
    guiCom.log(str);
    guiCom.log("[STATS] ==================================================================================================================================================================\n");
}
#endif

// Explicit template instantiation
// This avoids putting these definitions in header file
template int chessposition::alphabeta<NoPrune>(int alpha, int beta, int depth);
