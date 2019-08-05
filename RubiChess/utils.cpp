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


vector<string> SplitString(const char* s)
{
    string ss(s);
    vector<string> result;
    istringstream iss(ss);
    bool quotes = false;
    string sout;
    for (string s; iss >> s;)
    {
        if (s[0] == '"' && !quotes)
        {
            sout = "";
            quotes = true;
        }

        if (quotes)
            sout += s;
        else
            sout = s;

        if (s[s.length() - 1] == '"' && quotes)
        {
            quotes = false;
        }

        if (!quotes)
            result.push_back(sout);
    }
    return result;
}

string IndexToAlgebraic(int i)
{
    string s;
    s.push_back((char)(FILE(i) + 'a'));
    s.push_back((char)(RANK(i) + '1'));
    return s;
}

unsigned char AlgebraicToIndex(string s)
{
    char file = (char)(s[0] - 'a');
    char rank = (char)(s[1] - '1');

    return (unsigned char)(rank << 3 | file);
}

void BitboardDraw(U64 b)
{
    U64 mask;
    printf("/--------\\\n");
    for (int r = 7; r >= 0; r--)
    {
        printf("|");
        mask = (1ULL << (r * 8));
        for (int f = 0; f < 8; f++)
        {
            if (b & mask)
                printf("x");
            else
                printf(" ");
            mask <<= 1;
        }
        printf("|\n");
    }
    printf("\\--------/\n\n");
}


string AlgebraicFromShort(string s, chessposition *pos)
{
    string retval = "";
    int castle0 = 0;
    PieceType promotion = BLANKTYPE;
    chessmovelist ml;
    pos->prepareStack();
    ml.length = pos->getMoves(&ml.move[0]);
    PieceType pt = PAWN;
    int to = 0xc0, from = 0xc0;
    int i = (int)s.size() - 1;
    // Skip check +
    if (i >= 0 && s[i] == '+')
        i--;
    // Castle
    while (i >= 0 && (s[i] == '0' || s[i] == 'O'))
    {
        i -= 2;
        castle0++;
    }
    if (castle0 >= 2)
    {
        pt = KING;
        from = (from & 0x80) | ('e' - 'a');
        to = (to & 0x80) | (castle0 == 2 ? 'g' - 'a' : 'c' - 'a');
    }
    if (i >= 0 && s[i] >= 'A')
    {
        // Promotion
        promotion = GetPieceType(s[i--]);
        if (i >= 0 && s[i] == '=')
            // Skip promotion '='
            i--;
    }
    if (i > 0 && s[i] >= '1' && s[i] <= '8' && s[i - 1] >= 'a' && s[i - 1] <= 'h')
    {
        // Target field
        to = AlgebraicToIndex(&s[i - 1]);
        i -= 2;
    }
    // Skip the capture x
    if (i >= 0 && s[i] == 'x')
        i--;
    // rank or file for ambiguous moves
    if (i >= 0 && s[i] >= '1' && s[i] <= '8')
        from = (from & 0x47) | ((s[i--] - '1') << 3);
    if (i >= 0 && s[i] >= 'a' && s[i] <= 'h')
        from = (from & 0xb8) | (s[i--] - 'a');

    // The Figure
    if (i >= 0 && s[i] >= 'A')
        pt = GetPieceType(s[i--]);

    // i < 0 hopefully
    // get the correct move
    for (int i = 0; i < ml.length; i++)
    {
        if (pt == (GETPIECE(ml.move[i].code) >> 1)
            && promotion == (GETPROMOTION(ml.move[i].code) >> 1)
            && ((from & 0x80) || ((from & 0x38) == (GETFROM(ml.move[i].code) & 0x38)))
            && ((from & 0x40) || ((from & 0x07) == (GETFROM(ml.move[i].code) & 0x07)))
            && ((to & 0x80) || ((to & 0x38) == (GETTO(ml.move[i].code) & 0x38)))
            && ((to & 0x40) || ((to & 0x07) == (GETTO(ml.move[i].code) & 0x07))))
        {
            // test if the move is legal; otherwise we need to search further
            if (pos->playMove(&ml.move[i]))
            {
                pos->unplayMove(&ml.move[i]);
                retval = ml.move[i].toString();
                break;
            }
        }
    }
    return retval;
}


#ifdef EVALTUNE

chessposition pos;
U64 fenWritten;

static void writeFenToFile(ofstream *fenfile, string fenlines[], int gamepositions, int ppg)
{
    int fentowrite = (ppg ? ppg : gamepositions);

    while (fentowrite--)
    {
        int i = rand() % gamepositions;
        *fenfile << fenlines[i];
        fenWritten++;
        fenlines[i] = fenlines[--gamepositions];
    }
}


bool PGNtoFEN(string pgnfilename, bool quietonly, int ppg)
{
    pos.pwnhsh = new Pawnhash(0);
    pos.tps.count = 0;
    int gamescount = 0;
    fenWritten = 0ULL;
    bool mateFound;
    string line;
    string line1, line2;
    string rest_of_last_line = "";
    string fenfilename = pgnfilename + ".fen";
    string fenmovefilename = pgnfilename + ".fenmove";

    // The following is Windows-only; FIXME: C++--17 offers portable filesystem handling
    WIN32_FIND_DATA fd;
    HANDLE pgnhandle;
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
    ofstream fenmovefile;
    if (!quietonly)
    {
        fenmovefile.open(fenmovefilename);
        if (!fenmovefile.is_open())
        {
            printf("Cannot open %s for writing.\n", fenmovefilename.c_str());
            return false;
        }
    }
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
        string moves;
        string lastmove;
        bool valueChecked;
        int gamepositions = 0;
        string fenlines[2048];
        while (getline(pgnfile, line))
        {
            line2 = line1;
            line1 = line;

            smatch match;
            string fen;
            double score;
            // We assume that the [Result] section comes first, then the [FEN] section
            if (regex_search(line, match, regex("\\[Result\\s+\"(.*)\\-(.*)\"")))
            {
                gamescount++;
                if (gamepositions)
                    writeFenToFile(&fenfile, fenlines, gamepositions, ppg);
                gamepositions = 0;
                // Write last game
                if (!quietonly && !ppg && newgamestarts == 2)
                    fenmovefile << to_string(result) + "#" + newfen + " moves " + moves + "\n";

                if (match.str(1) == "0")
                    result = -1;
                else if (match.str(1) == "1")
                    result = 1;
                else
                    result = 0;
                newgamestarts = 1;
                moves = "";
                mateFound = false;
            }
            bool fenFound;
            if (newgamestarts == 1 && 
                ((fenFound = regex_search(line, match, regex("\\[FEN\\s+\"(.*)\"")))
                    || !regex_search(line, match, regex("\\["))))
            {
                newfen = fenFound ? match.str(1) : STARTFEN;
                newgamestarts++;
                valueChecked = true;
                fen = newfen;
                pos.getFromFen(fen.c_str());
                // Skip positions inside TB area
                if (POPCOUNT(pos.occupied00[0] | pos.occupied00[1]) >= 7)
                    fenlines[gamepositions++] = to_string(result) + "#" + fen + "#\n";
            }
            // Don't export games that were lost on time or by stalled connection
            if (regex_search(line, match, regex("\\[Termination\\s+\".*(forfeit|stalled).*\"")))
            {
                printf("Skip this match: %s\n", line.c_str());
                newgamestarts = 0;
                valueChecked = true;
            }

            // search for the moves
            if (newgamestarts == 2 && !regex_search(line, match, regex("\\[.*\\]")))
            {
                bool foundInLine;
                do
                {
                    foundInLine = false;
                    if (regex_search(line, match, regex("^(\\s*\\d+\\.\\s*)")))
                    {
                        // skip move number
                        line = match.suffix();
                    }
                    if (regex_search(line, match, regex("^\\s*(([O\\-]{3,5})\\+?|([KQRBN]?[a-h]*[1-8]*x?[a-h]+[1-8]+(=[QRBN])?)\\+?)")))
                    {
                        // Found move
                        if (!valueChecked)
                        {
                            // Score tag of last move missing; just output without score
                            fenlines[gamepositions++] = to_string(result) + "#" + fen + "#0\n";
                            moves = moves + lastmove + " ";
                        }

                        foundInLine = true;
                        valueChecked = false;
                        lastmove = AlgebraicFromShort(match.str(), &pos);
                        if (lastmove == "" || !pos.applyMove(lastmove))
                        {
                            printf("Alarm (game %d): %s\n", gamescount, match.str().c_str());
                            pos.print();
                            printf("last Lines:\n%s\n%s\n\n", line2.c_str(), line1.c_str());
                        }
                        pos.isQuiet = !pos.isCheckbb;
                        if (quietonly && pos.isQuiet)
                            pos.getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                        if (!quietonly || pos.isQuiet)
                            fen = pos.toFen();
                        else
                            fen = "";
                        line = match.suffix();
                    }
                    if (!valueChecked)
                    {
                        if (regex_search(line, match, regex("^\\s*(\\{((\\+|\\-)?((\\d+\\.\\d+)|(M\\d+)))\\/[^\\}]*\\})")))
                        {
                            foundInLine = true;
                            string scorestr = match.str(2);
                            // Only output if no mate score detected
                            if ((scorestr[0] == 'M' || scorestr[1] == 'M')
                                || (score = stod(match.str(2))) >= 30)
                                mateFound = true;
                            if (!mateFound && fen != "")
                            {
                                fenlines[gamepositions++] = to_string(result) + "#" + fen + "#" + to_string(score) + "\n";
                                moves = moves + lastmove + " ";
                            }
                            line = match.suffix();
                            valueChecked = true;
                        }
                    }
                } while (foundInLine);
            }
        }
        if (gamepositions)
            writeFenToFile(&fenfile, fenlines, gamepositions, ppg);

        printf("Position so far:  %9lld\n", fenWritten);

    } while (folderMode && FindNextFile(pgnhandle, &fd));

    delete pos.pwnhsh;
    return true;
}



static string getValueStringValue(eval *e)
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
    else {
        string si = to_string(e->groupindex);
        if (si.length() < 4)
            si.insert(si.begin(), 4 - si.length(), ' ');
        string sv = to_string(e->v);
        if (sv.length() < 4)
            sv.insert(sv.begin(), 4 - sv.length(), ' ');
        return "SQVALUE(" + si + "," + sv + ")";
    }
}


static string nameTunedParameter(chessposition *pos, int i)
{
    string name = pos->tps.name[i];
    if (pos->tps.bound2[i] > 0)
    {
        name += "[" + to_string(pos->tps.index2[i]) + "][" + to_string(pos->tps.index1[i]) + "]";
    }
    else if (pos->tps.bound1[i] > 0)
    {
        name += "[" + to_string(pos->tps.index1[i]) + "] = { ";
    }
    return name;
}


static void printTunedParameters(chessposition *pos)
{
    string lastname = "";
    string output = "";
    for (int i = 0; i < pos->tps.count; i++)
    {
        if (lastname != pos->tps.name[i])
        {
            if (output != "")
            {
                output += ";\n";
                printf("%s", output.c_str());
                output = "";
            }
            lastname = pos->tps.name[i];
            output = "    eval " + lastname;
            if (pos->tps.bound2[i] > 0)
            {
                output += "[" + to_string(pos->tps.bound2[i]) + "][" + to_string(pos->tps.bound1[i]) + "] = {\n        { ";
            }
            else if (pos->tps.bound1[i] > 0)
            {
                output += "[" + to_string(pos->tps.bound1[i]) + "] = { ";
            }
            else {
                output += " = ";
            }
        }

        output = output + " " + getValueStringValue(pos->tps.ev[i]);

        if (pos->tps.index1[i] < pos->tps.bound1[i] - 1)
        {
            output += ",";
            if (!((pos->tps.index1[i] + 1) & (pos->tps.bound2[i] ? 0x7 : 0x7)))
                output += "\n          ";
        }
        else if (pos->tps.index1[i] == pos->tps.bound1[i] - 1)
        {
            output += "  }";
            if (pos->tps.index2[i] < pos->tps.bound2[i] - 1)
                output += ",\n        { ";
            else if (pos->tps.index2[i] == pos->tps.bound2[i] - 1)
                output += "\n    }";
        }
    }
    output += ";\n";
    printf("%s", output.c_str());
}


int tuningratio = 1;

char *texelpts = NULL;
U64 texelptsnum;


static int getGradientValue(struct tuner *tn, positiontuneset *p, evalparam *e)
{
    int v = 0;
    int sqsum[4][2] = { 0 };
    for (int i = 0; i < p->num; i++)
    {
        if (tn->ev[e->index].type == 0)
        {
            v += tn->ev[e->index] * e->g[0];
        }
        else
        {
            int sqindex = tn->ev[e->index].groupindex;
            sqsum[sqindex][0] += tn->ev[e->index] * e->g[0];
            sqsum[sqindex][1] += tn->ev[e->index] * e->g[1];
        }
            ;// v += tn->ev[e->index].sqval(e->g[0]) + tn->ev[e->index].sqval(e->g[1]);
        e++;
    }
    for (int i = 0; i < 4; i++)
    {
        if (sqsum[i][0] == 0 && sqsum[i][1] == 0) continue;
        v += SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1);
    }

    return v;
}

const double texel_k = 1.121574;

static double TexelEvalError(struct tuner *tn)
{
    double Ri, Qi;
    double E = 0.0;

    positiontuneset *p = (positiontuneset*)texelpts;
    for (int i = 0; i < texelptsnum; i++)
    {
        evalparam *e = (evalparam *)((char*)p + sizeof(positiontuneset));

        Ri = p->R / 2.0;
        if (p->sc == SCALE_DRAW)
            Qi = SCOREBLACKWINS;
        else
            Qi = TAPEREDANDSCALEDEVAL(getGradientValue(tn, p, e), p->ph, p->sc);
        double sigmoid = 1 / (1 + pow(10.0, - texel_k * Qi / 400.0));
        E += (Ri - sigmoid) * (Ri - sigmoid);
        p = (positiontuneset*)((char*)p + sizeof(positiontuneset) + p->num * sizeof(evalparam));
    }

    return E / texelptsnum;
}

static void getGradsFromFen(chessposition *pos, string fenfilename)
{
    int gamescount = 0;
    bool fenmovemode = (fenfilename.find(".fenmove") != string::npos);
    string line;
    smatch match;
    int n;
    int c;
    int bw;
    char R;
    string fen;
    int Qi, Qa;
    U64 buffersize;
    char *pnext;
    long long minfreebuffer = sizeof(positiontuneset) + NUMOFEVALPARAMS * sizeof(evalparam) * 1024;
    int msb;
    GETMSB(msb, minfreebuffer);
    minfreebuffer = (1ULL << (msb + 1));
    const U64 maxbufferincrement = minfreebuffer << 10;
        
    n = 0;
    bw = 0;
    c = tuningratio;
    ifstream fenfile(fenfilename);
    if (!fenfile.is_open())
    {
        printf("Cannot open %s for reading.\n", fenfilename.c_str());
        return;
    }
    buffersize = minfreebuffer;
    texelpts = (char*)malloc(buffersize);
    pnext = (char*)texelpts;
    printf("Reading positions");
    while (getline(fenfile, line))
    {
        if (texelpts + buffersize - pnext < minfreebuffer)
        {
            buffersize = min(buffersize + maxbufferincrement, buffersize * 2);
            char *oldtexelpts = texelpts;
            texelpts = (char*)realloc(texelpts, buffersize);
            pnext += (texelpts - oldtexelpts);
        }
        if (!fenmovemode)
        {
            fen = "";
            if (regex_search(line, match, regex("(.*)#(.*)#(.*)")))
            {
                fen = match.str(2);
                R = (stoi(match.str(1)) + 1);
            }
            else if (regex_search(line, match, regex("(.*)\\s+(c9\\s+)?\"?((1\\-0)|(0\\-1)|(1/2))\"?")))
            {
                fen = match.str(1);
                R = (match.str(3) == "1-0" ? 2 : (match.str(3) == "0-1" ? 0 : 1));
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
                    pos->getFromFen(fen.c_str());
                    pos->ply = 0;
                    Qi = pos->getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                    if (!pos->w2m())
                        Qi = -Qi;
                    positiontuneset *nextpts = (positiontuneset*)pnext;
                    *nextpts = pos->pts;
                    nextpts->R = R;
                    Qa = 0;
                    evalparam *e = (evalparam *)(pnext + sizeof(positiontuneset));
                    int sqsum[4][2] = { 0 };
                    for (int i = 0; i < pos->pts.num; i++)
                    {
                        *e = pos->ev[i];
                        //printf("%20s: %08x  %3d\n", pos->tps.name[e->index].c_str(), *pos->tps.ev[i], e->g);
                        if (pos->tps.ev[e->index]->type == 0)
                        {
                            Qa += e->g[0] * *pos->tps.ev[e->index];
                            //printf("l %3d: %3d * %08x         = %08x\n", e->index, e->g[0], (int)*pos->tps.ev[e->index], Qa);
                        }
                        else
                        {
                            int sqindex = pos->tps.ev[e->index]->groupindex;
                            sqsum[sqindex][0] += e->g[0] * *pos->tps.ev[e->index];
                            sqsum[sqindex][1] += e->g[1] * *pos->tps.ev[e->index];
                        }
                        pos->tps.used[e->index]++;
                        e++;
                    }
                    for (int i = 0; i < 4; i++)
                    {
                        if (sqsum[i][0] == 0 && sqsum[i][1] == 0) continue;
                        Qa += SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1);
                    }
                    if (MATEDETECTED(Qi))
                        n--;
                    else if (Qi != (nextpts->sc == SCALE_DRAW ? SCOREDRAW : TAPEREDANDSCALEDEVAL(Qa, nextpts->ph, nextpts->sc)))
                        printf("%d  Alarm. Gradient evaluation differs from qsearch value: %d != %d.\n", n, TAPEREDANDSCALEDEVAL(Qa, nextpts->ph, nextpts->sc), Qi);
                    else
                    {
                        //printf("%d  gesamt: %08x\n", n, Qa);
                        pnext = (char*)e;
                        n++;
                        if (n % 0x2000 == 0) printf(".");
                    }
                }
            }
        }
        else
        {
            if (regex_search(line, match, regex("(.*)#(.*)moves(.*)")))
            {
                gamescount++;
                R = (stoi(match.str(1)) + 1);
                pos->getFromFen(match.str(2).c_str());
                pos->ply = 0;
                vector<string> movelist = SplitString(match.str(3).c_str());
                vector<string>::iterator move = movelist.begin();
                bool gameend;
                do
                {
                    bw = 1 - bw;
                    if (bw)
                        c++;
                    if (c > tuningratio)
                        c = 1;
                    if (c == tuningratio)
                    {
                        Qi = pos->getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                        if (!pos->w2m())
                            Qi = -Qi;
                        positiontuneset *nextpts = (positiontuneset*)pnext;
                        *nextpts = pos->pts;
                        nextpts->R = R;
                        Qa = 0;
                        evalparam *e = (evalparam *)(pnext + sizeof(positiontuneset));
                        int sqsum[4][2] = { 0 };
                        for (int i = 0; i < pos->pts.num; i++)
                        {
                            *e = pos->ev[i];
                            //printf("%20s: %08x  %3d\n", pos->tps.name[e->index].c_str(), *pos->tps.ev[i], e->g);
                            if (pos->tps.ev[e->index]->type == 0)
                            {
                                Qa += e->g[0] * *pos->tps.ev[e->index];
                                //printf("l %3d: %3d * %08x         = %08x\n", e->index, e->g, (int)*pos->tps.ev[e->index], Qa);
                            }
                            else
                            {
                                int sqindex = pos->tps.ev[e->index]->groupindex;
                                sqsum[sqindex][0] += e->g[0] * *pos->tps.ev[e->index];
                                sqsum[sqindex][1] += e->g[1] * *pos->tps.ev[e->index];
                            }
                            pos->tps.used[e->index]++;
                            e++;
                        }
                        for (int i = 0; i < 4; i++)
                        {
                            if (sqsum[i][0] == 0 && sqsum[i][1] == 0) continue;
                            Qa += SQRESULT(sqsum[i][0], 0) + SQRESULT(sqsum[i][1], 1);
                        }
                        if (Qi != (nextpts->sc == SCALE_DRAW ? SCOREDRAW : TAPEREDANDSCALEDEVAL(Qa, nextpts->ph, nextpts->sc)))
                            printf("Alarm. Gradient evaluation differs from qsearch value.\n");
                        else
                        {
                            //printf("gesamt: %d\n", Qa);
                            pnext = (char*)e;
                            n++;
                            if (n % 0x2000 == 0) printf(".");
                        }
                    }
                    gameend = (move == movelist.end());
                    if (!gameend)
                    {
                        if (!pos->applyMove(*move))
                        {
                            printf("Alarm (game %d)! Move %s seems illegal.\nLine: %s\n", gamescount, move->c_str(), line.c_str());
                            pos->print();
                        }
                        move++;
                    }

                } while (!gameend);
            }
        }
    }

    texelptsnum = n;
    printf("  got %d positions\n", n);
}



static void copyParams(chessposition *pos, struct tuner *tn)
{
    for (int i = 0; i < pos->tps.count; i++)
        tn->ev[i] = *pos->tps.ev[i];
    tn->paramcount = pos->tps.count;
}


static void tuneParameter(struct tuner *tn)
{
    tn->busy = true;

    double Error;
    double Emin = -1.0;
    int pmin;

    int tuned = 0;
    int g = 0;
    int subParam = (tn->ev[tn->paramindex].type ? 1 : 2);

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
            eval *e = &tn->ev[tn->paramindex];
            pa[0] = e->v;
            lastp = pa[g];
            p = lastp + delta;
            p = min(512, max(-512, p));
            tn->ev[tn->paramindex].replace(lastp);
            pmin = lastp;
        }
        if (Emin < 0)
            Emin = TexelEvalError(tn);
        do
        {
            if (subParam == 2)
                tn->ev[tn->paramindex].replace(g, p);
            else
                tn->ev[tn->paramindex].replace(p);
            Error = TexelEvalError(tn);
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


static void updateTunerPointer(chessposition *pos, tunerpool *pool)
{
    int num = pos->tps.count;
    int newLowRunning = pool->highRunning;

    for (int i = 0; i < en.Threads; i++)
    {
        tuner *tn = &pool->tn[i];
        int pi = tn->paramindex;

        if (tn->busy)
        {
            // remember for possible lowest runner
            if ((pool->highRunning - pi) % num > (pool->highRunning - newLowRunning) % num)
                newLowRunning = pi;
        }
    }
    pool->lowRunning = newLowRunning;
}

// Collects params of finished tuners, updates 'low' and 'improved' mark and returns free tuner
static void collectTuners(chessposition *pos, tunerpool *pool, tuner **freeTuner)
{
    if (freeTuner) *freeTuner = nullptr;
    for (int i = 0; i < en.Threads; i++)
    {
        tuner *tn = &pool->tn[i];
        int pi = tn->paramindex;

        while (!freeTuner && tn->busy)
            Sleep(10);

        if (!tn->busy)
        {
            if (tn->thr.joinable())
                tn->thr.join();

            if (freeTuner) *freeTuner = tn;

            if (pi >= 0)
            {
                if (tn->ev[pi].type == 0 &&  tn->ev[pi] != *pos->tps.ev[pi]
                    || tn->ev[pi].type == 1 && tn->ev[pi] != *pos->tps.ev[pi])
                {
                    printf("%2d %4d  %9lld   %40s  %0.10f  %s  -> %s\n", i, pi, pos->tps.used[pi], nameTunedParameter(pos, pi).c_str(), tn->error,
                        getValueStringValue(pos->tps.ev[pi]).c_str(),
                        getValueStringValue(&(tn->ev[pi])).c_str());
                    pool->lastImproved = pi;
                    *pos->tps.ev[pi] = tn->ev[pi];
                }
                else {
                    printf("%2d %4d  %9lld   %40s  %0.10f  %s  constant\n", i, pi, pos->tps.used[pi], nameTunedParameter(pos, pi).c_str(), tn->error,
                        getValueStringValue(&(tn->ev[pi])).c_str());
                }
            }
            tn->paramindex = -1;
        }
    }
}


void TexelTune(string fenfilename)
{
#if 0 // enable to calculate constant k 
    // FIXME: Needs to be rewritten after eval rewrite
    double E[2];
    double bound[2] = { 0.0, 2.0 };
    double x, lastx;
    //double delta;
    lastx = (bound[0] + bound[1]) / 2;

    E[0] = TexelEvalError(fenfilename, bound[0]);
    E[1] = TexelEvalError(fenfilename, bound[1]);
    Emin = TexelEvalError(fenfilename, lastx);
    if (Emin > E[0] || Emin > E[1])
    {
        printf("Tuning Error! Wrong bounds.\n");
        return;
    }

    while (bound[1] - bound[0] > 0.001)
    {
        x = (lastx + bound[direction]) / 2;
        Error = TexelEvalError(fenfilename, x);
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
    printf("Tuningscore b0=%0.10f (%0.10f) b1=%0.10f (%0.10f)", bound[0], E[0], bound[1], E[1]);
    k = (bound[0] + bound[1]) / 2;
#endif
    pos.pwnhsh = new Pawnhash(0);
    registeralltuners(&pos);
    en.setOption("hash", "4"); // we don't need tt; save all the memory for game data
    getGradsFromFen(&pos, fenfilename);

    printf("Tuning starts now.\nPress 'P' to output current parameters.\nPress 'B' to break after current tuning loop.\nPress 'S' for immediate break.\n\n");

    tunerpool tp;
    tp.tn = new struct tuner[en.Threads];
    tp.lowRunning = -1;
    tp.highRunning = -1;
    tp.lastImproved = -1;

    for (int i = 0; i < en.Threads; i++)
    {
        tp.tn[i].busy = false;
        tp.tn[i].index = i;
        tp.tn[i].paramindex = -1;
    }

    bool improved = true;
    bool leaveSoon = false;
    bool leaveNow = false;

    while (improved && !leaveSoon && !leaveNow)
    {
        for (int i = 0; i < pos.tps.count; i++)
        {
            if (leaveNow)
                break;
            if (!pos.tps.tune[i])
                continue;
            if (pos.tps.used[i] * 100000 / texelptsnum < 1)
            {
                printf("   %4d  %9lld   %40s   %s  canceled, too few positions in testset\n", i, pos.tps.used[i],
                    nameTunedParameter(&pos, i).c_str(),
                    getValueStringValue(pos.tps.ev[i]).c_str());
                continue;
            }

            tuner *tn;

            tp.highRunning = i;

            do
            {
                collectTuners(&pos, &tp, &tn);
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
                    }
                }
            } while (!tn);
            tn->busy = true;
            tn->paramindex = i;
            copyParams(&pos, tn);

            tn->thr = thread(&tuneParameter, tn);

            updateTunerPointer(&pos, &tp);
            if (tp.highRunning == tp.lastImproved)
            {
                while (tn->busy)
                    // Complete loop without improvement... wait for last tuning finish
                    Sleep(100);
                collectTuners(&pos, &tp, &tn);

                if (tp.highRunning == tp.lastImproved)
                {
                    // still no improvement after last finished thread => exit
                    improved = false;
                    break;
                }
            }
        }
    }
    collectTuners(&pos, &tp, nullptr);
    delete[] tp.tn;
    free(texelpts);
    delete pos.pwnhsh;
    printTunedParameters(&pos);
}

#endif //EVALTUNE


#ifdef _WIN32
U64 getTime()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

#else

U64 getTime()
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return (U64)(1000000000LL * now.tv_sec + now.tv_nsec);
}

void Sleep(long x)
{
    struct timespec now;
    now.tv_sec = 0;
    now.tv_nsec = x * 1000000;
    nanosleep(&now, NULL);
}

#endif


#ifdef STACKDEBUG
// Thanks to http://blog.aaronballman.com/2011/04/generating-a-stack-crawl/ for the following stacktracer
void GetStackWalk(chessposition *pos, const char* message, const char* _File, int Line, int num, ...)
{
    va_list args;
    va_start(args, num);
    string values = "Values: ";
    for (int i = 0; i < num; i++)
    {
        values = values + " " + to_string(va_arg(args, int));
    }
    va_end(args);

    ofstream ofile;
    bool bFileAssert = (en.assertfile != "");
    if (bFileAssert)
    {
        ofile.open(en.assertfile, fstream::out | fstream::app);
    }

    cout << "Assertion failed: " + string(message) + ", file " + string(_File) + ", line " + to_string(Line) + "\n";
    cout << values + "\n";
    if (pos)
        pos->print();
    if (bFileAssert)
    {
        ofile << "Assertion failed: " + string(message) + ", file " + string(_File) + ", line " + to_string(Line) + "\n";
        ofile << values + "\n";
        pos->print(&ofile);
    }

    std::string outWalk;
    // Set up the symbol options so that we can gather information from the current
    // executable's PDB files, as well as the Microsoft symbol servers.  We also want
    // to undecorate the symbol names we're returned.  If you want, you can add other
    // symbol servers or paths via a semi-colon separated list in SymInitialized.

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    HANDLE hProcess = GetCurrentProcess();
    if (!SymInitialize(hProcess, NULL, TRUE))
    {
        cout << "info string Cannot initialize symbols.\n";
        return;
    }

    // Capture up to 25 stack frames from the current call stack.  We're going to
    // skip the first stack frame returned because that's the GetStackWalk function
    // itself, which we don't care about.
    PVOID addrs[25] = { 0 };
    USHORT frames = CaptureStackBackTrace(1, 25, addrs, NULL);
    for (USHORT i = 0; i < frames; i++) {
        // Allocate a buffer large enough to hold the symbol information on the stack and get 
        // a pointer to the buffer.  We also have to set the size of the symbol structure itself
        // and the number of bytes reserved for the name.
        ULONG64 buffer[(sizeof(SYMBOL_INFO) + 1024 + sizeof(ULONG64) - 1) / sizeof(ULONG64)] = { 0 };
        SYMBOL_INFO *info = (SYMBOL_INFO *)buffer;
        info->SizeOfStruct = sizeof(SYMBOL_INFO);
        info->MaxNameLen = 1024;

        // Attempt to get information about the symbol and add it to our output parameter.
        DWORD64 displacement64 = 0;
        if (SymFromAddr(hProcess, (DWORD64)addrs[i], &displacement64, info)) {
            outWalk.append(info->Name, info->NameLen);

            DWORD dwDisplacement;
            IMAGEHLP_LINE64 line;

            if (SymGetLineFromAddr64(hProcess, (DWORD64)addrs[i], &dwDisplacement, &line))
            {
                outWalk.append(":" + to_string(line.LineNumber));
            }
            else
            {
                cout << "SymGetLineFromAddr64 failed.\n";
            }

            outWalk.append("\n");
        }
        else
        {
            cout << "SymFromAddr failed with " + to_string(GetLastError()) + "\n";
        }
    }

    SymCleanup(::GetCurrentProcess());

    cout << outWalk;
    if (bFileAssert)
    {
        ofile << outWalk;
        ofile.close();
    }

}
#endif
