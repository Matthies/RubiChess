
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

unsigned char AlgebraicToIndex(string s, int base)
{
    char file = (char)(s[0] - 'a');
    char rank = (char)(s[1] - '1');
    if (file >= 0 && file < 8 && rank >= 0 && rank < 8)
        return (unsigned char)(base == 64 ? (rank << 3 | file) : (rank << 4 | file));
    else
        return (unsigned char)0x88;
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
    int to = 0x88, from = 0x88;
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
        from = (from & 0xf0) | ('e' - 'a');
        to = (to & 0xf0) | (castle0 == 2 ? 'g' - 'a' : 'c' - 'a');
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
        to = AlgebraicToIndex(&s[i - 1], 0x88);
        i -= 2;
    }
    // Skip the capture x
    if (i >= 0 && s[i] == 'x')
        i--;
    // rank or file for ambiguous moves
    if (i >= 0 && s[i] >= 'a' && s[i] <= 'h')
        from = (from & 0xf0) | (s[i--] - 'a');
    if (i >= 0 && s[i] >= '1' && s[i] <= '8')
        from = (from & 0x0f) | ((s[i--] - '1') << 4);

    // The Figure
    if (i >= 0 && s[i] >= 'A')
        pt = GetPieceType(s[i--]);

    // i < 0 hopefully
    // get the correct move
    for (int i = 0; i < ml.length; i++)
    {
        if (pt == (GETPIECE(ml.move[i].code) >> 1)
            && promotion == (GETPROMOTION(ml.move[i].code) >> 1)
            && ((from & 0x80) || ((from & 0x70) == ((GETFROM(ml.move[i].code) & 0x38) << 1)))
            && ((from & 0x08) || ((from & 0x07) == (GETFROM(ml.move[i].code) & 0x07)))
            && ((to & 0x80) || ((to & 0x70) == ((GETTO(ml.move[i].code) & 0x38) << 1)))
            && ((to & 0x08) || ((to & 0x07) == (GETTO(ml.move[i].code) & 0x07))))
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
bool PGNtoFEN(string pgnfilename)
{
    int gamescount = 0;
    bool mateFound;
    string line;
    string line1, line2;
    string rest_of_last_line = "";
    string fenfilename = pgnfilename + ".fen";
    string fenmovefilename = pgnfilename + ".fenmove";
    // Open the pgn file for reading
    ifstream pgnfile(pgnfilename);
    if (!pgnfile.is_open())
    {
        printf("Cannot open %s for reading.\n", pgnfilename.c_str());
        return false;
    }
    ofstream fenfile(fenfilename);
    if (!fenfile.is_open())
    {
        printf("Cannot open %s for writing.\n", fenfilename.c_str());
        return false;
    }
    ofstream fenmovefile(fenmovefilename);
    if (!fenmovefile.is_open())
    {
        printf("Cannot open %s for writing.\n", fenmovefilename.c_str());
        return false;
    }

    int newgamestarts = 0;
    int result = 0;
    string newfen;
    string moves;
    string lastmove;
    bool valueChecked;
    while (getline(pgnfile, line))
    {
        line2 = line1;
        line1 = line;

        smatch match;
        string fen;
        double score;
        // We assume that the [Result] section comes first, the the [FEN] section
        if (regex_search(line, match, regex("\\[Result\\s+\"(.*)\\-(.*)\"")))
        {
            gamescount++;
            // Write last game
            if (newgamestarts == 2)
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
        if (newgamestarts == 1 && regex_search(line, match, regex("\\[FEN\\s+\"(.*)\"")))
        {
            newfen = match.str(1);
            newgamestarts++;
            valueChecked = true;
            fen = newfen;
            pos.getFromFen(fen.c_str());
            fenfile << to_string(result) + "#" + fen + "#\n";
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
                if (regex_search(line, match, regex("^(\\s*\\d+\\.\\s+)")))
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
                        fenfile << to_string(result) + "#" + fen + "#0\n";
                        moves = moves + lastmove + " ";
                    }

                    foundInLine = true;
                    valueChecked = false;
                    lastmove = AlgebraicFromShort(match.str());
                    if (lastmove == "" || !pos.applyMove(lastmove))
                    {
                        printf("Alarm (game %d): %s\n", gamescount, match.str().c_str());
                        pos.print();
                        printf("last Lines:\n%s\n%s\n\n", line2.c_str(), line1.c_str());
                    }
                    fen = pos.toFen();
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
                        if (!mateFound)
                        {
                            fenfile << to_string(result) + "#" + fen + "#" + to_string(score) + "\n";
                            moves = moves + lastmove + " ";
                        }
                        line = match.suffix();
                        valueChecked = true;
                    }
                }
            } while (foundInLine);
        }
    }
    return true;
}



static string getValueStringValue(eval *e)
{
    string sm = to_string(GETMGVAL(*e));
    if (sm.length() < 4)
        sm.insert(sm.begin(), 4 - sm.length(), ' ');
    string se = to_string(GETEGVAL(*e));
    if (se.length() < 4)
        se.insert(se.begin(), 4 - se.length(), ' ');
    return "VALUE(" + sm + "," + se + ")";
}


static string nameTunedParameter(int i)
{
    string name = pos.tps.name[i];
    if (pos.tps.bound2[i] > 0)
    {
        name += "[" + to_string(pos.tps.index2[i]) + "][" + to_string(pos.tps.index1[i]) + "]";
    }
    else if (pos.tps.bound1[i] > 0)
    {
        name += "[" + to_string(pos.tps.index1[i]) + "] = { ";
    }
    return name;
}


static void printTunedParameters()
{
    string lastname = "";
    string output = "";
    for (int i = 0; i < pos.tps.count; i++)
    {
        if (lastname != pos.tps.name[i])
        {
            if (output != "")
            {
                output += ";\n";
                printf("%s", output.c_str());
                output = "";
            }
            lastname = pos.tps.name[i];
            output = "    eval " + lastname;
            if (pos.tps.bound2[i] > 0)
            {
                output += "[" + to_string(pos.tps.bound2[i]) + "][" + to_string(pos.tps.bound1[i]) + "] = {\n        { ";
            }
            else if (pos.tps.bound1[i] > 0)
            {
                output += "[" + to_string(pos.tps.bound1[i]) + "] = { ";
            }
            else {
                output += " = ";
            }
        }

        output = output + " " + getValueStringValue(pos.tps.ev[i]);

        if (pos.tps.index1[i] < pos.tps.bound1[i] - 1)
        {
            output += ",";
            if (!((pos.tps.index1[i] + 1) & (pos.tps.bound2[i] ? 0x7 : 0x7)))
                output += "\n          ";
        }
        else if (pos.tps.index1[i] == pos.tps.bound1[i] - 1)
        {
            output += "  }";
            if (pos.tps.index2[i] < pos.tps.bound2[i] - 1)
                output += ",\n        { ";
            else if (pos.tps.index2[i] == pos.tps.bound2[i] - 1)
                output += "\n    }";
        }
    }
    output += ";\n";
    printf("%s", output.c_str());
}


int tuningratio = 1;

positiontuneset *texelpts = NULL;
int texelptsnum;

static double TexelEvalError(double k)
{
    double Ri, Qi;
    double E = 0.0;

    for (int i = 0; i < texelptsnum; i++)
    {
        Qi = NEWTAPEREDEVAL(pos.getGradientValue(&texelpts[i]), texelpts[i].ph);
        Ri = texelpts[i].R / 2.0;
        double sigmoid = 1 / (1 + pow(10.0, -k * Qi / 400.0));
        E += (Ri - sigmoid) * (Ri - sigmoid);
    }

    return E / texelptsnum;
}

void getGradsFromFen(string fenfilename)
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
    int tuningphase = 0;
    while (tuningphase < 2)
    {
        n = 0;
        bw = 0;
        c = tuningratio;
        ifstream fenfile(fenfilename);
        if (!fenfile.is_open())
        {
            printf("Cannot open %s for reading.\n", fenfilename.c_str());
            return;
        }
        while (getline(fenfile, line))
        {
            if (!fenmovemode)
            {
                fen = "";
                if (regex_search(line, match, regex("(.*)#(.*)#(.*)")))
                {
                    fen = match.str(2);
                    R = (stoi(match.str(1)) + 1);
                }
                else if (regex_search(line, match, regex("(.*)\\s+((1\\-0)|(0\\-1)|(1/2))")))
                {
                    fen = match.str(1);
                    R = (match.str(2) == "1-0" ? 2 : (match.str(2) == "0-1" ? 0 : 1));
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
                        if (tuningphase == 1)
                        {
                            Qi = getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                            if (!pos.w2m())
                                Qi = -Qi;
                            texelpts[n].ph = pos.pts.ph;
                            texelpts[n].R = R;
                            Qa = 0;
                            for (int i = 0; i < pos.tps.count; i++)
                            {
                                texelpts[n].g[i] = pos.pts.g[i];
                                Qa += texelpts[n].g[i] * *pos.tps.ev[i];
                            }
                            if (MATEDETECTED(Qi))
                                n--;
                            else if (Qi != NEWTAPEREDEVAL(Qa, texelpts[n].ph))
                                printf("Alarm. Gradient evaluation differs from qsearch value.\n");
                        }
                        n++;
                    }
                }
            }
            else
            {
                if (regex_search(line, match, regex("(.*)#(.*)moves(.*)")))
                {
                    gamescount++;
                    R = (stoi(match.str(1)) + 1);
                    pos.getFromFen(match.str(2).c_str());
                    pos.ply = 0;
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
                            if (tuningphase == 1)
                            {
                                Qi = getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                                if (!pos.w2m())
                                    Qi = -Qi;
                                texelpts[n].ph = pos.pts.ph;
                                texelpts[n].R = R;
                                Qa = 0;
                                for (int i = 0; i < pos.tps.count; i++)
                                {
                                    texelpts[n].g[i] = pos.pts.g[i];
                                    Qa += texelpts[n].g[i] * *pos.tps.ev[i];
                                }
                                if (Qi != NEWTAPEREDEVAL(Qa, texelpts[n].ph))
                                    printf("Alarm. Gradient evaluation differs from qsearch value.\n");
                            }
                            n++;
                        }
                        gameend = (move == movelist.end());
                        if (!gameend)
                        {
                            if (!pos.applyMove(*move))
                            {
                                printf("Alarm (game %d)! Move %s seems illegal.\nLine: %s\n", gamescount, move->c_str(), line.c_str());
                                pos.print();
                            }
                            move++;
                        }

                    } while (!gameend);
                }
            }
        }
        tuningphase++;
        if (tuningphase == 1)
        {
            texelpts = (positiontuneset*)calloc(n, sizeof(positiontuneset));
        }
        texelptsnum = n;
        printf("Round %d Positions: %d\n", tuningphase, n);
    }
}


void TexelTune(string fenfilename)
{
    double k = 1.121574;
    int direction = 0;
    double Error;
    bool improved;
    double Emin = -1.0;
    int pmin;

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

    en.setOption("hash", "4"); // we don't need tt; save all the memory for game data
    getGradsFromFen(fenfilename);
    do
    {
        improved = false;
        for (int i = 0; i < pos.tps.count; i++)
        {
            if (pos.tps.tune[i])
            {
                printTunedParameters();
                if (en.verbose)
                    fprintf(stderr, "Tuning %s  %s\n", nameTunedParameter(i).c_str(), getValueStringValue(pos.tps.ev[i]).c_str());

                int tuned = 0;
                int g = 0;
                while (true)     // loop over mg/eg parameter while notImproved <=2
                {
                    tuned++;
                    if (tuned > 2)
                        break;
                    if (en.verbose)
                        fprintf(stderr, "Tuning %s...\n", g ? "eg" : "mg");
                    int pbound[2] = { SHRT_MAX, SHRT_MIN };
                    int delta = 1;
                    direction = 0; // direction=0: go right; delta > 0; direction=1: go right; delta
                    int v = *pos.tps.ev[i];
                    int mg = GETMGVAL(v);
                    int eg = GETEGVAL(v);
                    int lastp = (g ? eg : mg);
                    int p = lastp + delta;
                    *pos.tps.ev[i] = (g ? VALUE(mg, lastp) : VALUE(lastp, eg));
                    pmin = lastp;
                    if (Emin < 0)
                        Emin = TexelEvalError(k);
                    if (en.verbose)
                        fprintf(stderr, "Min: %d/%0.10f\n", pmin, Emin);
                    do
                    {
                        *pos.tps.ev[i] = (g ? VALUE(mg, p) : VALUE(p, eg));
                        Error = TexelEvalError(k);
                        if (en.verbose)
                            fprintf(stderr, "Min: %d/%0.10f  This: %d / %0.10f  bounds : %d / %d  delta = %d  direction : %d\n", pmin, Emin, p, Error, pbound[0], pbound[1], delta, direction);
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
                            improved = true;
                            tuned = 1;
                            pmin = p;
                            delta *= 2;
                            p = p + delta;
                        }
                    } while (abs(pbound[1] - pbound[0]) > 2);
                    *pos.tps.ev[i] = (g ? VALUE(mg, pmin) : VALUE(pmin, eg));

                    g = 1 - g;
                };
                if (en.verbose)
                    fprintf(stderr, "E=%0.10f  %s new tuned value: %s\n", Emin,
                        nameTunedParameter(i).c_str(), getValueStringValue(pos.tps.ev[i]).c_str());
                printf("E=%0.10f  %s new tuned value: %s\n", Emin,
                    nameTunedParameter(i).c_str(), getValueStringValue(pos.tps.ev[i]).c_str());
            }
        }
        printf("\nTuning-Summary:\n");
        printTunedParameters();
    } while (improved);
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
