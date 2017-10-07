
#include "RubiChess.h"


int getQuiescence(int alpha, int beta, int depth, bool force)
{
    int score;
    int bestscore = SHRT_MIN + 1;
    bool isLegal;
    bool isCheck;
    bool LegalMovesPossible = false;

    // test for remis via repetition
    if (rp.getPositionCount(pos.hash) >= 3 && pos.testRepetiton())
        return SCOREDRAW;

    // test for remis via 50 moves rule
    if (pos.halfmovescounter >= 100)
        return SCOREDRAW;

    // FIXME: stand pat usually is not allowed if checked but this somehow works better
    score = (pos.state & S2MMASK ? -pos.getValue() : pos.getValue());
    PDEBUG(depth, "(getQuiscence) alpha=%d beta=%d patscore=%d\n", alpha, beta, score);
    if (score >= beta)
        return score;
    if (score > alpha)
        alpha = score;

    isCheck = pos.checkForChess();
    chessmovelist* movelist = pos.getMoves();
    //pos->sortMoves(movelist);

    for (int i = 0; i < movelist->length; i++)
    {
        //pos->debug(depth, "(getQuiscence) testing move %s... LegalMovesPossible=%d isCheck=%d Capture=%d Promotion=%d see=%d \n", movelist->move[i].toString().c_str(), (LegalMovesPossible?1:0), (isCheck ? 1 : 0), movelist->move[i].getCapture(), movelist->move[i].getPromotion(), pos->see(movelist->move[i].getFrom(), movelist->move[i].getTo()));
        if (isCheck || GETCAPTURE(movelist->move[i].code) != BLANK || GETPROMOTION(movelist->move[i].code) != BLANK || !LegalMovesPossible || force)
        {
            bool positiveSee = false;
            // FIXME!!! if (pos->mailbox[GETTO(movelist->move[i].code)] != BLANK)
            if (GETCAPTURE(movelist->move[i].code) != BLANK)
                positiveSee = (pos.see(GETFROM(movelist->move[i].code), GETTO(movelist->move[i].code)) >= 0);
            if (positiveSee || !LegalMovesPossible)
            {
                isLegal = pos.playMove(&(movelist->move[i]));
#ifdef DEBUG
                en.qnodes++;
#endif
                if (isLegal)
                {
                    LegalMovesPossible = true;
                    if (positiveSee)
                    {
                        score = -getQuiescence(-beta, -alpha, depth - 1, isCheck);
                        PDEBUG(depth, "(getQuiscence) played move %s score=%d\n", movelist->move[i].toString().c_str(), score);
                    }
                    pos.unplayMove(&(movelist->move[i]));
                    if (positiveSee && score > bestscore)
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


int rootsearch(int alpha, int beta, int depth)
{
    int score;
    unsigned long hashmovecode = 0;
    int  LegalMoves = 0;
    bool isLegal;
    bool isCheck;
    int bestscore = SHRT_MIN + 1;  // FIXME: Why not SHRT_MIN?
    chessmove best(0);
    int eval_type = HASHALPHA;
    chessmovelist* newmoves;
    chessmove *m;

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
            m->value = PVVAL;
#ifdef DEBUG
            en.pvnodes++;
#endif
        }

        // killermoves gets score better than non-capture
        if (pos.killer[0][0] == m->code)
            m->value = KILLERVAL1;
        if (pos.killer[1][0] == m->code)
            m->value = KILLERVAL2;
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
            PDEBUG(depth, "(alphabeta neu) played move %s   nodes:%d\n", newmoves->move[i].toString().c_str(), en.nodes);
            if (!eval_type == HASHEXACT)
            {
                score = -alphabeta(-beta, -alpha, depth - 1, true);
            }
            else {
                // try a PV-Search
#ifdef DEBUG
                unsigned long nodesbefore = en.nodes;
#endif
                score = -alphabeta(-alpha - 1, -alpha, depth - 1, true);
                if (score > alpha && score < beta)
                {
                    // reasearch with full window
#ifdef DEBUG
                    en.wastednodes += (en.nodes - nodesbefore);
#endif
                    score = -alphabeta(-beta, -alpha, depth - 1, true);
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
                    PDEBUG(depth, "(alphabetamax) score=%d >= beta=%d  -> cutoff\n", score, beta);
                    tp.addHash(beta, HASHBETA, depth, best.code);
                    free(newmoves);
                    return beta;   // fail hard beta-cutoff
                }

                if (score > alpha)
                {
                    PDEBUG(depth, "(alphabeta) score=%d > alpha=%d  -> new best move(%d) %s   Path:%s\n", score, alpha, depth, m->toString().c_str(), pos.actualpath.toString().c_str());
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


int alphabeta(int alpha, int beta, int depth, bool nullmoveallowed)
{
    int score;
    unsigned long hashmovecode = 0;
    int  LegalMoves = 0;
    bool isLegal;
    bool isCheck;
    int bestscore = SHRT_MIN + 1;  // FIXME: Why not SHRT_MIN?
    unsigned long bestcode = 0;
    int eval_type = HASHALPHA;
    chessmovelist* newmoves;
    chessmove *m;

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

    if (depth <= 0)
    {
        return getQuiescence(alpha, beta, depth, false);
    }

    // test for remis via repetition
    if (rp.getPositionCount(pos.hash) >= 3 && pos.testRepetiton())
        return SCOREDRAW;

    // test for remis via 50 moves rule
    if (pos.halfmovescounter >= 100)
        return SCOREDRAW;

    isCheck = pos.checkForChess();

    // Nullmove
    if (nullmoveallowed && !isCheck && depth >= 4 && pos.phase() < 150)
    {
        // FIXME: Something to avoid nullmove in endgame is missing... pos->phase() < 150 needs validation
        pos.playNullMove();

        score = -alphabeta(-beta, -beta + 1, depth - 4, false);
        
        pos.unplayNullMove();
        if (score >= beta)
        {
            return score;
        }
    }

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
            m->value = PVVAL;
#ifdef DEBUG
            en.pvnodes++;
#endif
        }

        // killermoves gets score better than non-capture
        if (pos.killer[0][pos.ply] == m->code)
            m->value = KILLERVAL1;
        if (pos.killer[1][pos.ply] == m->code)
            m->value = KILLERVAL2;
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
            if (!eval_type == HASHEXACT)
            {
                score = -alphabeta(-beta, -alpha, depth - 1, true);
            } else {
                // try a PV-Search
#ifdef DEBUG
                unsigned long nodesbefore = en.nodes;
#endif
                score = -alphabeta(-alpha - 1, -alpha, depth - 1, true);
                if (score > alpha && score < beta)
				{
					// reasearch with full window
#ifdef DEBUG
                    en.wastednodes += (en.nodes - nodesbefore);
#endif
                    score = -alphabeta(-beta, -alpha, depth - 1, true);
				}
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
                    PDEBUG(depth, "(alphabetamax) score=%d >= beta=%d  -> cutoff\n", score, beta);
                    tp.addHash(beta, HASHBETA, depth, bestcode);
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

    tp.addHash(alpha, eval_type, depth, bestcode);
    return bestscore;
}



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
        score = rootsearch(alpha, beta, depth);

        // new aspiration window
        if (score == alpha)
        {
            // research with lower alpha
            alpha = max(SHRT_MIN + 1, alpha - deltaalpha);
            deltaalpha <<= 1;
        }
        else if (score == beta)
        {
            // research with hight beta
            beta = min(SHRT_MAX, beta + deltabeta);
            deltabeta <<= 1;
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

            // next depth with new aspiration window
            deltaalpha = 25;
            deltabeta = 25;
            alpha = score - deltaalpha;
            beta = score + deltaalpha;
            depth += depthincrement;
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
        // sudden death; split the remaining time for TIMETOUSESLOTS moves; TRIMESLOTS is 32 for now
        // stop soon after one timeslot
        endtime1 = en.starttime + max(timeinc, (timetouse + timeinc) / TIMETOUSESLOTS) * en.frequency  / 1000;
        // stop immediately after 3 timeslots
        endtime2 = en.starttime + min(timetouse - en.moveOverhead, max(timeinc, 3 * (timetouse + timeinc) / TIMETOUSESLOTS)) * en.frequency / 1000;
    }
    else {
        endtime1 = endtime2 = 0;
    }

    en.nodes = 0;
#ifdef DEBUG
    en.qnodes = 0;
	en.wastednodes = 0;
    en.pvnodes = 0;
    en.nopvnodes = 0;
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
    sprintf_s(s, "info nodes %lu nps %llu hashfull %d\n", en.nodes, en.nodes * en.frequency / (en.endtime - en.starttime), tp.getUsedinPermill());
    cout << s;
#ifdef DEBUG
    sprintf_s(s, "info string %d%% quiscense\n", (int)en.qnodes * 100 / (en.nodes + en.qnodes));
    cout << s;
	sprintf_s(s, "info string %d%% wasted by research\n", (int)en.wastednodes * 100 / en.nodes);
	cout << s;
    sprintf_s(s, "info string %d/%d (%d%%) pv nodes\n", en.pvnodes, en.nopvnodes, (int)en.pvnodes * 100 / en.nopvnodes);
    cout << s;
#endif

}
