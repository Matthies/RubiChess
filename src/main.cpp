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


void generateEpd(string egn)
{
    chessposition *pos = &en.sthread[0].pos;
    int pcs[16];

    int n = 1000;
    string eg = egn;
    size_t si = egn.find('/');
    if (si != string::npos)
    {
        try { n = stoi(egn.substr(si + 1)); }
        catch (const invalid_argument&) {}
        eg = egn.substr(0, si);
    }

    pos->halfmovescounter = pos->ept = 0;
    pos->fullmovescounter = 1;
    int i = 0;
    srand((unsigned)time(NULL));
    while (i < n)
    {
        getPcsFromStr(eg.c_str(), pcs);
        memset(pos->mailbox, 0, sizeof(pos->mailbox));
        memset(pos->piece00, 0, sizeof(pos->piece00));
        pos->psqval = 0;
        pos->state = rand() % 2;
        for (int p = PAWN; p <= KING; p++)
            for (int c = WHITE; c <= BLACK; c++)
            {
                int pi = p + c * 8;
                while (pcs[pi])
                {
                    int sq = rand() % 64;
                    // Avoid pawns on rank 7
                    if (!pos->mailbox[sq] && (p != PAWN || (RRANK(sq, c) > 0 && RRANK(sq, c) < 6)))
                    {
                        pos->mailbox[sq] = p * 2 + c;
                        pos->BitboardSet(sq, p * 2 + c);
                        if (p == KING)
                            pos->kingpos[c] = sq;
                        pcs[pi]--;
                    }
                }
            }
        // Check if position is legal
        bool isLegal = !pos->isAttackedBy<OCCUPIED>(pos->kingpos[pos->state ^ S2MMASK], pos->state)
            && squareDistance[pos->kingpos[0]][pos->kingpos[1]] > 0;
        if (isLegal)
        {
            pos->hash = zb.getHash(pos);
            pos->pawnhash = zb.getPawnHash(pos);
            pos->materialhash = zb.getMaterialHash(pos);
            pos->mstop = 1;
            pos->movestack[0].movecode = -1;  // Avoid fast eval after null move
            pos->rootheight = 0;
            pos->lastnullmove = -1;
            int staticeval = pos->getEval<NOTRACE>();
            int quietval = pos->getQuiescence(SCOREBLACKWINS, SCOREWHITEWINS, 0);
            bool isQuiet = (abs(staticeval - quietval) < 100);
            if (isQuiet)
            {
                i++;
                cout << pos->toFen() << "\n";
            }
        }
    }
}


long long engine::perft(int depth, bool dotests)
{
    long long retval = 0;
    chessposition *rootpos = &en.sthread[0].pos;

    if (dotests)
    {
        if (rootpos->hash != zb.getHash(rootpos))
        {
            printf("Alarm! Wrong Hash! %llu\n", zb.getHash(rootpos));
            rootpos->print();
        }
        if (rootpos->pawnhash && rootpos->pawnhash != zb.getPawnHash(rootpos))
        {
            printf("Alarm! Wrong Pawn Hash! %llu\n", zb.getPawnHash(rootpos));
            rootpos->print();
        }
        if (rootpos->materialhash != zb.getMaterialHash(rootpos))
        {
            printf("Alarm! Wrong Material Hash! %llu\n", zb.getMaterialHash(rootpos));
            rootpos->print();
        }
        int val1 = rootpos->getEval<NOTRACE>();
        int psq1 = rootpos->getpsqval();
        if (rootpos->psqval != psq1)
        {
            printf("PSQ-Test  :error  incremental:%d  recalculated:%d\n", rootpos->psqval, psq1);
            rootpos->print();
        }
        rootpos->mirror();
        int val2 = rootpos->getEval<NOTRACE>();
        rootpos->mirror();
        int val3 = rootpos->getEval<NOTRACE>();
        if (!(val1 == val3 && val1 == val2))
        {
            printf("Mirrortest  :error  (%d / %d / %d)\n", val1, val2, val3);
            rootpos->print();
            rootpos->mirror();
            rootpos->print();
            rootpos->mirror();
            rootpos->print();
        }
    }

    if (depth == 0)
        return 1;

    chessmovelist movelist;
    if (rootpos->isCheckbb)
        movelist.length = CreateEvasionMovelist(rootpos, &movelist.move[0]);
    else
        movelist.length = CreateMovelist<ALL>(rootpos, &movelist.move[0]);

    rootpos->prepareStack();

    for (int i = 0; i < movelist.length; i++)
    {
        if (rootpos->playMove(&movelist.move[i]))
        {
            retval += perft(depth - 1, dotests);
            rootpos->unplayMove(&movelist.move[i]);
        }
    }
    return retval;
}

static void perftest(bool dotests, int maxdepth)
{
    struct perftestresultstruct
    {
        string fen;
        unsigned long long nodes[10];
    };
    perftestresultstruct perftestresults[] =
    {
        {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            { 1, 20, 400, 8902, 197281, 4865609, 119060324 }
        },
        {
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            { 1, 48,2039,97862, 4085603, 193690690 }
        },
        {
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            { 1, 14, 191, 2812, 43238, 674624, 11030083, 178633661 }
        },
        {
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            { 1, 6, 264, 9467, 422333, 15833292, 706045033 }
        },
        {
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            { 1, 44, 1486, 62379, 2103487, 89941194 }
        },
        {
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
            { 1, 46, 2079, 89890, 3894594, 164075551, 6923051137 }
        },
        {
            "",
            {}
        }
    };

    perftestresultstruct perftestresults960[] =
    {
        {
            "8/8/8/4B2b/6nN/8/5P2/2R1K2k w Q - 0 1",
            { 1, 34, 318, 9002, 118388, 3223406, 44554839, 1205627532 }
        },
        {
            "2r5/8/8/8/8/8/6PP/k2KR3 w K - 0 1",
            { 1, 17, 242, 3931, 57700, 985298, 14751778, 259604208, 3914405614 }
        },
        {
            "4r3/3k4/8/8/8/8/6PP/qR1K1R2 w KQ - 0 1",
            { 1, 19, 628, 12858, 405636, 8992652, 281330710, 6447669114 }
        },
        {
            "r1k1r2q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K1R2Q w KQkq - 0 1",
            { 1, 23, 522, 12333, 285754, 7096972, 172843489, 4557457200 }
        },
        {
            "r1k2r1q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K2R1Q w KQkq - 0 1",
            { 1, 28, 738, 20218, 541480, 15194841, 418430598, 12094237108 }
        },
        {
            "",
            {}
        }
    };

    int i = 0;
    printf("\n\nPerft results for %s (Build %s)\n", en.name().c_str(), BUILD);
    printf("System: %s\n", cinfo.SystemName().c_str());
    printf("CPU-Features of system: %s\nCPU-Features of binary: %s\n", cinfo.PrintCpuFeatures(cinfo.machineSupports).c_str(), cinfo.PrintCpuFeatures(cinfo.binarySupports).c_str());
    printf("Depth = %d    %8s  Hash-/Mirror-Tests %s\n", maxdepth, en.chess960 ? "Chess960" : "", dotests ? "enabled" : "disabled");
    printf("========================================================================\n");

    float df;
    U64 totalresult = 0ULL;

    long long perftstarttime = getTime();
    long long perftlasttime = perftstarttime;

    perftestresultstruct *ptr;
    if (en.chess960)
        ptr = perftestresults960;
    else
        ptr = perftestresults;

    while (ptr[i].fen != "")
    {
        en.sthread[0].pos.getFromFen(ptr[i].fen.c_str());
        int j = 0;
        while (ptr[i].nodes[j] > 0 && j <= maxdepth)
        {
            long long starttime = getTime();

            U64 result = en.perft(j, dotests);
            totalresult += result;

            perftlasttime = getTime();
            df = float(perftlasttime - starttime) / (float) en.frequency;
            printf("Perft %d depth %d  : %*llu  %*f sec.  %*d nps ", i + 1, j, 10, result, 10, df, 8, (int)(df > 0.0 ? (double)result / df : 0));
            if (result == ptr[i].nodes[j])
                printf("  OK\n");
            else
                printf("  wrong (should be %llu)\n", ptr[i].nodes[j]);
            j++;
        }
        if (ptr[++i].fen != "")
            printf("\n");
    }
    df = float(perftlasttime - perftstarttime) / (float)en.frequency;
    printf("========================================================================\n");
    printf("Total:             %*llu  %*f sec.  %*d nps \n", 10, totalresult, 10, df, 8, (int)(df > 0.0 ? (double)totalresult / df : 0));
}


struct benchmarkstruct
{
    string name;
    string fen;
    int depth;
    int terminationscore;
    long long time;
    long long nodes;
    int score;
    int depthAtExit;
    string move;
    int solved;
};

const string solvedstr[] = { "-", "+", "o" };


static void benchTableHeader(FILE* out)
{
        fprintf(out, "\n\nBenchmark results for %s (Build %s):\n", en.name().c_str(), BUILD);
        fprintf(out, "System: %s\n", cinfo.SystemName().c_str());
        fprintf(out, "CPU-Features of system: %s\nCPU-Features of binary: %s\n", cinfo.PrintCpuFeatures(cinfo.machineSupports).c_str(), cinfo.PrintCpuFeatures(cinfo.binarySupports).c_str());
        fprintf(out, "=============================================================================================================\n");
}

static void benchTableItem(FILE* out, int i, benchmarkstruct *bm)
{
    fprintf(out, "Bench # %3d (%14s / %2d): %s  %5s %6d cp %3d ply %10f sec. %10lld nodes %10lld nps\n", i, bm->name.c_str(), bm->depth, solvedstr[bm->solved].c_str(), bm->move.c_str(), bm->score, bm->depthAtExit, (float)bm->time / (float)en.frequency, bm->nodes, bm->nodes * en.frequency / bm->time);
}

static void benchTableFooder(FILE *out, long long totaltime, long long totalnodes, int totalsolved[2])
{
    int totaltests = totalsolved[0] + totalsolved[1];
    double fSolved = totaltests ? 100.0 * totalsolved[1] / (double)totaltests : 0.0;
    fprintf(out, "=============================================================================================================\n");
    fprintf(out, "Overall:                  %4d/%3d = %4.1f%%                    %10f sec. %10lld nodes %*lld nps\n",
        totalsolved[1], totaltests, fSolved, ((float)totaltime / (float)en.frequency), totalnodes, 10, totalnodes * en.frequency / totaltime);
}

static void doBenchmark(int constdepth, string epdfilename, int consttime, int startnum, bool openbench)
{
    benchmarkstruct benchmark[] =
    {
        {   
            "Startposition",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            14,
            0,
            0, 0, 0, 0, "", 0
        },
        {
            "Lasker Test",
            "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1",
            28,
            0,
            0, 0, 0, 0, "", 0
        },
        {
            "IQ4 63",
            "2R5/r3b1k1/p2p4/P1pPp2p/6q1/2P2N1r/4Q1P1/5RK1 w - - 0 1 ",
            14,
            300,
            0, 0, 0, 0, "", 0
        },
        {
            "Wacnew 167",
            "7Q/ppp2q2/3p2k1/P2Ppr1N/1PP5/7R/5rP1/6K1 b - - 0 1",
            14,
            1000,
            0, 0, 0, 0, "", 0
        },
        { 
            "Wacnew 212",
            "rn1qr2Q/pbppk1p1/1p2pb2/4N3/3P4/2N5/PPP3PP/R4RK1 w - - 0 1",
            14,
            500,
            0, 0, 0, 0, "", 0
        },
        {
            "Carlos 6",
            "rn1q1r2/1bp1bpk1/p3p2p/1p2N1pn/3P4/1BN1P1B1/PPQ2PPP/2R2RK1 w - - 0 1",
            13,
            300,
            0, 0, 0, 0, "", 0
        },
         
        {
            "Arasan19 83",
            "6k1/p4qp1/1p3r1p/2pPp1p1/1PP1PnP1/2P1KR1P/1B6/7Q b - - 0 1 ",
            14,
            200,
            0, 0, 0, 0, "", 0
        },
        {
            "Arasan19 192",
            "r2qk2r/1b1nbp1p/p1n1p1p1/1pp1P3/6Q1/2NPB1PN/PPP3BP/R4RK1 w kq - 0 1",
            13,
            150,
            0, 0, 0, 0, "", 0
        },
        {
            "BT2630 12",
            "8/pp3k2/2p1qp2/2P5/5P2/1R2p1rp/PP2R3/4K2Q b - - 0 1",
            15,
            300,
            0, 0, 0, 0, "", 0
        },
        {
            "IQ4 116",
            "4r1k1/1p2qrpb/p1p4p/2Pp1p2/1Q1Rn3/PNN1P1P1/1P3PP1/3R2K1 b - - 0 1",
            14,
            300,
            0, 0, 0, 0, "", 0
        },
        {
            "Arasan12 114",
            "br4k1/1qrnbppp/pp1ppn2/8/NPPBP3/PN3P2/5QPP/2RR1B1K w - - 0 1",
            15,
            150,
            0, 0, 0, 0, "", 0
        },
        {
            "Arasan12 140",
            "r1b1rk2/p1pq2p1/1p1b1p1p/n2P4/2P1NP2/P2B1R2/1BQ3PP/R6K w - - 0 1",
            15,
            300,
            0, 0, 0, 0, "", 0
        },
        {
            "Arasan12 137",
            "r4k2/1b3ppp/p2n1P2/q1p3PQ/Np1rp3/1P1B4/P1P4P/2K1R2R w - - 0 1",
            14,
            200,
            0, 0, 0, 0, "", 0
        },
        {
            "", "", 0, 0,
            0, 0, 0, 0, "", 0
        }
    };

    long long starttime, endtime;
    list<benchmarkstruct> bmlist;

    ifstream epdfile;
    bool bGetFromEpd = false;
    if (epdfilename != "")
    {
        epdfile.open(epdfilename, ifstream::in);
        bGetFromEpd = epdfile.is_open();
        if (!bGetFromEpd)
            printf("Cannot open file %s for reading.\n", epdfilename.c_str());
    }

    int i = 0;
    int totalSolved[2] = { 0 };
    benchmarkstruct epdbm;
    FILE *tableout = openbench ? stdout : stderr;

    while (true)
    {
        string avoidmoves = "";
        string bestmoves = "";
        benchmarkstruct *bm;
        if (!bGetFromEpd)
        {
            // standard bench with included positions
            bm = &benchmark[i];
        }
        else
        {
            // read positions from epd file
            bm = &epdbm;
            string line;
            getline(epdfile, line);
            getFenAndBmFromEpd(line, &bm->fen, &bestmoves, &avoidmoves);

            bm->depth = 10;  // default depth for epd bench
            bm->terminationscore = 0;
        }
        if (bm->fen == "") break;

        if (++i < startnum) continue;

        en.communicate("ucinewgame");
        en.communicate("position fen " + bm->fen);
        starttime = getTime();
        int dp = 0;
        int tm = consttime;
        if (constdepth)
            dp = constdepth;
        else
            dp = bm->depth;
        if (bm->terminationscore)
            en.terminationscore = bm->terminationscore;
        else
            en.terminationscore = SHRT_MAX;
        if (tm)
            en.communicate("go movetime " + to_string(tm * 1000));
        else if (dp)
            en.communicate("go depth " + to_string(dp));
        else
            en.communicate("go infinite");

        endtime = getTime();
        bm->time = endtime - starttime;
        bm->nodes = en.getTotalNodes();
        bm->score = en.rootposition.lastbestmovescore;
        bm->depthAtExit = en.benchdepth;
        bm->move = en.benchmove;
        bm->solved = 2;

        if (bestmoves != "")
            bm->solved = (bestmoves.find(bm->move) != string::npos) ? 1 : 0;
        if (avoidmoves != "")
            bm->solved = (bestmoves.find(bm->move) != string::npos) ? 0 : 1;
        if (bm->solved < 2)
            totalSolved[bm->solved]++;

        if (bGetFromEpd)
            benchTableItem(tableout, i, bm);

        bmlist.push_back(*bm);
    }

    en.terminationscore = SHRT_MAX;
    i = 0;
    long long totaltime = 0;
    long long totalnodes = 0;
    benchTableHeader(tableout);

    for (list<benchmarkstruct>::iterator bm = bmlist.begin(); bm != bmlist.end(); bm++)
    {
        totaltime += bm->time;
        totalnodes += bm->nodes;
        benchTableItem(tableout, ++i, &*bm);
    }
    if (totaltime)
    {
        benchTableFooder(tableout, totaltime, totalnodes, totalSolved);
        if (openbench)
            printf("Time  : %lld\nNodes : %lld\nNPS   : %lld\n", totaltime * 1000 / en.frequency, totalnodes, totalnodes * en.frequency / totaltime);
    }
}



#ifdef _WIN32

static void readfromengine(HANDLE pipe, enginestate *es)
{
    DWORD dwRead;
    CHAR chBuf[BUFSIZE];
    vector<string> lines, token;

    do
    {
        memset(chBuf, 0, BUFSIZE);
        ReadFile(pipe, chBuf, BUFSIZE, &dwRead, NULL);
        stringstream ss(chBuf);
        string line;

        while (getline(ss, line)) {
            const char *s = line.c_str();
            if (strstr(s, "uciok") != NULL && es->phase == 0)
                es->phase = 1;
            if (strstr(s, "readyok") != NULL && es->phase == 1)
                es->phase = 2;
            if (es->phase == 2)
            {
                if (es->doEval)
                {
                    const char *strEval;
                    if ((strEval = strstr(s, "evaluation: ")) || (strEval = strstr(s, "score: ")))
                    {
                        vector<string> scoretoken = SplitString(strEval);
                        if (scoretoken.size() > 1)
                        {
                            try
                            {
                                es->score = int(stof(scoretoken[1]) * 100);
                                //printf("%d\n", es->score);
                            }
                            catch (const invalid_argument&) {}
                        }
                        es->phase = 3;
                    }
                }
                else
                {

                    const char *bestmovestr = strstr(s, "bestmove ");
                    const char *pv = strstr(s, " pv ");
                    const char *score = strstr(s, " cp ");
                    const char *mate = strstr(s, " mate ");
                    const char *bmptr = NULL;
                    if (bestmovestr != NULL)
                    {
                        bmptr = bestmovestr;
                    }
                    if (pv != NULL)
                    {
                        bmptr = pv;
                    }
                    if (bmptr)
                    {
                        token = SplitString(bmptr);
                        if (token.size() > 1)
                        {
                            es->enginesbestmove = token[1];
                            string myPv = token[1];
                            bool bestmovefound = (strstr(es->bestmoves.c_str(), myPv.c_str()) != NULL
                                || (es->bestmoves == "" && strstr(es->avoidmoves.c_str(), myPv.c_str()) == NULL));

                            if (score)
                            {
                                vector<string> scoretoken = SplitString(score);
                                if (scoretoken.size() > 1)
                                {
                                    try
                                    {
                                        if (bestmovefound)
                                            es->score = stoi(scoretoken[1]);
                                        else
                                            es->allscore = stoi(scoretoken[1]);
                                    }
                                    catch (const invalid_argument&) {}
                                }
                            }
                            if (mate)
                            {
                                vector<string> matetoken = SplitString(mate);
                                if (matetoken.size() > 1)
                                {
                                    try
                                    {
                                        if (bestmovefound)
                                            es->score = SCOREWHITEWINS - stoi(matetoken[1]);
                                        else
                                            es->allscore = SCOREWHITEWINS - stoi(matetoken[1]);
                                    }
                                    catch (const invalid_argument&) {}
                                }
                            }
                            if (bestmovefound)
                            {
                                if (es->firstbesttimesec < 0)
                                {
                                    es->firstbesttimesec = (clock() - es->starttime) / CLOCKS_PER_SEC;
                                    //printf("%d %d  %d\n", es->firstbesttimesec, es->starttime, clock());
                                }
                            }
                            else {
                                es->firstbesttimesec = -1;
                                //printf("%d %d  %d\n", es->firstbesttimesec, es->starttime, clock());
                            }
                        }
                    }
                    if (bestmovestr)
                        es->phase = 3;
                }
            }
        }
    } while (true);
}

static BOOL writetoengine(HANDLE pipe, const char *s)
{
    DWORD written;
    return WriteFile(pipe, s, (DWORD)strlen(s), &written, NULL);
}

static void testengine(string epdfilename, int startnum, string engineprgs, string logfilename, string comparefilename, int maxtime, int flags)
{
    enginestate es[4];
    enginestate ges;
    string engineprg[4];
    int numEngines = 0;
    string line;
    thread *readThread[4];
    ifstream comparefile;
    bool compare = false;
    char buf[1024];
    bool doEval = (flags & 0x08);
    while (engineprgs != "" && numEngines < 4)
    {
        size_t i = engineprgs.find('*');
        engineprg[numEngines] = (i == string::npos) ? engineprgs : engineprgs.substr(0, i);
        engineprgs = (i == string::npos) ? "" : engineprgs.substr(i + 1, string::npos);
        es[numEngines++].doEval = (flags & 0x08);
    }

    int sleepDelay = 10;

    // Default time for enginetest: 30s
    if (!maxtime) maxtime = 30;
    // Open the epd file for reading
    ifstream epdfile(epdfilename);
    if (!epdfile.is_open())
    {
        printf("Cannot open %s.\n", epdfilename.c_str());
        return;
    }

    // Open the compare file for reading
    if (comparefilename != "")
    {
        comparefile.open(comparefilename);
        if (!comparefile.is_open())
        {
            printf("Cannot open %s.\n", comparefilename.c_str());
            return;
        }
        compare = true;
    }

    // Open the log file for writing
    if (logfilename == "")
        logfilename = epdfilename + ".log";
    ofstream logfile(logfilename, ios_base::app);
    logfile.setf(ios_base::unitbuf);
    if (!logfile.is_open())
    {
        printf("Cannot open %s.\n", logfilename.c_str());
        return;
    }
    if (!doEval)
        logfile << "num passed bestmove foundmove score time\n";
    else
        logfile << "fen eval\n";

    // Start the engine with linked pipes
    HANDLE g_hChildStd_IN_Rd[4] = { NULL };
    HANDLE g_hChildStd_IN_Wr[4] = { NULL };
    HANDLE g_hChildStd_OUT_Rd[4] = { NULL };
    HANDLE g_hChildStd_OUT_Wr[4] = { NULL };
    BOOL bSuccess;
    for (int i = 0; i < numEngines; i++)
    {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&g_hChildStd_OUT_Rd[i], &g_hChildStd_OUT_Wr[i], &sa, 0)
            || !SetHandleInformation(g_hChildStd_OUT_Rd[i], HANDLE_FLAG_INHERIT, 0)
            || !CreatePipe(&g_hChildStd_IN_Rd[i], &g_hChildStd_IN_Wr[i], &sa, 0)
            || !SetHandleInformation(g_hChildStd_IN_Wr[i], HANDLE_FLAG_INHERIT, 0))
        {
            printf("Cannot pipe connection to engine process.\n");
            return;
        }

        PROCESS_INFORMATION piProcInfo;
        STARTUPINFO siStartInfo;
        bSuccess = FALSE;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
        siStartInfo.cb = sizeof(STARTUPINFO);
        siStartInfo.hStdError = g_hChildStd_OUT_Wr[i];
        siStartInfo.hStdOutput = g_hChildStd_OUT_Wr[i];
        siStartInfo.hStdInput = g_hChildStd_IN_Rd[i];
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        bSuccess = CreateProcess(NULL, (LPSTR)engineprg[i].c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
        if (!bSuccess)
        {
            printf("Cannot create process for engine %s.\n", engineprg[i].c_str());
            return;
        }
        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
        readThread[i] = new thread(&readfromengine, g_hChildStd_OUT_Rd[i], &es[i]);
    }
    // Read epd line by line
    int linenum = 0;
    while (getline(epdfile, line))
    {
        string fenstr;
        getFenAndBmFromEpd(line, &fenstr, &(ges.bestmoves), &(ges.avoidmoves));

        if (fenstr != "" && ++linenum >= startnum)
        {
            if (doEval)
            {
                // Skip positions with check
                en.sthread[0].pos.getFromFen(fenstr.c_str());
                if (en.sthread[0].pos.isCheckbb)
                    continue;
                fenstr = en.sthread[0].pos.toFen();
            }

            // Get data from compare file
            ges.doCompare = false;
            if (compare)
            {
                vector<string> cv;
                string compareline;
                int compareindex = 0;
                while (compareindex != linenum && getline(comparefile, compareline, '\n'))
                {
                    cv = SplitString(compareline.c_str());
                    try
                    {
                        compareindex = stoi(cv[0]);
                    }
                    catch (const invalid_argument&) {}

                }
                if (compareindex == linenum)
                {
                    ges.doCompare = true;
                    ges.comparesuccess = (cv[1] == "+");
                    ges.comparescore = SCOREBLACKWINS;
                    ges.comparetime = -1;
                    if (cv.size() > 4)
                    {
                        try
                        {
                            ges.comparescore = stoi(cv[4]);
                        }
                        catch (const invalid_argument&) {}
                    }
                    if (cv.size() > 5)
                    {
                        try
                        {
                            ges.comparetime = stoi(cv[5]);
                        }
                        catch (const invalid_argument&) {}
                        if (ges.comparetime == 0 && (flags & 0x1))
                            // nothing to improve; skip this test
                            continue;
                    }
                }
            }
            for (int i = 0; i < numEngines; i++)
            {
                // Initialize the engine
                es[i].bestmoves = ges.bestmoves;
                es[i].avoidmoves = ges.avoidmoves;
                es[i].doCompare = ges.doCompare;
                es[i].comparescore = ges.comparescore;
                es[i].comparesuccess = ges.comparesuccess;
                es[i].comparetime = ges.comparetime;
                es[i].phase = 0;
                es[i].score = SCOREBLACKWINS;

                bSuccess = writetoengine(g_hChildStd_IN_Wr[i], "uci\n");
                while (es[i].phase == 0)
                    Sleep(sleepDelay);
                bSuccess = writetoengine(g_hChildStd_IN_Wr[i], "ucinewgame\n");
                bSuccess = writetoengine(g_hChildStd_IN_Wr[i], "isready\n");
                while (es[i].phase == 1)
                    Sleep(sleepDelay);

                es[i].starttime = clock();
                es[i].firstbesttimesec = -1;

                sprintf_s(buf, "position fen %s 0 1\n%s\n", fenstr.c_str(), doEval ? "eval" : "go infinite");

                bSuccess = writetoengine(g_hChildStd_IN_Wr[i], buf);
            }

            for (int i = 0; i < numEngines; i++)
            {
                bool engineStopped = false;
                while (es[i].phase < 3)
                {
                    Sleep(sleepDelay);
                    clock_t now = clock();
                    if (!engineStopped
                        && ((now - es[i].starttime) / CLOCKS_PER_SEC > maxtime
                            || es[i].score > SCOREWHITEWINS - MAXDEPTH
                            || ((flags & 0x2) && es[i].doCompare && es[i].comparesuccess && (now - es[i].starttime) / CLOCKS_PER_SEC > es[i].comparetime)
                            || ((flags & 0x2) && es[i].firstbesttimesec >= 0 && ((now - es[i].starttime) / CLOCKS_PER_SEC) > es[i].firstbesttimesec + 5)))
                    {
                        bSuccess = writetoengine(g_hChildStd_IN_Wr[i], "stop\n");
                        engineStopped = true;
                    }
                }
                if (!doEval)
                {
                    if (es[i].firstbesttimesec >= 0)
                    {
                        printf("e#%d  %d  %s: %s  found: %s  score: %d  time: %d\n", i, linenum, (es[i].bestmoves != "" ? "bm" : "am"), (es[i].bestmoves != "" ? es[i].bestmoves.c_str() : es[i].avoidmoves.c_str()), es[i].enginesbestmove.c_str(), es[i].score, es[i].firstbesttimesec);
                        logfile << "e#" << i << " " << linenum << " + \"" << (es[i].bestmoves != "" ? es[i].bestmoves.c_str() : (es[i].avoidmoves + "(a)").c_str()) << "\" " << es[i].enginesbestmove.c_str() << " " << es[i].score << " " << es[i].firstbesttimesec << "\n";

                    }
                    else
                    {
                        printf("e#%d  %d  %s: %s  found: %s ... failed  score: %d\n", i, linenum, (es[i].bestmoves != "" ? "bm" : "am"), (es[i].bestmoves != "" ? es[i].bestmoves.c_str() : es[i].avoidmoves.c_str()), es[i].enginesbestmove.c_str(), es[i].allscore);
                        logfile << "e#" << i << " " << linenum << " - \"" << (es[i].bestmoves != "" ? es[i].bestmoves.c_str() : (es[i].avoidmoves + "(a)").c_str()) << "\" " << es[i].enginesbestmove.c_str() << " " << es[i].allscore << "\n";
                    }
                }
            }

            if (doEval)
            {
                printf("\"%s\" ", fenstr.c_str());
                logfile << "\"" << fenstr << "\" ";
                for (int i = 0; i < numEngines; i++)
                {
                    printf("%5d ", es[i].score);
                    logfile << es[i].score << " ";
                }
                printf("\n");
                logfile << "\n";
            }
        }
    }
    for (int i = 0; i < numEngines; i++)
    {
        bSuccess = writetoengine(g_hChildStd_IN_Wr[i], "quit\n");
        readThread[i]->detach();
        delete readThread[i];
    }
}

#endif // _WIN32


int main(int argc, char* argv[])
{
    int startnum;
    int perfmaxdepth;
    bool verbose;
    bool benchmark;
    bool openbench;
    int depth;
    bool dotests;
    bool enginetest;
    string epdfile;
    string engineprg;
    string logfile;
    string comparefile;
    string genepd;
#ifdef EVALTUNE
    string pgnconvertfile;
    string fentuningfiles;
    bool quietonly;
    bool optk;
    string correlation;
    int ppg;
#endif
    int maxtime;
    int flags;

    struct arguments {
        const char *cmd;
        const char *info;
        void* variable;
        char type;
        const char *defaultval;
    } allowedargs[] = {
        { "-verbose", "Show the parameterlist and actuel values.", &verbose, 0, NULL },
        { "-bench", "Do benchmark test for some positions.", &benchmark, 0, NULL },
        { "bench", "Do benchmark with OpenBench compatible output.", &openbench, 0, NULL },
        { "-depth", "Depth for benchmark (0 for per-position-default)", &depth, 1, "0" },
        { "-perft", "Do performance and move generator testing.", &perfmaxdepth, 1, "0" },
        { "-dotests","test the hash function and value for positions and mirror (use with -perft)", &dotests, 0, NULL },
        { "-enginetest", "bulk testing of epd files", &enginetest, 0, NULL },
        { "-epdfile", "the epd file to test (use with -enginetest or -bench)", &epdfile, 2, "" },
        { "-logfile", "output file (use with -enginetest)", &logfile, 2, "enginetest.log" },
        { "-engineprg", "the uci engine to test (use with -enginetest)", &engineprg, 2, "" },
        { "-maxtime", "time for each test in seconds (use with -enginetest or -bench)", &maxtime, 1, "0" },
        { "-startnum", "number of the test in epd to start with (use with -enginetest or -bench)", &startnum, 1, "1" },
        { "-compare", "for fast comparision against logfile from other engine (use with -enginetest)", &comparefile, 2, "" },
        { "-flags", "1=skip easy (0 sec.) compares; 2=break 5 seconds after first find; 4=break after compare time is over; 8=eval only (use with -enginetest)", &flags, 1, "0" },
        { "-option", "Set UCI option by commandline", NULL, 3, NULL },
        { "-generate", "Generates epd file with n (default 1000) random endgame positions of the given type; format: egstr/n ", &genepd, 2, "" },
#ifdef STACKDEBUG
        { "-assertfile", "output assert info to file", &en.assertfile, 2, "" },
#endif
#ifdef EVALTUNE
        { "-pgnfile", "converts games in a PGN file to fen for tuning them later", &pgnconvertfile, 2, "" },
        { "-quietonly", "convert only quiet positions (when used with -pgnfile); don't do qsearch (when used with -fentuning)", &quietonly, 0, NULL },
        { "-ppg", "use only <n> positions per game (0 = every position, use with -pgnfile)", &ppg, 1, "0" },
        { "-fentuning", "reads FENs from files (filenames separated by *) and tunes eval parameters against it", &fentuningfiles, 2, "" },
        { "-optk", "optimize constant k before tuning, use with -fentuning)", &optk, 0, NULL },
        { "-correlation", "calculate correlation of parameters to the give list (seperated by *), use with -fentuning)", &correlation, 2, "" },
        { "-tuningratio", "use only every <n>th double move from the FEN to speed up the analysis", &tuningratio, 1, "1" },
#endif
        { NULL, NULL, NULL, 0, NULL }
    };

#ifdef FINDMEMORYLEAKS
    // Get current flag  
    int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

    // Turn on leak-checking bit.  
    tmpFlag |= _CRTDBG_LEAK_CHECK_DF;

    // Set flag to the new value.  
    _CrtSetDbgFlag(tmpFlag);
#endif

#ifdef STACKDEBUG
    myassert(1 == 0, nullptr, 0); // test stacktrace
#endif

#ifdef EVALOPTIONS
    registerallevals();
#endif

    searchinit();

    cout.setf(ios_base::unitbuf);

    int paramindex = 0;
    for (int j = 0; allowedargs[j].cmd; j++)
    {
        int val = 0;
        while (paramindex < argc)
        {
            if (strcmp(argv[paramindex], allowedargs[j].cmd) == 0)
            {
                val = paramindex;
                break;
            }
            paramindex++;
        }
        switch (allowedargs[j].type)
        {
        case 0:
            *(bool*)(allowedargs[j].variable) = (val > 0);
            if (verbose) printf(" %s: %s  (%s)\n", allowedargs[j].cmd, *(bool*)(allowedargs[j].variable) ? "yes" : "no", allowedargs[j].info);
            paramindex = 0;
            break;
        case 1:
            try { *(int*)(allowedargs[j].variable) = stoi((val > 0 && val < argc - 1 ? argv[val + 1] : allowedargs[j].defaultval)); }
            catch (const invalid_argument&) {}
            if (verbose) printf(" %s: %d  (%s)\n", allowedargs[j].cmd, *(int*)(allowedargs[j].variable), allowedargs[j].info);
            paramindex = 0;
            break;
        case 2:
            *(string*)(allowedargs[j].variable) = (val > 0 && val < argc - 1 ? argv[val + 1] : allowedargs[j].defaultval);
            if (verbose) printf(" %s: %s  (%s)\n", allowedargs[j].cmd, (*(string*)(allowedargs[j].variable)).c_str(), allowedargs[j].info);
            paramindex = 0;
            break;
        case 3:
            if (val > 0 && val < argc - 1)
            {
                string optionName(argv[val + 1]);
                string optionValue(val < argc - 2 ? argv[val + 2] : "");
                en.ucioptions.Set(optionName, optionValue);
                if (verbose) printf(" %s (%s) %s: %s\n", allowedargs[j].cmd, allowedargs[j].info, optionName.c_str(), optionValue.c_str());
                // search for more -option parameters starting after current (ugly hack)
                paramindex++;
                j--;
            }
            else {
                paramindex = 0;
            }
        }
    }

    if (verbose) printf("%s (Build %s)\n UCI compatible chess engine by %s\n", en.name().c_str(), BUILD, en.author);

    if (perfmaxdepth)
    {
        // do a perft test
        perftest(dotests, perfmaxdepth);
    } else if (benchmark || openbench)
    {
        // benchmark mode
#ifdef NNUE
        int oldNnueReady = NnueReady;
        NnueReady = false;
        doBenchmark(depth, epdfile, maxtime, startnum, openbench);
        if (oldNnueReady)
        {
            NnueReady = true;
            doBenchmark(depth, epdfile, maxtime, startnum, openbench);
        }
#endif
    } else if (enginetest)
    {
#ifdef _WIN32
        //engine test mode
        testengine(epdfile, startnum, engineprg, logfile, comparefile, maxtime, flags);
#endif
    }
    else if (genepd != "")
    {
        generateEpd(genepd);
    }
#ifdef EVALTUNE
    else if (pgnconvertfile != "")
    {
        PGNtoFEN(pgnconvertfile, quietonly, ppg);
    }
    else if (fentuningfiles != "")
    {
        TexelTune(fentuningfiles, quietonly, optk, correlation);
    }
#endif
    else {
        // usual uci mode
        en.communicate("");
    }

    return 0;
}
