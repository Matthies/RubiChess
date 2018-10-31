
#include "RubiChess.h"

#ifdef _WIN32

string GetSystemInfo()
{
    // shameless copy from MSDN example explaining __cpuid
    char CPUString[0x20];
    char CPUBrandString[0x40];
    int CPUInfo[4] = { -1 };
    //int nCacheLineSize = 0; // Maybe usefull for TT sizing

    unsigned    nIds, nExIds, i;
    //bool    bPOPCNT = false;
    //bool    bBMI2 = false;


    __cpuid(CPUInfo, 0);
    nIds = CPUInfo[0];
    memset(CPUString, 0, sizeof(CPUString));
    *((int*)CPUString) = CPUInfo[1];
    *((int*)(CPUString + 4)) = CPUInfo[3];
    *((int*)(CPUString + 8)) = CPUInfo[2];

    // Get the information associated with each valid Id
    for (i = 0; i <= nIds; ++i)
    {
        __cpuid(CPUInfo, i);
        // Interpret CPU feature information.
        if (i == 1)
        {
            //bPOPCNT = (CPUInfo[2] & 0x800000) || false;
        }

        if (i == 7)
        {
            // this is not in the MSVC2012 example but may be useful later
            //bBMI2 = (CPUInfo[1] & 0x100) || false;
        }
    }


    // Calling __cpuid with 0x80000000 as the InfoType argument
    // gets the number of valid extended IDs.
    __cpuid(CPUInfo, 0x80000000);
    nExIds = CPUInfo[0];
    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    // Get the information associated with each extended ID.
    for (i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(CPUInfo, i);

        // Interpret CPU brand string and cache information.
        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }

    return CPUBrandString;
}

#else

string GetSystemInfo()
{
    return "some Linux box";
}

#endif


long long perft(int depth, bool dotests)
{
    long long retval = 0;

    if (dotests)
    {
        if (pos.hash != zb.getHash())
        {
            printf("Alarm! Wrong Hash! %llu\n", zb.getHash());
            pos.print();
        }
        if (pos.pawnhash && pos.pawnhash != zb.getPawnHash())
        {
            printf("Alarm! Wrong Pawn Hash! %llu\n", zb.getPawnHash());
            pos.print();
        }
        if (pos.materialhash != zb.getMaterialHash())
        {
            printf("Alarm! Wrong Material Hash! %llu\n", zb.getMaterialHash());
            pos.print();
        }
        int val1 = getValueNoTrace(&pos);
        pos.mirror();
        int val2 = getValueNoTrace(&pos);
        pos.mirror();
        int val3 = getValueNoTrace(&pos);
        if (!(val1 == val3 && val1 == -val2))
        {
            printf("Mirrortest  :error  (%d / %d / %d)\n", val1, val2, val3);
            pos.print();
            //printf("Position value: %d\n", pos.getPositionValue<NOTRACE>());
            pos.mirror();
            pos.print();
            //printf("Position value: %d\n", pos.getPositionValue<NOTRACE>());
            pos.mirror();
            pos.print();
            //printf("Position value: %d\n", pos.getPositionValue<NOTRACE>());
        }
    }
    chessmovelist movelist;
    movelist.length = pos.getMoves(&movelist.move[0]);
    pos.prepareStack();
    //movelist->sort();
    //printf("Path: %s \nMovelist : %s\n", p->actualpath.toString().c_str(), movelist->toString().c_str());

    if (depth == 0)
        retval = 1;
    else if (depth == 1)
    {
        for (int i = 0; i < movelist.length; i++)
        {
            if (pos.playMove(&movelist.move[i]))
            {
                //printf("%s ok ", movelist->move[i].toString().c_str());
                retval++;
                pos.unplayMove(&movelist.move[i]);
            }
        }
    }

    else
    {
        for (int i = 0; i < movelist.length; i++)
        {
            if (pos.playMove(&movelist.move[i]))
            {
                //printf("\nMove: %s  ", movelist->move[i].toString().c_str());
                retval += perft(depth - 1, dotests);
                pos.unplayMove(&movelist.move[i]);
            }
        }
    }
    //printf("\nAnzahl: %d\n", retval);
    return retval;
}

void perftest(bool dotests, int maxdepth)
{
    struct perftestresultstruct
    {
        string fen;
        unsigned long long nodes[10];
    } perftestresults[] =
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

    int i = 0;
    printf("\n\nPerft results for %s (Build %s)\n", en.name, BUILD);
    printf("System: %s\n", GetSystemInfo().c_str());
    printf("Depth = %d      Hash-/Mirror-Tests %s\n", maxdepth, (dotests ? "enabled" : "disabled"));
    printf("========================================================================\n");

    float df;
    U64 totalresult = 0ULL;

    long long perftstarttime = getTime();
    long long perftlasttime = perftstarttime;

    while (perftestresults[i].fen != "")
    {
        pos.getFromFen(perftestresults[i].fen.c_str());
        int j = 0;
        while (perftestresults[i].nodes[j] > 0 && j <= maxdepth)
        {
            long long starttime = getTime();

            U64 result = perft(j, dotests);
            totalresult += result;

            perftlasttime = getTime();
            df = float(perftlasttime - starttime) / (float) en.frequency;
            printf("Perft %d depth %d  : %*llu  %*f sec.  %*d nps ", i + 1, j, 10, result, 10, df, 8, (int)(df > 0.0 ? (double)result / df : 0));
            if (result == perftestresults[i].nodes[j])
                printf("  OK\n");
            else
                printf("  wrong (should be %llu)\n", perftestresults[i].nodes[j]);
            j++;
        }
        if (perftestresults[++i].fen != "")
            printf("\n");
    }
    df = float(perftlasttime - perftstarttime) / (float)en.frequency;
    printf("========================================================================\n");
    printf("Total:             %*llu  %*f sec.  %*d nps \n", 10, totalresult, 10, df, 8, (int)(df > 0.0 ? (double)totalresult / df : 0));
}


void doBenchmark()
{
    struct benchmarkstruct
    {
        string name;
        string fen;
        int depth;
        int terminationscore;
        int resethash;
        long long time;
        long long nodes;
        int score;
        int depthAtExit;
    } benchmark[] =
    {
        {   
            "Startposition",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            10,
            0,
            1
        },
        {
            "Lasker Test",
            "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1",
            28,
            0,
            1
        },
        {
            "IQ4 63",
            "2R5/r3b1k1/p2p4/P1pPp2p/6q1/2P2N1r/4Q1P1/5RK1 w - - 0 1 ",
            14,
            300,
            1
        },
        {
            "Wacnew 167",
            "7Q/ppp2q2/3p2k1/P2Ppr1N/1PP5/7R/5rP1/6K1 b - - 0 1",
            14,
            1000,
            1
        },
        { 
            "Wacnew 212",
            "rn1qr2Q/pbppk1p1/1p2pb2/4N3/3P4/2N5/PPP3PP/R4RK1 w - - 0 1",
            13,
            500,
            1
        },
        { 
            "Carlos 6",
            "rn1q1r2/1bp1bpk1/p3p2p/1p2N1pn/3P4/1BN1P1B1/PPQ2PPP/2R2RK1 w - - 0 1",
            13,
            300,
            1
        },
         
        {
            "Arasan19 83",
            "6k1/p4qp1/1p3r1p/2pPp1p1/1PP1PnP1/2P1KR1P/1B6/7Q b - - 0 1 ",
            14,
            200,
            1
        },
        {
            "Arasan19 192",
            "r2qk2r/1b1nbp1p/p1n1p1p1/1pp1P3/6Q1/2NPB1PN/PPP3BP/R4RK1 w kq - 0 1",
            13,
            100,
            1
        },
        {
            "BT2630 12",
            "8/pp3k2/2p1qp2/2P5/5P2/1R2p1rp/PP2R3/4K2Q b - - 0 1",
            15,
            300,
            1
        },
        {
            "IQ4 116",
            "4r1k1/1p2qrpb/p1p4p/2Pp1p2/1Q1Rn3/PNN1P1P1/1P3PP1/3R2K1 b - - 0 1",
            14,
            300,
            1
        },
        {
            "", "", 0, 0
        }
    };

    long long starttime, endtime;

    int i = 0;
    while (benchmark[i].fen != "")
    {
        struct benchmarkstruct *bm = &benchmark[i];
        if (bm->resethash)
            en.setOption("clear hash", "true");

        en.communicate("ucinewgame" + bm->fen);
        en.communicate("position fen " + bm->fen);
        starttime = getTime();
        int dp = bm->depth;
        if (bm->terminationscore)
            en.terminationscore = bm->terminationscore;
        else
            en.terminationscore = SHRT_MAX;
        if (dp)
            en.communicate("go depth " + to_string(dp));
        else
            en.communicate("go infinite");

        endtime = getTime();
        bm->time = endtime - starttime;
        bm->nodes = en.nodes;
        bm->score = en.benchscore;
        bm->depthAtExit = en.benchdepth;
        i++;
    }

    en.terminationscore = SHRT_MAX;
    i = 0;
    long long totaltime = 0;
    long long totalnodes = 0;
    fprintf(stderr, "\n\nBenchmark results for %s (Build %s):\n", en.name, BUILD);
    fprintf(stderr, "System: %s\n", GetSystemInfo().c_str());
    fprintf(stderr, "===============================================================================================================\n");
    while (benchmark[i].fen != "")
    {
        struct benchmarkstruct *bm = &benchmark[i];
        totaltime += bm->time;
        totalnodes += bm->nodes;
        fprintf(stderr, "Bench # %2d (%20s / %2d): %7d cp  %4d ply  %10f sec.  %10lld nodes %10lld nps\n", i + 1, bm->name.c_str(), bm->depth, bm->score, bm->depthAtExit, (float)bm->time / (float)en.frequency, bm->nodes, bm->nodes * en.frequency / bm->time);
        i++;
    }
    fprintf(stderr, "===============================================================================================================\n");
    fprintf(stderr, "Overall:                                                      %10f sec.  %10lld nodes %*lld nps\n", ((float)totaltime / (float)en.frequency), totalnodes, 10, totalnodes * en.frequency / totaltime);
}



#ifdef _WIN32

void readfromengine(HANDLE pipe, enginestate *es)
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
    } while (true);
}

BOOL writetoengine(HANDLE pipe, const char *s)
{
    DWORD written;
    return WriteFile(pipe, s, (DWORD)strlen(s), &written, NULL);
}

void testengine(string epdfilename, int startnum, string engineprg, string logfilename, string comparefilename, int maxtime, int flags)
{
    struct enginestate es;
    string line;
    thread *readThread;
    ifstream comparefile;
    bool compare = false;
    char buf[1024];

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
    logfile << "num passed bestmove foundmove score time\n";

    // Start the engine with linked pipes
    HANDLE g_hChildStd_IN_Rd = NULL;
    HANDLE g_hChildStd_IN_Wr = NULL;
    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0)
        || !SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)
        || !CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &sa, 0)
        || !SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
    {
        printf("Cannot pipe connection to engine process.\n");
        return;
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = g_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = g_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    bSuccess = CreateProcess(NULL, (LPSTR)engineprg.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    if (!bSuccess)
    {
        printf("Cannot create process for engine %s.\n", engineprg.c_str());
        return;
    }
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    readThread = new thread(&readfromengine, g_hChildStd_OUT_Rd, &es);

    // Read epd line by line
    int linenum = 0;
    while (getline(epdfile, line))
    {
        vector<string> fv = SplitString(line.c_str());
        if (fv.size() > 4)
        {
            string fenstr = "";
            string opstr = "";
            // split fen from operation part
            for (int i = 0; i < 4; i++)
                fenstr = fenstr + fv[i] + " ";
            if (pos.getFromFen(fenstr.c_str()) == 0 && ++linenum >= startnum)
            {
                // Get data from compare file
                es.doCompare = false;
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
                        es.doCompare = true;
                        es.comparesuccess = (cv[1] == "+");
                        es.comparescore = SCOREBLACKWINS;
                        es.comparetime = -1;
                        if (cv.size() > 4)
                        {
                            try
                            {
                                es.comparescore = stoi(cv[4]);
                            }
                            catch (const invalid_argument&) {}
                        }
                        if (cv.size() > 5)
                        {
                            try
                            {
                                es.comparetime = stoi(cv[5]);
                            }
                            catch (const invalid_argument&) {}
                            if (es.comparetime == 0 && (flags & 0x1))
                                // nothing to improve; skip this test
                                continue;
                        }
                    }
                }
                // Extract the bm string
                bool searchbestmove = false;
                bool searchavoidmove = false;
                es.bestmoves = "";
                string moveliststr;
                for (unsigned int i = 4; i < fv.size(); i++)
                {
                    if (searchbestmove || searchavoidmove)
                    {
                        size_t smk = fv[i].find(';');
                        if (smk != string::npos)
                            fv[i] = fv[i].substr(0, smk);
                        if (moveliststr != "")
                            moveliststr += " ";
                        moveliststr += AlgebraicFromShort(fv[i]);
                        if (smk != string::npos)
                        {
                            if (searchbestmove)
                            {
                                es.bestmoves = moveliststr;
                                searchbestmove = false;
                            }
                            else if (searchavoidmove)
                            {
                                es.avoidmoves = moveliststr;
                                searchavoidmove = false;
                            }
                        }
                    }
                    if (strstr(fv[i].c_str(), "bm") != NULL)
                    {
                        searchbestmove = true;
                        moveliststr = "";
                    }
                    if (strstr(fv[i].c_str(), "am") != NULL)
                    {
                        searchavoidmove = true;
                        moveliststr = "";
                    }
                }

                // Initialize the engine
                es.phase = 0;
                es.score = SCOREBLACKWINS;
                bSuccess = writetoengine(g_hChildStd_IN_Wr, "uci\n");
                while (es.phase == 0)
                    Sleep(1000);
                bSuccess = writetoengine(g_hChildStd_IN_Wr, "ucinewgame\n");
                bSuccess = writetoengine(g_hChildStd_IN_Wr, "isready\n");
                while (es.phase == 1)
                    Sleep(1000);

                es.starttime = clock();
                es.firstbesttimesec = -1;
                sprintf_s(buf, "position fen %s 0 1\ngo infinite\n", fenstr.c_str());
                bSuccess = writetoengine(g_hChildStd_IN_Wr, buf);
                bool engineStopped = false;
                while (es.phase < 3)
                {
                    Sleep(1000);
                    clock_t now = clock();
                    if (!engineStopped
                        && ((now - es.starttime) / CLOCKS_PER_SEC > maxtime
                            || es.score > SCOREWHITEWINS - MAXDEPTH
                            || ((flags & 0x2) && es.doCompare && es.comparesuccess && (now - es.starttime) / CLOCKS_PER_SEC > es.comparetime)
                            || ((flags & 0x2) && es.firstbesttimesec >= 0 && ((now - es.starttime) / CLOCKS_PER_SEC) > es.firstbesttimesec + 5)))
                    {
                        bSuccess = writetoengine(g_hChildStd_IN_Wr, "stop\n");
                        engineStopped = true;
                    }
                }
                if (es.firstbesttimesec >= 0)
                {
                    printf("%d  %s: %s  found: %s  score: %d  time: %d\n", linenum, (es.bestmoves != "" ? "bm" : "am"), (es.bestmoves != "" ? es.bestmoves.c_str() : es.avoidmoves.c_str()), es.enginesbestmove.c_str(), es.score, es.firstbesttimesec);
                    logfile << linenum << " + \"" << (es.bestmoves != "" ? es.bestmoves.c_str() : (es.avoidmoves + "(a)").c_str()) << "\" " << es.enginesbestmove.c_str() << " " << es.score << " " << es.firstbesttimesec << "\n";

                }
                else
                {
                    printf("%d  %s: %s  found: %s ... failed  score: %d\n", linenum, (es.bestmoves != "" ? "bm" : "am"), (es.bestmoves != "" ? es.bestmoves.c_str() : es.avoidmoves.c_str()), es.enginesbestmove.c_str(), es.allscore);
                    logfile << linenum << " - \"" << (es.bestmoves != "" ? es.bestmoves.c_str() : (es.avoidmoves + "(a)").c_str()) << "\" " << es.enginesbestmove.c_str() << " " << es.allscore << "\n";
                }
            }
        }
    }
    bSuccess = writetoengine(g_hChildStd_IN_Wr, "quit\n");
    readThread->detach();
    delete readThread;
}


#else // _WIN32

void testengine(string epdfilename, int startnum, string engineprg, string logfilename, string comparefilename, int maxtime, int flags)
{
    // not yet implemented
}

#endif

int main(int argc, char* argv[])
{
    int startnum = 1;
    int perfmaxdepth = 0;
    bool benchmark = false;
    bool dotests = false;
    bool enginetest = false;
    bool verbose = false;
    string epdfile = "";
    string engineprg = "";
    string logfile = "";
    string comparefile = "";
    string pgnconvertfile = "";
    string fentuningfile = "";
    int maxtime = 0;
    int flags = 0;

    struct arguments {
        const char *cmd;
        const char *info;
        void* variable;
        char type;
        const char *defaultval;
    } allowedargs[] = {
        { "-bench", "Do benchmark test for some positions.", &benchmark, 0, NULL },
        { "-perft", "Do performance and move generator testing.", &perfmaxdepth, 1, "0" },
        { "-dotests","test the hash function and value for positions and mirror (use with -perft)", &dotests, 0, NULL },
        { "-enginetest", "bulk testing of epd files", &enginetest, 0, NULL },
        { "-epdfile", "the epd file to test (use with -enginetest)", &epdfile, 2, "" },
        { "-logfile", "output file (use with -enginetest)", &logfile, 2, "enginetest.log" },
        { "-engineprg", "the uci engine to test (use with -enginetest)", &engineprg, 2, "rubichess.exe" },
        { "-maxtime", "time for each test in seconds (use with -enginetest)", &maxtime, 1, "30" },
        { "-startnum", "number of the test in epd to start with (use with -enginetest)", &startnum, 1, "1" },
        { "-compare", "for fast comparision against logfile from other engine (use with -enginetest)", &comparefile, 2, "" },
        { "-flags", "1=skip easy (0 sec.) compares; 2=break 5 seconds after first find; 4=break after compare time is over (use with -enginetest)", &flags, 1, "0" },
        { "-verbose","more output (in tuning mode and maybe more in the future)", &verbose, 0, NULL },
#ifdef EVALTUNE
        { "-pgnfile", "converts games in a PGN file to fen for tuning them later", &pgnconvertfile, 2, "" },
        { "-fentuning", "reads FENs from file and tunes eval parameters against it", &fentuningfile, 2, "" },
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

    pos.init();
    searchinit();

    cout.setf(ios_base::unitbuf);

    printf("%s (Build %s)\n UCI compatible chess engine by %s\nParameterlist:\n", en.name, BUILD, en.author);

    for (int j = 0; allowedargs[j].cmd; j++)
    {
        int val = 0;
        for (int i = 1; i < argc; i++)
        {
            if (strcmp(argv[i], allowedargs[j].cmd) == 0)
            {
                val = i;
                break;
            }
        }
        switch (allowedargs[j].type)
        {
        case 0:
            *(bool*)(allowedargs[j].variable) = (val > 0);
            printf(" %s: %s  (%s)\n", allowedargs[j].cmd, *(bool*)(allowedargs[j].variable) ? "yes" : "no", allowedargs[j].info);
            break;
        case 1:
            try { *(int*)(allowedargs[j].variable) = stoi((val > 0 && val < argc - 1 ? argv[val + 1] : allowedargs[j].defaultval)); }
            catch (const invalid_argument&) {}
            printf(" %s: %d  (%s)\n", allowedargs[j].cmd, *(int*)(allowedargs[j].variable), allowedargs[j].info);
            break;
        case 2:
            *(string*)(allowedargs[j].variable) = (val > 0 && val < argc - 1 ? argv[val + 1] : allowedargs[j].defaultval);
            printf(" %s: %s  (%s)\n", allowedargs[j].cmd, (*(string*)(allowedargs[j].variable)).c_str(), allowedargs[j].info);
        }
    }

    printf("Here we go...\n\n");
    en.verbose = verbose;

    if (perfmaxdepth)
    {
        // do a perft test
        perftest(dotests, perfmaxdepth);
    } else if (benchmark)
    {
        // benchmark mode
        doBenchmark();
    } else if (enginetest)
    {
        //engine test mode
        testengine(epdfile, startnum, engineprg, logfile, comparefile, maxtime, flags);
    }
#ifdef EVALTUNE
    else if (pgnconvertfile != "")
    {
        PGNtoFEN(pgnconvertfile);
    }
    else if (fentuningfile != "")
    {
        TexelTune(fentuningfile);
    }
#endif
    else {
        // usual uci mode
        en.communicate("");
    }

    return 0;
}
