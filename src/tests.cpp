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

using namespace rubichess;

namespace rubichess {

// perft Tests
void perftjob(workingthread* thr)
{
    chessposition* pos = thr->pos;
    if (thr->lastCompleteDepth & 1)
    {
        memcpy((void*)pos, thr->rootpos, offsetof(chessposition, history));
        thr->lastCompleteDepth ^= 1;
        pos->tbhits = 0;
    }

    pos->prepareStack();
    chessmovelist* ml;
    int startmove = 0;
    if (pos->ply == 0)
    {
        pos->nodes = 0;
        ml = &pos->rootmovelist;
        startmove = thr->index;
        ml->length = startmove + 1;
    }
    else {
        ml = &pos->quietslist[pos->ply];
        if (pos->isCheckbb)
            ml->length = pos->CreateEvasionMovelist(&ml->move[0]);
        else
            ml->length = pos->CreateMovelist<ALL>(&ml->move[0]);
    }

    if (startmove >= ml->length)
        return;

    uint32_t mc = 0;
    for (int i = startmove; i < ml->length; i++)
    {
        mc = ml->move[i].code;
        if (pos->playMove<true>(mc))
        {
            if (thr->depth > pos->ply)
                perftjob(thr);
            else
                pos->nodes++;

            pos->unplayMove<true>(mc);
        }
    }
    if (pos->ply == 0)
    {
        if (thr->lastCompleteDepth & 2)
        {
            stringstream ss;
            ss << setw(5) << left << moveToString(mc) << ": " << pos->nodes << "\n";
            guiCom << ss.str();
        }
        // We use the tbhits field for counting total nodes
        pos->tbhits += pos->nodes;
    }
}

U64 engine::perft(int depth, bool printsysteminfo)
{
    long long starttime = 0;
    long long endtime = 0;
    U64 retval = 0;
    chessposition* rootpos = &en.rootposition;

    if (printsysteminfo) {
        starttime = getTime();
        guiCom << "Perft for depth " + to_string(maxdepth) + (en.chess960 ? "  Chess960" : "") + "\n";
    }

    rootpos->rootmovelist.length = rootpos->CreateMovelist<ALL>(&rootpos->rootmovelist.move[0]);

    for (int i = 0; i < en.Threads; i++)
        // write "position needs init" and print flag to the thread
        sthread[i].lastCompleteDepth = 1 + 2 * printsysteminfo;

    int tnum = 0;
    for (int i = 0; i < rootpos->rootmovelist.length; i++)
    {

        workingthread* wt;
        while ((wt = &sthread[tnum]) && en.Threads > 1 && wt->working)
        {
            tnum = (tnum + 1) % en.Threads;
            if (depth > 4 && tnum == 0)
                Sleep(1);
            continue;
        }
        wt->wait_for_work_finished();
        wt->index = i;
        wt->depth = depth;
        wt->run_job(perftjob);
        tnum = (tnum + 1) % en.Threads;
    }
    for (int i = 0; i < min(en.Threads, rootpos->rootmovelist.length); i++)
    {
        workingthread* wt = &sthread[i];
        wt->wait_for_work_finished();
        retval += wt->pos->tbhits;
    }

    if (printsysteminfo) {
        endtime = getTime();
        long long perftime = (long long)((endtime - starttime) * 1000.0 / frequency);
        guiCom << "Total nodes: " + to_string(retval) + "\n";
        guiCom << "Time (ms):   " + to_string(perftime) + "\n";
        guiCom << "NPS:         " + to_string((long long)(retval / (perftime / 1000.0))) + "\n";
    }

    return retval;
}

void perftest(int maxdepth)
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
    char str[256];

    guiCom << "Perft for depth " + to_string(maxdepth) + (en.chess960 ? "  Chess960" : "") + "\n";

    float df;
    U64 totalresult = 0ULL;

    long long perftstarttime = getTime();
    long long perftlasttime = perftstarttime;

    perftestresultstruct* ptr;
    if (en.chess960)
        ptr = perftestresults960;
    else
        ptr = perftestresults;

    while (ptr[i].fen != "")
    {
        en.rootposition.getFromFen(ptr[i].fen.c_str());
        int j = 1;
        while (ptr[i].nodes[j] > 0 && j <= maxdepth)
        {
            long long starttime = getTime();

            U64 result = en.perft(j);
            totalresult += result;

            perftlasttime = getTime();
            df = float(perftlasttime - starttime) / (float)en.frequency;
            snprintf(str, 256, "Perft %d depth %d  : %*llu  %*f sec.  %*d nps   %s\n", i + 1, j, 10, result, 10, df,
                8, (int)(df > 0.0 ? (double)result / df : 0), (result == ptr[i].nodes[j] ? "OK" : "Wrong!"));
            guiCom << str;
            j++;
        }
        if (ptr[++i].fen != "")
            guiCom << "\n";
    }
    df = float(perftlasttime - perftstarttime) / (float)en.frequency;
    guiCom << "========================================================================\n";
    snprintf(str, 256, "Total:             %*llu  %*f sec.  %*d nps \n", 10, totalresult, 10, df, 8, (int)(df > 0.0 ? (double)totalresult / df : 0));
    guiCom << str;
}


struct benchmarkstruct
{
    string name;
    string fen;
    int depth;
    long long time;
    U64 nodes;
    int score;
    int depthAtExit;
    string move;
    int solved;
};

const string solvedstr[] = { "-", "+", "o" };


static void benchTableHeader(bool bToErr)
{
    if (bToErr)
        guiCom.switchStream();
    guiCom << "\n" << "Benchmark results\n";
    engineHeader();
    if (bToErr)
        guiCom.switchStream();
}

static void benchTableItem(bool bToErr, int i, benchmarkstruct* bm)
{
    if (bToErr)
        guiCom.switchStream();
    char str[256];
    int sc = bm->score;
    string score = to_string(UCISCORE(sc)) + " cp";
    if (MATEDETECTED(sc)) {
        score = (sc < 0 ? "-" : " ");
        score = score + "M" + to_string(abs(MATEIN(bm->score)));
    }

    snprintf(str, 256, "Bench # %3d (%14s / %2d): %s  %6s%9s %3d ply %10f sec. %10lld nodes %10lld nps\n", i, bm->name.c_str(), bm->depth, solvedstr[bm->solved].c_str(), bm->move.c_str(), score.c_str(), bm->depthAtExit, (float)bm->time / (float)en.frequency, bm->nodes, bm->nodes * en.frequency / bm->time);
    guiCom << str;
    if (bToErr)
        guiCom.switchStream();
}

static void benchTableFooder(bool bToErr, long long totaltime, long long totalnodes, int totalsolved[2])
{
    if (bToErr)
        guiCom.switchStream();
    int totaltests = totalsolved[0] + totalsolved[1];
    double fSolved = totaltests ? 100.0 * totalsolved[1] / (double)totaltests : 0.0;
    guiCom << "=============================================================================================================\n";
    char str[256];
    snprintf(str, 256, "Overall:                  %4d/%3d = %4.1f%%                    %10f sec. %10lld nodes %*lld nps\n",
        totalsolved[1], totaltests, fSolved, ((float)totaltime / (float)en.frequency), totalnodes, 10, totalnodes * en.frequency / totaltime);
    guiCom << str;
    if (bToErr)
        guiCom.switchStream();
}

void engine::bench(int constdepth, string epdfilename, int consttime, int startnum, bool openbench)
{
    string benchmarkfens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1",
        //"2R5/r3b1k1/p2p4/P1pPp2p/6q1/2P2N1r/4Q1P1/5RK1 w - - 0 1 ",
        "7Q/ppp2q2/3p2k1/P2Ppr1N/1PP5/7R/5rP1/6K1 b - - 0 1",
        "rn1qr2Q/pbppk1p1/1p2pb2/4N3/3P4/2N5/PPP3PP/R4RK1 w - - 0 1",
        "rn1q1r2/1bp1bpk1/p3p2p/1p2N1pn/3P4/1BN1P1B1/PPQ2PPP/2R2RK1 w - - 0 1",
        "6k1/p4qp1/1p3r1p/2pPp1p1/1PP1PnP1/2P1KR1P/1B6/7Q b - - 0 1 ",
        "r2qk2r/1b1nbp1p/p1n1p1p1/1pp1P3/6Q1/2NPB1PN/PPP3BP/R4RK1 w kq - 0 1",
        "8/pp3k2/2p1qp2/2P5/5P2/1R2p1rp/PP2R3/4K2Q b - - 0 1",
        "4r1k1/1p2qrpb/p1p4p/2Pp1p2/1Q1Rn3/PNN1P1P1/1P3PP1/3R2K1 b - - 0 1",
        "br4k1/1qrnbppp/pp1ppn2/8/NPPBP3/PN3P2/5QPP/2RR1B1K w - - 0 1",
        "r1b1rk2/p1pq2p1/1p1b1p1p/n2P4/2P1NP2/P2B1R2/1BQ3PP/R6K w - - 0 1",
        "r4k2/1b3ppp/p2n1P2/q1p3PQ/Np1rp3/1P1B4/P1P4P/2K1R2R w - - 0 1",
        "N4B2/8/2K5/8/8/5k2/8/8 w - - 0 1",
        "N7/8/2K5/2Q5/8/1N3k2/5q2/8 b - - 0 1",
        "8/5N2/2K5/5b2/8/1N3k2/8/8 b - - 0 1",
        "7n/BBP2P1P/8/P1PpK3/P5RR/5k2/Pn2NPN1/3Q2b1 w - d6 0 1",
        "1r4Rk/4Nq2/7K/8/8/8/b5Q1/6R1 b - - 0 1",
        "8/8/8/8/8/6k1/6p1/6K1 w - - 0 1",
        "7k/7P/6K1/8/3B4/8/8/8 b - - 0 1",
        ""
    };


    long long endtime;
    list<benchmarkstruct> bmlist;

    ifstream epdfile;
    bool bGetFromEpd = false;
    if (epdfilename != "")
    {
        epdfile.open(epdfilename, ifstream::in);
        bGetFromEpd = epdfile.is_open();
        if (!bGetFromEpd)
            guiCom << "Cannot open file " + epdfilename + " for reading.\n";
    }

    int i = 0;
    int totalSolved[2] = { 0 };
    benchmarkstruct epdbm;
    bool bFollowup = false;

    while (true)
    {
        string avoidmoves = "";
        string bestmoves = "";
        benchmarkstruct* bm = &epdbm;
        if (!bGetFromEpd)
        {
            // standard bench with included positions
            bm->fen = benchmarkfens[i];
            bm->name = "Builtin #" + to_string(i + 1);
            bm->depth = 14; // default depth for builtin bench
        }
        else
        {
            // read positions from epd file
            string line;
            getline(epdfile, line);
            getFenAndBmFromEpd(line, &bm->fen, &bestmoves, &avoidmoves);

            bm->depth = 10;  // default depth for epd bench
        }
        if (bm->fen == "") break;

        if (++i < startnum) continue;

        if (!bFollowup)
            en.communicate("ucinewgame");
        string strPosition = "position fen " + bm->fen;
        if (bFollowup)
        {
            if (en.benchpondermove == "")
            {
                bFollowup = false;
                continue;
            }
            strPosition += " moves " + en.benchmove + " " + en.benchpondermove;
            bm->name = " ... " + en.benchmove + " " + en.benchpondermove;
        }
        cout << strPosition << "\n";
        communicate(strPosition);
        thinkstarttime = getTime();
        int dp = 0;
        int tm = consttime;
        if (constdepth)
            dp = constdepth;
        else
            dp = bm->depth;
        if (bFollowup)
            dp += 2;

        bm->depth = dp;

        if (tm)
            communicate("go movetime " + to_string(tm * 1000));
        else if (dp)
            communicate("go depth " + to_string(dp));
        else
            guiCom << "No depth and no movetime. Skipping.\n";

        searchWaitStop(false);
        endtime = getTime();
        bm->time = endtime - thinkstarttime;
        U64 tbhits;
        en.getNodesAndTbhits(&bm->nodes, &tbhits);
        bm->score = en.lastbestmovescore;
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
            benchTableItem(!openbench, i, bm);

        if (!bGetFromEpd)
            bFollowup = !bFollowup;
        if (bFollowup)
            i--;

        bmlist.push_back(*bm);
    }

    i = 0;
    long long totaltime = 0;
    long long totalnodes = 0;
    benchTableHeader(!openbench);

    for (list<benchmarkstruct>::iterator bm = bmlist.begin(); bm != bmlist.end(); bm++)
    {
        totaltime += bm->time;
        totalnodes += bm->nodes;
        benchTableItem(!openbench, ++i, &*bm);
    }
    if (totaltime)
    {
        benchTableFooder(!openbench, totaltime, totalnodes, totalSolved);
        if (openbench) {
            guiCom << "Time  : " + to_string(totaltime * 1000 / en.frequency) + "\n";
            guiCom << "Nodes : " + to_string(totalnodes) + "\n";
            guiCom << "NPS   : " + to_string(totalnodes * en.frequency / totaltime) + "\n";
        }
    }
}




// Speedtest using positions and setup of Stockfish for comarison
const vector<vector<string>> BenchmarkPositions = {
    {
        "rnbq1k1r/ppp1bppp/4pn2/8/2B5/2NP1N2/PPP2PPP/R1BQR1K1 b - - 2 8",
        "rnbq1k1r/pp2bppp/4pn2/2p5/2B2B2/2NP1N2/PPP2PPP/R2QR1K1 b - - 1 9",
        "r1bq1k1r/pp2bppp/2n1pn2/2p5/2B1NB2/3P1N2/PPP2PPP/R2QR1K1 b - - 3 10",
        "r1bq1k1r/pp2bppp/2n1p3/2p5/2B1PB2/5N2/PPP2PPP/R2QR1K1 b - - 0 11",
        "r1b2k1r/pp2bppp/2n1p3/2p5/2B1PB2/5N2/PPP2PPP/3RR1K1 b - - 0 12",
        "r1b1k2r/pp2bppp/2n1p3/2p5/2B1PB2/2P2N2/PP3PPP/3RR1K1 b - - 0 13",
        "r1b1k2r/1p2bppp/p1n1p3/2p5/4PB2/2P2N2/PP2BPPP/3RR1K1 b - - 1 14",
        "r1b1k2r/4bppp/p1n1p3/1pp5/P3PB2/2P2N2/1P2BPPP/3RR1K1 b - - 0 15",
        "r1b1k2r/4bppp/p1n1p3/1P6/2p1PB2/2P2N2/1P2BPPP/3RR1K1 b - - 0 16",
        "r1b1k2r/4bppp/2n1p3/1p6/2p1PB2/1PP2N2/4BPPP/3RR1K1 b - - 0 17",
        "r3k2r/3bbppp/2n1p3/1p6/2P1PB2/2P2N2/4BPPP/3RR1K1 b - - 0 18",
        "r3k2r/3bbppp/2n1p3/8/1pP1P3/2P2N2/3BBPPP/3RR1K1 b - - 1 19",
        "1r2k2r/3bbppp/2n1p3/8/1pPNP3/2P5/3BBPPP/3RR1K1 b - - 3 20",
        "1r2k2r/3bbppp/2n1p3/8/2PNP3/2B5/4BPPP/3RR1K1 b - - 0 21",
        "1r2k2r/3bb1pp/2n1pp2/1N6/2P1P3/2B5/4BPPP/3RR1K1 b - - 1 22",
        "1r2k2r/3b2pp/2n1pp2/1N6/1BP1P3/8/4BPPP/3RR1K1 b - - 0 23",
        "1r2k2r/3b2pp/4pp2/1N6/1nP1P3/8/3RBPPP/4R1K1 b - - 1 24",
        "1r5r/3bk1pp/4pp2/1N6/1nP1PP2/8/3RB1PP/4R1K1 b - - 0 25",
        "1r5r/3bk1pp/2n1pp2/1N6/2P1PP2/8/3RBKPP/4R3 b - - 2 26",
        "1r5r/3bk1pp/2n2p2/1N2p3/2P1PP2/6P1/3RBK1P/4R3 b - - 0 27",
        "1r1r4/3bk1pp/2n2p2/1N2p3/2P1PP2/6P1/3RBK1P/R7 b - - 2 28",
        "1r1r4/N3k1pp/2n1bp2/4p3/2P1PP2/6P1/3RBK1P/R7 b - - 4 29",
        "1r1r4/3bk1pp/2N2p2/4p3/2P1PP2/6P1/3RBK1P/R7 b - - 0 30",
        "1r1R4/4k1pp/2b2p2/4p3/2P1PP2/6P1/4BK1P/R7 b - - 0 31",
        "3r4/4k1pp/2b2p2/4P3/2P1P3/6P1/4BK1P/R7 b - - 0 32",
        "3r4/R3k1pp/2b5/4p3/2P1P3/6P1/4BK1P/8 b - - 1 33",
        "8/3rk1pp/2b5/R3p3/2P1P3/6P1/4BK1P/8 b - - 3 34",
        "8/3r2pp/2bk4/R1P1p3/4P3/6P1/4BK1P/8 b - - 0 35",
        "8/2kr2pp/2b5/R1P1p3/4P3/4K1P1/4B2P/8 b - - 2 36",
        "1k6/3r2pp/2b5/RBP1p3/4P3/4K1P1/7P/8 b - - 4 37",
        "8/1k1r2pp/2b5/R1P1p3/4P3/3BK1P1/7P/8 b - - 6 38",
        "1k6/3r2pp/2b5/2P1p3/4P3/3BK1P1/7P/R7 b - - 8 39",
        "1k6/r5pp/2b5/2P1p3/4P3/3BK1P1/7P/5R2 b - - 10 40",
        "1k3R2/6pp/2b5/2P1p3/4P3/r2BK1P1/7P/8 b - - 12 41",
        "5R2/2k3pp/2b5/2P1p3/4P3/r2B2P1/3K3P/8 b - - 14 42",
        "5R2/2k3pp/2b5/2P1p3/4P3/3BK1P1/r6P/8 b - - 16 43",
        "5R2/2k3pp/2b5/2P1p3/4P3/r2B2P1/4K2P/8 b - - 18 44",
        "5R2/2k3pp/2b5/2P1p3/4P3/3B1KP1/r6P/8 b - - 20 45",
        "8/2k2Rpp/2b5/2P1p3/4P3/r2B1KP1/7P/8 b - - 22 46",
        "3k4/5Rpp/2b5/2P1p3/4P3/r2B2P1/4K2P/8 b - - 24 47",
        "3k4/5Rpp/2b5/2P1p3/4P3/3B1KP1/r6P/8 b - - 26 48",
        "3k4/5Rpp/2b5/2P1p3/4P3/r2B2P1/4K2P/8 b - - 28 49",
        "3k4/5Rpp/2b5/2P1p3/4P3/3BK1P1/r6P/8 b - - 30 50",
        "3k4/5Rpp/2b5/2P1p3/4P3/r2B2P1/3K3P/8 b - - 32 51",
        "3k4/5Rpp/2b5/2P1p3/4P3/2KB2P1/r6P/8 b - - 34 52",
        "3k4/5Rpp/2b5/2P1p3/4P3/r2B2P1/2K4P/8 b - - 36 53",
        "3k4/5Rpp/2b5/2P1p3/4P3/1K1B2P1/r6P/8 b - - 38 54",
        "3k4/6Rp/2b5/2P1p3/4P3/1K1B2P1/7r/8 b - - 0 55",
        "3k4/8/2b3Rp/2P1p3/4P3/1K1B2P1/7r/8 b - - 1 56",
        "8/2k3R1/2b4p/2P1p3/4P3/1K1B2P1/7r/8 b - - 3 57",
        "3k4/8/2b3Rp/2P1p3/4P3/1K1B2P1/7r/8 b - - 5 58",
        "8/2k5/2b3Rp/2P1p3/1K2P3/3B2P1/7r/8 b - - 7 59",
        "8/2k5/2b3Rp/2P1p3/4P3/2KB2P1/3r4/8 b - - 9 60",
        "8/2k5/2b3Rp/2P1p3/1K2P3/3B2P1/6r1/8 b - - 11 61",
        "8/2k5/2b3Rp/2P1p3/4P3/2KB2P1/3r4/8 b - - 13 62",
        "8/2k5/2b3Rp/2P1p3/2K1P3/3B2P1/6r1/8 b - - 15 63",
        "4b3/2k3R1/7p/2P1p3/2K1P3/3B2P1/6r1/8 b - - 17 64",
    },
    {
        "r1bqkbnr/npp1pppp/p7/3P4/4pB2/2N5/PPP2PPP/R2QKBNR w KQkq - 1 6",
        "r1bqkb1r/npp1pppp/p4n2/3P4/4pB2/2N5/PPP1QPPP/R3KBNR w KQkq - 3 7",
        "r2qkb1r/npp1pppp/p4n2/3P1b2/4pB2/2N5/PPP1QPPP/2KR1BNR w kq - 5 8",
        "r2qkb1r/1pp1pppp/p4n2/1n1P1b2/4pB2/2N4P/PPP1QPP1/2KR1BNR w kq - 1 9",
        "r2qkb1r/1pp1pppp/5n2/1p1P1b2/4pB2/7P/PPP1QPP1/2KR1BNR w kq - 0 10",
        "r2qkb1r/1ppbpppp/5n2/1Q1P4/4pB2/7P/PPP2PP1/2KR1BNR w kq - 1 11",
        "3qkb1r/1Qpbpppp/5n2/3P4/4pB2/7P/rPP2PP1/2KR1BNR w k - 0 12",
        "q3kb1r/1Qpbpppp/5n2/3P4/4pB2/7P/rPP2PP1/1K1R1BNR w k - 2 13",
        "r3kb1r/2pbpppp/5n2/3P4/4pB2/7P/1PP2PP1/1K1R1BNR w k - 0 14",
        "r3kb1r/2Bb1ppp/4pn2/3P4/4p3/7P/1PP2PP1/1K1R1BNR w k - 0 15",
        "r3kb1r/2Bb2pp/4pn2/8/4p3/7P/1PP2PP1/1K1R1BNR w k - 0 16",
        "r3k2r/2Bb2pp/4pn2/2b5/4p3/7P/1PP1NPP1/1K1R1B1R w k - 2 17",
        "r6r/2Bbk1pp/4pn2/2b5/3Np3/7P/1PP2PP1/1K1R1B1R w - - 4 18",
        "r6r/b2bk1pp/4pn2/4B3/3Np3/7P/1PP2PP1/1K1R1B1R w - - 6 19",
        "r1r5/b2bk1pp/4pn2/4B3/2BNp3/7P/1PP2PP1/1K1R3R w - - 8 20",
        "r7/b2bk1pp/4pn2/2r1B3/2BNp3/1P5P/2P2PP1/1K1R3R w - - 1 21",
        "rb6/3bk1pp/4pn2/2r1B3/2BNpP2/1P5P/2P3P1/1K1R3R w - - 1 22",
        "1r6/3bk1pp/4pn2/2r5/2BNpP2/1P5P/2P3P1/1K1R3R w - - 0 23",
        "1r6/3bk1p1/4pn1p/2r5/2BNpP2/1P5P/2P3P1/2KR3R w - - 0 24",
        "8/3bk1p1/1r2pn1p/2r5/2BNpP1P/1P6/2P3P1/2KR3R w - - 1 25",
        "8/3bk3/1r2pnpp/2r5/2BNpP1P/1P6/2P3P1/2K1R2R w - - 0 26",
        "2b5/4k3/1r2pnpp/2r5/2BNpP1P/1P4P1/2P5/2K1R2R w - - 1 27",
        "8/1b2k3/1r2pnpp/2r5/2BNpP1P/1P4P1/2P5/2K1R1R1 w - - 3 28",
        "8/1b1nk3/1r2p1pp/2r5/2BNpPPP/1P6/2P5/2K1R1R1 w - - 1 29",
        "8/1b2k3/1r2p1pp/2r1nP2/2BNp1PP/1P6/2P5/2K1R1R1 w - - 1 30",
        "8/1b2k3/1r2p1p1/2r1nPp1/2BNp2P/1P6/2P5/2K1R1R1 w - - 0 31",
        "8/1b2k3/1r2p1n1/2r3p1/2BNp2P/1P6/2P5/2K1R1R1 w - - 0 32",
        "8/1b2k3/1r2p1n1/6r1/2BNp2P/1P6/2P5/2K1R3 w - - 0 33",
        "8/1b2k3/1r2p3/4n1P1/2BNp3/1P6/2P5/2K1R3 w - - 1 34",
        "8/1b2k3/1r2p3/4n1P1/2BN4/1P2p3/2P5/2K4R w - - 0 35",
        "8/1b2k3/1r2p2R/6P1/2nN4/1P2p3/2P5/2K5 w - - 0 36",
        "8/1b2k3/3rp2R/6P1/2PN4/4p3/2P5/2K5 w - - 1 37",
        "8/4k3/3rp2R/6P1/2PN4/2P1p3/6b1/2K5 w - - 1 38",
        "8/4k3/r3p2R/2P3P1/3N4/2P1p3/6b1/2K5 w - - 1 39",
        "8/3k4/r3p2R/2P2NP1/8/2P1p3/6b1/2K5 w - - 3 40",
        "8/3k4/4p2R/2P3P1/8/2P1N3/6b1/r1K5 w - - 1 41",
        "8/3k4/4p2R/2P3P1/8/2P1N3/3K2b1/6r1 w - - 3 42",
        "8/3k4/4p2R/2P3P1/8/2PKNb2/8/6r1 w - - 5 43",
        "8/4k3/4p1R1/2P3P1/8/2PKNb2/8/6r1 w - - 7 44",
        "8/4k3/4p1R1/2P3P1/3K4/2P1N3/8/6rb w - - 9 45",
        "8/3k4/4p1R1/2P1K1P1/8/2P1N3/8/6rb w - - 11 46",
        "8/3k4/4p1R1/2P3P1/5K2/2P1N3/8/4r2b w - - 13 47",
        "8/3k4/2b1p2R/2P3P1/5K2/2P1N3/8/4r3 w - - 15 48",
        "8/3k4/2b1p3/2P3P1/5K2/2P1N2R/8/6r1 w - - 17 49",
        "2k5/7R/2b1p3/2P3P1/5K2/2P1N3/8/6r1 w - - 19 50",
        "2k5/7R/4p3/2P3P1/b1P2K2/4N3/8/6r1 w - - 1 51",
        "2k5/3bR3/4p3/2P3P1/2P2K2/4N3/8/6r1 w - - 3 52",
        "3k4/3b2R1/4p3/2P3P1/2P2K2/4N3/8/6r1 w - - 5 53",
        "3kb3/6R1/4p1P1/2P5/2P2K2/4N3/8/6r1 w - - 1 54",
        "3kb3/6R1/4p1P1/2P5/2P2KN1/8/8/2r5 w - - 3 55",
        "3kb3/6R1/4p1P1/2P1N3/2P2K2/8/8/5r2 w - - 5 56",
        "3kb3/6R1/4p1P1/2P1N3/2P5/4K3/8/4r3 w - - 7 57",
    },
    {
        "rnbq1rk1/ppp1npb1/4p1p1/3P3p/3PP3/2N2N2/PP2BPPP/R1BQ1RK1 b - - 0 8",
        "rnbq1rk1/ppp1npb1/6p1/3pP2p/3P4/2N2N2/PP2BPPP/R1BQ1RK1 b - - 0 9",
        "rn1q1rk1/ppp1npb1/6p1/3pP2p/3P2b1/2N2N2/PP2BPPP/R1BQR1K1 b - - 2 10",
        "r2q1rk1/ppp1npb1/2n3p1/3pP2p/3P2bN/2N5/PP2BPPP/R1BQR1K1 b - - 4 11",
        "r4rk1/pppqnpb1/2n3p1/3pP2p/3P2bN/2N4P/PP2BPP1/R1BQR1K1 b - - 0 12",
        "r4rk1/pppqnpb1/2n3p1/3pP2p/3P3N/7P/PP2NPP1/R1BQR1K1 b - - 0 13",
        "r4rk1/pppq1pb1/2n3p1/3pPN1p/3P4/7P/PP2NPP1/R1BQR1K1 b - - 0 14",
        "r4rk1/ppp2pb1/2n3p1/3pPq1p/3P1N2/7P/PP3PP1/R1BQR1K1 b - - 1 15",
        "r4rk1/pppq1pb1/2n3p1/3pP2p/P2P1N2/7P/1P3PP1/R1BQR1K1 b - - 0 16",
        "r2n1rk1/pppq1pb1/6p1/3pP2p/P2P1N2/R6P/1P3PP1/2BQR1K1 b - - 2 17",
        "r4rk1/pppq1pb1/4N1p1/3pP2p/P2P4/R6P/1P3PP1/2BQR1K1 b - - 0 18",
        "r4rk1/ppp2pb1/4q1p1/3pP1Bp/P2P4/R6P/1P3PP1/3QR1K1 b - - 1 19",
        "r3r1k1/ppp2pb1/4q1p1/3pP1Bp/P2P1P2/R6P/1P4P1/3QR1K1 b - - 0 20",
        "r3r1k1/ppp3b1/4qpp1/3pP2p/P2P1P1B/R6P/1P4P1/3QR1K1 b - - 1 21",
        "r3r1k1/ppp3b1/4q1p1/3pP2p/P4P1B/R6P/1P4P1/3QR1K1 b - - 0 22",
        "r4rk1/ppp3b1/4q1p1/3pP1Bp/P4P2/R6P/1P4P1/3QR1K1 b - - 2 23",
        "r4rk1/pp4b1/4q1p1/2ppP1Bp/P4P2/3R3P/1P4P1/3QR1K1 b - - 1 24",
        "r4rk1/pp4b1/4q1p1/2p1P1Bp/P2p1PP1/3R3P/1P6/3QR1K1 b - - 0 25",
        "r4rk1/pp4b1/4q1p1/2p1P1B1/P2p1PP1/3R4/1P6/3QR1K1 b - - 0 26",
        "r5k1/pp3rb1/4q1p1/2p1P1B1/P2p1PP1/6R1/1P6/3QR1K1 b - - 2 27",
        "5rk1/pp3rb1/4q1p1/2p1P1B1/P2pRPP1/6R1/1P6/3Q2K1 b - - 4 28",
        "5rk1/1p3rb1/p3q1p1/P1p1P1B1/3pRPP1/6R1/1P6/3Q2K1 b - - 0 29",
        "4r1k1/1p3rb1/p3q1p1/P1p1P1B1/3pRPP1/1P4R1/8/3Q2K1 b - - 0 30",
        "4r1k1/5rb1/pP2q1p1/2p1P1B1/3pRPP1/1P4R1/8/3Q2K1 b - - 0 31",
        "4r1k1/5rb1/pq4p1/2p1P1B1/3pRPP1/1P4R1/4Q3/6K1 b - - 1 32",
        "4r1k1/1r4b1/pq4p1/2p1P1B1/3pRPP1/1P4R1/2Q5/6K1 b - - 3 33",
        "4r1k1/1r4b1/1q4p1/p1p1P1B1/3p1PP1/1P4R1/2Q5/4R1K1 b - - 1 34",
        "4r1k1/3r2b1/1q4p1/p1p1P1B1/2Qp1PP1/1P4R1/8/4R1K1 b - - 3 35",
        "4r1k1/3r2b1/4q1p1/p1p1P1B1/2Qp1PP1/1P4R1/5K2/4R3 b - - 5 36",
        "4r1k1/3r2b1/6p1/p1p1P1B1/2Pp1PP1/6R1/5K2/4R3 b - - 0 37",
        "4r1k1/3r2b1/6p1/p1p1P1B1/2P2PP1/3p2R1/5K2/3R4 b - - 1 38",
        "5rk1/3r2b1/6p1/p1p1P1B1/2P2PP1/3p2R1/8/3RK3 b - - 3 39",
        "5rk1/6b1/6p1/p1p1P1B1/2Pr1PP1/3R4/8/3RK3 b - - 0 40",
        "5rk1/3R2b1/6p1/p1p1P1B1/2r2PP1/8/8/3RK3 b - - 1 41",
        "5rk1/3R2b1/6p1/p1p1P1B1/4rPP1/8/3K4/3R4 b - - 3 42",
        "1r4k1/3R2b1/6p1/p1p1P1B1/4rPP1/2K5/8/3R4 b - - 5 43",
        "1r4k1/3R2b1/6p1/p1p1P1B1/2K2PP1/4r3/8/3R4 b - - 7 44",
        "1r3bk1/8/3R2p1/p1p1P1B1/2K2PP1/4r3/8/3R4 b - - 9 45",
        "1r3bk1/8/6R1/2p1P1B1/p1K2PP1/4r3/8/3R4 b - - 0 46",
        "1r3b2/5k2/R7/2p1P1B1/p1K2PP1/4r3/8/3R4 b - - 2 47",
        "5b2/1r3k2/R7/2p1P1B1/p1K2PP1/4r3/8/7R b - - 4 48",
        "5b2/5k2/R7/2pKP1B1/pr3PP1/4r3/8/7R b - - 6 49",
        "5b2/5k2/R1K5/2p1P1B1/p2r1PP1/4r3/8/7R b - - 8 50",
        "8/R4kb1/2K5/2p1P1B1/p2r1PP1/4r3/8/7R b - - 10 51",
        "8/R5b1/2K3k1/2p1PPB1/p2r2P1/4r3/8/7R b - - 0 52",
        "8/6R1/2K5/2p1PPk1/p2r2P1/4r3/8/7R b - - 0 53",
        "8/6R1/2K5/2p1PP2/p2r1kP1/4r3/8/5R2 b - - 2 54",
        "8/6R1/2K2P2/2p1P3/p2r2P1/4r1k1/8/5R2 b - - 0 55",
        "8/5PR1/2K5/2p1P3/p2r2P1/4r3/6k1/5R2 b - - 0 56",
    },
    {
        "rn1qkb1r/p1pbpppp/5n2/8/2pP4/2N5/1PQ1PPPP/R1B1KBNR w KQkq - 0 7",
        "r2qkb1r/p1pbpppp/2n2n2/8/2pP4/2N2N2/1PQ1PPPP/R1B1KB1R w KQkq - 2 8",
        "r2qkb1r/p1pbpppp/5n2/8/1npPP3/2N2N2/1PQ2PPP/R1B1KB1R w KQkq - 1 9",
        "r2qkb1r/p1pb1ppp/4pn2/8/1npPP3/2N2N2/1P3PPP/R1BQKB1R w KQkq - 0 10",
        "r2qk2r/p1pbbppp/4pn2/8/1nBPP3/2N2N2/1P3PPP/R1BQK2R w KQkq - 1 11",
        "r2q1rk1/p1pbbppp/4pn2/8/1nBPP3/2N2N2/1P3PPP/R1BQ1RK1 w - - 3 12",
        "r2q1rk1/2pbbppp/p3pn2/8/1nBPPB2/2N2N2/1P3PPP/R2Q1RK1 w - - 0 13",
        "r2q1rk1/2p1bppp/p3pn2/1b6/1nBPPB2/2N2N2/1P3PPP/R2QR1K1 w - - 2 14",
        "r2q1rk1/4bppp/p1p1pn2/1b6/1nBPPB2/1PN2N2/5PPP/R2QR1K1 w - - 0 15",
        "r4rk1/3qbppp/p1p1pn2/1b6/1nBPPB2/1PN2N2/3Q1PPP/R3R1K1 w - - 2 16",
        "r4rk1/1q2bppp/p1p1pn2/1b6/1nBPPB2/1PN2N1P/3Q1PP1/R3R1K1 w - - 1 17",
        "r3r1k1/1q2bppp/p1p1pn2/1b6/1nBPPB2/1PN2N1P/4QPP1/R3R1K1 w - - 3 18",
        "r3r1k1/1q1nbppp/p1p1p3/1b6/1nBPPB2/1PN2N1P/4QPP1/3RR1K1 w - - 5 19",
        "r3rbk1/1q1n1ppp/p1p1p3/1b6/1nBPPB2/1PN2N1P/3RQPP1/4R1K1 w - - 7 20",
        "r3rbk1/1q3ppp/pnp1p3/1b6/1nBPPB2/1PN2N1P/3RQPP1/4R2K w - - 9 21",
        "2r1rbk1/1q3ppp/pnp1p3/1b6/1nBPPB2/1PN2N1P/3RQPP1/1R5K w - - 11 22",
        "2r1rbk1/1q4pp/pnp1pp2/1b6/1nBPPB2/1PN2N1P/4QPP1/1R1R3K w - - 0 23",
        "2r1rbk1/5qpp/pnp1pp2/1b6/1nBPP3/1PN1BN1P/4QPP1/1R1R3K w - - 2 24",
        "2r1rbk1/5qp1/pnp1pp1p/1b6/1nBPP3/1PN1BN1P/4QPP1/1R1R2K1 w - - 0 25",
        "2r1rbk1/5qp1/pnp1pp1p/1b6/2BPP3/1P2BN1P/n3QPP1/1R1R2K1 w - - 0 26",
        "r3rbk1/5qp1/pnp1pp1p/1b6/2BPP3/1P2BN1P/Q4PP1/1R1R2K1 w - - 1 27",
        "rr3bk1/5qp1/pnp1pp1p/1b6/2BPP3/1P2BN1P/Q4PP1/R2R2K1 w - - 3 28",
        "rr2qbk1/6p1/pnp1pp1p/1b6/2BPP3/1P2BN1P/4QPP1/R2R2K1 w - - 5 29",
        "rr2qbk1/6p1/1np1pp1p/pb6/2BPP3/1P1QBN1P/5PP1/R2R2K1 w - - 0 30",
        "rr2qbk1/6p1/1n2pp1p/pp6/3PP3/1P1QBN1P/5PP1/R2R2K1 w - - 0 31",
        "rr2qbk1/6p1/1n2pp1p/1p1P4/p3P3/1P1QBN1P/5PP1/R2R2K1 w - - 0 32",
        "rr2qbk1/3n2p1/3Ppp1p/1p6/p3P3/1P1QBN1P/5PP1/R2R2K1 w - - 1 33",
        "rr3bk1/3n2p1/3Ppp1p/1p5q/pP2P3/3QBN1P/5PP1/R2R2K1 w - - 1 34",
        "rr3bk1/3n2p1/3Ppp1p/1p5q/1P2P3/p2QBN1P/5PP1/2RR2K1 w - - 0 35",
        "1r3bk1/3n2p1/r2Ppp1p/1p5q/1P2P3/pQ2BN1P/5PP1/2RR2K1 w - - 2 36",
        "1r2qbk1/2Rn2p1/r2Ppp1p/1p6/1P2P3/pQ2BN1P/5PP1/3R2K1 w - - 4 37",
        "1r2qbk1/2Rn2p1/r2Ppp1p/1pB5/1P2P3/1Q3N1P/p4PP1/3R2K1 w - - 0 38",
        "1r2q1k1/2Rn2p1/r2bpp1p/1pB5/1P2P3/1Q3N1P/p4PP1/R5K1 w - - 0 39",
        "1r2q1k1/2Rn2p1/3rpp1p/1p6/1P2P3/1Q3N1P/p4PP1/R5K1 w - - 0 40",
        "2r1q1k1/2Rn2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 1 41",
        "1r2q1k1/1R1n2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 3 42",
        "2r1q1k1/2Rn2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 5 43",
        "1r2q1k1/1R1n2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 7 44",
        "1rq3k1/R2n2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 9 45",
        "2q3k1/Rr1n2p1/3rpp1p/1p6/1P2P3/5N1P/4QPP1/R5K1 w - - 11 46",
        "Rrq3k1/3n2p1/3rpp1p/1p6/1P2P3/5N1P/4QPP1/R5K1 w - - 13 47",
    },
    {
        "rn1qkb1r/1pp2ppp/p4p2/3p1b2/5P2/1P2PN2/P1PP2PP/RN1QKB1R b KQkq - 1 6",
        "r2qkb1r/1pp2ppp/p1n2p2/3p1b2/3P1P2/1P2PN2/P1P3PP/RN1QKB1R b KQkq - 0 7",
        "r2qkb1r/1pp2ppp/p4p2/3p1b2/1n1P1P2/1P1BPN2/P1P3PP/RN1QK2R b KQkq - 2 8",
        "r2qkb1r/1pp2ppp/p4p2/3p1b2/3P1P2/1P1PPN2/P5PP/RN1QK2R b KQkq - 0 9",
        "r2qk2r/1pp2ppp/p2b1p2/3p1b2/3P1P2/1PNPPN2/P5PP/R2QK2R b KQkq - 2 10",
        "r2qk2r/1p3ppp/p1pb1p2/3p1b2/3P1P2/1PNPPN2/P5PP/R2Q1RK1 b kq - 1 11",
        "r2q1rk1/1p3ppp/p1pb1p2/3p1b2/3P1P2/1PNPPN2/P2Q2PP/R4RK1 b - - 3 12",
        "r2qr1k1/1p3ppp/p1pb1p2/3p1b2/3P1P2/1P1PPN2/P2QN1PP/R4RK1 b - - 5 13",
        "r3r1k1/1p3ppp/pqpb1p2/3p1b2/3P1P2/1P1PPNN1/P2Q2PP/R4RK1 b - - 7 14",
        "r3r1k1/1p3ppp/pqp2p2/3p1b2/1b1P1P2/1P1PPNN1/P1Q3PP/R4RK1 b - - 9 15",
        "r3r1k1/1p1b1ppp/pqp2p2/3p4/1b1P1P2/1P1PPNN1/P4QPP/R4RK1 b - - 11 16",
        "2r1r1k1/1p1b1ppp/pqp2p2/3p4/1b1PPP2/1P1P1NN1/P4QPP/R4RK1 b - - 0 17",
        "2r1r1k1/1p1b1ppp/pq3p2/2pp4/1b1PPP2/PP1P1NN1/5QPP/R4RK1 b - - 0 18",
        "2r1r1k1/1p1b1ppp/pq3p2/2Pp4/4PP2/PPbP1NN1/5QPP/R4RK1 b - - 0 19",
        "2r1r1k1/1p1b1ppp/p4p2/2Pp4/4PP2/PqbP1NN1/5QPP/RR4K1 b - - 1 20",
        "2r1r1k1/1p1b1ppp/p4p2/2Pp4/q3PP2/P1bP1NN1/R4QPP/1R4K1 b - - 3 21",
        "2r1r1k1/1p3ppp/p4p2/1bPP4/q4P2/P1bP1NN1/R4QPP/1R4K1 b - - 0 22",
        "2r1r1k1/1p3ppp/p4p2/2PP4/q4P2/P1bb1NN1/R4QPP/2R3K1 b - - 1 23",
        "2r1r1k1/1p3ppp/p2P1p2/2P5/2q2P2/P1bb1NN1/R4QPP/2R3K1 b - - 0 24",
        "2rr2k1/1p3ppp/p2P1p2/2P5/2q2P2/P1bb1NN1/R4QPP/2R4K b - - 2 25",
        "2rr2k1/1p3ppp/p2P1p2/2Q5/5P2/P1bb1NN1/R5PP/2R4K b - - 0 26",
        "3r2k1/1p3ppp/p2P1p2/2r5/5P2/P1bb1N2/R3N1PP/2R4K b - - 1 27",
        "3r2k1/1p3ppp/p2P1p2/2r5/5P2/P1b2N2/4R1PP/2R4K b - - 0 28",
        "3r2k1/1p3ppp/p2P1p2/2r5/1b3P2/P4N2/4R1PP/3R3K b - - 2 29",
        "3r2k1/1p2Rppp/p2P1p2/b1r5/5P2/P4N2/6PP/3R3K b - - 4 30",
        "3r2k1/1R3ppp/p1rP1p2/b7/5P2/P4N2/6PP/3R3K b - - 0 31",
        "3r2k1/1R3ppp/p2R1p2/b7/5P2/P4N2/6PP/7K b - - 0 32",
        "6k1/1R3ppp/p2r1p2/b7/5P2/P4NP1/7P/7K b - - 0 33",
        "6k1/1R3p1p/p2r1pp1/b7/5P1P/P4NP1/8/7K b - - 0 34",
        "6k1/3R1p1p/pr3pp1/b7/5P1P/P4NP1/8/7K b - - 2 35",
        "6k1/5p2/pr3pp1/b2R3p/5P1P/P4NP1/8/7K b - - 1 36",
        "6k1/5p2/pr3pp1/7p/5P1P/P1bR1NP1/8/7K b - - 3 37",
        "6k1/5p2/p1r2pp1/7p/5P1P/P1bR1NP1/6K1/8 b - - 5 38",
        "6k1/5p2/p1r2pp1/b2R3p/5P1P/P4NP1/6K1/8 b - - 7 39",
        "6k1/5p2/p4pp1/b2R3p/5P1P/P4NPK/2r5/8 b - - 9 40",
        "6k1/2b2p2/p4pp1/7p/5P1P/P2R1NPK/2r5/8 b - - 11 41",
        "6k1/2b2p2/5pp1/p6p/3N1P1P/P2R2PK/2r5/8 b - - 1 42",
        "6k1/2b2p2/5pp1/p6p/3N1P1P/P1R3PK/r7/8 b - - 3 43",
        "6k1/5p2/1b3pp1/p6p/5P1P/P1R3PK/r1N5/8 b - - 5 44",
        "8/5pk1/1bR2pp1/p6p/5P1P/P5PK/r1N5/8 b - - 7 45",
        "3b4/5pk1/2R2pp1/p4P1p/7P/P5PK/r1N5/8 b - - 0 46",
        "8/4bpk1/2R2pp1/p4P1p/6PP/P6K/r1N5/8 b - - 0 47",
        "8/5pk1/2R2pP1/p6p/6PP/b6K/r1N5/8 b - - 0 48",
        "8/6k1/2R2pp1/p6P/7P/b6K/r1N5/8 b - - 0 49",
        "8/6k1/2R2p2/p6p/7P/b5K1/r1N5/8 b - - 1 50",
        "8/8/2R2pk1/p6p/7P/b4K2/r1N5/8 b - - 3 51",
        "8/8/2R2pk1/p6p/7P/4NK2/rb6/8 b - - 5 52",
        "2R5/8/5pk1/7p/p6P/4NK2/rb6/8 b - - 1 53",
        "6R1/8/5pk1/7p/p6P/4NK2/1b6/r7 b - - 3 54",
        "R7/5k2/5p2/7p/p6P/4NK2/1b6/r7 b - - 5 55",
        "R7/5k2/5p2/7p/7P/p3N3/1b2K3/r7 b - - 1 56",
        "8/R4k2/5p2/7p/7P/p3N3/1b2K3/7r b - - 3 57",
        "8/8/5pk1/7p/R6P/p3N3/1b2K3/7r b - - 5 58",
        "8/8/5pk1/7p/R6P/p7/4K3/2bN3r b - - 7 59",
        "8/8/5pk1/7p/R6P/p7/4KN1r/2b5 b - - 9 60",
        "8/8/5pk1/7p/R6P/p3K3/1b3N1r/8 b - - 11 61",
        "8/8/R4pk1/7p/7P/p1b1K3/5N1r/8 b - - 13 62",
        "8/8/5pk1/7p/7P/2b1K3/R4N1r/8 b - - 0 63",
        "8/8/5pk1/7p/3K3P/8/R4N1r/4b3 b - - 2 64",
    }
};

struct speedtestsetup
{
};


void speedtest()
{
    int desiredTimeS = 150;
    vector<string> commands;

    cout << "speedtest\n";
    double totalTime = 0;
    for (const auto& game : BenchmarkPositions)
    {
        int ply = 1;
        for (int i = 0; i < static_cast<int>(game.size()); ++i)
        {
            totalTime += 50000.0 / ((double)(ply) + 15.0);
            ply += 1;
        }
    }

    double timeScaleFactor = static_cast<float>(desiredTimeS * 1000) / totalTime;

    for (const auto& game : BenchmarkPositions)
    {
        commands.emplace_back("ucinewgame");
        int ply = 1;
        for (const string fen : game)
        {
            commands.emplace_back("position fen " + fen);
            int correctTime = (int)(timeScaleFactor * 50000.0 / ((double)(ply) + 15.0));
            commands.emplace_back("go movetime " + to_string(correctTime)); 
            ply += 1;
        }
    }

}

#ifdef _WIN32

static void readfromengine(HANDLE pipe, enginestate* es)
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
            const char* s = line.c_str();
            if (strstr(s, "uciok") != NULL && es->phase == 0)
                es->phase = 1;
            if (strstr(s, "readyok") != NULL && es->phase == 1)
                es->phase = 2;
            if (es->phase == 2)
            {
                if (es->doEval)
                {
                    const char* strEval;
                    if ((strEval = strstr(s, "evaluation: ")) || (strEval = strstr(s, "score: ")))
                    {
                        vector<string> scoretoken = SplitString(strEval);
                        if (scoretoken.size() > 1)
                        {
                            try
                            {
                                es->score = int(stof(scoretoken[1]) * 100);
                            }
                            catch (const invalid_argument&) {}
                        }
                        es->phase = 3;
                    }
                }
                else
                {
                    const char* bestmovestr = strstr(s, "bestmove ");
                    const char* pv = strstr(s, " pv ");
                    const char* score = strstr(s, " cp ");
                    const char* mate = strstr(s, " mate ");
                    const char* bmptr = NULL;
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

static BOOL writetoengine(HANDLE pipe, const char* s)
{
    DWORD written;
    return WriteFile(pipe, s, (DWORD)strlen(s), &written, NULL);
}

void testengine(string epdfilename, int startnum, string engineprgs, string logfilename, string comparefilename, int maxtime, int flags)
{
    enginestate es[4];
    enginestate ges;
    string engineprg[4];
    int numEngines = 0;
    string line;
    thread* readThread[4];
    ifstream comparefile;
    bool compare = false;
    char str[256];
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
                en.sthread[0].pos->getFromFen(fenstr.c_str());
                if (en.sthread[0].pos->isCheckbb)
                    continue;
                fenstr = en.sthread[0].pos->toFen();
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

                snprintf(str, 256, "position fen %s 0 1\n%s\n", fenstr.c_str(), doEval ? "eval" : "go infinite");

                bSuccess = writetoengine(g_hChildStd_IN_Wr[i], str);
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


} // namespace