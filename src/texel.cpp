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

//
// This file contains code related to Texel tuning and pgn/fen manipulating tools
//

#include "RubiChess.h"

#ifdef EVALTUNE

alignas(64) evalparamset epsdefault;

chessposition pos;
U64 fenWritten;

string pgnfilename;
string fentuningfiles;
bool quietonly;
bool balancedonly;
string correlationlist;
int ppg;
tunerpool tpool;


#define MAXFENS 256
string fenlines[MAXFENS];
U64 fenhash[MAXFENS];
int8_t fenresult[MAXFENS];
int fenscore[MAXFENS];

// Variation of TT to detect non-unique positions in the pgn
struct poshashentry {
    U64 hash;
    int num;
};


#define POSHASHSIZEMB  1024 // max size for poshash in MB
U64 poshashsize;
U64 poshashmask;
poshashentry* poshash;

static void allocPoshash()
{
    U64 maxsize = ((U64)POSHASHSIZEMB << 20) / sizeof(poshashentry);
    int msb;
    GETMSB(msb, maxsize);
    poshashsize = (1ULL << msb);
    poshashmask = poshashsize - 1;
    size_t allocsize = (size_t)(poshashsize * sizeof(poshashentry));
    poshash = (poshashentry*)allocalign64(allocsize);
    if (!poshash)
    {
        printf("Error allocating poshash memory.\n");
        return;
    }
    memset(poshash, 0, allocsize);
}

int addPos(U64 h)
{
    U64 index = h & poshashmask;
    poshashentry* entry = poshash + index;
    if (!entry->hash || entry->hash != h)
    {
        entry->hash = h;
        entry->num = 1;
        return 1;
    }
    return ++entry->num;
}


static void writeFenToFile(ofstream* fenfile, int gamepositions)
{
    int fentowrite = (ppg ? ppg : gamepositions);

    int i = 0;
    while (gamepositions && fentowrite)
    {
        if (ppg)
            i = rand() % gamepositions;

        U64 h = fenhash[i];
        int num = addPos(h);

        if (num == 1)
        {
            int result = fenresult[i];
            *fenfile << fenlines[i] << (result == 0 ? " 1/2" : (result > 0 ? " 1-0" : " 0-1")) << " " << fenscore[i] <<  "\n";
            fentowrite--;
            fenWritten++;
        }

        if (ppg)
            fenlines[i] = fenlines[gamepositions];
        else
            i++;
        gamepositions--;
    }
}


const int minMaterial = 7;
const int maxPly = 200;

bool PGNtoFEN(int depth)
{
    pos.tps.count = 0;
    pos.pwnhsh.setSize(1);
    pos.mtrlhsh.init();

    int gamescount = 0;
    fenWritten = 0ULL;
    bool mateFound = false;
    string line, newline;
    string line1, line2;
    string rest_of_last_line = "";
    string fenfilename = pgnfilename + ".d" + to_string(depth) + ".fen";

    // The following is Windows-only; FIXME: C++--17 offers portable filesystem handling
    WIN32_FIND_DATA fd;
    HANDLE pgnhandle = INVALID_HANDLE_VALUE;
    bool folderMode = (GetFileAttributes(pgnfilename.c_str()) & FILE_ATTRIBUTE_DIRECTORY);
    if (folderMode)
    {
        if (pgnfilename.substr(pgnfilename.length() - 1, 1) != "\\")
            pgnfilename += "\\";

        pgnhandle = FindFirstFile((pgnfilename + "*.pgn").c_str(), &fd);
        if (pgnhandle == INVALID_HANDLE_VALUE)
            return false;
    }

    ofstream fenfile(fenfilename);
    if (!fenfile.is_open())
    {
        printf("Cannot open %s for writing.\n", fenfilename.c_str());
        return false;
    }

    allocPoshash();

    do
    {
        string pgnfullname = (folderMode ? pgnfilename + fd.cFileName : pgnfilename);
        // Open the pgn file for reading
        ifstream pgnfile(pgnfullname);

        if (!pgnfile.is_open())
        {
            printf("Cannot open %s for reading.\n", pgnfullname.c_str());
            return false;
        }
        printf("Reading %s ...\n", pgnfullname.c_str());
        int newgamestarts = 0;
        int result = 0;
        string newfen;
        string lastmove;
        //bool valueChecked = true;;
        int gamepositions = 0;
        string scoreBracket;
        while (getline(pgnfile, newline))
        {
            line2 = line1;
            line1 = newline;
            line = line + newline;

            smatch match;
            string fen;
            int score;
            // We assume that the [Result] section comes first, then the [FEN] section
            if (regex_search(line, match, regex("\\[Result\\s+\"(.*)\\-(.*)\"")))
            {
                gamescount++;
                if (gamepositions)
                    writeFenToFile(&fenfile, gamepositions);
                gamepositions = 0;
                // Write last game
                if (match.str(1) == "0")
                    result = -1;
                else if (match.str(1) == "1")
                    result = 1;
                else
                    result = 0;
                newgamestarts = 1;
                mateFound = false;
            }
            bool fenFound;
            if (newgamestarts == 1 &&
                ((fenFound = regex_search(line, match, regex("\\[FEN\\s+\"(.*)\"")))
                    || !regex_search(line, match, regex("\\["))))
            {
                newfen = fenFound ? match.str(1) : STARTFEN;
                newgamestarts++;
                fen = newfen;
                cout << "Reading game " << gamescount << "\n";
                pos.getFromFen(fen.c_str());
                pos.resetStats();
                pos.ply = 0;
            }
            // Don't export games that were lost on time or by stalled connection
            if (regex_search(line, match, regex("\\[Termination\\s+\".*(forfeit|stalled|unterminated).*\"")))
            {
                printf("Skip this match: %s\n", line.c_str());
                newgamestarts = 0;
            }

            if (regex_search(line, match, regex("^\\[.*\\]$")))
                line = "";

            // search for the moves
            if (newgamestarts == 2 && !regex_search(line, match, regex("^\\[.*\\]$")))
            {
                bool foundInLine;
                do
                {
                    if ((foundInLine = regex_search(line, match, regex("^(\\s*\\d+\\.\\s*)"))))
                    {
                        // skip move number
                        line = match.suffix();
                    }
                    else if ((foundInLine = regex_search(line, match, regex("^\\s*(([O\\-]{3,5})\\+?|([KQRBN]?[a-h]*[1-8]*x?[a-h]+[1-8]+(=[QRBN])?)\\+?)"))))
                    {
                        // Found move
                        if (depth >= 0)
                        {
                            // AGE mode (search and apply the pv of this search)
                            score = pos.alphabeta(SCOREBLACKWINS, SCOREWHITEWINS, depth);
                            int s2m = pos.state & S2MMASK;
                            uint32_t* pvt = pos.pvtable[pos.ply];
                            int num = pos.applyPv(pvt);
                            if ((pos.state & S2MMASK) != s2m)
                                score = -score;
                            fen = pos.toFen();
                            fenlines[gamepositions] = fen;
                            fenhash[gamepositions] = pos.hash;
                            fenscore[gamepositions] = score;
                            fenresult[gamepositions++] = result;
                            pos.reapplyPv(pvt, num);
                        }
                        else
                        {
                            // Just save current position with the score in the fen
                            fen = pos.toFen();
                            fenlines[gamepositions] = fen;
                            fenhash[gamepositions] = pos.hash;
                            fenscore[gamepositions] = NOSCORE;
                            fenresult[gamepositions++] = result;
                        }
                        lastmove = AlgebraicFromShort(match.str(1), &pos);
                        if (lastmove == "" || !pos.applyMove(lastmove))
                        {
                            printf("Alarm (game %d): %s\n", gamescount, match.str(1).c_str());
                            pos.print();
                            printf("last Lines:\n%s\n%s\n\n", line2.c_str(), line1.c_str());
                        }
                        line = match.suffix();
                    }
                    else if ((foundInLine = regex_search(line, match, regex("\\s*(\\{[^\\}]*\\})"))))
                    {
                        scoreBracket = match.str(1);
                        line = match.suffix();

                        if (depth < 0 )
                        {
                            // Use the score from the fen file
                            string scorestr;
                            bool foundValue;
                            if ((foundValue = regex_search(scoreBracket, match, regex("\\{(\\(.*\\))*(\\+|\\-)?(M?)(\\d+(\\.?)\\d*)"))))
                            {
                                // cutechess pgn
                                scorestr = match.str(2) + match.str(4);
                                double dScore = stod(scorestr);
                                if (match.str(5) == ".")
                                    dScore *= 100;
                                score = int(dScore);
                                // Only output if no mate score detected
                                if (match.str(3) == "M" || abs(score) >= 3000)
                                    mateFound = true;
                            }
                            else if ((foundValue = regex_search(scoreBracket, match, regex("(\\(.*\\))*(eval\\s+)([\\+\\-]?)(M?)(\\d+(\\.?)\\d*)"))))
                            {
                                // CCRL pgn
                                scorestr = match.str(3) + match.str(5);
                                score = stoi(scorestr);
                                // Only output if no mate score detected
                                if (match.str(4) == "M" || abs(score) >= 3000)
                                    mateFound = true;
                            }
                            if (foundValue)
                            {
                                foundInLine = true;
                                if (!mateFound && fen != "")
                                {
                                    fenscore[gamepositions - 1] = score;
                                }
                            }
                        }
                    }
                } while (foundInLine);
            }

            if (newgamestarts == 2)
            {
                // Test for early exit of the game
                if (pos.ply > maxPly)
                    // Too many plies; abort game
                    newgamestarts = 3;

                int material = POPCOUNT(pos.piece00[0] | pos.piece00[1]);
                if (material < minMaterial)
                    newgamestarts = 3;
            }


            if (newgamestarts != 2)
                // not in moves section delete rest of line
                line = "";
        }
        if (gamepositions)
            writeFenToFile(&fenfile, gamepositions);

        printf("Position so far:  %9lld\n", fenWritten);

    } while (folderMode && FindNextFile(pgnhandle, &fd));

    return true;
}



static string getValueStringValue(eval* e)
{
    if (e->type == 0)
    {
        string sm = to_string(GETMGVAL(*e));
        if (sm.length() < 4)
            sm.insert(sm.begin(), 4 - sm.length(), ' ');
        string se = to_string(GETEGVAL(*e));
        if (se.length() < 4)
            se.insert(se.begin(), 4 - se.length(), ' ');
        return "VALUE(" + sm + "," + se + ")";
    }
    else if (e->type == 1)
    {
        string si = to_string(*e);
        if (si.length() < 4)
            si.insert(si.begin(), 4 - si.length(), ' ');
        return "CVALUE(" + si + ")";
    }
    else if (e->type == 2)
    {
        string si = to_string(e->groupindex);
        if (si.length() < 4)
            si.insert(si.begin(), 4 - si.length(), ' ');
        string sv = to_string(e->v);
        if (sv.length() < 4)
            sv.insert(sv.begin(), 4 - sv.length(), ' ');
        return "SQVALUE(" + si + "," + sv + ")";
    }
    else
    {
        string si = to_string(*e);
        if (si.length() < 4)
            si.insert(si.begin(), 4 - si.length(), ' ');
        return "EVALUE(" + si + ")";
    }
}


static string nameTunedParameter(chessposition* p, int i)
{
    string name = p->tps.name[i];
    if (p->tps.bound2[i] > 0)
    {
        name += "[" + to_string(p->tps.index2[i]) + "][" + to_string(p->tps.index1[i]) + "]";
    }
    else if (p->tps.bound1[i] > 0)
    {
        name += "[" + to_string(p->tps.index1[i]) + "]";
    }
    return name;
}


static void printTunedParameters(chessposition* p)
{
    string lastname = "";
    string output = "";
    for (int i = 0; i < p->tps.count; i++)
    {
        if (lastname != p->tps.name[i])
        {
            if (output != "")
            {
                output += ";\n";
                printf("%s", output.c_str());
                output = "";
            }
            lastname = p->tps.name[i];
            output = "    eval " + lastname;
            if (p->tps.bound2[i] > 0)
            {
                output += "[" + to_string(p->tps.bound2[i]) + "][" + to_string(p->tps.bound1[i]) + "] = {\n        { ";
            }
            else if (p->tps.bound1[i] > 0)
            {
                output += "[" + to_string(p->tps.bound1[i]) + "] = { ";
            }
            else {
                output += " = ";
            }
        }

        output = output + " " + getValueStringValue(p->tps.ev[i]);

        if (p->tps.index1[i] < p->tps.bound1[i] - 1)
        {
            output += ",";
            if (!((p->tps.index1[i] + 1) & (p->tps.bound2[i] ? 0x7 : 0x7)))
                output += "\n          ";
        }
        else if (p->tps.index1[i] == p->tps.bound1[i] - 1)
        {
            output += "  }";
            if (p->tps.index2[i] < p->tps.bound2[i] - 1)
                output += ",\n        { ";
            else if (p->tps.index2[i] == p->tps.bound2[i] - 1)
                output += "\n    }";
        }
    }
    output += ";\n";
    printf("%s", output.c_str());
}


int tuningratio = 1;
char* texelpts = NULL;
U64 texelptsnum;
int iThreads;
sqevallist sqglobal;

void chessposition::resetTuner()
{
    for (int i = 0; i < tps.count; i++)
        tps.ev[i]->resetGrad();
}

void chessposition::getPositionTuneSet(positiontuneset* p, evalparam* e)
{
    p->ph = ph;
    p->sc = sc;
    p->num = 0;
    for (int i = 0; i < tps.count; i++)
        if (tps.ev[i]->getGrad() || (tps.ev[i]->type == 2 && tps.ev[i]->getGrad(1)))
        {
            e->index = i;
            e->g[0] = tps.ev[i]->getGrad(0);
            e->g[1] = tps.ev[i]->getGrad(1);
            p->num++;
            e++;
        }
}

void chessposition::copyPositionTuneSet(positiontuneset* from, evalparam* efrom, positiontuneset* to, evalparam* eto)
{
    to->ph = from->ph;
    to->sc = from->sc;
    to->num = from->num;
    for (int i = 0; i < from->num; i++)
    {
        *eto = *efrom;
        eto++;
        efrom++;
    }
}

string chessposition::getCoeffString()
{
    string s = "";
    for (int i = 0; i < pts.num; i++)
    {
        if (tps.ev[i]->type != 2)
            s = s + tps.name[ev[i].index] + "(" + to_string(ev[i].g[0]) + ") ";
        else
            s = s + tps.name[ev[i].index] + "(" + to_string(ev[i].g[0]) + "/" + to_string(ev[i].g[1]) + ") ";
    }

    return s;
}


static void copyParams(chessposition* p, tuner* tn)
{
    for (int i = 0; i < p->tps.count; i++)
        tn->ev[i] = *p->tps.ev[i];
    tn->paramcount = p->tps.count;
}


static int getValueByCoeff(eval* ev, positiontuneset* p, evalparam* e, precalculated *precalc, bool debug = false, int corParam = -1)
{
    int v = 0;
    int complexity = 0;
    int sqsum[4][2] = { { 0 } };
    if (precalc)
    {
        precalc->v = 0;
        precalc->vi = 0;
        precalc->g = 0;
    }
    for (int i = 0; i < p->num; i++, e++)
    {
        if (corParam >= 0 && corParam != e->index)
            continue;
        int type = ev[e->index].type;
        if (debug)
            printf("%30s ", nameTunedParameter(&pos, e->index).c_str());
        if (type <= 1)
        {
            if (precalc && precalc->index == e->index)
            {
                precalc->g = e->g[0];
                precalc->vi = ev[e->index];
            }

            v += ev[e->index] * e->g[0];

            if (debug)
                printf(" %08x * %3d = %08x \n", ev[e->index].v, e->g[0], ev[e->index] * e->g[0]);
        }
        else if (type == 2)
        {
            int sqindex = ev[e->index].groupindex;
            sqsum[sqindex][0] += ev[e->index] * e->g[0];
            sqsum[sqindex][1] += ev[e->index] * e->g[1];
            if (debug) {
                printf(" sq0: %d * %d = %d \n", ev[e->index].v, e->g[0], ev[e->index] * e->g[0]);
                printf(" sq1: %d * %d = %d \n", ev[e->index].v, e->g[1], ev[e->index] * e->g[1]);
            }
        }
        else {
            complexity += ev[e->index] * e->g[0];
            if (debug)
                printf(" Compl: %08x * %3d = %08x \n", ev[e->index].v, e->g[0], ev[e->index] * e->g[0]);
        }
    }
    if (debug)
        printf("linear total: %08x\n", v);
    for (int i = 0; i < 4; i++)
    {
        if (sqsum[i][0] == 0 && sqsum[i][1] == 0) continue;
        v += SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1);
        if (debug)
            printf("sq total: %3d + %3d = %3d\n", SQRESULT(sqsum[i][0], 0), SQRESULT(sqsum[i][1], 1), SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1));
    }

    if (precalc) {
        precalc->complexity = complexity;
        precalc->v = v - precalc->g * precalc->vi;
    }

    // compelexity
    int evaleg = GETEGVAL(v);
    int sign = (evaleg > 0) - (evaleg < 0);
    v += sign * max(complexity, -abs(evaleg));
    if (debug)
        printf("total: %08x\n", v);

    return v;
}


double texel_k = 1.121574;

static double TexelEvalErrorDiff(tuner* tn, precalculated* precalc)
{
    double Ri;
    double E = 0.0;
    int Qi;

    positiontuneset* p = (positiontuneset*)texelpts;
    for (U64 i = 0; i < texelptsnum; i++)
    {
        evalparam* e = (evalparam*)((char*)p + sizeof(positiontuneset));
        precalculated* prec = precalc + i;
        int index = prec->index;

        Ri = p->R / 2.0;
        if (p->sc == SCALE_DRAW)
        {
            Qi = SCOREDRAW;
        }
        else
        {
            int v = prec->v + prec->g * (int)tn->ev[index];
            int evaleg = GETEGVAL(v);
            int sign = (evaleg > 0) - (evaleg < 0);
            v += sign * max(prec->complexity, -abs(evaleg));
            Qi = TAPEREDANDSCALEDEVAL(v, p->ph, p->sc);
        }
        double sigmoid = 1 / (1 + pow(10.0, -texel_k * Qi / 400.0));
        E += (Ri - sigmoid) * (Ri - sigmoid);
        p = (positiontuneset*)((char*)p + sizeof(positiontuneset) + p->num * sizeof(evalparam));
    }

    return E / texelptsnum;
}


static double TexelEvalError(tuner* tn, precalculated* precalc, int index = -1, double k = texel_k)
{
    double Ri;
    double E = 0.0;
    int Qi;

    precalculated* prec = precalc;
    positiontuneset* p = (positiontuneset*)texelpts;
    for (U64 i = 0; i < texelptsnum; i++)
    {
        evalparam* e = (evalparam*)((char*)p + sizeof(positiontuneset));
        if (precalc)
        {
            prec = precalc + i;
            prec->index = index;
        }
 
        Ri = p->R / 2.0;
        if (p->sc == SCALE_DRAW)
            Qi = SCOREDRAW;
        else
            Qi = TAPEREDANDSCALEDEVAL(getValueByCoeff(tn->ev, p, e, prec), p->ph, p->sc);
        double sigmoid = 1 / (1 + pow(10.0, -k * Qi / 400.0));
        E += (Ri - sigmoid) * (Ri - sigmoid);
        p = (positiontuneset*)((char*)p + sizeof(positiontuneset) + p->num * sizeof(evalparam));
    }

    return E / texelptsnum;
}



static void optk()
{
    printf("Finding optimal tuning constant k for this position set...\n");
    tuner* tn = &tpool.tn[0];
    copyParams(&pos, tn);
    double E[2];
    double Emin, Error;
    double bound[2] = { 0.0, 10.0 };
    double x, lastx;
    int direction = 0;
    //double delta;
    lastx = (bound[0] + bound[1]) / 2;

    E[0] = TexelEvalError(tn, nullptr, -1, bound[0]);
    E[1] = TexelEvalError(tn, nullptr, -1, bound[1]);
    Emin = TexelEvalError(tn, nullptr, -1, lastx);
    if (Emin > E[0] && Emin > E[1])
    {
        printf("Tuning Error! Wrong bounds. E0=%0.10f  E1=%0.10f  Ed=%0.10f\n", E[0], E[1], Emin);
        return;
    }

    while (bound[1] - bound[0] > 0.00001)
    {
        x = (lastx + bound[direction]) / 2;
        Error = TexelEvalError(tn, nullptr, -1, x);
        printf("Tuningscore b0=%0.10f (%0.10f) b1=%0.10f (%0.10f)\n", bound[0], E[0], bound[1], E[1]);
        if (Error > Emin)
        {
            bound[direction] = x;
            E[direction] = Error;
        }
        else {
            E[1 - direction] = Emin;
            bound[1 - direction] = lastx;
            lastx = x;
            Emin = Error;
        }
        direction = 1 - direction;
    }
    printf("Best k for this tuning set changed from %0.10f to %0.10f\n", texel_k, (bound[0] + bound[1]) / 2.0);
    texel_k = (bound[0] + bound[1]) / 2.0;
}


// parser for the most common fen files with tuning information
void getCoeffsFromFen(string fenfilenames)
{
    int n = 0;
    while (fenfilenames != "")
    {
        size_t spi = fenfilenames.find('*');
        string filename = (spi == string::npos) ? fenfilenames : fenfilenames.substr(0, spi);
        fenfilenames = (spi == string::npos) ? "" : fenfilenames.substr(spi + 1, string::npos);

        int fentype = -1;
        string line;
        smatch match;
        int c;
        int bw;
        char R;
        string fen;
        int Qi, Qr;
        int Q[4];
        U64 buffersize;
        char* pnext;
        long long minfreebuffer = sizeof(positiontuneset) + NUMOFEVALPARAMS * sizeof(evalparam) * 1024;
        int msb;
        GETMSB(msb, minfreebuffer);
        minfreebuffer = (1ULL << (msb + 1));
        const U64 maxbufferincrement = minfreebuffer << 10;

        bw = 0;
        c = tuningratio;
        ifstream fenfile(filename);
        if (!fenfile.is_open())
        {
            printf("\nCannot open %s for reading.\n", filename.c_str());
            continue;
        }
        buffersize = minfreebuffer;
        if (texelpts)
            free(texelpts);
        texelpts = (char*)malloc(buffersize);
        pnext = (char*)texelpts;
        printf("\nReading positions from %s\n", filename.c_str());
        while (getline(fenfile, line))
        {
            if (texelpts + buffersize - pnext < minfreebuffer)
            {
                buffersize = min(buffersize + maxbufferincrement, buffersize * 2);
                char* oldtexelpts = texelpts;
                texelpts = (char*)realloc(texelpts, buffersize);
                pnext += (texelpts - oldtexelpts);
            }
            fen = "";
            R = 1; // draw as default
            if ((fentype < 0 || fentype == 1) && regex_search(line, match, regex("(.*)#(.*)#(.*)")))
            {
                // my own fen format
                if (fentype < 0)
                {
                    printf("Format: score#fen#eval (from pgn2fen)\n");
                    fentype = 1;
                }
                fen = match.str(2);
                R = (stoi(match.str(1)) + 1);
            }
            else if ((fentype < 0 || fentype == 2) && regex_search(line, match, regex("(.*)\\s+((1\\-0)|(0\\-1)|(1/2))")))
            {
                // FENS_JEFFREY
                if (fentype < 0)
                {
                    printf("Format: fen 1-0|0-1|1/2 (from FENS_JEFFREY)\n");
                    fentype = 2;
                }
                fen = match.str(1);
                R = (match.str(2) == "1-0" ? 2 : (match.str(2) == "0-1" ? 0 : 1));
            }
            else if ((fentype < 0 || fentype == 3) && regex_search(line, match, regex("(.*)\\s+c9\\s+\"((1\\-0)|(0\\-1)|(1/2))")))
            {
                // quiet-labled (zurichess)
                if (fentype < 0)
                {
                    printf("Format: fen c9 \"1-0|0-1|1/2\" (from quiet-labled)\n");
                    fentype = 3;
                }
                fen = match.str(1);
                R = (match.str(2) == "1-0" ? 2 : (match.str(2) == "0-1" ? 0 : 1));
            }
            else if ((fentype < 0 || fentype == 4) && regex_search(line, match, regex("(.*)\\s+c1(.*)c2\\s+\"((1.0)|(0.5)|(0.0))")))
            {
                // big3
                if (fentype < 0)
                {
                    printf("Format: fen c1 ... c2 \"1.0|0.5|0.0\" (from big3)\n");
                    fentype = 4;
                }
                fen = match.str(1);
                R = (match.str(3) == "1.0" ? 2 : (match.str(3) == "0.0" ? 0 : 1));
            }
            else if ((fentype < 0 || fentype == 5) && regex_search(line, match, regex("(.*)\\|(.*)")))
            {
                // lichess
                if (fentype < 0)
                {
                    printf("Format: fen |White|Black|Draw (from lichess-quiet)\n");
                    fentype = 5;
                }
                fen = match.str(1);
                R = (match.str(2) == "White" ? 2 : (match.str(2) == "Black" ? 0 : 1));
            }
            else if ((fentype < 0 || fentype == 6) && regex_search(line, match, regex("(.*)\\s+\\[((1.0)|(0.5)|(0.0))\\]")))
            {
                // Ethereal book
                if (fentype < 0)
                {
                    printf("Format: fen [0.0|0.5|1.0] (from Ethereal book)\n");
                    fentype = 6;
                }
                fen = match.str(1);
                R = (match.str(2) == "1.0" ? 2 : (match.str(2) == "0.0" ? 0 : 1));
            }
            if (fen != "")
            {
                bw = 1 - bw;
                if (bw)
                    c++;
                if (c > tuningratio)
                    c = 1;
                if (c == tuningratio)
                {
                    pos.getFromFen(fen.c_str());
                    pos.ply = 0;
                    Qi = pos.getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                    if (!pos.w2m())
                        Qi = -Qi;

                    // Skip positions with mate score
                    if (MATEDETECTED(Qi))
                        continue;

                    // Skip positions inside tablebase 6 pieces
                    if (POPCOUNT(pos.occupied00[0] | pos.occupied00[1]) <= 6)
                        continue;

                    if (balancedonly &&
                        (POPCOUNT(pos.piece00[WPAWN]) != POPCOUNT(pos.piece00[BPAWN])
                            || POPCOUNT(pos.piece00[WKNIGHT]) != POPCOUNT(pos.piece00[BKNIGHT])
                            || POPCOUNT(pos.piece00[WBISHOP]) != POPCOUNT(pos.piece00[BBISHOP])
                            || POPCOUNT(pos.piece00[WROOK]) != POPCOUNT(pos.piece00[BROOK])
                            || POPCOUNT(pos.piece00[WQUEEN]) != POPCOUNT(pos.piece00[BQUEEN])))
                        continue;

                    positiontuneset* nextpts = (positiontuneset*)pnext;
                    *nextpts = pos.pts;
                    nextpts->R = R;
                    Q[0] = Q[1] = Q[2] = Q[3] = 0;
                    evalparam* e = (evalparam*)(pnext + sizeof(positiontuneset));
                    int sqsum[4][2] = { { 0 } };
                    for (int i = 0; i < pos.pts.num; i++)
                    {
                        *e = pos.ev[i];
                        int ty = pos.tps.ev[e->index]->type;
                        if (ty != 2)
                        {
                            Q[ty] += e->g[0] * *pos.tps.ev[e->index];
                        }
                        else
                        {
                            int sqindex = pos.tps.ev[e->index]->groupindex;
                            sqsum[sqindex][0] += e->g[0] * *pos.tps.ev[e->index];
                            sqsum[sqindex][1] += e->g[1] * *pos.tps.ev[e->index];
                        }
                        pos.tps.used[e->index]++;
                        e++;
                    }
                    for (int i = 0; i < 4; i++)
                    {
                        if (sqsum[i][0] == 0 && sqsum[i][1] == 0) continue;
                        Q[0] += SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1);
                    }
                    int evaleg = GETEGVAL(Q[0]);
                    int sign = (evaleg > 0) - (evaleg < 0);
                    Q[3] = sign * max(Q[3], -abs(evaleg));
                    Qr = TAPEREDANDSCALEDEVAL(Q[0] + Q[3], nextpts->ph, nextpts->sc) + Q[1];
                    
                    if (Qi != (nextpts->sc == SCALE_DRAW ? SCOREDRAW : Qr))
                    {
                        printf("\n%d  Alarm. Evaluation by coeffs differs from qsearch value: %d != %d.\nFEN: %s\n", n, Qr, Qi, fen.c_str());
                        getValueByCoeff(*pos.tps.ev, nextpts, (evalparam*)(pnext + sizeof(positiontuneset)), nullptr, true);
                    }
                    else
                    {
                        //printf("%d  gesamt: %08x\n", n, Q);
                        pnext = (char*)e;
                        n++;
                        if (n % 0x2000 == 0) printf(".");
                    }
                }
            }
        }

    }

    texelptsnum = n;
    printf("  got %d positions\n", n);
}


static void tuneParameter(tuner* tn)
{
    tn->busy = true;

    double Error;
    double Emin = -1.0;
    int pmin;

    int tuned = 0;
    int g = 0;
    int subParam = (tn->ev[tn->paramindex].type ? 1 : 2);
    bool diffEval = tn->ev[tn->paramindex].type <= 1;

    while (true)     // loop over mg/eg parameter while notImproved <=2
    {
        int pa[4];
        int p, lastp;
        tuned++;
        if (tuned > subParam)
            break;

        int pbound[2] = { SHRT_MAX, SHRT_MIN };
        int delta = 1;
        int direction = 0; // direction=0: go right; delta > 0; direction=1: go right; delta
        if (subParam == 2)
        {
            // linear parameter
            int v = tn->ev[tn->paramindex];
            pa[0] = GETEGVAL(v);
            pa[1] = GETMGVAL(v);
            lastp = pa[g];
            p = lastp + delta;
            p = min(512, max(-512, p));
            tn->ev[tn->paramindex].replace(g, lastp);
            pmin = lastp;
        }
        else
        {
            // square parameter
            eval* e = &tn->ev[tn->paramindex];
            pa[0] = e->v;
            lastp = pa[g];
            p = lastp + delta;
            p = min(512, max(-512, p));
            tn->ev[tn->paramindex].replace(lastp);
            pmin = lastp;
        }
        if (Emin < 0)
            tn->starterror = Emin = TexelEvalError(tn, tn->precalcptr, tn->paramindex);
        do
        {
            if (subParam == 2)
                tn->ev[tn->paramindex].replace(g, p);
            else
                tn->ev[tn->paramindex].replace(p);
            Error = diffEval ? TexelEvalErrorDiff(tn, tn->precalcptr) : TexelEvalError(tn, nullptr, -1);

            if (Error >= Emin)
            {
                direction = (p > pmin ? 1 : 0);
                pbound[direction] = p;
                delta = (direction ? -1 : 1);
                p = pmin + delta;
            }
            else
            {
                pbound[direction] = pmin;
                Emin = Error;
                tuned = 1;
                pmin = p;
                delta *= 2;
                p = p + delta;
            }
        } while (abs(pbound[1] - pbound[0]) > 2);
        if (subParam == 2)
            tn->ev[tn->paramindex].replace(g, pmin);
        else
            tn->ev[tn->paramindex].replace(pmin);

        g = (g + 1) % subParam;
    }

    tn->error = Emin;
    tn->busy = false;
}


// Collects params of finished tuners, updates 'low' and 'improved' mark and returns free tuner
static void collectTuners(chessposition* p, tunerpool* pool, tuner** freeTuner)
{
    if (freeTuner) *freeTuner = nullptr;

    for (int i = 0; i < MAXTHREADS; i++)
    {
        tuner* tn = &pool->tn[i];
        int pi = tn->paramindex;

        while (!freeTuner && tn->busy)
            Sleep(10);

        if (tn->inUse)
        {
            if (!tn->busy)
            {
                if (tn->thr.joinable())
                {
                    pool->tuninginprogress[pi] = false;
                    tn->thr.join();
                }

                if (i >= iThreads)
                    tn->inUse = false;
                else if (freeTuner)
                    *freeTuner = tn;

                if (pi >= 0)
                {
                    if (tn->ev[pi] != *p->tps.ev[pi])
                    {
                        printf("%2d %4d  %9lld   %40s  %0.10f -> %0.10f  %s  -> %s\n", i, pi, p->tps.used[pi], nameTunedParameter(p, pi).c_str(), tn->starterror, tn->error,
                            getValueStringValue(p->tps.ev[pi]).c_str(),
                            getValueStringValue(&(tn->ev[pi])).c_str());
                        pool->lastImprovedIteration = max(pool->lastImprovedIteration, tn->iteration);
                        *p->tps.ev[pi] = tn->ev[pi];
                    }
                    else {
                        printf("%2d %4d  %9lld   %40s  %0.10f  %s  constant\n", i, pi, p->tps.used[pi], nameTunedParameter(p, pi).c_str(), tn->error,
                            getValueStringValue(&(tn->ev[pi])).c_str());
                    }
                }
                tn->paramindex = -1;
            }
        }
        if (!freeTuner)
        {
            freealigned64(tn->precalcptr);
            tn->precalcptr = nullptr;
        }
    }
}


static double getAvgParamVal(int iParam)
{
    // Get average
    positiontuneset* p = (positiontuneset*)texelpts;
    int pSum = 0;
    for (U64 i = 0; i < texelptsnum; i++)
    {
        evalparam* e = (evalparam*)((char*)p + sizeof(positiontuneset));
        if (p->sc != SCALE_DRAW)
            pSum += TAPEREDANDSCALEDEVAL(getValueByCoeff(*pos.tps.ev, p, e, false, iParam), p->ph, p->sc);
        p = (positiontuneset*)((char*)p + sizeof(positiontuneset) + p->num * sizeof(evalparam));
    }
    double pAvg = pSum / (double)texelptsnum;
    return pAvg;
}


static double getCorrelationCoeff(int ix, int iy, double ax, double ay)
{
    // Get correlation
    double counter = 0.0;
    double denominatorfactorx = 0.0;
    double denominatorfactory = 0.0;
    positiontuneset* p = (positiontuneset*)texelpts;
    for (U64 i = 0; i < texelptsnum; i++)
    {
        int px = 0, py = 0;
        evalparam* e = (evalparam*)((char*)p + sizeof(positiontuneset));
        if (p->sc != SCALE_DRAW)
        {
            px = TAPEREDANDSCALEDEVAL(getValueByCoeff(*pos.tps.ev, p, e, false, ix), p->ph, p->sc);
            py = TAPEREDANDSCALEDEVAL(getValueByCoeff(*pos.tps.ev, p, e, false, iy), p->ph, p->sc);
        }
        counter += (px - ax) * (py - ay);
        denominatorfactorx += (px - ax) * (px - ax);
        denominatorfactory += (py - ay) * (py - ay);

        p = (positiontuneset*)((char*)p + sizeof(positiontuneset) + p->num * sizeof(evalparam));
    }

    double cor = counter / sqrt(denominatorfactorx * denominatorfactory);
    return cor;
}


struct {
    double avg;
    double coeff;
    int index;
} correlation[NUMOFEVALPARAMS];


void getCorrelation(string correlationParams)
{
    while (correlationParams != "")
    {
        size_t spi = correlationParams.find('*');
        string correlationparam = (spi == string::npos) ? correlationParams : correlationParams.substr(0, spi);
        correlationParams = (spi == string::npos) ? "" : correlationParams.substr(spi + 1, string::npos);
        int x = 0;
        try {
            x = stoi(correlationparam);
        }
        catch (...) {}

        // skip (almost) unused parameters
        if (pos.tps.used[x] * 100000 / texelptsnum < 1)
            continue;

        // Get average of this param
        double xAvg = getAvgParamVal(x);
        // loop all parameters and get correlation
        for (int y = 0; y < pos.tps.count; y++)
        {
            correlation[y].index = -1;
            if (pos.tps.ev[y]->type != pos.tps.ev[x]->type)
                continue;
            if (pos.tps.used[y] * 100000 / texelptsnum < 1)
                continue;
            correlation[y].avg = getAvgParamVal(y);
            correlation[y].coeff = getCorrelationCoeff(x, y, xAvg, correlation[y].avg);
            correlation[y].index = y;
        }

        // sort them by absolute correlation
        for (int i = 0; i < pos.tps.count; i++)
        {
            if (correlation[i].index < 0) continue;
            for (int j = i + 1; j < pos.tps.count; j++)
            {
                if (correlation[j].index < 0) continue;
                if (abs(correlation[j].coeff) > abs(correlation[i].coeff))
                    swap(correlation[i], correlation[j]);
            }
            int y = correlation[i].index;
            printf("%3d\t%30s\tavg=\t%.4f\t%3d\t%30s\tavg=\t%.4f\tcor=\t%.4f\n", x, nameTunedParameter(&pos, x).c_str(), xAvg, y, nameTunedParameter(&pos, y).c_str(), correlation[i].avg, correlation[i].coeff);
        }
    }

}


// main fucntion for texel tuning
void TexelTune()
{
    pos.noQs = quietonly;  //FIXME: do we need it? Where to set?
    if (!texelptsnum) return;

    if (correlationlist != "")
    {
        getCorrelation(correlationlist);
        return;
    }

    tpool.lastImprovedIteration = 0;
    tuner* tn;

    for (int i = 0; i < MAXTHREADS; i++)
    {
        tpool.tn[i].busy = false;
        tpool.tn[i].inUse = false;
        tpool.tn[i].paramindex = -1;
        tpool.tn[i].precalcptr = nullptr;
    }

    iThreads = 1;
    for (int i = 0; i < iThreads; i++)
    {
        tpool.tn[i].inUse = true;
        tpool.tn[i].precalcptr = (precalculated*)allocalign64(texelptsnum * sizeof(precalculated));
    }

    bool improved = true;
    bool leaveSoon = false;
    bool leaveNow = false;

    ranctx rnd;
    raninit(&rnd, time(NULL));

    vector<int> tpindex;
    for (int i = 0; i < pos.tps.count; i++)
    {
        if (!pos.tps.tune[i])
            continue;
        if (pos.tps.used[i] * 100000 / texelptsnum < 1)
        {
            printf("   %4d  %9lld   %40s   %s  canceled, too few positions in testset\n", i, pos.tps.used[i],
                nameTunedParameter(&pos, i).c_str(),
                getValueStringValue(pos.tps.ev[i]).c_str());
            continue;
        }
        tpindex.insert(tpindex.end(), i); 
    }

    int tpcount = (int)tpindex.size();

    tpool.tuninginprogress.resize(pos.tps.count);
    fill(tpool.tuninginprogress.begin(), tpool.tuninginprogress.end(), false);

    printf("Tuning starts now.\nPress 'P' to output current parameters.\nPress 'B' to break after current tuning loop.\nPress 'S' for immediate break.\nPress '+', '-' to increase/decrease threads to use.\n\n");

    int iteration = 0;
    // loop over iterations until no parameter improves
    while (improved && !leaveSoon && !leaveNow)
    {
        iteration++;

        printf("Starting iteration %d...\n", iteration);

        vector<int> tpinloop = tpindex;
        int tpcountinloop = tpcount;

        // loop over all parameters in this iteration
        while (tpcountinloop)
        {
            if (leaveNow)
                break;

            int nextindex;

            do
            {
                collectTuners(&pos, &tpool, &tn);
                if (!tn)
                {
                    Sleep(100);
                    if (_kbhit())
                    {
                        char c = _getch();
                        if (c == 'p')
                            printTunedParameters(&pos);
                        if (c == 'b')
                        {
                            printf("Stopping after this tuning loop...\n");
                            leaveSoon = true;
                        }
                        if (c == 's')
                        {
                            printf("Stopping now!\n");
                            leaveNow = true;
                        }
                        if (c == '-' && iThreads > 1)
                        {
                            iThreads--;
                            // inUse will be reset by the collector
                            printf("Now using %d threads...\n", iThreads);
                        }
                        if (c == '+' && iThreads < min(MAXTHREADS, tpcount / 3))
                        {
                            if (!tpool.tn[iThreads].precalcptr)
                                tpool.tn[iThreads].precalcptr = (precalculated*)allocalign64(texelptsnum * sizeof(precalculated));
                            tpool.tn[iThreads++].inUse = true;
                            printf("Now using %d threads...\n", iThreads);
                        }
                    }
                }
            } while (!tn);

            while (true)
            {
                int i = ranval(&rnd) % tpcountinloop;
                nextindex = tpinloop[i];
                if (!tpool.tuninginprogress[nextindex])
                {
                    tpool.tuninginprogress[nextindex] = true;
                    tpinloop[i] = tpinloop[tpcountinloop - 1];
                    tpcountinloop--;
                    break;
                }
            }

            tn->busy = true;
            tn->paramindex = nextindex;
            tn->iteration = iteration;
            copyParams(&pos, tn);

            tn->thr = thread(&tuneParameter, tn);

        }
        improved = (tpool.lastImprovedIteration >= iteration - 1);
    }
    collectTuners(&pos, &tpool, nullptr);
    printTunedParameters(&pos);
}


static void listParams(int start, int end, bool onlyActive)
{
    for (int i = max(0, start); i <= min(end, pos.tps.count - 1); i++)
    {
        if (onlyActive && !pos.tps.tune[i])
            continue;

        printf("%s %4d %9lld %32s   %s\n", pos.tps.tune[i] ? "*" : " ", i, pos.tps.used[i], nameTunedParameter(&pos, i).c_str(), getValueStringValue(pos.tps.ev[i]).c_str());
    }
}


static void enableTune(int start, int end, bool enable)
{
    for (int i = max(0, start); i <= min(end, pos.tps.count - 1); i++)
        pos.tps.tune[i] = enable;
}


void parseTune(vector<string> commandargs)
{
    size_t ci = 0;
    size_t cs = commandargs.size();

    while (ci < cs)
    {
        string s = commandargs[ci];
        transform(s.begin(), s.end(), s.begin(), ::tolower);
        ci++;
        if (s == "help")
            printf("tune list [active] [start] [end]\ntune reset active|values\ntune enable [start] [end]\ntune fenfile {balancedonly] file1 [file2] ...\ntune optk\ntune go\n");
        if (s == "list")
        {
            bool onlyActive = false;
            if (ci < cs && commandargs[ci] == "active")
            {
                onlyActive = true;
                ci++;
            }
            int start = 0;
            int end = pos.tps.count - 1;
            if (ci < cs)
            {
                try {
                    start = end = stoi(commandargs[ci++]);
                }
                catch (...) {}
            }
            if (ci < cs)
            {
                try {
                    end = stoi(commandargs[ci++]);
                }
                catch (...) {}
            }

            listParams(start, end, onlyActive);

        }
        if (s == "reset")
        {
            bool resetvalues = false;
            bool resetactive = false;
            while (ci < cs)
            {
                string subcmd = commandargs[ci++];
                if (subcmd == "values") resetvalues = true;
                if (subcmd == "active") resetactive = true;
            }
            if (resetvalues)
            {
                eps = epsdefault;
                printf("Values of all parameters reset.\n");
            }
            if (resetactive)
            {
                enableTune(0, pos.tps.count - 1, false);
                printf("All parematers disabled for tuning. Use tune enable to select.\n");
            }
        }
        if (s == "enable")
        {
            int start = 0;
            int end = pos.tps.count - 1;
            if (ci < cs)
            {
                try {
                    start = end = stoi(commandargs[ci++]);
                }
                catch (...) {}
            }
            if (ci < cs)
            {
                try {
                    end = stoi(commandargs[ci++]);
                }
                catch (...) {}
            }
            enableTune(start, end, true);
        }
        if (s == "fenfile")
        {
            fentuningfiles = "";
            balancedonly = false;
            while (ci < cs) {
                string fn = commandargs[ci++];
                if (fn == "balancedonly")
                {
                    balancedonly = true;
                    continue;
                }
                fn.erase(remove(fn.begin(), fn.end(), '\"'), fn.end());
                fentuningfiles = (fentuningfiles != "" ? fentuningfiles + "*" : "") + fn;
            }
            getCoeffsFromFen(fentuningfiles);
        }
        if (s == "optk")
            optk();
        if (s == "go")
            TexelTune();
    }

}


void tuneInit()
{
    pos.mtrlhsh.init();
    pos.pwnhsh.setSize(0);
    pos.tps.count = 0;
    pos.resetStats();
    registerallevals(&pos);
}


void tuneCleanup()
{
    if (texelpts)
        free(texelpts);
    pos.mtrlhsh.remove();
    pos.pwnhsh.remove();
}
#endif // EVALTUNE
