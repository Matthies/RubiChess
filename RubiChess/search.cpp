
#include "RubiChess.h"


int getQuiescence(engine *en, int alpha, int beta, int depth, bool force)
{
    short score;
    bool isLegal;
    bool isCheck;
    bool LegalMovesPossible = false;
    chessposition *pos = en->pos;

    // test for remis via repetition
    if (pos->rp->getPositionCount(pos->hash) >= 3 && pos->testRepetiton())
        return SCOREDRAW;

    // test for remis via 50 moves rule
    if (pos->halfmovescounter >= 100)
        return SCOREDRAW;

    // FIXME: stand pat usually is not allowed if checked but this somehow works better
    score = (pos->state & S2MMASK ? -pos->getValue() : pos->getValue());
    pos->debug(depth, "(getQuiscence) alpha=%d beta=%d patscore=%d\n", alpha, beta, score);
    if (score >= beta)
        return beta;
    if (score > alpha)
        alpha = score;

    isCheck = pos->checkForChess();
    chessmovelist* movelist = pos->getMoves();
    //pos->sortMoves(movelist);

    for (int i = 0; i < movelist->length; i++)
    {
        //pos->debug(depth, "(getQuiscence) testing move %s... LegalMovesPossible=%d isCheck=%d Capture=%d Promotion=%d see=%d \n", movelist->move[i].toString().c_str(), (LegalMovesPossible?1:0), (isCheck ? 1 : 0), movelist->move[i].getCapture(), movelist->move[i].getPromotion(), pos->see(movelist->move[i].getFrom(), movelist->move[i].getTo()));
        if (isCheck || GETCAPTURE(movelist->move[i].code) != BLANK || GETPROMOTION(movelist->move[i].code) != BLANK || !LegalMovesPossible || force)
        {
            bool positiveSee = false;
            // FIXME!!! if (pos->mailbox[GETTO(movelist->move[i].code)] != BLANK)
            if (GETCAPTURE(movelist->move[i].code) != BLANK)
                positiveSee = (pos->see(GETFROM(movelist->move[i].code), GETTO(movelist->move[i].code)) >= 0);
            if (positiveSee || !LegalMovesPossible)
            {
                isLegal = pos->playMove(&(movelist->move[i]));
#ifdef DEBUG
                en->qnodes++;
#endif
                if (isLegal)
                {
                    LegalMovesPossible = true;
                    if (positiveSee)
                    {
                        score = -getQuiescence(en, -beta, -alpha, depth - 1, isCheck);
                        pos->debug(depth, "(getQuiscence) played move %s score=%d\n", movelist->move[i].toString().c_str(), score);
                    }
                    pos->unplayMove(&(movelist->move[i]));
                    if (positiveSee)
                    {
                        if (score >= beta)
                        {
                            free(movelist);
                            pos->debug(depth, "(getQuiscence) beta cutoff\n");
                            return beta;
                        }
                        if (score > alpha)
                        {
                            alpha = score;
                            pos->debug(depth, "(getQuiscence) new alpha\n");
                        }
                    }
                }
            }
        }
    }
    free(movelist);
    if (LegalMovesPossible)
        return alpha;
    // No valid move found
    if (isCheck)
        // It's a mate
        return max(alpha, SCOREBLACKWINS + pos->ply);
    else
        // It's a stalemate
        return max(alpha, SCOREDRAW);

}


int alphabeta(engine *en, int alpha, int beta, int depth, bool nullmoveallowed)
{
    int score;
    short bestscore = SHRT_MIN + 1;
    chessmove best;
    int eval_type = HASHALPHA;
    chessposition *pos = en->pos;
    chessmovelist* newmoves;
    unsigned long hashmovecode;
    int  LegalMoves = 0;
    bool isLegal;
    bool isCheck;

    en->nodes++;

    pos->debug(depth, "depth=%d alpha=%d beta=%d\n", depth, alpha, beta);

    if (pos->tp->probeHash(&score, &hashmovecode, depth, alpha, beta))
    {
        pos->debug(depth, "(alphabeta) got value %d from TP\n", score);
        if (pos->rp->getPositionCount(pos->hash) <= 1)  //FIXME: This is a rough guess to avoid draw by repetition hidden by the TP table
            return score;
    }

    if (depth <= 0)
    {
        return getQuiescence(en, alpha, beta, depth, false);
    }

    // test for remis via repetition
    if (pos->rp->getPositionCount(pos->hash) >= 3 && pos->testRepetiton())
        return SCOREDRAW;

    // test for remis via 50 moves rule
    if (pos->halfmovescounter >= 100)
        return SCOREDRAW;

    isCheck = pos->checkForChess();

    // Nullmove
    if (nullmoveallowed && !isCheck && depth >= 4 && pos->ply > 0 && pos->phase() < 150)
    {
        // FIXME: Something to avoid nullmove in endgame is missing... pos->phase() < 150 needs validation
        pos->playNullMove();
        pos->ply++;

        score = -alphabeta(en, -beta, -beta + 1, depth - 4, false);
        
        pos->unplayNullMove();
        pos->ply--;
        if (score >= beta && !MATEDETECTED(score))
        {
            return beta;
        }
    }

     newmoves = pos->getMoves();
    if (isCheck)
        depth++;

    for (int i = 0; i < newmoves->length; i++)
    {
        //PV moves gets top score
        if (hashmovecode == newmoves->move[i].code)
        {
            newmoves->move[i].value = PVVAL;
        }

        // killermoves gets score better than non-capture (which have negative value)
        if (pos->killer[0][pos->ply] == newmoves->move[i].code)
            newmoves->move[i].value = KILLERVAL1;
        if (pos->killer[1][pos->ply] == newmoves->move[i].code)
            newmoves->move[i].value = KILLERVAL2;
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

        isLegal = pos->playMove(&(newmoves->move[i]));

        if (isLegal)
        {
            LegalMoves++;
#ifdef DEBUG
            int oldmaxdebugdepth;
            int oldmindebugdepth;
            if (en->debug && pos->debughash == pos->hash)
            {
                oldmaxdebugdepth = pos->maxdebugdepth;
                oldmindebugdepth = pos->mindebugdepth;
                printf("Reached position to debug... starting debug.\n");
                pos->print();
                pos->maxdebugdepth = depth;
                pos->mindebugdepth = -100;
            }
            pos->debug(depth, "(alphabeta) played move %s\n", newmoves->move[i].toString().c_str());
#endif
            if (!eval_type == HASHEXACT)
            {
                score = -alphabeta(en, -beta, -alpha, depth - 1, true);
            } else {
                // try a PV-Search
				unsigned long nodesbefore = en->nodes;
                score = -alphabeta(en, -alpha - 1, -alpha, depth - 1, true);
				if (score > alpha && score < beta)
				{
					// reasearch with full window
					score = -alphabeta(en, -beta, -alpha, depth - 1, true);
#ifdef DEBUG
					en->wastednodes += (en->nodes - nodesbefore);
#endif
				}
            }

#ifdef DEBUG
            if (en->debug && pos->debughash == pos->hash)
            {
                pos->actualpath.length = pos->ply;
                printf("Leaving position to debug... stoping debug. Score:%d\n", score);
                pos->maxdebugdepth = oldmaxdebugdepth;
                pos->mindebugdepth = oldmindebugdepth;
            }
#endif
            pos->unplayMove(&(newmoves->move[i]));

            if (en->stopLevel == ENGINESTOPIMMEDIATELY)
            {
                free(newmoves);
                return alpha;
            }

            if (score > bestscore)
            {
                bestscore = score;
                best = newmoves->move[i];
                if (pos->ply == 0)
                {
                    pos->bestmove = best;
                    //pos->debug(0, "List of moves now that it is played: %s\n", newmoves->toStringWithValue().c_str());
                }

                if (score >= beta)
                {
                    // Killermove
                    if (GETCAPTURE(best.code) == BLANK)
                    {

                        pos->killer[1][pos->ply] = pos->killer[0][pos->ply];
                        pos->killer[0][pos->ply] = best.code;
                    }

                    en->fh++;
                    if (LegalMoves == 1)
                        en->fhf++;
                    pos->debug(depth, "(alphabetamax) score=%d >= beta=%d  -> cutoff\n", score, beta);
                    pos->tp->addHash(beta, HASHBETA, depth, 0);
                    free(newmoves);
                    return beta;   // fail hard beta-cutoff
                }

                if (score > alpha && en->stopLevel != ENGINESTOPIMMEDIATELY)
                {
                    pos->debug(depth, "(alphabeta) score=%d > alpha=%d  -> new best move(%d) %s   Path:%s\n", score, alpha, depth, newmoves->move[i].toString().c_str(), pos->actualpath.toString().c_str());
                    alpha = score;
                    eval_type = HASHEXACT;
                    if (GETCAPTURE(newmoves->move[i].code) == BLANK)
                    {
                        pos->history[pos->Piece(GETFROM(newmoves->move[i].code))][GETTO(newmoves->move[i].code)] += depth * depth;
                    }
                }
            }
        }
    }

    free(newmoves);
    if (LegalMoves == 0)
    {
        if (pos->ply == 0)
        {
            // No root move; finish the search
            pos->bestmove.code = 0;
            en->stopLevel = ENGINEWANTSTOP;
        }
        if (isCheck)
            // It's a mate
            return SCOREBLACKWINS + pos->ply;
        else
            // It's a stalemate
            return SCOREDRAW;
    }

    pos->tp->addHash(alpha, eval_type, depth, best.code);
    return alpha;
}



static void search_gen1(engine *en)
{
    string move;
    char s[16384];

    int score;
    int matein;
    int alpha, beta;
    int deltaalpha, deltabeta;
    int depth, maxdepth, depthincrement;
    chessposition *pos = en->pos;
    string pvstring;

    sprintf_s(s, "info string Phase is %d\n", pos->phase());
    cout << s;


    depthincrement = 1;
    if (en->mate > 0)
    {
        depth = maxdepth = en->mate * 2;
    }
    else
    {
        depth = 1;
        if (en->maxdepth > 0)
            maxdepth = en->maxdepth;
        else
            maxdepth = MAXDEPTH;
    }

    alpha = SHRT_MIN + 1;
    beta = SHRT_MAX;

    // iterative deepening
    do
    {
        matein = MAXDEPTH;
        pos->maxdebugdepth = -1;
        pos->mindebugdepth = 0;
        if (en->debug)
        {
            // Basic debuging
            pos->maxdebugdepth = depth;
            pos->mindebugdepth = depth;
            printf("\n\nNext depth: %d\n\n", depth);
        }

        // Reset bestmove to detect alpha raise in interrupted serach
        pos->bestmove.code = 0;
        score = alphabeta(en, alpha, beta, depth, true);

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
            if (en->fh > 0)
                pos->debug(depth, "Searchorder-Success: %f\n", en->fhf / en->fh);

            if (en->stopLevel == ENGINERUN || en->stopLevel == ENGINEWANTSTOP || en->stopLevel == ENGINESTOPSOON
                || (en->stopLevel == ENGINESTOPIMMEDIATELY && pos->bestmove.code > 0))
            {
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                int secondsrun = (int)((now.QuadPart - en->starttime) * 1000 / en->frequency);

                pos->getpvline(depth);
                pvstring = pos->pvline.toString();

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
            }

            // next depth with new aspiration window
            deltaalpha = 25;
            deltabeta = 25;
            alpha = score - deltaalpha;
            beta = score + deltaalpha;
            depth += depthincrement;
        }

        if (pos->pvline.length > 0 && pos->pvline.move[0].code)
            move = pos->pvline.move[0].toString();
        else
            move = pos->bestmove.toString();

    } while (en->stopLevel == ENGINERUN && depth <= min(maxdepth, abs(matein) * 2));

    en->stopLevel = ENGINESTOPPED;
    sprintf_s(s, "bestmove %s\n", move.c_str());
    cout << s;
}


void searchguide(engine *en)
{
    char s[100];
    unsigned long nodes, lastnodes = 0;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    en->starttime = now.QuadPart;
    long long difftime1, difftime2;
    en->stopLevel = ENGINERUN;
    int timetouse = (en->isWhite ? en->wtime : en->btime);
    int timeinc = (en->isWhite ? en->winc : en->binc);
    int movestogo = 0;
    thread enginethread;
    chessposition *pos = en->pos;

    if (en->movestogo)
        movestogo = en->movestogo;
    if (timeinc)
    {
        difftime1 = en->starttime + timeinc * en->frequency / 1000 ;
        difftime2 = en->starttime + (timetouse - 50) * en->frequency / 1000;
    }
    else if (movestogo)
    {
        if (movestogo == 1)
        {
            // we exactly know how much time we can consume; stop 50ms before timeout 
            difftime1 = difftime2 = en->starttime + (timetouse - 50) * en->frequency / 1000;

        }
        else {
            // consume about 0.6 .. 1.6 x average move time
            difftime1 = en->starttime + timetouse * en->frequency * 6 / movestogo / 10000;
            difftime2 = en->starttime + max(en->frequency, (timetouse * en->frequency * 16 / movestogo / 10000) - en->frequency);
        }
    }
    else {
        difftime1 = difftime2 = 0;
    }

    en->nodes = 0;
#ifdef DEBUG
    en->qnodes = 0;
	en->wastednodes = 0;
#endif
    en->fh = en->fhf = 0;

    enginethread = thread(&search_gen1, en);

    long long lastinfotime = en->starttime;

    long long nowtime;
    while (en->stopLevel != ENGINESTOPPED)
    {
        QueryPerformanceCounter(&now);
        nowtime = now.QuadPart;

        nodes = en->nodes;
        if (nodes != lastnodes && nowtime - lastinfotime > en->frequency)
        {
            sprintf_s(s, "info nodes %lu nps %llu hashfull %d\n", nodes, (nodes - lastnodes) * en->frequency / (nowtime - lastinfotime), pos->tp->getUsedinPermill());
            cout << s;
            lastnodes = nodes;
            lastinfotime = nowtime;
        }
        if (en->stopLevel != ENGINESTOPPED)
        {
            if (difftime2 && nowtime >= difftime2 && en->stopLevel < ENGINESTOPIMMEDIATELY)
            {
                en->stopLevel = ENGINESTOPIMMEDIATELY;
            }
            else if (difftime1 && nowtime >= difftime1 && en->stopLevel < ENGINESTOPSOON)
            {
                en->stopLevel = ENGINESTOPSOON;
                Sleep(10);
            }
            else {
                Sleep(10);
            }
        }
    }
    enginethread.join();
    QueryPerformanceCounter(&now);
    en->endtime = now.QuadPart;
    sprintf_s(s, "info nodes %lu nps %llu hashfull %d\n", en->nodes, en->nodes * en->frequency / (en->endtime - en->starttime), pos->tp->getUsedinPermill());
    cout << s;
#ifdef DEBUG
    sprintf_s(s, "info string %d%% quiscense\n", (int)en->qnodes * 100 / (en->nodes + en->qnodes));
    cout << s;
	sprintf_s(s, "info string %d%% wasted by research\n", (int)en->wastednodes * 100 / en->nodes);
	cout << s;
#endif

}
