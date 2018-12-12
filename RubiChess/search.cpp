
#include "RubiChess.h"


const int deltapruningmargin = 100;

int reductiontable[2][MAXDEPTH][64];

#define MAXLMPDEPTH 9
int lmptable[2][MAXLMPDEPTH];

void searchinit()
{
    for (int d = 0; d < MAXDEPTH; d++)
        for (int m = 0; m < 64; m++)
        {
            // reduction for not improving positions
            reductiontable[0][d][m] = (int)round(log(d) * log(m) / 1.5);
            // reduction for improving positions
            reductiontable[1][d][m] = (int)round(log(d) * log(m) / 2.5);
        }
    for (int d = 0; d < MAXLMPDEPTH; d++)
    {
        // lmp for not improving positions
        lmptable[0][d] = (int)(2.5 + 0.7 * round(pow(d, 1.85)));
        // lmp for improving positions
        lmptable[1][d] = (int)(4.0 + 1.3 * round(pow(d, 1.85)));
    }
}


int getQuiescence(int alpha, int beta, int depth)
{
    int patscore, score;
    int bestscore = SHRT_MIN;
    bool isLegal;
    bool LegalMovesPossible = false;
#ifdef EVALTUNE
    positiontuneset pts;
    bool foundpts = false;
#endif

    // FIXME: Should quiescience nodes count for the statistics?
    //en.nodes++;

#ifdef SDEBUG
    chessmove debugMove;
    int debugInsert = pos.ply - pos.rootheight;
    bool isDebugPv = pos.triggerDebug(&debugMove);
    pos.pvtable[pos.ply][0] = 0;
#endif

    if (!pos.isCheck)
    {
        patscore = (pos.state & S2MMASK ? -getValueNoTrace(&pos) : getValueNoTrace(&pos));
        bestscore = patscore;
        if (patscore >= beta)
        {
            SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch (fail high by patscore).", patscore);
            return patscore;
        }
        if (patscore > alpha)
        {
#ifdef EVALTUNE
            pos.getPositionTuneSet(&pts);
            foundpts = true;
#endif
            alpha = patscore;
        }

        // Delta pruning
        int bestCapture = pos.getBestPossibleCapture();
        if (patscore + deltapruningmargin + bestCapture < alpha)
        {
            SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch (delta pruning by patscore).", patscore);
            return patscore;
        }
    }

    pos.prepareStack();

    chessmovelist *movelist = new chessmovelist;

    if (pos.isCheck)
        movelist->length = pos.getMoves(&movelist->move[0]);
    else
        movelist->length = pos.getMoves(&movelist->move[0], TACTICAL);

    movelist->sort(lva[QUEEN]);

    for (int i = 0; i < movelist->length; i++)
    {
        bool MoveIsUsefull = (pos.isCheck
            || ISPROMOTION(movelist->move[i].code)
            || (patscore + prunematerialvalue[GETCAPTURE(movelist->move[i].code) >> 1] + deltapruningmargin > alpha
                && pos.see(movelist->move[i].code, 0)));

        if (MoveIsUsefull || !LegalMovesPossible)
        {
            isLegal = pos.playMove(&(movelist->move[i]));
            if (isLegal)
            {
                LegalMovesPossible = true;
                if (MoveIsUsefull)
                {
                    score = -getQuiescence(-beta, -alpha, depth - 1);
                }
                pos.unplayMove(&(movelist->move[i]));
                if (MoveIsUsefull && score > bestscore)
                {
                    bestscore = score;
                    if (score >= beta)
                    {
                        delete movelist;
                        SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch (fail high).", score);
                        return score;
                    }
                    if (score > alpha)
                    {
                        alpha = score;
#ifdef EVALTUNE
                        foundpts = true;
                        pos.copyPositionTuneSet(&pos.pts, &pts);
#endif
                    }
                }
            }
        }
    }
#ifdef EVALTUNE
    if (foundpts)
        pos.copyPositionTuneSet(&pts, &pos.pts);
#endif

    if (LegalMovesPossible)
    {
        delete movelist;
        SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch.", bestscore);
        return bestscore;
    }

    // No valid move found; try quiet moves
    if (!pos.isCheck)
    {
        if (pos.getMoves(&movelist->move[0], QUIETWITHCHECK))
        {
            delete movelist;
            SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch.", bestscore);
            return bestscore;
        }

        // It's a stalemate
        delete movelist;
        SDEBUGPRINT(isDebugPv, debugInsert, " Got score 0 from qsearch (stalemate).");
        return SCOREDRAW;
    }
    else {
        // It's a mate
        delete movelist;
        SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from qsearch (mate).", SCOREBLACKWINS + pos.ply);
        return SCOREBLACKWINS + pos.ply;
    }
}



int alphabeta(int alpha, int beta, int depth, bool nullmoveallowed)
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
    unsigned int nmrefutetarget = BOARDSIZE;
    bool PVNode = (alpha != beta - 1);

    en.nodes++;

#ifdef SDEBUG
    chessmove debugMove;
    string excludestr = "";
    int debugInsert = pos.ply - pos.rootheight;
    bool isDebugPv = pos.triggerDebug(&debugMove);
    pos.pvtable[pos.ply][0] = 0;
#endif

    // test for remis via repetition
    if (pos.testRepetiton() >= 2)
    {
        SDEBUGPRINT(isDebugPv, debugInsert, "Draw (repetition)");
        return SCOREDRAW;
    }

    // test for remis via 50 moves rule
    if (pos.halfmovescounter >= 100)
    {
        SDEBUGPRINT(isDebugPv, debugInsert, "Draw (50 moves)");
        return SCOREDRAW;
    }

    // Get move for singularity check and change hash to seperate partial searches from full searches
    uint16_t excludeMove = movestack[mstop - 1].excludemove;
    movestack[mstop].excludemove = 0;

#ifdef SDEBUG
    if (isDebugPv)
    {
        chessmove cm, em;
        string s;
        for (int i = pos.rootheight; i < mstop; i++)
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

    U64 hash = pos.hash ^ excludeMove;

    if (tp.probeHash(hash, &hashscore, &staticeval, &hashmovecode, depth, alpha, beta)
        && rp.getPositionCount(pos.hash) <= 1)  //FIXME: This test on the repetition table works like a "is not PV"; should be fixed in the future)
    {
        SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from TT.", hashscore);
        return hashscore;
    }


    // TB
    // The test for rule50_count() == 0 is required to prevent probing in case
    // the root position is a TB position but only WDL tables are available.
    // In that case the search should not probe before a pawn move or capture
    // is made.
    if (POPCOUNT(pos.occupied00[0] | pos.occupied00[1]) <= pos.useTb && pos.halfmovescounter == 0)
    {
        int success;
        int v = probe_wdl(&success);
        if (success) {
            en.tbhits++;
            if (v < -1)
                score = -SCORETBWIN + pos.ply;
            else if (v > 1)
                score = SCORETBWIN - pos.ply;
            else 
                score = SCOREDRAW + v;
            tp.addHash(pos.hash, score, staticeval, HASHEXACT, depth, 0);
            SDEBUGPRINT(isDebugPv, debugInsert, " Got score %d from TB.", score);
            return score;
        }
    }


    if (depth <= 0)
    {
        // update selective depth info
        if (pos.seldepth < pos.ply + 1)
            pos.seldepth = pos.ply + 1;

        return getQuiescence(alpha, beta, depth);
    }

    // Check extension
    if (pos.isCheck)
        extendall = 1;

    pos.prepareStack();

    // get static evaluation of the position
    if (staticeval == NOSCORE)
        staticeval = S2MSIGN(pos.state & S2MMASK) * getValueNoTrace(&pos);
    movestack[mstop].staticeval = staticeval;

    // Nullmove pruning
    int bestknownscore = (hashscore != NOSCORE ? hashscore : staticeval);
    if (nullmoveallowed && !pos.isCheck && depth >= 3 && bestknownscore >= beta && pos.ph < 250)
    {
        pos.playNullMove();
        int R = 3 + (depth / 6) + (bestknownscore - beta) / 150;
        score = -alphabeta(-beta, -beta + 1, depth - R, false);
        pos.unplayNullMove();

        if (score >= beta)
        {
            SDEBUGPRINT(isDebugPv, debugInsert, " Cutoff by null move: %d", score);
            return score;
        }
        else {
            uint16_t nmrefutemove = tp.getMoveCode(hash);
            if (nmrefutemove && pos.mailbox[GETTO(nmrefutemove)] != BLANK)
                nmrefutetarget = GETTO(nmrefutemove);
        }
    }

    bool positionImproved = (mstop >= pos.rootheight + 2
        && movestack[mstop].staticeval > movestack[mstop - 2].staticeval);

    // futility pruning
    bool futility = false;
    if (depth <= 6)
    {
        // reverse futility pruning
        if (!pos.isCheck && staticeval - depth * (72 - 20 * positionImproved) > beta)
        {
            SDEBUGPRINT(isDebugPv, debugInsert, " Cutoff by reverse futility pruning: staticscore(%d) - revMargin(%d) > beta(%d)", staticeval, depth * (72 - 20 * positionImproved), beta);
            return staticeval;
        }
        futility = (staticeval < alpha - (100 + 80 * depth));
    }

    // Internal iterative deepening 
    const int iidmin = 3;
    const int iiddelta = 2;
    if (PVNode && !hashmovecode && depth >= iidmin)
    {
        SDEBUGPRINT(isDebugPv, debugInsert, " Entering iid...");
        alphabeta(alpha, beta, depth - iiddelta, true);
        hashmovecode = tp.getMoveCode(hash);
    }

    MoveSelector ms = {};
    ms.SetPreferredMoves(&pos, hashmovecode, pos.killer[0][pos.ply], pos.killer[1][pos.ply], nmrefutetarget);

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
            continue;

        // Check for futility pruning condition for this move and skip move if at least one legal move is already found
        bool futilityPrune = futility && !ISTACTICAL(m->code) && !pos.isCheck && alpha <= 900;
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
        if (!pos.isCheck && depth < 8 && bestscore > NOSCORE && ms.state >= BADTACTICALSTATE && !pos.see(m->code, -20 * depth * depth))
        {
            SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s pruned by bad SEE", debugMove.toString().c_str());
            continue;
        }

        int extendMove = 0;

        if ((m->code & 0xffff) == hashmovecode
            && depth > 7
            && !excludeMove
            && tp.probeHash(hash, &hashscore, &staticeval, &hashmovecode, depth - 3, alpha, beta)
            && hashscore > alpha)
        {
            SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s tested for singularity", debugMove.toString().c_str());
            movestack[mstop - 1].excludemove = hashmovecode;
            int sBeta = max(hashscore - 2 * depth, SCOREBLACKWINS);
            int redScore = alphabeta(sBeta - 1, sBeta, depth / 2, true);
            movestack[mstop - 1].excludemove = 0;

            if (redScore < sBeta)
            {
                SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s is singular", debugMove.toString().c_str());
                extendMove = 1;
            }
        }

        isLegal = pos.playMove(m);
        if (isLegal)
        {
            LegalMoves++;

            // Check again for futility pruning now that we found a valid move
            if (futilityPrune)
            {
                SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s pruned by futility: staticeval(%d) < alpha(%d) - futilityMargin(%d)", debugMove.toString().c_str(), staticeval, alpha, 100 + 80 * depth);
                pos.unplayMove(m);
                continue;
            }

            int reduction = 0;
            // Late move reduction
            if (!extendall && depth > 2 && !ISTACTICAL(m->code))
            {
                reduction = reductiontable[positionImproved][depth][min(63, LegalMoves)];
                SDEBUGPRINT(isDebugPv && isDebugMove && reduction, debugInsert, " PV move %s (value=%d) with depth reduced by %d", debugMove.toString().c_str(), m->value, reduction);
            }

            if (eval_type != HASHEXACT)
            {
                // First move ("PV-move"); do a normal search
                effectiveDepth = depth + extendall - reduction + extendMove;
                score = -alphabeta(-beta, -alpha, effectiveDepth - 1, true);
                if (reduction && score > alpha)
                {
                    // research without reduction
                    effectiveDepth += reduction;
                    score = -alphabeta(-beta, -alpha, effectiveDepth - 1, true);
                }
            }
            else {
                // try a PV-Search
                effectiveDepth = depth + extendall;
                score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1, true);
                if (score > alpha && score < beta)
                {
                    // reasearch with full window
                    score = -alphabeta(-beta, -alpha, effectiveDepth - 1, true);
                }
            }
            pos.unplayMove(m);

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
                    // Killermove
                    if (!ISCAPTURE(m->code))
                    {
                        pos.history[pos.state & S2MMASK][GETFROM(m->code)][GETTO(m->code)] += depth * depth;
                        for (int i = 0; i < quietsPlayed; i++)
                        {
                            uint32_t qm = quietMoves[i];
                            pos.history[pos.state & S2MMASK][GETFROM(qm)][GETTO(qm)] -= depth * depth;
                        }

                        if (pos.killer[0][pos.ply] != m->code)
                        {
                            pos.killer[1][pos.ply] = pos.killer[0][pos.ply];
                            pos.killer[0][pos.ply] = m->code;
                        }
                    }

                    SDEBUGPRINT(isDebugPv, debugInsert, " Beta-cutoff by move %s: %d  %s%s", m->toString().c_str(), score, excludestr.c_str(), excludeMove ? " : not singular" : "");
                    if (!excludeMove)
                    {
                        SDEBUGPRINT(isDebugPv, debugInsert, " ->Hash(%d) = %d(beta)", effectiveDepth, score);
                        tp.addHash(hash, score, staticeval, HASHBETA, effectiveDepth, (uint16_t)bestcode);
                    }
                    return score;   // fail soft beta-cutoff
                }

                if (score > alpha)
                {
                    SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s raising alpha to %d", debugMove.toString().c_str(), score);
                    alpha = score;
                    eval_type = HASHEXACT;
#ifdef SDEBUG
                    pos.updatePvTable(bestcode);
#endif
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

        if (pos.isCheck) {
            // It's a mate
            SDEBUGPRINT(isDebugPv, debugInsert, " Return score: %d  (mate)", SCOREBLACKWINS + pos.ply);
            return SCOREBLACKWINS + pos.ply;
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
        tp.addHash(hash, bestscore, staticeval, eval_type, depth, (uint16_t)bestcode);
    }

    return bestscore;
}


enum RootsearchType { SinglePVSearch, MultiPVSearch };


template <RootsearchType RT>
int rootsearch(int alpha, int beta, int depth)
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

    en.nodes++;

    if (isMultiPV)
    {
        lastmoveindex = 0;
        maxmoveindex = min(en.MultiPV, pos.rootmovelist.length);
        for (int i = 0; i < maxmoveindex; i++)
            pos.bestmovescore[i] = SHRT_MIN + 1;
    }

#ifdef SDEBUG
    chessmove debugMove;
    int debugInsert = pos.ply - pos.rootheight;
    bool isDebugPv = pos.triggerDebug(&debugMove);
    pos.pvtable[0][0] = 0;
    SDEBUGPRINT(true, debugInsert, "(depth=%2d) Rootsearch Next pv debug move: %s  [%3d,%3d]", depth, debugMove.code ? debugMove.toString().c_str() : "", alpha, beta);
#endif

    if (!isMultiPV
        && !pos.useRootmoveScore
        && tp.probeHash(pos.hash, &score, &staticeval, &hashmovecode, depth, alpha, beta)
        && rp.getPositionCount(pos.hash) <= 1)  //FIXME: Is this really needed in rootsearch?
    {
        return score;
    }

    // test for remis via repetition
    if (rp.getPositionCount(pos.hash) >= 3 && pos.testRepetiton() >= 2)
        return SCOREDRAW;

    // test for remis via 50 moves rule
    if (pos.halfmovescounter >= 100)
        return SCOREDRAW;

    if (pos.isCheck)
        extendall = 1;

    if (!pos.tbPosition)
    {
        // Reset move values
        for (int i = 0; i < pos.rootmovelist.length; i++)
        {
            m = &pos.rootmovelist.move[i];

            //PV moves gets top score
            if (hashmovecode == (m->code & 0xffff))
            {
                m->value = PVVAL;
            }
            // killermoves gets score better than non-capture
            else if (pos.killer[0][0] == m->code)
            {
                m->value = KILLERVAL1;
            }
            else if (pos.killer[1][0] == m->code)
            {
                m->value = KILLERVAL2;
            }
            else if (GETCAPTURE(m->code) != BLANK)
            {
                m->value = (mvv[GETCAPTURE(m->code) >> 1] | lva[GETPIECE(m->code) >> 1]);
            }
            else {
                m->value = pos.history[pos.state & S2MMASK][GETFROM(m->code)][GETTO(m->code)];
            }
        }
    }

    // get static evaluation of the position
    if (staticeval == NOSCORE)
        staticeval = S2MSIGN(pos.state & S2MMASK) * getValueNoTrace(&pos);
    movestack[mstop].staticeval = staticeval;

    int quietsPlayed = 0;
    uint32_t quietMoves[MAXMOVELISTLENGTH];

    for (int i = 0; i < pos.rootmovelist.length; i++)
    {
        for (int j = i + 1; j < pos.rootmovelist.length; j++)
        {
            if (pos.rootmovelist.move[i] < pos.rootmovelist.move[j])
            {
                swap(pos.rootmovelist.move[i], pos.rootmovelist.move[j]);
            }
        }

        m = &pos.rootmovelist.move[i];
#ifdef SDEBUG
        bool isDebugMove = (debugMove.code == (m->code & 0xeff));
#endif

        pos.playMove(m);

        if (en.moveoutput)
        {
            char s[256];
            sprintf_s(s, "info depth %d currmove %s currmovenumber %d nodes %llu tbhits %llu\n", depth, m->toString().c_str(), i + 1, en.nodes, en.tbhits);
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
            score = -alphabeta(-beta, -alpha, effectiveDepth - 1, true);
            if (reduction && score > alpha)
            {
                // research without reduction
                effectiveDepth += reduction;
                score = -alphabeta(-beta, -alpha, effectiveDepth - 1, true);
            }
        }
        else {
            // try a PV-Search
            effectiveDepth = depth + extendall;
            score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1, true);
            if (score > alpha && score < beta)
            {
                // reasearch with full window
                score = -alphabeta(-beta, -alpha, effectiveDepth - 1, true);
            }
        }

        SDEBUGPRINT(isDebugPv && isDebugMove, debugInsert, " PV move %s scored %d", debugMove.toString().c_str(), score);

        pos.unplayMove(m);
        if (pos.useRootmoveScore)
            score = m->value;

        if (en.stopLevel == ENGINESTOPIMMEDIATELY)
        {
            // time over; immediate stop requested
            return bestscore;
        }

        if (!ISTACTICAL(m->code))
            quietMoves[quietsPlayed++] = m->code;

        if ((isMultiPV && score <= pos.bestmovescore[lastmoveindex])
            || (!isMultiPV && score <= bestscore))
            continue;

        bestscore = score;
        if (isMultiPV && score > pos.bestmovescore[lastmoveindex])
        {
            int newindex = lastmoveindex;
            while (newindex > 0 && score > pos.bestmovescore[newindex - 1])
            {
                pos.bestmovescore[newindex] = pos.bestmovescore[newindex - 1];
                pos.bestmove[newindex] = pos.bestmove[newindex - 1];
                newindex--;
            }
            pos.bestmovescore[newindex] = score;
            pos.bestmove[newindex] = *m;
            if (lastmoveindex < maxmoveindex - 1)
                lastmoveindex++;
        }
        else {
            // overwriting best move even at fail low seems good but don't throw away a win
            if (alpha < 400 || !pos.bestmove[0].code)
                pos.bestmove[0] = *m;
        }

        if (score >= beta)
        {
            // Killermove
            if (!ISCAPTURE(m->code))
            {
                pos.history[pos.state & S2MMASK][GETFROM(m->code)][GETTO(m->code)] += depth * depth;
                for (int i = 0; i < quietsPlayed - 1; i++)
                {
                    uint32_t qm = quietMoves[i];
                    pos.history[pos.state & S2MMASK][GETFROM(qm)][GETTO(qm)] -= depth * depth;
                }

                if (pos.killer[0][0] != m->code)
                {
                    pos.killer[1][0] = pos.killer[0][0];
                    pos.killer[0][0] = m->code;
                }
            }
            SDEBUGPRINT(isDebugPv, debugInsert, " Beta-cutoff by move %s: %d", m->toString().c_str(), score);
            tp.addHash(pos.hash, beta, staticeval, HASHBETA, effectiveDepth, (uint16_t)m->code);
            return beta;   // fail hard beta-cutoff
        }

        if (score > alpha)
        {
            if (isMultiPV)
            {
                if (pos.bestmovescore[maxmoveindex - 1] > alpha)
                    alpha = pos.bestmovescore[maxmoveindex - 1];
            }
            else {
                alpha = score;
            }
            eval_type = HASHEXACT;
#ifdef SDEBUG
            pos.updatePvTable(m->code);
#endif
        }
    }

    SDEBUGPRINT(true, 0, pos.getPv().c_str());

    if (isMultiPV)
    {
        if (eval_type == HASHEXACT)
        {
            tp.addHash(pos.hash, pos.bestmovescore[0], staticeval, eval_type, depth, (uint16_t)pos.bestmove[0].code);
            return pos.bestmovescore[maxmoveindex - 1];
        }
        else {
            tp.addHash(pos.hash, alpha, staticeval, eval_type, depth, (uint16_t)pos.bestmove[0].code);
            return alpha;
        }
    }
    else {
        tp.addHash(pos.hash, alpha, staticeval, eval_type, depth, (uint16_t)pos.bestmove[0].code);
        return alpha;
    }
}


template <RootsearchType RT>
static void search_gen1()
{
    string bestmovestr = "";
    string newbestmovestr;
    string pondermovestr = "";

    int score;
    int matein;
    int alpha, beta;
    int deltaalpha = 8;
    int deltabeta = 8;
    int depth, maxdepth, depthincrement;
    string pvstring;
    int inWindow;
    int lastsecondsrun = 0;
    const char* boundscore[] = { "upperbound", "", "lowerbound" };

    const bool isMultiPV = (RT == MultiPVSearch);

    depthincrement = 1;
    if (en.mate > 0)
    {
        depth = maxdepth = en.mate * 2;
    }
    else
    {
        depth = 1;
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
    // iterative deepening
    do
    {
        matein = MAXDEPTH;
        // Reset bestmove to detect alpha raise in interrupted search
        pos.bestmove[0].code = 0;
        inWindow = 1;
        pos.seldepth = depth;

        if (pos.rootmovelist.length == 0)
        {
            pos.bestmove[0].code = 0;
            score =  (pos.isCheck ? SCOREBLACKWINS : SCOREDRAW);
            en.stopLevel = ENGINESTOPPED;
        } else
        {
            score = rootsearch<RT>(alpha, beta, depth);
            //printf("info string Rootsearch: alpha=%d beta=%d depth=%d score=%d bestscore[0]=%d bestscore[%d]=%d\n", alpha, beta, depth, score, pos.bestmovescore[0], en.MultiPV - 1,  pos.bestmovescore[en.MultiPV - 1]);

            // new aspiration window
            if (score == alpha)
            {
                // research with lower alpha and reduced beta
                beta = (alpha + beta) / 2;
                alpha = max(SHRT_MIN + 1, alpha - deltaalpha);
                deltaalpha += deltaalpha / 4 + 2;
                //deltaalpha <<= 1;
                if (abs(alpha) > 1000)
                    deltaalpha = SHRT_MAX << 1;
                inWindow = 0;
            }
            else if (score == beta)
            {
                // research with higher beta
                beta = min(SHRT_MAX, beta + deltabeta);
                deltabeta += deltabeta / 4 + 2;
                //deltabeta <<= 1;
                if (abs(beta) > 1000)
                    deltabeta = SHRT_MAX << 1;
                inWindow = 2;
            }
            else
            {
                if (score >= en.terminationscore)
                {
                    // bench mode reached needed score
                    en.stopLevel = ENGINEWANTSTOP;
                }
                else {
                    // next depth with new aspiration window
                    deltaalpha = 8;
                    deltabeta = 8;
                    //printf("info string depth=%d delta=%d\n", depth, deltaalpha);
                    if (isMultiPV)
                        alpha = pos.bestmovescore[en.MultiPV - 1] - deltaalpha;
                    else
                        alpha = score - deltaalpha;
                    beta = score + deltabeta;
                }
            }
        }
        if (score > NOSCORE)
        {
            long long nowtime = getTime();
            int secondsrun = (int)((nowtime - en.starttime) * 1000 / en.frequency);

            // search was successfull
            if (isMultiPV)
            {
                if (inWindow == 1)
                {
                    // MultiPV output only if in aspiration window
                    // FIXME: This is a bit ugly... code more consistent with SinglePV would be better
                    // but I had to fight against performance regression so I devided it this way
                    int i = 0;
                    int maxmoveindex = min(en.MultiPV, pos.rootmovelist.length);
                    do
                    {
                        // The only case that bestmove is not set can happen if rootsearch hit the TP table
                        // so get bestmovecode from there
                        if (!pos.bestmove[i].code)
                        {
                            uint16_t mc;
                            int dummystaticeval;
                            tp.probeHash(pos.hash, &pos.bestmovescore[i], &dummystaticeval, &mc, depth, alpha, beta);
                            pos.bestmove[i].code = pos.shortMove2FullMove(mc);
                        }

                        pos.getpvline(depth, i);
                        pvstring = pos.pvline.toString();
                        if (i == 0)
                        {
                            // get bestmove
                            if (pos.pvline.length > 0 && pos.pvline.move[0].code)
                                bestmovestr = pos.pvline.move[0].toString();
                            else
                                bestmovestr = pos.bestmove[0].toString();
                        }
                        char s[4096];
                        if (!MATEDETECTED(pos.bestmovescore[i]))
                        {
                            sprintf_s(s, "info depth %d seldepth %d multipv %d time %d score cp %d %s pv %s\n", depth, pos.seldepth, i + 1, secondsrun, pos.bestmovescore[i], boundscore[inWindow], pvstring.c_str());
                        }
                        else
                        {
                            matein = (pos.bestmovescore[i] > 0 ? (SCOREWHITEWINS - pos.bestmovescore[i] + 1) / 2 : (SCOREBLACKWINS - pos.bestmovescore[i]) / 2);
                            sprintf_s(s, "info depth %d seldepth %d multipv %d time %d score mate %d pv %s\n", depth, pos.seldepth, i + 1, secondsrun, matein, pvstring.c_str());
                        }
                        cout << s;
                        i++;
                    } while (i < maxmoveindex
                        && (pos.bestmove[i].code || (pos.bestmove[i].code = tp.getMoveCode(pos.hash)))
                        && pos.bestmovescore[i] > SHRT_MIN + 1);
                }
            }
            else {
                // The only two cases that bestmove is not set can happen if alphabeta hit the TP table or we are in TB
                // so get bestmovecode from there or it was a TB hit so just get the first rootmove
                if (!pos.bestmove[0].code)
                {
                    uint16_t mc = 0;
                    int dummystaticeval;
                    tp.probeHash(pos.hash, &score, &dummystaticeval, &mc, MAXDEPTH, alpha, beta);
                    pos.bestmove[0].code = pos.shortMove2FullMove(mc);
                }
                    
                // still no bestmove...
                if (!pos.bestmove[0].code && pos.rootmovelist.length > 0)
                    pos.bestmove[0].code = pos.rootmovelist.move[0].code;

                pos.getpvline(depth, 0);
                pvstring = pos.pvline.toString();
                bool getponderfrompvline = false;
                if (pos.pvline.length > 0 && pos.pvline.move[0].code)
                {
                    newbestmovestr= pos.pvline.move[0].toString();
                    getponderfrompvline = (en.ponder && pos.pvline.length > 1 && pos.pvline.move[1].code);
                }
                else {
                    newbestmovestr = pos.bestmove[0].toString();
                }
                if (newbestmovestr != bestmovestr)
                {
                    bestmovestr = newbestmovestr;
                    pondermovestr = "";
                }
                if (getponderfrompvline)
                    pondermovestr = " ponder " + pos.pvline.move[1].toString();

                char s[4096];
                if (inWindow == 1 || (secondsrun - lastsecondsrun) > 200)
                {
                    lastsecondsrun = secondsrun;
                    if (!MATEDETECTED(score))
                    {
                        sprintf_s(s, "info depth %d seldepth %d time %d score cp %d %s nodes %llu nps %llu tbhits %llu hashfull %d pv %s\n",
                            depth, pos.seldepth, secondsrun, score, boundscore[inWindow], en.nodes,
                            (nowtime > en.starttime ? en.nodes * en.frequency / (nowtime - en.starttime) : 1),
                            en.tbhits, tp.getUsedinPermill(), pvstring.c_str());
                    }
                    else
                    {
                        matein = (score > 0 ? (SCOREWHITEWINS - score + 1) / 2 : (SCOREBLACKWINS - score) / 2);
                        sprintf_s(s, "info depth %d seldepth %d time %d score mate %d nodes %llu nps %llu tbhits %llu hashfull %d pv %s\n",
                            depth, pos.seldepth, secondsrun, matein, en.nodes,
                            (nowtime > en.starttime ? en.nodes * en.frequency / (nowtime - en.starttime) : 1),
                            en.tbhits, tp.getUsedinPermill(), pvstring.c_str());
                    }
                    cout << s;
                }
            }
        }
        if (inWindow == 1)
        {
            depth += depthincrement;
            constantRootMoves++;
            if (lastBestMove != pos.bestmove[0].code)
            {
                lastBestMove = pos.bestmove[0].code;
                constantRootMoves = 0;
            }
            resetEndTime(constantRootMoves);
        }

        if (pos.rootmovelist.length == 1 && depth > 4 && en.endtime1 && !en.isPondering())
            // early exit in playing mode as there is exactly one possible move
            en.stopLevel = ENGINEWANTSTOP;
        if (pos.tbPosition && abs(score) >= SCORETBWIN - 100)
            // early exit in TB win/lose position
            en.stopLevel = ENGINEWANTSTOP;
    } while (en.stopLevel == ENGINERUN && depth <= min(maxdepth, abs(matein) * 2));
    
    if (bestmovestr == "")
        // not a single move found (serious time trouble); fall back to default move
        bestmovestr = pos.defaultmove.toString();

    char s[64];
    sprintf_s(s, "bestmove %s%s\n", bestmovestr.c_str(), pondermovestr.c_str());

    // when pondering prevent from stopping search before STOP
    while (en.isPondering() && en.stopLevel != ENGINESTOPIMMEDIATELY)
        Sleep(10);
 
    cout << s;
    en.stopLevel = ENGINESTOPPED;
    // Remember some exit values for benchmark output
    en.benchscore = score;
    en.benchdepth = depth - 1;
}

void resetEndTime(int constantRootMoves, bool complete)
{
    int timetouse = (en.isWhite ? en.wtime : en.btime);
    int timeinc = (en.isWhite ? en.winc : en.binc);

    if (en.movestogo)
    {
        // should garantee timetouse > 0
        // stop soon at 0.5...1.5 x average movetime
        // stop immediately at 1.3...2.3 x average movetime
        int f1 = max(5, 15 - constantRootMoves * 2);
        int f2 = max(13, 23 - constantRootMoves * 2);
        if (complete)
            en.endtime1 = en.starttime + timetouse * en.frequency * f1 / (en.movestogo + 1) / 10000;
        en.endtime2 = en.starttime + min(timetouse - en.moveOverhead, f2 * timetouse / (en.movestogo + 1) / 10) * en.frequency / 1000;
        //printf("info string difftime1=%lld  difftime2=%lld\n", (endtime1 - en.starttime) * 1000 / en.frequency , (endtime2 - en.starttime) * 1000 / en.frequency);
    }
    else if (timetouse) {
        int ph = pos.phase();
        if (timeinc)
        {
            // sudden death with increment; split the remaining time in (256-phase) timeslots
            // stop soon after 6..10 timeslot
            // stop immediately after 10..18 timeslots
            int f1 = max(6, 10 - constantRootMoves);
            int f2 = max(10, 18 - constantRootMoves);
            if (complete)
                en.endtime1 = en.starttime + max(timeinc, f1 * (timetouse + timeinc) / (256 - ph)) * en.frequency / 1000;
            en.endtime2 = en.starttime + min(timetouse - en.moveOverhead, max(timeinc, f2 * (timetouse + timeinc) / (256 - ph))) * en.frequency / 1000;
        }
        else {
            // sudden death without increment; play for another x;y moves
            // stop soon at 1/35...1/45 time slot
            // stop immediately at 1/20...1/30 time slot
            int f1 = min(45, 35 + constantRootMoves * 2);
            int f2 = min(30, 20 + constantRootMoves * 2);
            if (complete)
                en.endtime1 = en.starttime + timetouse / f1 * en.frequency / 1000;
            en.endtime2 = en.starttime + min(timetouse - en.moveOverhead, timetouse / f2) * en.frequency / 1000;
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
    en.moveoutput = false;
    en.stopLevel = ENGINERUN;
    thread enginethread;

    startSearchTime();

    en.nodes = 0;
    en.tbhits = 0;
    en.fh = en.fhf = 0;

    if (en.MultiPV > 1)
        enginethread = thread(&search_gen1<MultiPVSearch>);
    else
        enginethread = thread(&search_gen1<SinglePVSearch>);

    long long nowtime;
    while (en.stopLevel != ENGINESTOPPED)
    {
        nowtime = getTime();

        if (nowtime - en.starttime > 3 * en.frequency)
            en.moveoutput = true;

        if (en.stopLevel != ENGINESTOPPED)
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
    enginethread.join();
    en.stopLevel = ENGINETERMINATEDSEARCH;
}
