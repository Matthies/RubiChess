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

void engineHeader()
{
    guiCom << "========================================================================================\n";
    guiCom << en.name() + " (Build " + BUILD + ")\n";
    guiCom << "UCI compatible chess engine by " + en.author + "\n";
    guiCom << "----------------------------------------------------------------------------------------\n";
    guiCom << "System: " + cinfo.SystemName() + "\n";
    guiCom << "CPU-Features of system: " + cinfo.PrintCpuFeatures(cinfo.machineSupports) + "\n";
    guiCom << "CPU-Features of binary: " + cinfo.PrintCpuFeatures(cinfo.binarySupports) + "\n";
    guiCom << "========================================================================================\n";
}


//
// callbacks for ucioptions
//
#ifdef _WIN32
static void uciAllowLargePages()
{
    if (!en.Hash)
        return;
    printf("info string Reallocating hash table %s large pages\n", en.allowlargepages ? "using" : "without");
    // This gets a little tricky
    // First reset to old value and free the TT
    en.allowlargepages = !en.allowlargepages;
    tp.setSize(0);

    // Now back to new value and allocate TT
    en.allowlargepages = !en.allowlargepages;
    tp.setSize(en.Hash);
}
#endif

static void uciSetThreads()
{
    en.sizeOfPh = min(128, max(16, en.restSizeOfTp / en.Threads));
    en.allocThreads();
}

static void uciSetHash()
{
    int newRestSizeTp = tp.setSize(en.Hash);
    if (en.restSizeOfTp != newRestSizeTp)
    {
        en.restSizeOfTp = newRestSizeTp;
        uciSetThreads();
    }
}

static void uciClearHash()
{
    tp.clean();
}

static void uciSetSyzygyParam()
{
    // Changing Syzygy related parameters may affect rootmoves filtering
    en.rootposition.useTb = min(TBlargest, en.SyzygyProbeLimit);
    en.rootposition.getRootMoves();
    en.rootposition.tbFilterRootMoves();
    en.prepareThreads();
}

static void uciSetSyzygyPath()
{
    init_tablebases((char*)en.SyzygyPath.c_str());
    if (en.SyzygyPath != "<empty>")
        uciSetSyzygyParam();
}

static void uciSetBookFile()
{
    if (en.BookFile == "<empty>")
    {
        pbook.Open("");
        return;
    }
    if (!pbook.Open(en.BookFile))
        en.BookFile = "<empty>";
}

void uciSetLogFile()
{
    string filename = (en.LogFile == "" ? "" : en.ExecPath + en.LogFile);
    string sLogging;
    if (!guiCom.openLog(filename, en.frequency))
        sLogging = "Cannot open Logfile " + filename;
    else
        sLogging = (filename == "" ? "No logging." : "Logging to " + filename);

    engineHeader();
    guiCom << "info string " + sLogging + "\n";
}

static void uciSetNnuePath()
{
    if (!en.usennue)
    {
        if (NnueReady != NnueDisabled)
            guiCom << "info string NNUE evaluation is disabled.\n";
        NnueReady = NnueDisabled;
        return;
    }

    NnueReady = NnueDisabled;
#ifdef NNUEINCLUDED
    guiCom << "info string Initializing net included in binary...";
    char* p = (char*)&_binary_net_nnue_start;
    if (!NnueReadNet(&p))
        guiCom << " failed. The embedded network seems corrupted.\n";
    else
        guiCom << " successful. Using NNUE evaluation. (" + to_string(NnueReady) + ")\n";
    return;
#else
    string NnueNetPath = en.GetNnueNetPath();
    ifstream is;
    is.open(NnueNetPath, ios::binary);
    if (!is && en.ExecPath != "")
        is.open(en.ExecPath + NnueNetPath, ios::binary);

    if (is && NnueReadNet(&is))
    {
        guiCom << "info string Loading net " + NnueNetPath + " successful. Using NNUE evaluation. (" + to_string(NnueReady) + ")\n";
        if (NnueNetPath.find(NNUEDEFAULTSTR) == string::npos)
            guiCom << "info string Warning! You are not using the default network file. Playing strength of the engine highly depends on it.\n";

        return;
    }

    guiCom << "info string Loading net " + NnueNetPath + " failed. The network file seems corrupted or doesn't exist. Set correct path to network file or disable 'Use_NNUE' for handcrafted evaluation.\n";
#endif
}


engine::engine(compilerinfo *c)
{
    compinfo = c;
#ifdef _WIN32
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    frequency = f.QuadPart;
#else
    frequency = 1000000000LL;
#endif
    rootposition.resetStats();
}

engine::~engine()
{
    ucioptions.Set("SyzygyPath", "<empty>");
    Threads = 0;
    allocThreads();
    rootposition.pwnhsh.remove();
    rootposition.mtrlhsh.remove();
    NnueRemove();
}


void engine::registerOptions()
{
#ifndef NNUEINCLUDED
    ucioptions.Register(&NnueNetpath, "NNUENetpath", ucistring, "<Default>", 0, 0, uciSetNnuePath);
#endif
    ucioptions.Register(&usennue, "Use_NNUE", ucicheck, "true", 0, 0, uciSetNnuePath);
    ucioptions.Register(&LogFile, "LogFile", ucistring, "", 0, 0, uciSetLogFile);
#ifdef _WIN32
    ucioptions.Register(&allowlargepages, "Allow Large Pages", ucicheck, "true", 0, 0, uciAllowLargePages);
#endif
    ucioptions.Register(&Threads, "Threads", ucispin, "1", 1, MAXTHREADS, uciSetThreads);  // order is important as the pawnhash depends on Threads > 0
    ucioptions.Register(&Hash, "Hash", ucispin, to_string(DEFAULTHASH), 1, MAXHASH, uciSetHash);
    ucioptions.Register(&moveOverhead, "Move_Overhead", ucispin, "100", 0, 5000, nullptr);
    ucioptions.Register(&MultiPV, "MultiPV", ucispin, "1", 1, MAXMULTIPV, nullptr);
    ucioptions.Register(&ponder, "Ponder", ucicheck, "false");
    ucioptions.Register(&SyzygyPath, "SyzygyPath", ucistring, "<empty>", 0, 0, uciSetSyzygyPath);
    ucioptions.Register(&Syzygy50MoveRule, "Syzygy50MoveRule", ucicheck, "true", 0, 0, uciSetSyzygyParam);
    ucioptions.Register(&SyzygyProbeLimit, "SyzygyProbeLimit", ucispin, "7", 0, 7, uciSetSyzygyParam);
    ucioptions.Register(&BookFile, "BookFile", ucistring, "<empty>", 0, 0, uciSetBookFile);
    ucioptions.Register(&BookBestMove, "BookBestMove", ucicheck, "true");
    ucioptions.Register(&BookDepth, "BookDepth", ucispin, "255", 0, 255);
    ucioptions.Register(&chess960, "UCI_Chess960", ucicheck, "false");
    ucioptions.Register(nullptr, "Clear Hash", ucibutton, "", 0, 0, uciClearHash);
}


void engine::allocThreads()
{
    // first cleanup the old searchthreads memory
    for (int i = 0; i < oldThreads; i++)
    {
        sthread[i].pos.mtrlhsh.remove();
        sthread[i].pos.pwnhsh.remove();
        sthread[i].pos.~chessposition();
    }

    freealigned64(sthread);

    oldThreads = Threads;

    if (!Threads)
        return;

    size_t size = Threads * sizeof(searchthread);
    myassert(size % 64 == 0, nullptr, 1, size % 64);

    char* buf = (char*)allocalign64(size);
    sthread = new (buf) searchthread[Threads];
    for (int i = 0; i < Threads; i++)
    {
        sthread[i].index = i;
        sthread[i].pos.pwnhsh.setSize(sizeOfPh);
        sthread[i].pos.mtrlhsh.init();
    }
    prepareThreads();
    resetStats();
}


void engine::prepareThreads()
{
    for (int i = 0; i < Threads; i++)
    {
        chessposition *pos = &sthread[i].pos;
        // copy new position to the threads copy but keep old history data
        memcpy((void*)pos, &rootposition, offsetof(chessposition, history));
        pos->threadindex = i;
        // early reset of variables that are important for bestmove selection
        pos->bestmovescore[0] = NOSCORE;
        pos->bestmove = 0;
        pos->nodes = 0;
        pos->nullmoveply = 0;
        pos->nullmoveside = 0;

        pos->accumulator[0].computationState[WHITE] = false;
        pos->accumulator[0].computationState[BLACK] = false;
    }
}


void engine::resetStats()
{
    for (int i = 0; i < Threads; i++)
    {
        chessposition* pos = &sthread[i].pos;
        pos->resetStats();
    }
}


void chessposition::resetStats()
{
    memset(history, 0, sizeof(chessposition::history));
    memset(tacticalhst, 0, sizeof(chessposition::tacticalhst));
    memset(counterhistory, 0, sizeof(chessposition::counterhistory));
    memset(countermove, 0, sizeof(chessposition::countermove));
    memset(conthistptr, 0, sizeof(chessposition::conthistptr));
    for (int i = 0; i < 4; i++)
        prerootconthistptr[i] = counterhistory[0][0];
    he_yes = 0ULL;
    he_all = 0ULL;
    he_threshold = 8100;
}


U64 engine::getTotalNodes()
{
    U64 nodes = 0;
    for (int i = 0; i < Threads; i++)
        nodes += sthread[i].pos.nodes;

    return nodes;
}


void engine::communicate(string inputstring)
{
    string fen;
    vector<string> moves;
    vector<string> commandargs;
    GuiToken command = UNKNOWN;
    size_t ci, cs;
    bool bGetName, bGetValue;
    string sName, sValue;
    bool bMoves;
    bool pendingisready = false;
    bool pendingposition = false;
    do
    {
        if (stopLevel >= ENGINESTOPIMMEDIATELY)
        {
            searchWaitStop();
        }
        if (pendingisready || pendingposition)
        {
            if (pendingposition)
            {
                // new position first stops current search
                if (stopLevel < ENGINESTOPIMMEDIATELY)
                {
                    stopLevel = ENGINESTOPIMMEDIATELY;
                    searchWaitStop();
                }
                if (rootposition.getFromFen(fen.c_str()) < 0)
                {
                    guiCom << "info string Illegal FEN string " + fen + ". Startposition will be used instead.\n";
                    fen = STARTFEN;
                    rootposition.getFromFen(fen.c_str());
                    moves.clear();
                }

                uint32_t lastopponentsmove = 0;
                for (vector<string>::iterator it = moves.begin(); it != moves.end(); ++it)
                {
                    if (!(lastopponentsmove = rootposition.applyMove(*it)))
                        guiCom << "info string Alarm! Move " + (*it)  + "%s illegal (possible engine error)\n";
                }
                rootposition.contempt = S2MSIGN(rootposition.state & S2MMASK) * eps.eContempt;
                ponderhit = (lastopponentsmove && lastopponentsmove == rootposition.pondermove);
                // Preserve hashes of earlier position up to last halfmove counter reset for repetition detection
                rootposition.prerootmovenum = rootposition.ply;
                int i = 0;
                int j = PREROOTMOVES - rootposition.ply;
                while (i < rootposition.ply)
                {
                    rootposition.prerootmovecode[j] = rootposition.movecode[i];
                    rootposition.prerootmovestack[j++] = rootposition.movestack[i++];
                }
                rootposition.lastnullmove = -rootposition.ply - 1;
                rootposition.ply = 0;
                rootposition.useTb = min(TBlargest, en.SyzygyProbeLimit);
                rootposition.getRootMoves();
                rootposition.tbFilterRootMoves();
                prepareThreads();
                if (debug)
                {
                    sthread[0].pos.print();
                }
                pendingposition = false;
            }
            if (pendingisready)
            {
                guiCom << "readyok\n";
                pendingisready = false;
            }
        }
        else {
            commandargs.clear();
            command = parse(&commandargs, inputstring);  // blocking!!
            ci = 0;
            cs = commandargs.size();
            if (en.stopLevel == ENGINESTOPIMMEDIATELY)
                searchWaitStop();
            switch (command)
            {
            case UCIDEBUG:
                if (ci < cs)
                {
                    if (commandargs[ci] == "on")
                        debug = true;
                    else if (commandargs[ci] == "off")
                        debug = false;
#ifdef SDEBUG
                    else if (commandargs[ci] == "this")
                        rootposition.debughash = rootposition.hash;
                    else if (commandargs[ci] == "pv")
                    {
                        int i = 0;
                        moves.clear();
                        while (++ci < cs)
                                moves.push_back(commandargs[ci]);

                        for (vector<string>::iterator it = moves.begin(); it != moves.end(); ++it)
                        {
                            if (!rootposition.applyMove(*it, false))
                            {
                                printf("info string Alarm! Debug PV Zug %s nicht anwendbar (oder Enginefehler)\n", (*it).c_str());
                                continue;
                            }
                            U64 h = rootposition.movestack[rootposition.ply - 1].hash;
                            tp.markDebugSlot(h, i);
                            rootposition.pvmovecode[i++] = rootposition.movecode[rootposition.ply - 1];
                        }
                        rootposition.pvmovecode[i] = 0;
                        while (i--) {
                            uint32_t mc;
                            mc = rootposition.pvmovecode[i];
                            rootposition.unplayMove(mc);
                        }
                        prepareThreads();   // To copy the debug information to the threads position object
                    }
#endif
                }
                break;
            case UCI:
                guiCom << "id name " + name(false) + "\n";
                guiCom << "id author " + author + "\n";
                ucioptions.Print();
                guiCom << "uciok\n";
                break;
            case UCINEWGAME:
                // invalidate hash and history
                tp.clean();
                resetStats();
                sthread[0].pos.lastbestmovescore = NOSCORE;
                pbook.currentDepth = 0;
                break;
            case SETOPTION:
                if (en.stopLevel != ENGINETERMINATEDSEARCH)
                {
                    guiCom << "info string Changing option while searching is not supported. stopLevel = " + to_string(en.stopLevel) + "\n";
                    break;
                }
                bGetName = bGetValue = false;
                sName = sValue = "";
                while (ci < cs)
                {
                    string sLower = commandargs[ci];
                    transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);

                    if (sLower == "name")
                    {
                        if (sName != "")
                            ucioptions.Set(sName, sValue);
                        bGetName = true;
                        bGetValue = false;
                        sName = "";
                    }
                    else if (sLower == "value")
                    {
                        bGetValue = true;
                        bGetName = false;
                        sValue = "";
                    }
                    else if (bGetName)
                    {
                        if (sName != "")
                            sName += " ";
                        sName += commandargs[ci];
                    }
                    else if (bGetValue)
                    {
                        if (sValue != "")
                            sValue += " ";
                        sValue += commandargs[ci];
                    }
                    ci++;
                }
                ucioptions.Set(sName, sValue);
                break;
            case ISREADY:
                pendingisready = true;
                break;
            case POSITION:
                if (cs == 0)
                    break;
                bMoves = false;
                moves.clear();
                fen = "";

                if (commandargs[ci] == "startpos")
                {
                    ci++;
                    fen = STARTFEN;
                }
                else if (commandargs[ci] == "fen")
                {
                    while (++ci < cs && commandargs[ci] != "moves")
                        fen = fen + commandargs[ci] + " ";
                }
                while (ci < cs)
                {
                    if (commandargs[ci] == "moves")
                    {
                        bMoves = true;
                    }
                    else if (bMoves)
                    {
                        moves.push_back(commandargs[ci]);
                    }
                    ci++;
                }

                pendingposition = (fen != "");
                break;
            case GO:
                if (en.usennue && !NnueReady)
                    break;
                pondersearch = NO;
                searchmoves.clear();
                mytime = yourtime = myinc = yourinc = movestogo = mate = maxdepth = 0;
                int *wtime, *btime, *winc, *binc;
                if (rootposition.w2m())
                {
                    wtime = &mytime;
                    winc = &myinc;
                    btime = &yourtime;
                    binc = &yourinc;
                } else {
                    wtime = &yourtime;
                    winc = &yourinc;
                    btime = &mytime;
                    binc = &myinc;
                }
                maxnodes = 0ULL;
                infinite = false;
                while (ci < cs)
                {
                    if (commandargs[ci] == "searchmoves")
                    {
                        while (++ci < cs && AlgebraicToIndex(commandargs[ci]) < 64 && AlgebraicToIndex(&commandargs[ci][2]) < 64)
                            searchmoves.insert(commandargs[ci]);
                        // Filter root moves again
                        rootposition.getRootMoves();
                        rootposition.tbFilterRootMoves();
                        prepareThreads();
                    }

                    else if (commandargs[ci] == "wtime")
                    {
                        if (++ci < cs)
                            *wtime = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "btime")
                    {
                        if (++ci < cs)
                            *btime = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "winc")
                    {
                        if (++ci < cs)
                            *winc = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "binc")
                    {
                        if (++ci < cs)
                            *binc = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "movetime")
                    {
                        mytime = yourtime = 0;
                        if (++ci < cs)
                            myinc = yourinc = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "movestogo")
                    {
                        if (++ci < cs)
                            movestogo = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "nodes")
                    {
                        if (++ci < cs)
                            maxnodes = stoull(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "mate")
                    {
                        if (++ci < cs)
                            mate = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "depth")
                    {
                        if (++ci < cs)
                            maxdepth = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "infinite")
                    {
                        infinite = true;
                        ci++;
                    }
                    else if (commandargs[ci] == "ponder")
                    {
                        pondersearch = PONDERING;
                        ci++;
                    }
                    else
                        ci++;
                }
                searchStart();
                break;
            case WAIT:
                searchWaitStop(false);
                break;
            case PONDERHIT:
                pondersearch = HITPONDER;
                break;
            case STOP:
            case QUIT:
                if (stopLevel < ENGINESTOPIMMEDIATELY)
                    stopLevel = ENGINESTOPIMMEDIATELY;
                break;
            case EVAL:
                en.evaldetails = (ci < cs && commandargs[ci] == "detail");
                sthread[0].pos.getEval<TRACE>();
                break;
            case PERFT:
                if (ci < cs) {
                    try { maxdepth = stoi(commandargs[ci++]); } catch (...) {}
                    perft(max(1, maxdepth), true);
                }
                break;
            case BENCH:
                maxdepth = 0;
                mytime = 0;
                if (ci < cs)
                    try { maxdepth = stoi(commandargs[ci++]); } catch (...) {}
                if (ci < cs)
                    try { mytime = stoi(commandargs[ci++]); } catch (...) {}
                bench(max(0, maxdepth), "", max(0, mytime), 1, true);
                break;
#ifdef NNUELEARN
            case GENSFEN:
                gensfen(commandargs);
                break;
            case CONVERT:
                convert(commandargs);
                break;
            case LEARN:
                learn(commandargs);
                break;
#endif
#ifdef EVALTUNE
            case TUNE:
                parseTune(commandargs);
                break;
#endif
#ifdef EVALOPTIONS
            case EXPORT:
                NnueWriteNet(commandargs);
                break;
#endif
            default:
                break;
            }
        }
    } while (command != QUIT && (inputstring == "" || pendingposition));
    if (command == QUIT)
        searchWaitStop();
}


GuiToken engine::parse(vector<string>* args, string ss)
{
    bool firsttoken = false;

    if (ss == "")
        getline(cin, ss);

    if (cin.eof())
        return QUIT;
    guiCom.fromGui(ss);
    GuiToken result = UNKNOWN;
    istringstream iss(ss);
    for (string s; iss >> s; )
    {
        if (!firsttoken)
        {
            if (GuiCommandMap.find(s.c_str()) != GuiCommandMap.end())
            {
                result = GuiCommandMap.find(s.c_str())->second;
                firsttoken = true;
            }
        }
        else {
            args->push_back(s);
        }
    }

    return result;
}


//
// ucioptions interface
//
void ucioptions_t::Register(void *e, string n, ucioptiontype t, string d, int mi, int ma, void(*setop)(), string v)
{
    string ln = n;
    transform(n.begin(), n.end(), ln.begin(), ::tolower);
    optionmapiterator it = optionmap.find(ln);

    if (it != optionmap.end())
        return;

    ucioption_t u;
    u.name = n;
    u.type = t;
    u.def = d;
    u.min = mi;
    u.max = ma;
    u.varlist = v;
    u.enginevar = e;
    u.setoption = setop;

    it = optionmap.insert(optionmap.end(), pair<string, ucioption_t>(ln , u));

    if (t < ucibutton)
        // Set default value
        Set(n, d, true);
}


void ucioptions_t::Set(string n, string v, bool force)
{
    transform(n.begin(), n.end(), n.begin(), ::tolower);
    optionmapiterator it = optionmap.find(n);

    if (it == optionmap.end())
    {
        // Lets see if whitespace was replaced with _ in the setoption command
        replace(n.begin(), n.end(), '_', ' ');
        it = optionmap.find(n);
        if (it == optionmap.end())
            return;
    }

    ucioption_t *op = &(it->second);
    bool bChanged = false;
    smatch m;
    switch (op->type)
    {
    case ucinnueweight:
        int8_t iVal8;
        try {
            iVal8 = stoi(v);
            if ((bChanged = (iVal8 >= op->min && iVal8 <= op->max && (force || iVal8 != *(int8_t*)(op->enginevar)))))
                *(int8_t*)(op->enginevar) = iVal8;
        }
        catch (...) {}
        break;
    case ucinnuebias:
    case ucispin:
        int iVal;
        try {
            iVal = stoi(v);
            if ((bChanged = (iVal >= op->min && iVal <= op->max && (force || iVal != *(int*)(op->enginevar)))))
                *(int*)(op->enginevar) = iVal;
        }
        catch (...) {}
        break;
    case ucistring:
        if ((bChanged = (force || v != *(string*)op->enginevar)))
            *(string*)op->enginevar = v;
        break;
    case ucicheck:
        transform(v.begin(), v.end(), v.begin(), ::tolower);
        bool bVal;
        bVal = (v == "true");
        if ((bChanged = (force || (bVal != *(bool*)(op->enginevar)))))
            *(bool*)op->enginevar = bVal;
        break;
    case ucicombo:
        break;  // FIXME: to be implemented when Rubi gets first combo option
    case ucibutton:
        bChanged = true;
        break;
#ifdef EVALOPTIONS
    case ucieval:
        int newval;
        try {
            eval oldeval = *(eval*)(op->enginevar);
            eval eVal = oldeval;
            newval = stoi(v);
            if (n.find("_mg") != string::npos)
            {
                eVal = VALUE(newval, GETEGVAL(oldeval));
            }
            if (n.find("_eg") != string::npos)
            {
                eVal = VALUE(GETMGVAL(oldeval), newval);
            }
            if ((bChanged = (force || eVal != oldeval)))
                *(eval*)(op->enginevar) = eVal;
        }
        catch (...) {}
        break;
#endif
#ifdef SEARCHOPTIONS
    case ucisearch:
        int sVal;
        try {
            sVal = stoi(v);
            if ((bChanged = (force || sVal != *(int*)(op->enginevar))))
                *(int*)(op->enginevar) = sVal;
        }
        catch (...) {}
        break;
#endif
    default:
        break;
    }
    if (bChanged && op->setoption) op->setoption();
}


void ucioptions_t::Print()
{
    for (optionmapiterator it = optionmap.begin(); it != optionmap.end(); it++)
    {
        ucioption_t *op = &(it->second);
        string optionStr = "option name " + op->name + " type ";

        switch (op->type)
        {
        case ucinnuebias:
        case ucinnueweight:
        case ucispin:
            guiCom << optionStr + "spin default " + op->def + " min " + to_string(op->min) + " max " + to_string(op->max) + "\n";
            break;
        case ucistring:
            guiCom << optionStr + "string default " + op->def + "\n";
            break;
        case ucicheck:
            guiCom << optionStr + "check default " + op->def + "\n";
            break;
        case ucibutton:
            guiCom << optionStr + "button\n";
            break;
#ifdef EVALOPTIONS
        case ucieval:
            guiCom << optionStr + "string default " + op->def + "\n";
            break;
#endif
#ifdef SEARCHOPTIONS
        case ucisearch:
            guiCom << optionStr + "string default " + op->def << "\n";
            break;
#endif
        case ucicombo:
            break; // FIXME: to be implemented...
        default:
            break;
        }
    }
}



