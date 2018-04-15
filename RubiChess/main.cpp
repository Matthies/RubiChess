
#include "RubiChess.h"


#ifdef _WIN32

string GetSystemInfo()
{
    // shameless copy from MSDN example explaining __cpuid
    char CPUString[0x20];
    char CPUBrandString[0x40];
    int CPUInfo[4] = { -1 };
    int nSteppingID = 0;
    int nModel = 0;
    int nFamily = 0;
    int nProcessorType = 0;
    int nExtendedmodel = 0;
    int nExtendedfamily = 0;
    int nBrandIndex = 0;
    int nCLFLUSHcachelinesize = 0;
    int nLogicalProcessors = 0;
    int nAPICPhysicalID = 0;
    int nFeatureInfo = 0;
    int nCacheLineSize = 0;
    int nL2Associativity = 0;
    int nCacheSizeK = 0;
    int nPhysicalAddress = 0;
    int nVirtualAddress = 0;
    int nRet = 0;

    int nCores = 0;
    int nCacheType = 0;
    int nCacheLevel = 0;
    int nMaxThread = 0;
    int nSysLineSize = 0;
    int nPhysicalLinePartitions = 0;
    int nWaysAssociativity = 0;
    int nNumberSets = 0;

    unsigned    nIds, nExIds, i;

    bool    bSSE3Instructions = false;
    bool    bMONITOR_MWAIT = false;
    bool    bCPLQualifiedDebugStore = false;
    bool    bVirtualMachineExtensions = false;
    bool    bEnhancedIntelSpeedStepTechnology = false;
    bool    bThermalMonitor2 = false;
    bool    bSupplementalSSE3 = false;
    bool    bL1ContextID = false;
    bool    bCMPXCHG16B = false;
    bool    bxTPRUpdateControl = false;
    bool    bPerfDebugCapabilityMSR = false;
    bool    bSSE41Extensions = false;
    bool    bSSE42Extensions = false;
    bool    bPOPCNT = false;
    bool    bBMI2 = false;

    bool    bMultithreading = false;

    bool    bLAHF_SAHFAvailable = false;
    bool    bCmpLegacy = false;
    bool    bSVM = false;
    bool    bExtApicSpace = false;
    bool    bAltMovCr8 = false;
    bool    bLZCNT = false;
    bool    bSSE4A = false;
    bool    bMisalignedSSE = false;
    bool    bPREFETCH = false;
    bool    bSKINITandDEV = false;
    bool    bSYSCALL_SYSRETAvailable = false;
    bool    bExecuteDisableBitAvailable = false;
    bool    bMMXExtensions = false;
    bool    bFFXSR = false;
    bool    b1GBSupport = false;
    bool    bRDTSCP = false;
    bool    b64Available = false;
    bool    b3DNowExt = false;
    bool    b3DNow = false;
    bool    bNestedPaging = false;
    bool    bLBRVisualization = false;
    bool    bFP128 = false;
    bool    bMOVOptimization = false;

    bool    bSelfInit = false;
    bool    bFullyAssociative = false;

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
            nSteppingID = CPUInfo[0] & 0xf;
            nModel = (CPUInfo[0] >> 4) & 0xf;
            nFamily = (CPUInfo[0] >> 8) & 0xf;
            nProcessorType = (CPUInfo[0] >> 12) & 0x3;
            nExtendedmodel = (CPUInfo[0] >> 16) & 0xf;
            nExtendedfamily = (CPUInfo[0] >> 20) & 0xff;
            nBrandIndex = CPUInfo[1] & 0xff;
            nCLFLUSHcachelinesize = ((CPUInfo[1] >> 8) & 0xff) * 8;
            nLogicalProcessors = ((CPUInfo[1] >> 16) & 0xff);
            nAPICPhysicalID = (CPUInfo[1] >> 24) & 0xff;
            bSSE3Instructions = (CPUInfo[2] & 0x1) || false;
            bMONITOR_MWAIT = (CPUInfo[2] & 0x8) || false;
            bCPLQualifiedDebugStore = (CPUInfo[2] & 0x10) || false;
            bVirtualMachineExtensions = (CPUInfo[2] & 0x20) || false;
            bEnhancedIntelSpeedStepTechnology = (CPUInfo[2] & 0x80) || false;
            bThermalMonitor2 = (CPUInfo[2] & 0x100) || false;
            bSupplementalSSE3 = (CPUInfo[2] & 0x200) || false;
            bL1ContextID = (CPUInfo[2] & 0x300) || false;
            bCMPXCHG16B = (CPUInfo[2] & 0x2000) || false;
            bxTPRUpdateControl = (CPUInfo[2] & 0x4000) || false;
            bPerfDebugCapabilityMSR = (CPUInfo[2] & 0x8000) || false;
            bSSE41Extensions = (CPUInfo[2] & 0x80000) || false;
            bSSE42Extensions = (CPUInfo[2] & 0x100000) || false;
            bPOPCNT = (CPUInfo[2] & 0x800000) || false;
            nFeatureInfo = CPUInfo[3];
            bMultithreading = (nFeatureInfo & (1 << 28)) || false;
        }

        if (i == 7)
        {
            // this is not in the MSVC2012 example but may be useful later
            bBMI2 = (CPUInfo[1] & 0x100) || false;
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
        if (i == 0x80000001)
        {
            bLAHF_SAHFAvailable = (CPUInfo[2] & 0x1) || false;
            bCmpLegacy = (CPUInfo[2] & 0x2) || false;
            bSVM = (CPUInfo[2] & 0x4) || false;
            bExtApicSpace = (CPUInfo[2] & 0x8) || false;
            bAltMovCr8 = (CPUInfo[2] & 0x10) || false;
            bLZCNT = (CPUInfo[2] & 0x20) || false;
            bSSE4A = (CPUInfo[2] & 0x40) || false;
            bMisalignedSSE = (CPUInfo[2] & 0x80) || false;
            bPREFETCH = (CPUInfo[2] & 0x100) || false;
            bSKINITandDEV = (CPUInfo[2] & 0x1000) || false;
            bSYSCALL_SYSRETAvailable = (CPUInfo[3] & 0x800) || false;
            bExecuteDisableBitAvailable = (CPUInfo[3] & 0x10000) || false;
            bMMXExtensions = (CPUInfo[3] & 0x40000) || false;
            bFFXSR = (CPUInfo[3] & 0x200000) || false;
            b1GBSupport = (CPUInfo[3] & 0x400000) || false;
            bRDTSCP = (CPUInfo[3] & 0x8000000) || false;
            b64Available = (CPUInfo[3] & 0x20000000) || false;
            b3DNowExt = (CPUInfo[3] & 0x40000000) || false;
            b3DNow = (CPUInfo[3] & 0x80000000) || false;
        }

        // Interpret CPU brand string and cache information.
        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000006)
        {
            nCacheLineSize = CPUInfo[2] & 0xff;
            nL2Associativity = (CPUInfo[2] >> 12) & 0xf;
            nCacheSizeK = (CPUInfo[2] >> 16) & 0xffff;
        }
        else if (i == 0x80000008)
        {
            nPhysicalAddress = CPUInfo[0] & 0xff;
            nVirtualAddress = (CPUInfo[0] >> 8) & 0xff;
        }
        else if (i == 0x8000000A)
        {
            bNestedPaging = (CPUInfo[3] & 0x1) || false;
            bLBRVisualization = (CPUInfo[3] & 0x2) || false;
        }
        else if (i == 0x8000001A)
        {
            bFP128 = (CPUInfo[0] & 0x1) || false;
            bMOVOptimization = (CPUInfo[0] & 0x2) || false;
        }
    }

    for (i = 0;; i++)
    {
        __cpuidex(CPUInfo, 0x4, i);
        if (!(CPUInfo[0] & 0xf0)) break;

        if (i == 0)
        {
            //FIXME: This is not the real number of cores but the maximum possible - 1
            nCores = CPUInfo[0] >> 26;
        }

        nCacheType = (CPUInfo[0] & 0x1f);
        nCacheLevel = (CPUInfo[0] & 0xe0) >> 5;
        bSelfInit = (((CPUInfo[0] & 0x100) >> 8) > 0);
        bFullyAssociative = (((CPUInfo[0] & 0x200) >> 9) > 0);
        nMaxThread = (CPUInfo[0] & 0x03ffc000) >> 14;
        nSysLineSize = (CPUInfo[1] & 0x0fff);
        nPhysicalLinePartitions = (CPUInfo[1] & 0x03ff000) >> 12;
        nWaysAssociativity = (CPUInfo[1]) >> 22;
        nNumberSets = CPUInfo[2];
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
        int val1 = pos.getValue();
        pos.mirror();
        int val2 = pos.getValue();
        pos.mirror();
        int val3 = pos.getValue();
        if (!(val1 == val3 && val1 == -val2))
        {
            printf("Mirrortest  :error  (%d / %d / %d)\n", val1, val2, val3);
            pos.print();
//            printf("Material value: %d\n", pos.countMaterial());
            printf("Position value: %d\n", pos.getPositionValue());
            pos.mirror();
            pos.print();
//            printf("Material value: %d\n", pos.countMaterial());
            printf("Position value: %d\n", pos.getPositionValue());
            pos.mirror();
            pos.print();
//            printf("Material value: %d\n", pos.countMaterial());
            printf("Position value: %d\n", pos.getPositionValue());
        }
    }
    chessmovelist* movelist = pos.getMoves();
    //movelist->sort();
    //printf("Path: %s \nMovelist : %s\n", p->actualpath.toString().c_str(), movelist->toString().c_str());

    if (depth == 0)
        retval = 1;
    else if (depth == 1)
    {
        for (int i = 0; i < movelist->length; i++)
        {
            if (pos.playMove(&movelist->move[i]))
            {
                //printf("%s ok ", movelist->move[i].toString().c_str());
                retval++;
                pos.unplayMove(&movelist->move[i]);
            }
        }
    }

    else
    {
        for (int i = 0; i < movelist->length; i++)
        {
            if (pos.playMove(&movelist->move[i]))
            {
                //printf("\nMove: %s  ", movelist->move[i].toString().c_str());
                retval += perft(depth - 1, dotests);
                pos.unplayMove(&movelist->move[i]);
            }
        }
    }
    free(movelist);
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
    } benchmark[] =
    {
        {   
            "Startposition",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            8,
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
            "Matt in 6",
            "3k4/3P4/3P4/3P4/3P4/1P1P1P2/3P4/1R1K4 w - - 0 1",
            0,
            30000,
            1
        },
        { 
            "Wacnew 167",
            "7Q/ppp2q2/3p2k1/P2Ppr1N/1PP5/7R/5rP1/6K1 b - - 0 1",
            0,
            30000,
            1
        },
        { 
            "Wacnew 212",
            "rn1qr2Q/pbppk1p1/1p2pb2/4N3/3P4/2N5/PPP3PP/R4RK1 w - - 0 1",
            0,
            30000,
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
        i++;
    }

    en.terminationscore = SHRT_MAX;
    i = 0;
    long long totaltime = 0;
    long long totalnodes = 0;
    printf("\n\nBenchmark results for %s (Build %s):\n", en.name, BUILD);
    printf("System: %s\n", GetSystemInfo().c_str());
    printf("==========================================================================================\n");
    while (benchmark[i].fen != "")
    {
        struct benchmarkstruct *bm = &benchmark[i];
        totaltime += bm->time;
        totalnodes += bm->nodes;
        printf("Bench # %2d (%20s / %2d):  %10f sec.  %10lld nodes %10lld nps\n", i + 1, bm->name.c_str(), bm->depth, (float)bm->time / (float)en.frequency, bm->nodes, bm->nodes * en.frequency / bm->time);
        i++;
    }
    printf("==========================================================================================\n");
    printf("Overall:                                 %10f sec.  %10lld nodes %*lld nps\n", ((float)totaltime / (float)en.frequency), totalnodes, 10, totalnodes * en.frequency / totaltime);
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
                        if (strstr(es->bestmoves.c_str(), myPv.c_str()) != NULL
                            || (es->bestmoves == "" && strstr(es->avoidmoves.c_str(), myPv.c_str()) == NULL))
                        {
                            if (es->firstbesttimesec < 0)
                            {
                                es->firstbesttimesec = (clock() - es->starttime) / CLOCKS_PER_SEC;
                                //printf("%d %d  %d\n", es->firstbesttimesec, es->starttime, clock());
                            }

                            if (score)
                            {
                                vector<string> scoretoken = SplitString(score);
                                if (scoretoken.size() > 1)
                                {
                                    try
                                    {
                                        es->score = stoi(scoretoken[1]);
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
                                        es->score = SCOREWHITEWINS - stoi(matetoken[1]);
                                    }
                                    catch (const invalid_argument&) {}
                                }
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
                    printf("%d  %s: %s  found: %s ... failed\n", linenum, (es.bestmoves != "" ? "bm" : "am"), (es.bestmoves != "" ? es.bestmoves.c_str() : es.avoidmoves.c_str()), es.enginesbestmove.c_str());
                    logfile << linenum << " - \"" << (es.bestmoves != "" ? es.bestmoves.c_str() : (es.avoidmoves + "(a)").c_str()) << "\" " << es.enginesbestmove.c_str() << "\n";
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
