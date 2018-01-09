
#include "RubiChess.h"


const int deltapruningmargin = 200;

int getQuiescence(int alpha, int beta, int depth)
{
    int patscore, score;
    int bestscore;
    bool isLegal;
    bool LegalMovesPossible = false;

    // FIXME: Should quiescience nodes count for the statistics?
    //en.nodes++;
#ifdef DEBUG
    en.qnodes++;
#endif


    patscore = (pos.state & S2MMASK ? -pos.getValue() : pos.getValue());
    bestscore = patscore;
    if (!pos.isCheck)
    {
        PDEBUG(depth, "(getQuiscence) qnode=%d alpha=%d beta=%d patscore=%d\n", en.qnodes, alpha, beta, patscore);
        if (patscore >= beta)
            return patscore;
        if (patscore > alpha)
            alpha = patscore;
    }

    chessmovelist* movelist = pos.getMoves();
    //pos->sortMoves(movelist);

    for (int i = 0; i < movelist->length; i++)
    {
        bool noDeltaprune = (patscore + materialvalue[GETCAPTURE(movelist->move[i].code) >> 1] + deltapruningmargin > alpha);
        PDEBUG(depth, "(getQuiscence) testing move %s ... LegalMovesPossible=%d Capture=%d Promotion=%d see=%d \n", movelist->move[i].toString().c_str(), (LegalMovesPossible?1:0), GETCAPTURE(movelist->move[i].code), GETPROMOTION(movelist->move[i].code), pos.see(GETFROM(movelist->move[i].code), GETTO(movelist->move[i].code)));
        bool MoveIsUsefull = ((pos.isCheck && noDeltaprune)
            || ISPROMOTION(movelist->move[i].code)
            || (ISCAPTURE(movelist->move[i].code) 
                && noDeltaprune
                && (pos.see(GETFROM(movelist->move[i].code), GETTO(movelist->move[i].code)) >= 0)
            ));
#ifdef DEBUG
        if (ISCAPTURE(movelist->move[i].code) && patscore + materialvalue[GETCAPTURE(movelist->move[i].code) >> 1] + deltapruningmargin <= alpha)
        {
            en.dpnodes++;
            //printf("delta prune: patscore:%d move:%s  alpha=%d\n", patscore, movelist->move[i].toString().c_str(), alpha);
            //pos.print();
        }
#endif

        if (MoveIsUsefull || !LegalMovesPossible)
        {
            isLegal = pos.playMove(&(movelist->move[i]));
            if (isLegal)
            {
                LegalMovesPossible = true;
                if (MoveIsUsefull)
                {
                    score = -getQuiescence(-beta, -alpha, depth - 1);
                    PDEBUG(depth, "(getQuiscence) played move %s score=%d\n", movelist->move[i].toString().c_str(), score);
                }
                pos.unplayMove(&(movelist->move[i]));
                if (MoveIsUsefull && score > bestscore)
                {
                    bestscore = score;
                    if (score >= beta)
                    {
                        free(movelist);
                        PDEBUG(depth, "(getQuiscence) beta cutoff\n");
                        return score;
                    }
                    if (score > alpha)
                    {
                        alpha = score;
                        PDEBUG(depth, "(getQuiscence) new alpha\n");
                    }
                }
            }
        }
    }
    free(movelist);
    if (LegalMovesPossible)
        return bestscore;
    // No valid move found
    if (pos.isCheck)
        // It's a mate
        return SCOREBLACKWINS + pos.ply;
    else
        // It's a stalemate
        return SCOREDRAW;
}



int alphabeta(int alpha, int beta, int depth, bool nullmoveallowed)
{
    int score;
    uint32_t hashmovecode = 0;
    int  LegalMoves = 0;
    bool isLegal;
    int bestscore = SHRT_MIN + 1;
    uint32_t bestcode = 0;
    int eval_type = HASHALPHA;
    chessmovelist* newmoves;
    chessmove *m;
    int extendall = 0;
    int reduction;
    int effectiveDepth;

    en.nodes++;

#ifdef DEBUG
    int oldmaxdebugdepth;
    int oldmindebugdepth;
    if (en.debug && pos.debughash == pos.hash)
    {
        oldmaxdebugdepth = pos.maxdebugdepth;
        oldmindebugdepth = pos.mindebugdepth;
        printf("Reached position to debug... starting debug.\n");
        pos.print();
        pos.maxdebugdepth = depth;
        pos.mindebugdepth = -100;
    }
#endif

    PDEBUG(depth, "depth=%d alpha=%d beta=%d\n", depth, alpha, beta);

    if (tp.probeHash(&score, &hashmovecode, depth, alpha, beta))
    {
        PDEBUG(depth, "(alphabeta) got value %d from TP\n", score);
        if (rp.getPositionCount(pos.hash) <= 1)  //FIXME: This is a rough guess to avoid draw by repetition hidden by the TP table
            return score;
    }

    // test for remis via repetition
    if (rp.getPositionCount(pos.hash) >= 3 && pos.testRepetiton())
        return SCOREDRAW;

    // test for remis via 50 moves rule
    if (pos.halfmovescounter >= 100)
        return SCOREDRAW;

    if (depth <= 0)
    {
        return getQuiescence(alpha, beta, depth);
    }

    if (pos.isCheck)
        extendall = 1;

    // chessmove lastmove = pos.actualpath.move[pos.actualpath.length - 1];
    // Here some reduction/extension depending on the lastmove...

    // Nullmove
    if (nullmoveallowed && !pos.isCheck && depth >= 4 && pos.phase() < 150)
    {
        // FIXME: Something to avoid nullmove in endgame is missing... pos->phase() < 150 needs validation
        pos.playNullMove();

        score = -alphabeta(-beta, -beta + 1, depth - 4, false);
        
        if (score >= beta)
        {
            pos.unplayNullMove();
            PDEBUG(depth, "Nullmove beta cuttoff score=%d >= beta=%d\n", score, beta);
            return score;
        }
        else {
#if 0 // disabled for now; first working on better quiescense
            if (score < alpha - 300)
            {
                PDEBUG(depth, "Nullmove a=%d b=%d score=%d thread detected => extension\n", alpha, beta, score);
                extendall++;
            }
#endif
            pos.unplayNullMove();
        }
    }

    // futility pruning
    const int futilityMargin = 130;
    bool futility = false;
#ifdef DEBUG
    int futilityscore;
#endif
    if (depth == 1)
    {
        score = S2MSIGN(pos.state & S2MMASK) * pos.getValue();
        futility = (score < alpha - futilityMargin);
#ifdef DEBUG
        futilityscore = score;
#endif
    }

    newmoves = pos.getMoves();

#ifdef DEBUG
    en.nopvnodes++;
#endif
    for (int i = 0; i < newmoves->length; i++)
    {
        m = &newmoves->move[i];
        //PV moves gets top score
        if (hashmovecode == m->code)
        {
#ifdef DEBUG
            en.pvnodes++;
#endif
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
    }

    for (int i = 0; i < newmoves->length; i++)
    {
        for (int j = i + 1; j < newmoves->length; j++)
        {
            if (newmoves->move[i] < newmoves->move[j])
            {
                swap(newmoves->move[i], newmoves->move[j]);
            }
        }

        m = &newmoves->move[i];
        int moveExtension = 0;
        isLegal = pos.playMove(m);
        if (isLegal)
        {
            LegalMoves++;
            PDEBUG(depth, "(alphabeta) played move %s   nodes:%d\n", newmoves->move[i].toString().c_str(), en.nodes);

            // Check for valid futility pruning
            bool avoidFutilityPrune = !futility || ISTACTICAL(m->code) || pos.isCheck || alpha > 900;
#ifdef DEBUG
            if (!avoidFutilityPrune)
            {
                en.fpnodes++;
            }
#endif
            if (avoidFutilityPrune) // disable this test to debug wrongfp
            {
                //extendall = 0; //FIXME: Indroduce extend variable for move specific extension
                reduction = 0;
                if (!extendall && depth > 2 && LegalMoves > 3 && !ISTACTICAL(m->code) && !pos.isCheck)
                    reduction = 1;
#if 0
                // disabled; capture extension doesn't seem to work
                else if (ISTACTICAL(m->code) && GETPIECE(m->code) <= GETCAPTURE(m->code))
                    extendall = 1;
#endif
                if (!eval_type == HASHEXACT)
                {
#if 0
                    // disabled; even 'good capture' extension doesn't seem to work
                    if (ISCAPTURE(m->code) && materialvalue[GETPIECE(m->code) >> 1] - materialvalue[GETCAPTURE(m->code) >> 1] < 30)
                        moveExtension = 1;
#endif
                    effectiveDepth = depth + moveExtension + extendall - reduction;
                    score = -alphabeta(-beta, -alpha, effectiveDepth - 1, true);
                    if (reduction && score > alpha)
                    {
                        // research without reduction
                        score = -alphabeta(-beta, -alpha, depth + extendall - 1, true);
                        effectiveDepth--;
                    }
                }
                else {
                    // try a PV-Search
#ifdef DEBUG
                    unsigned long nodesbefore = en.nodes;
#endif
                    effectiveDepth = depth + extendall;
                    score = -alphabeta(-alpha - 1, -alpha, effectiveDepth - 1, true);
                    if (score > alpha && score < beta)
                    {
                        // reasearch with full window
#ifdef DEBUG
                        en.wastedpvsnodes += (en.nodes - nodesbefore);
#endif
                        score = -alphabeta(-beta, -alpha, effectiveDepth - 1, true);
                    }
                }
#ifdef DEBUG
                if (score > alpha && !avoidFutilityPrune)
                {
                    en.wrongfp++;
                    printf("Wrong pruning: Futility-Score:%d Move:%s Score:%d\nPosition:\n", futilityscore, m->toString().c_str(), score);
                    pos.print();
                    printf("\n\n");
                }
#endif
            }

            pos.unplayMove(m);

#ifdef DEBUG
            if (en.debug && pos.debughash == pos.hash)
            {
                pos.actualpath.length = pos.ply;
                printf("Leaving position to debug... stoping debug. Score:%d\n", score);
                pos.maxdebugdepth = oldmaxdebugdepth;
                pos.mindebugdepth = oldmindebugdepth;
            }
#endif
            if (en.stopLevel == ENGINESTOPIMMEDIATELY && LegalMoves > 1)
            {
                // At least one move is found and we can safely exit here
                // Lets hope this doesn't take too much time...
                free(newmoves);
                return alpha;
            }

            if (score > bestscore)
            {
                bestscore = score;
                bestcode = m->code;

                if (score >= beta)
                {
                    // Killermove
                    if (GETCAPTURE(m->code) == BLANK)
                    {
                        pos.killer[1][pos.ply] = pos.killer[0][pos.ply];
                        pos.killer[0][pos.ply] = m->code;
                    }

#ifdef DEBUG
                    en.fh++;
                    if (LegalMoves == 1)
                        en.fhf++;
#endif
                    PDEBUG(depth, "(alphabeta) score=%d >= beta=%d  -> cutoff\n", score, beta);
                    tp.addHash(score, HASHBETA, effectiveDepth, bestcode);
                    free(newmoves);
                    return score;   // fail soft beta-cutoff
                }

                if (score > alpha)
                {
                    PDEBUG(depth, "(alphabeta) score=%d > alpha=%d  -> new best move(%d) %s   Path:%s\n", score, alpha, depth, newmoves->move[i].toString().c_str(), pos.actualpath.toString().c_str());
                    alpha = score;
                    eval_type = HASHEXACT;
                    if (!ISCAPTURE(m->code))
                    {
                        pos.history[pos.Piece(GETFROM(m->code))][GETTO(m->code)] += depth * depth;
                    }
                }
            }
        }
    }

    free(newmoves);
    if (LegalMoves == 0)
    {
        if (pos.isCheck)
            // It's a mate
            return SCOREBLACKWINS + pos.ply;
        else
            // It's a stalemate
            return SCOREDRAW;
    }

    tp.addHash(bestscore, eval_type, depth, bestcode);
    return bestscore;
}



int rootsearch(int alpha, int beta, int depth)
{
    int score;
    uint32_t hashmovecode = 0;
    int  LegalMoves = 0;
    bool isLegal;
    //int bestscore = SHRT_MIN + 1;
    //chessmove best;
    int eval_type = HASHALPHA;
    chessmovelist* newmoves;
    chessmove *m;
    int extendall = 0;
    int reduction;
    int lastmoveindex = 0;

    en.nodes++;

    for (int i = 0; i < en.MultiPV; i++)
        pos.bestmovescore[i] = SHRT_MIN + 1;

#ifdef DEBUG
    int oldmaxdebugdepth;
    int oldmindebugdepth;
    if (en.debug && pos.debughash == pos.hash)
    {
        oldmaxdebugdepth = pos.maxdebugdepth;
        oldmindebugdepth = pos.mindebugdepth;
        printf("Reached position to debug... starting debug.\n");
        pos.print();
        pos.maxdebugdepth = depth;
        pos.mindebugdepth = -100;
    }
#endif

    PDEBUG(depth, "depth=%d alpha=%d beta=%d\n", depth, alpha, beta);
#if 0 //FIXME: Disable TT is needed for MultiPV but is this good? 
    if (tp.probeHash(&score, &hashmovecode, depth, alpha, beta))
    {
        if (rp.getPositionCount(pos.hash) <= 1)  //FIXME: This is a rough guess to avoid draw by repetition hidden by the TP table
            return score;
    }
#endif
    // test for remis via repetition
    if (rp.getPositionCount(pos.hash) >= 3 && pos.testRepetiton())
        return SCOREDRAW;

    // test for remis via 50 moves rule
    if (pos.halfmovescounter >= 100)
        return SCOREDRAW;

    newmoves = pos.getMoves();
    if (pos.isCheck)
        depth++;

#ifdef DEBUG
    en.nopvnodes++;
#endif
    for (int i = 0; i < newmoves->length; i++)
    {
        m = &newmoves->move[i];
        //PV moves gets top score
        if (hashmovecode == m->code)
        {
#ifdef DEBUG
            en.pvnodes++;
#endif
            m->value = PVVAL;
        }
        // killermoves gets score better than non-capture
        else if (pos.killer[0][pos.ply] == m->code)
        {
            m->value = KILLERVAL1;
        }
        else if (pos.killer[1][pos.ply] == m->code)
        {
            m->value = KILLERVAL2;
        }
    }

    for (int i = 0; i < newmoves->length; i++)
    {
        for (int j = i + 1; j < newmoves->length; j++)
        {
            if (newmoves->move[i] < newmoves->move[j])
            {
                swap(newmoves->move[i], newmoves->move[j]);
            }
        }

        m = &newmoves->move[i];
        isLegal = pos.playMove(m);

        if (isLegal)
        {
            LegalMoves++;
            PDEBUG(depth, "(rootsearch) played move %s   nodes:%d\n", newmoves->move[i].toString().c_str(), en.nodes);

            reduction = 0;
            if (!extendall && depth > 2 && LegalMoves > 3 && !ISTACTICAL(m->code) && !pos.isCheck)
                reduction = 1;

            if (!eval_type == HASHEXACT)
            {
                score = -alphabeta(-beta, -alpha, depth + extendall - reduction - 1, true);
                if (reduction && score > alpha)
                    // research without reduction
                    score = -alphabeta(-beta, -alpha, depth + extendall - 1, true);
            }
            else {
                // try a PV-Search
#ifdef DEBUG
                unsigned long nodesbefore = en.nodes;
#endif
                score = -alphabeta(-alpha - 1, -alpha, depth + extendall - 1, true);
                if (score > alpha && score < beta)
                {
                    // reasearch with full window
#ifdef DEBUG
                    en.wastedpvsnodes += (en.nodes - nodesbefore);
#endif
                    score = -alphabeta(-beta, -alpha, depth + extendall - reduction - 1, true);
                }
            }

#ifdef DEBUG
            if (en.debug && pos.debughash == pos.hash)
            {
                pos.actualpath.length = pos.ply;
                printf("Leaving position to debug... stoping debug. Score:%d\n", score);
                pos.maxdebugdepth = oldmaxdebugdepth;
                pos.mindebugdepth = oldmindebugdepth;
            }
#endif
            pos.unplayMove(m);

            if (en.stopLevel == ENGINESTOPIMMEDIATELY && LegalMoves > 1)
            {
                // At least one move is found and we can safely exit here
                // Lets hope this doesn't take too much time...
                free(newmoves);
                return pos.bestmovescore[0];
            }

            if (score > pos.bestmovescore[lastmoveindex])
            {
                //best = *m;
                int newindex = lastmoveindex;
                while (newindex > 0 && score > pos.bestmovescore[newindex - 1])
                {
                    pos.bestmovescore[newindex] = pos.bestmovescore[newindex - 1];
                    pos.bestmove[newindex] = pos.bestmove[newindex - 1];
                    newindex--;
                }
                pos.bestmovescore[newindex] = score;
                pos.bestmove[newindex] =*m;
                if (lastmoveindex < en.MultiPV - 1)
                    lastmoveindex++;

                if (score >= beta)
                {
                    // Killermove
                    if (GETCAPTURE(m->code) == BLANK)
                    {
                        pos.killer[1][0] = pos.killer[0][0];
                        pos.killer[0][0] = m->code;
                    }
#ifdef DEBUG
                    en.fh++;
                    if (LegalMoves == 1)
                        en.fhf++;
#endif
                    PDEBUG(depth, "(rootsearch) score=%d >= beta=%d  -> cutoff\n", score, beta);
                    tp.addHash(beta, HASHBETA, depth, m->code);
                    free(newmoves);
                    return beta;   // fail hard beta-cutoff
                }

                if (pos.bestmovescore[en.MultiPV - 1] > alpha)
                {
                    PDEBUG(depth, "(rootsearch) score=%d > alpha=%d  -> new best move(%d) %s   Path:%s\n", score, alpha, depth, m->toString().c_str(), pos.actualpath.toString().c_str());
                    alpha = pos.bestmovescore[lastmoveindex];
                    eval_type = HASHEXACT;
                    if (!ISCAPTURE(m->code))
                    {
                        pos.history[pos.Piece(GETFROM(m->code))][GETTO(m->code)] += depth * depth;
                    }
                }
            }
        }
    }

    free(newmoves);
    if (LegalMoves == 0)
    {
        pos.bestmove[0].code = 0;
        en.stopLevel = ENGINEWANTSTOP;
        if (pos.isCheck)
            // It's a mate
            return SCOREBLACKWINS;
        else
            // It's a stalemate
            return SCOREDRAW;
    }

    tp.addHash(pos.bestmovescore[0], eval_type, depth, pos.bestmove[0].code);
    return pos.bestmovescore[0];
}


#ifdef DEBUG
int aspirationdelta[MAXDEPTH][2000] = { 0 };
#endif

static void search_gen1()
{
    string move;
    char s[16384];

    int score;
    int matein;
    int alpha, beta;
    int deltaalpha = 25;
    int deltabeta = 25;
    int depth, maxdepth, depthincrement;
    string pvstring;

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

    // iterative deepening
    do
    {
        matein = MAXDEPTH;
        pos.maxdebugdepth = -1;
        pos.mindebugdepth = 0;
        if (en.debug)
        {
            // Basic debuging
            pos.maxdebugdepth = depth;
            pos.mindebugdepth = depth;
            PDEBUG(depth, "Next depth: %d\n", depth);
        }

        // Reset bestmove to detect alpha raise in interrupted search
        pos.bestmove[0].code = 0;
#ifdef DEBUG
        int oldscore = 0;
        unsigned long nodesbefore = en.nodes;
        en.npd[depth] = 0;
#endif
        score = rootsearch(alpha, beta, depth);
        //printf("info string Rootsearch: alpha=%d beta=%d depth=%d score=%d bestscore[0]=%d bestscore[%d]=%d\n", alpha, beta, depth, score, pos.bestmovescore[0], en.MultiPV - 1,  pos.bestmovescore[en.MultiPV - 1]);

        // new aspiration window
        if (score >= beta)
        {
            // research with higher beta
            beta = min(SHRT_MAX, beta + deltabeta);
            deltabeta <<= 1;
#ifdef DEBUG
            en.wastedaspnodes += (en.nodes - nodesbefore);
#endif
        }
        else if (pos.bestmovescore[en.MultiPV - 1] <= alpha)
        {
            // research with lower alpha
            alpha = max(SHRT_MIN + 1, alpha - deltaalpha);
            deltaalpha <<= 1;
#ifdef DEBUG
            en.wastedaspnodes += (en.nodes - nodesbefore);
#endif
        }
        else
        {
            int secondsrun = (int)((getTime() - en.starttime) * 1000 / en.frequency);

            // search was successfull
            PDEBUG(depth, "Searchorder-Success: %f\n", (en.fh > 0 ? en.fhf / en.fh : 0.0));
            for (int i = 0; i < en.MultiPV; i++)
            {
                // The only case that bestmove is not set can happen if rootsearch hit the TP table
                // so get bestmovecode from there
                if (!pos.bestmove[i].code)
                    tp.probeHash(&score, &pos.bestmove[i].code, MAXDEPTH, alpha, beta);

                pos.getpvline(depth, i);
                pvstring = pos.pvline.toString();

                if (!MATEDETECTED(pos.bestmovescore[i]))
                {
                    sprintf_s(s, "info depth %d multipv %d time %d score cp %d pv %s\n", depth, i + 1, secondsrun, pos.bestmovescore[i], pvstring.c_str());
                }
                else
                {
                    matein = (pos.bestmovescore[i] > 0 ? (SCOREWHITEWINS - pos.bestmovescore[i] + 1) / 2 : (SCOREBLACKWINS - pos.bestmovescore[i]) / 2);
                    sprintf_s(s, "info depth %d multipv %d time %d score mate %d pv %s\n", depth, i + 1, secondsrun, matein, pvstring.c_str());
                }
                cout << s;
            }
            if (score >= en.terminationscore)
            {
                // bench mode reached needed score
                en.stopLevel = ENGINEWANTSTOP;
            }
            else {
                // next depth with new aspiration window
                deltaalpha = 25;
                deltabeta = 25;
                alpha = pos.bestmovescore[en.MultiPV - 1] - deltaalpha;
                beta = score + deltaalpha;

#ifdef DEBUG
                if (en.stopLevel == ENGINERUN)
                {
                    en.npd[depth] = en.nodes - en.npd[depth - 1];
                    if (depth >= 2)
                    {
                        int deltascore = (score - oldscore) + 1000;
                        if (deltascore >= 0 && deltascore < 2000)
                            aspirationdelta[depth][deltascore]++;
                    }
                    oldscore = score;
                }
#endif
                depth += depthincrement;
            }
        }

        if (pos.pvline.length > 0 && pos.pvline.move[0].code)
            move = pos.pvline.move[0].toString();
        else
            move = pos.bestmove[0].toString();

    } while (en.stopLevel == ENGINERUN && depth <= min(maxdepth, abs(matein) * 2));
    
    en.stopLevel = ENGINESTOPPED;
    sprintf_s(s, "bestmove %s\n", move.c_str());
    cout << s;
}


void searchguide()
{
    char s[100];
    unsigned long nodes, lastnodes = 0;
    en.starttime = getTime();
    long long endtime1 = 0;    // time to send STOPSOON signal
    long long endtime2 = 0;    // time to send STOPPIMMEDIATELY signal
    en.stopLevel = ENGINERUN;
    int timetouse = (en.isWhite ? en.wtime : en.btime);
    int timeinc = (en.isWhite ? en.winc : en.binc);
    int movestogo = 0;
    thread enginethread;

    if (en.movestogo)
        movestogo = en.movestogo;

    if (movestogo)
    {
        // should garantee timetouse > 0
        // stop soon at 0.7 x average movetime
        endtime1 = en.starttime + timetouse * en.frequency * 7 / movestogo / 10000;
        // stop immediately at 1.3 x average movetime
        endtime2 = en.starttime + min(timetouse - en.moveOverhead,  13 * timetouse / movestogo / 10) * en.frequency / 1000;
        //printf("info string difftime1=%lld  difftime2=%lld\n", (endtime1 - en.starttime) * 1000 / en.frequency , (endtime2 - en.starttime) * 1000 / en.frequency);
    }
    else if (timetouse) {
        int ph = pos.phase();
        // sudden death; split the remaining time in (256-phase) timeslots
        // stop soon after 6 timeslot
        endtime1 = en.starttime + max(timeinc, 6 * (timetouse + timeinc) / (256 - ph)) * en.frequency  / 1000;
        // stop immediately after 10 timeslots
        endtime2 = en.starttime + min(timetouse - en.moveOverhead, max(timeinc, 10 * (timetouse + timeinc) / (256 - ph))) * en.frequency / 1000;
    }
    else {
        endtime1 = endtime2 = 0;
    }

    en.nodes = 0;
#ifdef DEBUG
    en.qnodes = 0;
	en.wastedpvsnodes = 0;
    en.wastedaspnodes = 0;
    en.pvnodes = 0;
    en.nopvnodes = 0;
    en.fpnodes = 0;
    en.wrongfp = 0;
    en.dpnodes = 0;
    en.npd[0] = 1;
#endif
    en.fh = en.fhf = 0;

    enginethread = thread(&search_gen1);

    long long lastinfotime = en.starttime;

    long long nowtime;
    while (en.stopLevel != ENGINESTOPPED)
    {
        nowtime = getTime();
        nodes = en.nodes;
        if (nodes != lastnodes && nowtime - lastinfotime > en.frequency)
        {
            sprintf_s(s, "info nodes %lu nps %llu hashfull %d\n", nodes, (nodes - lastnodes) * en.frequency / (nowtime - lastinfotime), tp.getUsedinPermill());
            // enable so see the pawn hash usage
            //sprintf_s(s, "info nodes %lu nps %llu hashfull %d\n", nodes, (nodes - lastnodes) * en.frequency / (nowtime - lastinfotime), pwnhsh.getUsedinPermill());
            cout << s;
            lastnodes = nodes;
            lastinfotime = nowtime;
        }
        if (en.stopLevel != ENGINESTOPPED)
        {
            if (endtime2 && nowtime >= endtime2 && en.stopLevel < ENGINESTOPIMMEDIATELY)
            {
                en.stopLevel = ENGINESTOPIMMEDIATELY;
            }
            else if (endtime1 && nowtime >= endtime2 && en.stopLevel < ENGINESTOPSOON)
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
    en.endtime = getTime();
    if (en.endtime > en.starttime)
    {
        sprintf_s(s, "info nodes %lu nps %llu hashfull %d\n", en.nodes, en.nodes * en.frequency / (en.endtime - en.starttime), tp.getUsedinPermill());
        cout << s;
    }
#ifdef DEBUG
    if (en.nodes)
    {
        sprintf_s(s, "quiscense;%d;%d;%d\n", en.qnodes, en.nodes + en.qnodes, (int)en.qnodes * 100 / (en.nodes + en.qnodes));
        en.fdebug << s;
        cout << s;
        if (en.dpnodes)
        {
            sprintf_s(s, "deltaprune;%d;%d\n", en.dpnodes, (int)en.dpnodes * 100 / en.qnodes);
            en.fdebug << s;
            cout << s;
        }

        sprintf_s(s, "pvs;%d;%d;%d\n", en.wastedpvsnodes, en.nodes, (int)en.wastedpvsnodes * 100 / en.nodes);
        en.fdebug << s;
        cout << s;
        sprintf_s(s, "asp;%d;%d;%d\n", en.wastedaspnodes, en.nodes, (int)en.wastedaspnodes * 100 / en.nodes);
        en.fdebug << s;
        cout << s;
    }
    if (en.fpnodes)
    {
        sprintf_s(s, "futilityprune;%d;%d;%d\n", en.fpnodes, en.wrongfp, (int)en.wrongfp * 100 / en.fpnodes);
        en.fdebug << s;
        cout << s;
    }
    if (en.nopvnodes)
    {
        sprintf_s(s, "pv;%d;%d;%d\n", en.pvnodes, en.nopvnodes, (int)en.pvnodes * 100 / en.nopvnodes);
        en.fdebug << s;
        cout << s;
    }
    sprintf_s(s, "ebf;");
    en.fdebug << s;
    cout << s;
    for (int d = 2; en.npd[d] && en.npd[d - 2]; d++)
    {
        sprintf_s(s, "%0.2f;", sqrt((double)en.npd[d] / (double)en.npd[d - 2]));
        en.fdebug << s;
        cout << s;
    }
    sprintf_s(s, "\n");
    en.fdebug << s;
    cout << s;
    if (pwnhsh.query > 0)
    {
        sprintf_s(s, "info string pawnhash-hits: %0.2f%%\n", (float)pwnhsh.hit / (float)pwnhsh.query * 100.0f);
        cout << s;
        en.fdebug << s;
    }

#endif
    en.stopLevel = ENGINETERMINATEDSEARCH;
}
