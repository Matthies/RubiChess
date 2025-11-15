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


int main(int argc, char* argv[])
{
    int startnum;
    int perfmaxdepth;
    bool verbose;
    bool benchmark;
    bool openbench;
    int depth;
    bool enginetest;
    string epdfile;
    string engineprg;
    string logfile;
    string comparefile;
    string genepd;
    int maxtime;
    int flags;

    struct arguments {
        const char *cmd;
        const char *info;
        void* variable;
        char type;
        const char *defaultval;
    } allowedargs[] = {
        { "-uci", "Execute UCI command", NULL, 4, NULL },
        { "-verbose", "Show the parameterlist and actuel values.", &verbose, 0, NULL },
        { "-bench", "Do benchmark test for some positions.", &benchmark, 0, NULL },
        { "bench", "Do benchmark with OpenBench compatible output.", &openbench, 0, NULL },
        { "-depth", "Depth for benchmark (0 for per-position-default)", &depth, 1, "0" },
        { "-perft", "Do performance and move generator testing.", &perfmaxdepth, 1, "0" },
        { "-enginetest", "bulk testing of epd files", &enginetest, 0, NULL },
        { "-epdfile", "the epd file to test (use with -enginetest or -bench)", &epdfile, 2, "" },
        { "-logfile", "output file (use with -enginetest)", &logfile, 2, "enginetest.log" },
        { "-engineprg", "the uci engine to test (use with -enginetest)", &engineprg, 2, "" },
        { "-maxtime", "time for each test in seconds (use with -enginetest or -bench)", &maxtime, 1, "0" },
        { "-startnum", "number of the test in epd to start with (use with -enginetest or -bench)", &startnum, 1, "1" },
        { "-compare", "for fast comparision against logfile from other engine (use with -enginetest)", &comparefile, 2, "" },
        { "-flags", "1=skip easy (0 sec.) compares; 2=break 5 seconds after first find; 4=break after compare time is over; 8=eval only (use with -enginetest)", &flags, 1, "0" },
        { "-option", "Set UCI option by commandline", NULL, 3, NULL },
        { "-generate", "Generates epd file with n (default 1000) random endgame positions of given type; format: egstr/n ", &genepd, 2, "" },
#ifdef STACKDEBUG
        { "-assertfile", "output assert info to file", &en.assertfile, 2, "" },
#endif
#ifdef EVALTUNE
        { "-pgnfile", "converts games in a PGN file to fen; use with -depth parameter (-1: position from pgn, >=0: do a (q)search and apply pv", &pgnfilename, 2, "" },
        { "-ppg", "use only <n> positions per game (0 = every position, use with -pgnfile)", &ppg, 1, "0" },
        { "-fentuning", "reads FENs from files (filenames separated by *) and tunes eval parameters against it", &fentuningfiles, 2, "" },
        { "-quietonly", "don't do qsearch (when used with -fentuning)", &quietonly, 0, NULL },
        { "-correlation", "calculate correlation of parameters to the give list (seperated by *), use with -fentuning)", &correlationlist, 2, "" },
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

    en.ExecPath = "";
    if (argc > 0)
    {
        // Get path of the executable
        string execPath = argv[0];
        size_t si;
        if ((si = execPath.rfind('/')) != string::npos || (si = execPath.rfind('\\')) != string::npos)
            en.ExecPath = execPath.substr(0, si + 1);
    }

    initBitmaphelper();
    NnueInit();

    en.registerOptions();

#ifdef EVALOPTIONS
    registerallevals();
#endif

#ifdef EVALTUNE
    tuneInit();
#endif

    engineHeader();
    searchinit();

    cout.setf(ios_base::unitbuf);

    vector<string> ucicmds;
    ucicmds.push_back("position startpos");
    int paramindex = 1;
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
            paramindex = 1;
            break;
        case 1:
            try { *(int*)(allowedargs[j].variable) = stoi((val > 0 && val < argc - 1 ? argv[val + 1] : allowedargs[j].defaultval)); }
            catch (const invalid_argument&) {}
            if (verbose) printf(" %s: %d  (%s)\n", allowedargs[j].cmd, *(int*)(allowedargs[j].variable), allowedargs[j].info);
            paramindex = 1;
            break;
        case 2:
            *(string*)(allowedargs[j].variable) = (val > 0 && val < argc - 1 ? argv[val + 1] : allowedargs[j].defaultval);
            if (verbose) printf(" %s: %s  (%s)\n", allowedargs[j].cmd, (*(string*)(allowedargs[j].variable)).c_str(), allowedargs[j].info);
            paramindex = 1;
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
                paramindex = 1;
            }
            break;
        case 4:
            if (val > 0 && val < argc - 1)
            {
                while (++val < argc && argv[val][0] != '-')
                    ucicmds.push_back(argv[val]);
            }
            else {
                ucicmds.push_back("");
            }
            paramindex = 1;
            break;
        }
    }

    if (perfmaxdepth)
    {
        // do a perft test
        perftest(perfmaxdepth);
    } else if (benchmark || openbench)
    {
        en.bench(depth, epdfile, maxtime, startnum, openbench);
        if (!openbench && epdfile == "")
        {
            NnueType oldNnueReady = NnueReady;
            // Profile build; switch eval mode for more recording
            en.ucioptions.Set("Use_NNUE", NnueReady ? "false" : "true");
            if (NnueReady || oldNnueReady)
                en.bench(depth, epdfile, maxtime, startnum, openbench);
        }
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
    else if (pgnfilename != "")
    {
        PGNtoFEN(depth);
    }
    else if (fentuningfiles != "")
    {
        getCoeffsFromFen(fentuningfiles);
        TexelTune();
    }
#endif
    else {
        // usual uci mode
        for (auto it = ucicmds.begin(); it != ucicmds.end(); it++)
            en.communicate(*it);
    }

#ifdef EVALTUNE
    tuneCleanup();
#endif
    en.searchWaitStop();
    return 0;
}
