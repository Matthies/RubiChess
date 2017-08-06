
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


string AlgebraicFromShort(string s, chessposition *p)
{
    string retval = "";
    int castle0 = 0;
    PieceType promotion = BLANKTYPE;
    chessmovelist* ml = p->getMoves();
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
        // Promotion
        promotion = GetPieceType(s[i--]);
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
        if (pt == (p->mailbox[GETFROM(ml->move[i].code)] >> 1)
            && promotion == (GETPROMOTION(ml->move[i].code) >> 1)
            && ((from & 0x80) || ((from & 0x70) == ((GETFROM(ml->move[i].code) & 0x38) << 1)))
            && ((from & 0x08) || ((from & 0x07) == (GETFROM(ml->move[i].code) & 0x07)))
            && ((to & 0x80) || ((to & 0x70) == ((GETTO(ml->move[i].code) & 0x38) << 1)))
            && ((to & 0x08) || ((to & 0x07) == (GETTO(ml->move[i].code) & 0x07))))
#else
        if (pt == (p->board[GETFROM(ml->move[i].code)] >> 1)
            && promotion == (GETPROMOTION(ml->move[i].code) >> 1)
            && ((from & 0x80) || ((from & 0x70) == (GETFROM(ml->move[i].code) & 0x70)))
            && ((from & 0x08) || ((from & 0x07) == (GETFROM(ml->move[i].code) & 0x07)))
            && ((to & 0x80) || ((to & 0x70) == (GETTO(ml->move[i].code) & 0x70)))
            && ((to & 0x08) || ((to & 0x07) == (GETTO(ml->move[i].code) & 0x07))))
#endif
        {
            retval = ml->move[i].toString();
            break;
        }
    }
    free(ml);
    return retval;
}

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
    return 1000000000 * now.tv_sec + now.tv_nsec;
}

void Sleep(long x)
{
    struct timespec now;
    now.tv_sec = 0;
    now.tv_nsec = x * 1000000;
    nanosleep(&now, NULL);
}

#endif
