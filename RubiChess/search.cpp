
#include "RubiChess.h"


int getQuiescence(int alpha, int beta, int depth)
{
    int score;
    int bestscore = SHRT_MIN + 1;
    bool isLegal;
    bool isCheck;
    bool LegalMovesPossible = false;

    isCheck = pos.checkForChess();
#if 1
    if (isCheck)
        return alphabeta(alpha, beta, 1, false);
#endif
    bestscore = (pos.state & S2MMASK ? -pos.getValue() : pos.getValue());
    PDEBUG(depth, "(getQuiscence) alpha=%d beta=%d patscore=%d\n", alpha, beta, bestscore);
    if (bestscore >= beta)
        return bestscore;
    if (bestscore > alpha)
        alpha = bestscore;

    chessmovelist* movelist = pos.getMoves();
    //pos->sortMoves(movelist);

    for (int i = 0; i < movelist->length; i++)
    {
        //pos->debug(depth, "(getQuiscence) testing move %s... LegalMovesPossible=%d isCheck=%d Capture=%d Promotion=%d see=%d \n", movelist->move[i].toString().c_str(), (LegalMovesPossible?1:0), (isCheck ? 1 : 0), movelist->move[i].getCapture(), movelist->move[i].getPromotion(), pos->see(movelist->move[i].getFrom(), movelist->move[i].getTo()));
        bool MoveIsUsefull = (ISCAPTURE(movelist->move[i].code) && (pos.see(GETFROM(movelist->move[i].code), GETTO(movelist->move[i].code)) >= 0));
        // FIXME: Promotion should be handled but it seems to slow down
        // || ISPROMOTION(movelist->move[i].code);

        if (MoveIsUsefull || !LegalMovesPossible)
        {
            isLegal = pos.playMove(&(movelist->move[i]));
#ifdef DEBUG
            en.qnodes++;
#endif
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
    if (isCheck)
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
    bool isCheck;
    int bestscore = SHRT_MIN + 1;
    uint32_t bestcode = 0;
    int eval_type = HASHALPHA;
    chessmovelist* newmoves;
    chessmove *m;
    int extendall = 0;
    int reduction;

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

    isCheck = pos.checkForChess();
    // FIXME: Maybe some check extension? This is handled by quiescience search now
#if 0
    if (isCheck)
        extendall = 1;
#endif

    // chessmove lastmove = pos.actualpath.move[pos.actualpath.length - 1];
    // Here some reduction/extension depending on the lastmove...

    // Nullmove
    if (nullmoveallowed && !isCheck && depth >= 4 && pos.phase() < 150)
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
    const int futilityMargin = 120;
    bool futility = false;
    int futilityscore;
    if (depth == 1)
    {
        futilityscore = S2MSIGN(pos.state & S2MMASK) * pos.getValue();
        futility = (futilityscore < alpha - futilityMargin);
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
        isLegal = pos.playMove(m);
        if (isLegal)
        {
            LegalMoves++;
            PDEBUG(depth, "(alphabeta) played move %s   nodes:%d\n", newmoves->move[i].toString().c_str(), en.nodes);

            // Check for valid futility pruning
            bool avoidFutilityPrune = !futility || ISTACTICAL(m->code) || pos.checkForChess() || alpha > 900;
#ifdef DEBUG
            if (!avoidFutilityPrune)
                en.fpnodes++;
#endif

            if (avoidFutilityPrune) // disable this test to debug wrongfp
            {
                reduction = 0;
                if (!extendall && depth > 2 && LegalMoves > 3 && !ISTACTICAL(m->code) && !isCheck)
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
                if (score > alpha && !avoidFutilityPrune)
                {
                    en.wrongfp++;
                    //printf("Wrong pruning: Futility-Score:%d Move:%s Score:%d\nPosition:\n", futilityscore, m->toString().c_str(), score);
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
                    tp.addHash(score, HASHBETA, depth, bestcode);
                    free(newmoves);
                    return score;   // fail soft beta-cutoff
                }

                if (score > alpha)
                {
                    PDEBUG(depth, "(alphabeta) score=%d > alpha=%d  -> new best move(%d) %s   Path:%s\n", score, alpha, depth, newmoves->move[i].toString().c_str(), pos.actualpath.toString().c_str());
                    alpha = score;
                    eval_type = HASHEXACT;
                    if (GETCAPTURE(m->code) == BLANK)
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
        if (isCheck)
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
    bool isCheck;
    int bestscore = SHRT_MIN + 1;
    chessmove best;
    int eval_type = HASHALPHA;
    chessmovelist* newmoves;
    chessmove *m;
    int extendall = 0;
    int reduction;

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
        if (rp.getPositionCount(pos.hash) <= 1)  //FIXME: This is a rough guess to avoid draw by repetition hidden by the TP table
            return score;
    }

    // test for remis via repetition
    if (rp.getPositionCount(pos.hash) >= 3 && pos.testRepetiton())
        return SCOREDRAW;

    // test for remis via 50 moves rule
    if (pos.halfmovescounter >= 100)
        return SCOREDRAW;

    isCheck = pos.checkForChess();

    newmoves = pos.getMoves();
    if (isCheck)
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
            if (!extendall && depth > 2 && LegalMoves > 3 && !ISTACTICAL(m->code) && !isCheck)
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
                return bestscore;
            }

            if (score > bestscore)
            {
                bestscore = score;
                best = *m;
                pos.bestmove = best;

                if (score >= beta)
                {
                    // Killermove
                    if (GETCAPTURE(best.code) == BLANK)
                    {

                        pos.killer[1][0] = pos.killer[0][0];
                        pos.killer[0][0] = best.code;
                    }
#ifdef DEBUG
                    en.fh++;
                    if (LegalMoves == 1)
                        en.fhf++;
#endif
                    PDEBUG(depth, "(rootsearch) score=%d >= beta=%d  -> cutoff\n", score, beta);
                    tp.addHash(beta, HASHBETA, depth, best.code);
                    free(newmoves);
                    return beta;   // fail hard beta-cutoff
                }

                if (score > alpha)
                {
                    PDEBUG(depth, "(rootsearch) score=%d > alpha=%d  -> new best move(%d) %s   Path:%s\n", score, alpha, depth, m->toString().c_str(), pos.actualpath.toString().c_str());
                    alpha = score;
                    eval_type = HASHEXACT;
                    if (GETCAPTURE(m->code) == BLANK)
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
        pos.bestmove.code = 0;
        en.stopLevel = ENGINEWANTSTOP;
        if (isCheck)
            // It's a mate
            return SCOREBLACKWINS;
        else
            // It's a stalemate
            return SCOREDRAW;
    }

    tp.addHash(alpha, eval_type, depth, best.code);
    return alpha;
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
        pos.bestmove.code = 0;
#ifdef DEBUG
        int oldscore = 0;
        unsigned long nodesbefore = en.nodes;
        en.npd[depth] = 0;
#endif
        score = rootsearch(alpha, beta, depth);

        // new aspiration window
        if (score == alpha)
        {
            // research with lower alpha
            alpha = max(SHRT_MIN + 1, alpha - deltaalpha);
            deltaalpha <<= 1;
#ifdef DEBUG
            en.wastedaspnodes += (en.nodes - nodesbefore);
#endif
        }
        else if (score == beta)
        {
            // research with higher beta
            beta = min(SHRT_MAX, beta + deltabeta);
            deltabeta <<= 1;
#ifdef DEBUG
            en.wastedaspnodes += (en.nodes - nodesbefore);
#endif
        }
        else
        {
            // search was successfull
            PDEBUG(depth, "Searchorder-Success: %f\n", (en.fh > 0 ? en.fhf / en.fh : 0.0));
            // The only case that bestmove is not set can happen if alphabeta hit the TP table
            // so get bestmovecode from there
            if (!pos.bestmove.code)
                tp.probeHash(&score, &pos.bestmove.code, MAXDEPTH, alpha, beta);

            int secondsrun = (int)((getTime() - en.starttime) * 1000 / en.frequency);

            pos.getpvline(depth);
            pvstring = pos.pvline.toString();

            if (!MATEDETECTED(score))
            {
                sprintf_s(s, "info depth %d time %d score cp %d pv %s\n", depth, secondsrun, score, pvstring.c_str());
            }
            else
            {
                matein = (score > 0 ? (SCOREWHITEWINS - score + 1) / 2 : (SCOREBLACKWINS - score) / 2);
                sprintf_s(s, "info depth %d time %d score mate %d pv %s\n", depth, secondsrun, matein, pvstring.c_str());
            }
            cout << s;
            if (score >= en.terminationscore)
            {
                // bench mode reached needed score
                en.stopLevel = ENGINEWANTSTOP;
            }
            else {
                // next depth with new aspiration window
                deltaalpha = 25;
                deltabeta = 25;
                alpha = score - deltaalpha;
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
            move = pos.bestmove.toString();

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
