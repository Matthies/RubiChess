
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
    s.push_back(FILE(i) + 'a');
    s.push_back(RANK(i) + '1');
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
#ifdef BITBOARD
        if (pt == (GETPIECE(ml->move[i].code) >> 1)
            && promotion == (GETPROMOTION(ml->move[i].code) >> 1)
            && ((from & 0x80) || ((from & 0x70) == ((GETFROM(ml->move[i].code) & 0x38) << 1)))
            && ((from & 0x08) || ((from & 0x07) == (GETFROM(ml->move[i].code) & 0x07)))
            && ((to & 0x80) || ((to & 0x70) == ((GETTO(ml->move[i].code) & 0x38) << 1)))
            && ((to & 0x08) || ((to & 0x07) == (GETTO(ml->move[i].code) & 0x07))))
#else
        if (pt == (pos.mailbox[GETFROM(ml->move[i].code)] >> 1)
            && promotion == (GETPROMOTION(ml->move[i].code) >> 1)
            && ((from & 0x80) || ((from & 0x70) == (GETFROM(ml->move[i].code) & 0x70)))
            && ((from & 0x08) || ((from & 0x07) == (GETFROM(ml->move[i].code) & 0x07)))
            && ((to & 0x80) || ((to & 0x70) == (GETTO(ml->move[i].code) & 0x70)))
            && ((to & 0x08) || ((to & 0x07) == (GETTO(ml->move[i].code) & 0x07))))
#endif
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
    string line;
    string rest_of_last_line = "";
    string fenfilename = pgnfilename + ".fen";
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

    int newgamestarts = 0;
    int result = 0;
    while (getline(pgnfile, line))
    {
        //printf(">%s<\n", line.c_str());

        smatch match;
        string fen, newfen;
        string moves = "";
        double score;
        bool valueChecked = true;
        // We assume that the [Result] section comes first, the the [FEN] section
        if (regex_search(line, match, regex("\\[Result\\s+\"(.*)\\-(.*)\"")))
        {
            if (match.str(1) == "0")
                result = -1;
            else if (match.str(1) == "1")
                result = 1;
            else
                result = 0;
            newgamestarts = 1;
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
        // search for the moves
        if (newgamestarts == 2 && !regex_search(line, match, regex("\\[.*\\]")))
        {
            bool foundInLine;
            do
            {
                foundInLine = false;
                if (valueChecked && regex_search(line, match, regex("([O\\-]{3,5})|([KQRBN]?[a-h]*[1-8]*x?[a-h]+[1-8]+(=[QRBN])?)\\+?")))
                {
                    // Found move
                    //printf("%s\n", match.str().c_str());
                    foundInLine = true;
                    valueChecked = false;
                    string move = AlgebraicFromShort(match.str());
                    if (move == "" || !pos.applyMove(move))
                    {
                        printf("Alarm: %s\n", match.str().c_str());
                        pos.print();
                    }
                    fen = pos.toFen();
                    //printf("FEN: %s\n", fen.c_str());
                    line = match.suffix();
                }
                if (!valueChecked && regex_search(line, match, regex("\\{((\\+|\\-)?((\\d+\\.\\d+)|(M\\d+)))\\/")))
                {
                    foundInLine = true;
                    valueChecked = true;
                    string scorestr = match.str(1);
                    // Only output if no mate score detected
                    if (!(scorestr[0] == 'M' || scorestr[1] == 'M'))
                    {
                        score = stod(match.str(1));
                        //printf("score:%f\n", score);
                        line = match.suffix();
                        if (abs(score) < 30)
                            fenfile << to_string(result) + "#" + fen + "#" + to_string(score) + "\n";
                    }
                }
            } while (foundInLine);
        }
    }
    return true;
}


static double TexelEvalError(string fenfilename, double k)
{
    string line;
    smatch match;
    double Ri, Qi;
    double E = 0.0;
    int n = 0;
    ifstream fenfile(fenfilename);
    if (!fenfile.is_open())
    {
        printf("Cannot open %s for reading.\n", fenfilename.c_str());
        return 0.0;
    }
    while (getline(fenfile, line))
    {
       //cout << line + "\n";
        if (regex_search(line, match, regex("(.*)#(.*)#(.*)")))
        {
            Ri = (stoi(match.str(1)) + 1) / 2.0;
            pos.getFromFen(match.str(2).c_str());
            Qi = getQuiescence(SHRT_MIN + 1, SHRT_MAX, 0);
            if (!pos.w2m())
                Qi = -Qi;
            //printf("Ri=%f Qi=%f\n", Ri, Qi);
            n++;
            double sigmoid = 1 / (1 + pow(10.0, -k * Qi / 400.0));
            E += (Ri - sigmoid) * (Ri - sigmoid);
        }
        else
            printf("Alarm");
    }
    return E / n;
}


void TexelTune(string fenfilename)
{
    double E;
    double k = 2.00;
    double delta = 0.5;
    double lastE = TexelEvalError(fenfilename, k - delta);

    while (abs(delta) > 0.0001)
    {
        E = TexelEvalError(fenfilename, k);
        printf("k=%0.4f  E=%0.10f  \n", k, E);
        if (E > lastE)
            delta = -delta / 2;
        k += delta;
        lastE = E;


    }
    printf("Texel-Tune:%f\n", TexelEvalError(fenfilename, k));
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
