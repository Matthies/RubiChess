
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


string AlgebraicFromShort(string s)
{
    string retval = "";
    int castle0 = 0;
    PieceType promotion = BLANKTYPE;
    chessmovelist* ml = pos.getMoves();
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
    for (int i = 0; i < ml->length; i++)
    {
        if (pt == (GETPIECE(ml->move[i].code) >> 1)
            && promotion == (GETPROMOTION(ml->move[i].code) >> 1)
            && ((from & 0x80) || ((from & 0x70) == ((GETFROM(ml->move[i].code) & 0x38) << 1)))
            && ((from & 0x08) || ((from & 0x07) == (GETFROM(ml->move[i].code) & 0x07)))
            && ((to & 0x80) || ((to & 0x70) == ((GETTO(ml->move[i].code) & 0x38) << 1)))
            && ((to & 0x08) || ((to & 0x07) == (GETTO(ml->move[i].code) & 0x07))))
        {
            // test if the move is legal; otherwise we need to search further
            if (pos.playMove(&ml->move[i]))
            {
                pos.unplayMove(&ml->move[i]);
                retval = ml->move[i].toString();
                break;
            }
        }
    }
    free(ml);
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
        //printf(">%s<\n", line.c_str());

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
                    //printf("%s\n", match.str().c_str());
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
                    //printf("FEN: %s\n", fen.c_str());
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


tuningintparameter tip[10000];
int tipnum = 0;

void registerTuner(int *addr, string name, int def, int index1, int bound1, int index2, int bound2, initevalfunc init, bool notune, int initialdelta)
{
    tip[tipnum].addr = addr;
    tip[tipnum].name = name;
    tip[tipnum].defval = def;
    tip[tipnum].initialdelta = initialdelta;
    tip[tipnum].index1 = index1;
    tip[tipnum].bound1 = bound1;
    tip[tipnum].index2 = index2;
    tip[tipnum].bound2 = bound2;
    tip[tipnum].init = init;
    tip[tipnum].notune = notune;
    tipnum++;
}


static string nameTunedParameter(int i)
{
    string name = tip[i].name;
    if (tip[i].bound2 > 0)
    {
        name += "[" + to_string(tip[i].index2) + "][" + to_string(tip[i].index1) + "]";
    }
    else if (tip[i].bound1 > 0)
    {
        name += "[" + to_string(tip[i].index1) + "] = { ";
    }
    return name;
}


static void printTunedParameters()
{
    string lastname = "";
    string output = "";
    for (int i = 0; i < tipnum; i++)
    {
        if (lastname != tip[i].name)
        {
            if (output != "")
            {
                output += ";\n";
                printf("%s", output.c_str());
                output = "";
            }
            lastname = tip[i].name;
            output = "CONSTEVAL int " + tip[i].name;
            if (tip[i].bound2 > 0)
            {
                output += "[" + to_string(tip[i].bound2) + "][" + to_string(tip[i].bound1) + "] = {\n  { ";
            }
            else if (tip[i].bound1 > 0)
            {
                output += "[" + to_string(tip[i].bound1) + "] = { ";
            }
            else {
                output += " = ";
            }
        }

        string numstr = to_string(*tip[i].addr);
        numstr.insert(numstr.begin(), 5 - numstr.length(), ' ');
        output = output + numstr;

        if (tip[i].index1 < tip[i].bound1 - 1)
        {
            output += ",";
            if (!((tip[i].index1 + 1) & (tip[i].bound2 ? 0x7 : 0x7)))
                output += "\n    ";
        }
        else if (tip[i].index1 == tip[i].bound1 - 1)
        {
            output += "  }";
            if (tip[i].index2 < tip[i].bound2 - 1)
                output += ",\n  { ";
            else if (tip[i].index2 == tip[i].bound2 - 1)
                output += "\n }";
        }
    }
    output += ";\n";
    printf("%s", output.c_str());
}


int tuningratio = 1;

static double TexelEvalError(string fenfilename, double k)
{
    long long starttime = getTime();
    int gamescount = 0;
    bool fenmovemode = (fenfilename.find(".fenmove") != string::npos);
    string line;
    smatch match;
    double Ri, Qi;
    double E = 0.0;
    int n = 0;
    int c = tuningratio;
    int bw = 0;
    ifstream fenfile(fenfilename);
    if (!fenfile.is_open())
    {
        printf("Cannot open %s for reading.\n", fenfilename.c_str());
        return 0.0;
    }
    while (getline(fenfile, line))
    {
        //cout << line + "\n";
        if (!fenmovemode)
        {
            if (regex_search(line, match, regex("(.*)#(.*)#(.*)")))
            {
                bw = 1 - bw;
                if (bw)
                    c++;
                if (c > tuningratio)
                    c = 1;
                if (c == tuningratio)
                {

                    Ri = (stoi(match.str(1)) + 1) / 2.0;
                    pos.getFromFen(match.str(2).c_str());
                    pos.ply = 0;
                    Qi = getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                    if (!pos.w2m())
                        Qi = -Qi;
                    //printf("Ri=%f Qi=%f\n", Ri, Qi);
                    n++;
                    double sigmoid = 1 / (1 + pow(10.0, -k * Qi / 400.0));
                    E += (Ri - sigmoid) * (Ri - sigmoid);
                }
            }
        }
        else
        {
            if (regex_search(line, match, regex("(.*)#(.*)moves(.*)")))
            {
                gamescount++;
                Ri = (stoi(match.str(1)) + 1) / 2.0;
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
                        Qi = getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
                        if (!pos.w2m())
                            Qi = -Qi;
                        //printf("FEN=%s Ri=%f Qi=%f\n", pos.toFen().c_str(), Ri, Qi);
                        n++;
                        double sigmoid = 1 / (1 + pow(10.0, -k * Qi / 400.0));
                        E += (Ri - sigmoid) * (Ri - sigmoid);
                }
                    gameend = (move == movelist.end());
                    if (!gameend)
                    {
                        if (!pos.applyMove(*move))
                        {
                            printf("Alarm (game %d)! Zug %s konnte nicht ausgef?hrt werden.\nLine: %s\n", gamescount, move->c_str(), line.c_str());
                            pos.print();
                        }
                        move++;
                    }

            } while (!gameend);
        }
    }
}
    printf("Positions: %d  Tuning-Zeit: %10f sec.", n, (float)(getTime() - starttime) / (float)en.frequency);
    return E / n;
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

    do
    {
        improved = false;
        for (int i = 0; i < tipnum; i++)
        {
            if (tip[i].notune == false)
            {
                printTunedParameters();
                printf("Tuning %s (default:%d)\n", nameTunedParameter(i).c_str(), tip[i].defval);

                int pbound[2] = { SHRT_MAX, SHRT_MIN };
                int delta = tip[i].initialdelta;
                direction = 0;
                // direction=0: go right; delta > 0; direction=1: go right; delta

                int lastp = *tip[i].addr;
                int p = lastp + delta;

                //*tip[i].addr = lastp;
                if (tip[i].init)
                    tip[i].init();
                pmin = lastp;
                if (Emin < 0)
                    Emin = TexelEvalError(fenfilename, k);
                printf("Min: %d/%0.10f\n", pmin, Emin);
                do
                {
                    *tip[i].addr = p;
                    if (tip[i].init)
                        tip[i].init();
                    Error = TexelEvalError(fenfilename, k);
                    printf("Min: %d/%0.10f  This: %d / %0.10f  bounds : %d / %d  delta = %d  direction : %d\n", pmin, Emin, p, Error, pbound[0], pbound[1], delta, direction);
                    if (Error >= Emin)
                    {
                        direction = (p > pmin ? 1 : 0);
                        pbound[direction] = p;
                        delta = (direction ? -1 : 1) * tip[i].initialdelta;
                        p = pmin + delta;
                    }
                    else
                    {
                        pbound[direction] = pmin;
                        Emin = Error;
                        improved = true;
                        pmin = p;
                        delta *= 2;
                        p = p + delta;
                    }
                } while (abs(pbound[1] - pbound[0]) > 2);
                *tip[i].addr = pmin;
                if (tip[i].init)
                    tip[i].init();
                printf("%s new tuned value: %d (Default:%d)\n", nameTunedParameter(i).c_str(), *tip[i].addr, tip[i].defval);
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
