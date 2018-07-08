
#include "RubiChess.h"

U64 mybitset[64];

PieceType GetPieceType(char c)
{
    switch (c)
    {
    case 'n':
    case 'N':
        return KNIGHT;
    case 'b':
    case 'B':
        return BISHOP;
    case 'r':
    case 'R':
        return ROOK;
    case 'q':
    case 'Q':
        return QUEEN;
    case 'k':
    case 'K':
        return KING;
    default:
        break;
    }
    return BLANKTYPE;
}


char PieceChar(PieceCode c)
{
    PieceType p = (PieceType)(c >> 1);
    int color = (c & 1);
    char o;
    switch (p)
    {
    case PAWN:
        o = 'p';
        break;
    case KNIGHT:
        o = 'n';
        break;
    case BISHOP:
        o = 'b';
        break;
    case ROOK:
        o = 'r';
        break;
    case QUEEN:
        o = 'q';
        break;
    case KING:
        o = 'k';
        break;
    default:
        o = ' ';
        break;
    }
    if (!color)
        o = (char)(o + ('A' - 'a'));
    return o;
}
 
chessmovestack movestack[MAXMOVESEQUENCELENGTH];
int mstop;


chessmove::chessmove(int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece)
{
    code = (piece << 28) | (ept << 20) | (capture << 16) | (promote << 12) | (from << 6) | to;
}

chessmove::chessmove(int from, int to, PieceCode promote, PieceCode capture, PieceCode piece)
{
    code = (piece << 28) | (capture << 16) | (promote << 12) | (from << 6) | to;
}


chessmove::chessmove(int from, int to, PieceCode piece)
{
    code = (piece << 28) | (from << 6) | to;
}


chessmove::chessmove(int from, int to, PieceCode capture, PieceCode piece)
{
    code = (piece << 28) | (capture << 16) | (from << 6) | to;
}

chessmove::chessmove()
{
    code = 0;
}

string chessmove::toString()
{
    char s[100];

    if (code == 0)
        return "(none)";

    int from, to;
    PieceCode promotion;
    from = GETFROM(code);
    to = GETTO(code);
    promotion = GETPROMOTION(code);

    sprintf_s(s, "%c%d%c%d%c", (from & 0x7) + 'a', ((from >> 3) & 0x7) + 1, (to & 0x7) + 'a', ((to >> 3) & 0x7) + 1, PieceChar(promotion));
    return s;
}

void chessmove::print()
{
    cout << toString();
}


chessmovelist::chessmovelist()
{
    length = 0;
}

string chessmovelist::toString()
{
    string s = "";
    for (int i = 0; i < length; i++ )
    {
        s = s + move[i].toString() + " ";
    }
    return s;
}

string chessmovelist::toStringWithValue()
{
    string s = "";
    for (int i = 0; i < length; i++)
    {
        s = s + move[i].toString() + "(" + to_string((int)move[i].value) + ") ";
    }
    return s;
}

void chessmovelist::print()
{
    printf("%s", toString().c_str());
}


chessmovesequencelist::chessmovesequencelist()
{
    length = 0;
}

string chessmovesequencelist::toString()
{
    string s = "";
    for (int i = 0; i < length; i++)
    {
        s = s + move[i].toString() + " ";
    }
    return s;
}

void chessmovesequencelist::print()
{
    printf("%s", toString().c_str());
}


bool chessposition::w2m()
{
    return !(state & S2MMASK);
}


PieceType chessposition::Piece(int index)
{
    return (PieceType)(mailbox[index] >> 1);
}


int chessposition::getFromFen(const char* sFen)
{
    string s;
    vector<string> token = SplitString(sFen);
    int numToken = (int)token.size();

    for (int i = 0; i < 14; i++)
        piece00[i] = 0ULL;

    for (int i = 0; i < BOARDSIZE; i++)
        mailbox[i] = BLANK;

    for (int i = 0; i < 2; i++)
        occupied00[i] = 0ULL;

    // At least four token are needed (EPD style string)
    if (numToken < 4)
        return -1;

    /* the board */
    s = token[0];
    int rank = 7;
    int file = 0;
    for (unsigned int i = 0; i < s.length(); i++)
    {
        PieceCode p;
        int num = 1;
        int index = INDEX(rank, file);
        char c = s[i];
        switch (c)
        {
        case 'k':
            p = BKING;
            kingpos[1] = index;
            break;
        case 'q':
            p = BQUEEN;
            break;
        case 'r':
            p = BROOK;
            break;
        case 'b':
            p = BBISHOP;
            break;
        case 'n':
            p = BKNIGHT;
            break;
        case 'p':
            p = BPAWN;
            break;
        case 'K':
            p = WKING;
            kingpos[0] = index;
            break;
        case 'Q':
            p = WQUEEN;
            break;
        case 'R':
            p = WROOK;
            break;
        case 'B':
            p = WBISHOP;
            break;
        case 'N':
            p = WKNIGHT;
            break;
        case 'P':
            p = WPAWN;
            break;
        case '/':
            rank--;
            num = 0;
            file = 0;
            break;
        default:	/* digit */
            num = 0;
            file += (c - '0');
            break;
        }
        if (num)
        {
            mailbox[index] = p;
            BitboardSet(index, p);
            file++;
        }
    }

    if (rank != 0 || file != 8)
        return -1;

    if (numToken < 0)
        return -1;

    state = 0;
    /* side to move */
    if (token[1] == "b")
        state |= S2MMASK;

    /* castle rights */
    s = token[2];
    for (unsigned int i = 0; i < s.length(); i++)
    {
        switch (s[i])
        {
        case 'Q':
            state |= WQCMASK;
            break;
        case 'K':
            state |= WKCMASK;
            break;
        case 'q':
            state |= BQCMASK;
            break;
        case 'k':
            state |= BKCMASK;
            break;
        default:
            break;
        }
    }

    /* en passant target */
    ept = 0;
    s = token[3];
    if (s.length() == 2)
    {
        int i = AlgebraicToIndex(s, BOARDSIZE);
        if (i < 0x88)
        {
            ept = i;
        }
    }

    /* half moves */
    if (numToken > 4)
        halfmovescounter = stoi(token[4]);

    /* full moves */
    if (numToken > 5)
        fullmovescounter = stoi(token[5]);

    isCheck = isAttacked(kingpos[state & S2MMASK]);

    actualpath.length = 0;
    hash = zb.getHash();
    pawnhash = zb.getPawnHash();
    materialhash = zb.getMaterialHash();
    rp.clean();
    rp.addPosition(hash);
    for (int i = 0; i < 7; i++)
    {
        for (int j = 0; j < BOARDSIZE; j++)
        {
            history[i][j] = 0;
        }
    }
    for (int i = 0; i < MAXDEPTH; i++)
        killer[0][i] = killer[1][i] = 0;
    mstop = 0;
    return 0;
}



bool chessposition::applyMove(string s)
{
    unsigned int from, to;
    bool retval = false;
    PieceCode promotion;

    from = AlgebraicToIndex(s, BOARDSIZE);
    to = AlgebraicToIndex(&s[2], BOARDSIZE);
    chessmovelist cmlist;
    cmlist.length = getMoves(&cmlist.move[0]);
    if (s.size() > 4)
        promotion = (PieceCode)((GetPieceType(s[4]) << 1) | (state & S2MMASK));
    else
        promotion = BLANK;
    for (int i = 0; i < cmlist.length; i++)
    {
        if (GETFROM(cmlist.move[i].code) == from && GETTO(cmlist.move[i].code) == to && GETPROMOTION(cmlist.move[i].code) == promotion)
        {
            if (playMove(&(cmlist.move[i])))
            {
                if (halfmovescounter == 0)
                {
                    // Keep the list short, we have to keep below MAXMOVELISTLENGTH
                    actualpath.length = 0;
                    mstop = 0;
                }
                retval = true;
            }
            else {
                retval = false;
            }
            break;
        }
    }
    return retval;
}


void chessposition::getRootMoves()
{
    // Precalculating the list of legal moves didn't work well for some unknown reason but we need the number of legal moves in MultiPV mode
    chessmovelist movelist;
    movelist.length = getMoves(&movelist.move[0]);
    int bestval = SCOREBLACKWINS;
    rootmovelist.length = 0;
    for (int i = 0; i < movelist.length; i++)
    {
        if (playMove(&movelist.move[i]))
        {
            rootmovelist.move[rootmovelist.length++] = movelist.move[i];
            unplayMove(&movelist.move[i]);
            if (bestval < movelist.move[i].value)
            {
                defaultmove = movelist.move[i];
                bestval = movelist.move[i].value;
            }
        }
    }
}


void chessposition::tbFilterRootMoves()
{
    useTb = TBlargest;
    tbPosition = 0;
    useRootmoveScore = 0;
    if (POPCOUNT(pos.occupied00[0] | pos.occupied00[1]) <= TBlargest)
    {
        if ((tbPosition = root_probe())) {
            en.tbhits++;
            // The current root position is in the tablebases.
            // RootMoves now contains only moves that preserve the draw or win.

            // Do not probe tablebases during the search.
            useTb = 0;
        }
        else // If DTZ tables are missing, use WDL tables as a fallback
        {
            // Filter out moves that do not preserve a draw or win
            tbPosition = root_probe_wdl();
            // useRootmoveScore is set within root_probe_wdl
        }

        if (tbPosition)
        {
            // Sort the moves
            for (int i = 0; i < pos.rootmovelist.length; i++)
            {
                for (int j = i + 1; j < pos.rootmovelist.length; j++)
                {
                    if (pos.rootmovelist.move[i] < pos.rootmovelist.move[j])
                    {
                        swap(pos.rootmovelist.move[i], pos.rootmovelist.move[j]);
                    }
                }
            }
        }
    }
}


/* test the actualmove for three-fold-repetition as the repetition table may give false positive due to table collisions */
int chessposition::testRepetiton()
{
    unsigned long long h = hash;
    chessmovesequencelist *ml = &actualpath;
    int oldlength = ml->length;
    int hit = 0;
    int i;
    for (i = oldlength; i > 0;)
    {
        if (ml->move[--i].code == 0)
            unplayNullMove();
        else
            unplayMove(&ml->move[i]);
        if (hash == h)
            hit++;
        if (halfmovescounter == 0)
            break;
    }
    for (; i < oldlength;)
    {
        if (ml->move[i].code == 0)
        {
            playNullMove();
        }
        else
        {
            if (!playMove(&ml->move[i]))
            {
                printf("Alarm. Wie kommt ein illegaler Zug %s (%d) in die actuallist\n", ml->move[i].toString().c_str(), i);
                ml->print();
            }
        }
        i++;
    }

    return hit;
}



void chessposition::mirror()
{
    int newmailbox[BOARDSIZE] = { 0 };
    for (int r = 0; r < 8; r++)
    {
        for (int f = 0; f < 8; f++)
        {
            int index = INDEX(r, f);
            int mirrorindex = INDEX(7 - r, f);
            if (mailbox[index] != BLANK)
            {
                newmailbox[mirrorindex] = mailbox[index] ^ S2MMASK;
                BitboardClear(index, mailbox[index]);
                if ((mailbox[index] >> 1) == PAWN)
                    pawnhash ^= zb.boardtable[(index << 4) | mailbox[index]];
            }
            else {
                newmailbox[mirrorindex] = BLANK;
            }
        }
    }

    for (int i = 0; i < BOARDSIZE; i++)
    {
        mailbox[i] = newmailbox[i];
        if (mailbox[i] != BLANK)
        {
            BitboardSet(i, mailbox[i]);
            if ((mailbox[i] >> 1) == PAWN)
                pawnhash ^= zb.boardtable[(i << 4) | mailbox[i]];
        }
    }

    int newstate = (state & S2MMASK) ^ S2MMASK;
    if (state & WQCMASK) newstate |= BQCMASK;
    if (state & WKCMASK) newstate |= BKCMASK;
    if (state & BQCMASK) newstate |= WQCMASK;
    if (state & BKCMASK) newstate |= WKCMASK;
    state = newstate;
    if (ept)
        ept ^= RANKMASK;

    int kingpostemp = kingpos[0];
    kingpos[0] = kingpos[1] ^= RANKMASK;
    kingpos[1] = kingpostemp ^= RANKMASK;
}


void chessposition::playNullMove()
{
    state ^= S2MMASK;
    hash ^= zb.s2m;
    actualpath.move[actualpath.length++].code = 0;
    ply++;
}


void chessposition::unplayNullMove()
{
    state ^= S2MMASK;
    hash ^= zb.s2m;
    actualpath.length--;
    ply--;
}


void chessposition::getpvline(int depth, int pvnum)
{
    int dummyval;
    chessmove cm;
    pvline.length = 0;
    while (depth > 0)
    {
        if (pvline.length == 0 && bestmove[pvnum].code != 0)
        {
            cm = bestmove[pvnum];
        }
        else if (!tp.probeHash(&dummyval, &(cm.code), depth, 0, 0) || cm.code == 0)
        {
            break;
        }

        if (!playMove(&cm))
        {
            printf("info string Alarm - Illegaler Zug %s in pvline\n", cm.toString().c_str());
            print();
            tp.printHashentry();
        }
        pvline.move[pvline.length++] = cm;
        depth--;
    }
    for (int i = pvline.length; i;)
        unplayMove(&(pvline.move[--i]));
}



inline void chessposition::testMove_old(chessmovelist *movelist, int from, int to, PieceCode promote, PieceCode capture, PieceCode piece)
{
    chessmove cm(from, to, promote, capture, piece);
    if (capture != BLANK)
    {
        cm.value = (mvv[capture >> 1] | lva[piece >> 1]);
    }
    else {
        cm.value = history[piece >> 1][to];
    }
    movelist->move[movelist->length++] = cm;
}


inline void chessposition::testMove_old(chessmovelist *movelist, int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece)
{
    chessmove cm(from, to, promote, capture, ept, piece);
    if (capture != BLANK)
    {
        cm.value = (mvv[capture >> 1] | lva[piece >> 1]);
    }
    else {
        cm.value = history[piece >> 1][to];
    }
    movelist->move[movelist->length++] = cm;
}


void chessposition::print()
{
    for (int r = 7; r >= 0; r--)
    {
        printf("info string ");
        for (int f = 0; f < 8; f++)
        {
            char pc = PieceChar(mailbox[INDEX(r, f)]);
            if (pc == 0)
                pc = '.';
            printf("%c", pc);
        }
        printf("\n");
    }
    chessmovelist pseudolegalmoves;
    pseudolegalmoves.length = getMoves(&pseudolegalmoves.move[0]);
    printf("info string FEN: %s\n", toFen().c_str());
    printf("info string State: %0x\n", state);
    printf("info string EPT: %0x\n", ept);
    printf("info string Halfmoves: %d\n", halfmovescounter);
    printf("info string Fullmoves: %d\n", fullmovescounter);
    printf("info string Hash: %llu (%llx)  (getHash(): %llu)\n", hash, hash, zb.getHash());
    printf("info string Pawn Hash: %llu (%llx)  (getPawnHash(): %llu)\n", pawnhash, pawnhash, zb.getPawnHash());
    printf("info string Value: %d\n", getValue<NOTRACE>());
    printf("info string Repetitions: %d\n", rp.getPositionCount(hash));
    printf("info string Phase: %d\n", phase());
    printf("info string Pseudo-legal Moves: %s\n", pseudolegalmoves.toStringWithValue().c_str());
    if (tp.size > 0 && tp.testHash())
    {
        chessmove cm;
        cm.code = tp.getMoveCode();
        printf("info string Hash-Info: depth=%d Val=%d (%d) Move:%s\n", tp.getDepth(), tp.getValue(), tp.getValtype(), cm.toString().c_str());
    }
    if (actualpath.length)
        printf("info string Moves in current search: %s\n", actualpath.toString().c_str());
}


string chessposition::toFen()
{
    int index;
    string s = "";
    for (int r = 7; r >= 0; r--)
    {
        int blanknum = 0;
        for (int f = 0; f < 8; f++)
        {
            char c = 0;
            index = INDEX(r, f);
            switch (Piece(index))
            {
            case PAWN:
                c = 'p';
                break;
            case BISHOP:
                c = 'b';
                break;
            case KNIGHT:
                c = 'n';
                break;
            case ROOK:
                c = 'r';
                break;
            case QUEEN:
                c = 'q';
                break;
            case KING:
                c = 'k';
                break;
            default:
                blanknum++;

            }
            if (c)
            {
                if (!(mailbox[index] & S2MMASK))
                {
                    c += 'A' - 'a';
                }
                if (blanknum)
                {
                    s += to_string(blanknum);
                    blanknum = 0;
                }
                s += c;
            }
        }
        if (blanknum)
            s += to_string(blanknum);
        if (r)
            s += "/";
    }
    s += " ";

    // side 2 move
    s += ((state & S2MMASK) ? "b " : "w ");

    // castle rights
    if (!(state & CASTLEMASK))
        s += "-";
    else
    {
        if (state & WKCMASK)
            s += "K";
        if (state & WQCMASK)
            s += "Q";
        if (state & BKCMASK)
            s += "k";
        if (state & BQCMASK)
            s += "q";
    }
    s += " ";

    // EPT
    if (!ept)
        s += "-";
    else
        s += IndexToAlgebraic(ept);

    // halfmove and fullmove counter
    s += " " + to_string(halfmovescounter) + " " + to_string(fullmovescounter);
    return s;
}



#ifdef DEBUG
void chessposition::debug(int depth, const char* format, ...)
{
    if (depth > maxdebugdepth || depth < mindebugdepth)
        return;
    printf("!");
    for (int i = 0; i < maxdebugdepth - depth; i++)
    {
        if (depth & 1)
            printf("-");
        else
            printf("+");
    }
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    va_end(argptr);
}
#endif

#ifdef DEBUGEVAL
void chessposition::debugeval(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    va_end(argptr);
}
#endif


U64 knight_attacks[64];
U64 king_attacks[64];
U64 pawn_attacks_free[64][2];
U64 pawn_attacks_free_double[64][2];
U64 pawn_attacks_occupied[64][2];
U64 epthelper[64];
U64 passedPawnMask[64][2];
U64 filebarrierMask[64][2];
U64 neighbourfilesMask[64];
U64 phalanxMask[64];
U64 kingshieldMask[64][2];
U64 kingdangerMask[64][2];
U64 fileMask[64];
U64 rankMask[64];
int castleindex[64][64] = { 0 };

// shameless copy from http://chessprogramming.wikispaces.com/Magic+Bitboards#Plain
U64 mBishopAttacks[64][1 << BISHOPINDEXBITS];
U64 mRookAttacks[64][1 << ROOKINDEXBITS];

SMagic mBishopTbl[64];
SMagic mRookTbl[64];



U64 patternToMask(int i, int d, int p)
{
    U64 occ = 0ULL;
    int j = i;
    // run to lower border
    while (ISNEIGHBOUR(j, j - d)) j -= d;
    while (p)
    {
        // loop steps to upper border
        if (p & 1)
            occ |= (1ULL << j);
        p >>= 1;
        if (!ISNEIGHBOUR(j, j + d))
            p = 0;
        j += d;
    }
    return occ;
}


U64 getAttacks(int index, U64 occ, int delta)
{
    U64 attacks = 0ULL;
    U64 blocked = 0ULL;
    for (int shift = index + delta; ISNEIGHBOUR(shift, shift - delta); shift += delta)
    {
        if (!blocked)
        {
            attacks |= BITSET(shift);
        }
        blocked |= ((1ULL << shift) & occ);
    }
    return attacks;
}


U64 getOccupiedFromMBIndex(int j, U64 mask)
{
    U64 occ = 0ULL;
    int k;
    int i = 0;
    while (LSB(k, mask))
    {
        if (j & BITSET(i))
            occ |= (1ULL << k);
        mask ^= BITSET(k);
        i++;
    }
    return occ;
}


U64 getMagicCandidate(U64 mask)
{
    U64 magic;
    do {
        magic = zb.getRnd() & zb.getRnd() & zb.getRnd();
    } while (POPCOUNT((mask * magic) & 0xFF00000000000000ULL) < 6);
    return magic;
}


void initBitmaphelper()
{
    int to;
    castleindex[4][2] = WQC;
    castleindex[4][6] = WKC;
    castleindex[60][58] = BQC;
    castleindex[60][62] = BKC;
    for (int from = 0; from < 64; from++)
    {
        mybitset[from] = (1ULL << from);
    }

    printf("Searching for magics, may take a while .");
    for (int from = 0; from < 64; from++)
    {
        king_attacks[from] = knight_attacks[from] = 0ULL;
        pawn_attacks_free[from][0] = pawn_attacks_occupied[from][0] = pawn_attacks_free_double[from][0] = 0ULL;
        pawn_attacks_free[from][1] = pawn_attacks_occupied[from][1] = pawn_attacks_free_double[from][1] = 0ULL;
        passedPawnMask[from][0] = passedPawnMask[from][1] = 0ULL;
        filebarrierMask[from][0] = filebarrierMask[from][1] = 0ULL;
        phalanxMask[from] = 0ULL;
        kingshieldMask[from][0] = kingshieldMask[from][1] = 0ULL;
        kingdangerMask[from][0] = kingdangerMask[from][1] = 0ULL;
        neighbourfilesMask[from] = 0ULL;
        fileMask[from] = 0ULL;
        rankMask[from] = 0ULL;

        for (int j = 0; j < 64; j++)
        {
            if (abs(FILE(from) - FILE(j)) == 1)
            {
                neighbourfilesMask[from] |= BITSET(j);
                if (RANK(from) == RANK(j))
                    phalanxMask[from] |= BITSET(j);
            }
            if (FILE(from) == FILE(j))
                fileMask[from] |= BITSET(j);
            if (RANK(from) == RANK(j))
                rankMask[from] |= BITSET(j);
        }

        for (int j = 0; j < 8; j++)
        {
            // King attacks
            to = from + orthogonalanddiagonaloffset[j];
            if (to >= 0 && to < 64 && abs(FILE(from) - FILE(to)) <= 1)
                king_attacks[from] |= BITSET(to);

            // Knight attacks
            to = from + knightoffset[j];
            if (to >= 0 && to < 64 && abs(FILE(from) - FILE(to)) <= 2)
                knight_attacks[from] |= BITSET(to);
        }

        // Pawn attacks; even for rank 0 and 7 needed for the isAttacked detection
        for (int s = 0; s < 2; s++)
        {
            pawn_attacks_free[from][s] |= BITSET(from + S2MSIGN(s) * 8);
            if (RANK(from) == 1 + 5 * s)
            {
                // Double move
                pawn_attacks_free_double[from][s] |= BITSET(from + S2MSIGN(s) * 16);
            }
            // Captures
            for (int d = -1; d <= 1; d++)
            {
                to = from + S2MSIGN(s) * 8 + d;
                if (abs(FILE(from) - FILE(to)) <= 1 && to >= 0 && to < 64)
                {
                    if (d)
                        pawn_attacks_occupied[from][s] |= BITSET(to);
                    for (int r = to; r >= 0 && r < 64; r += S2MSIGN(s) * 8)
                    {
                        passedPawnMask[from][s] |= BITSET(r);
                        if (!d)
                            filebarrierMask[from][s] |= BITSET(r);
                        if (abs(RANK(from) - RANK(r)) <= 2)
                            kingshieldMask[from][s] |= BITSET(r);
                    }
                }
            }
            kingdangerMask[from][s] = king_attacks[from] | kingshieldMask[from][s];
        }

        // Slider attacks
        // Fill the mask
        mBishopTbl[from].mask = 0ULL;
        mRookTbl[from].mask = 0ULL;
        for (int j = 0; j < 64; j++)
        {
            if (from == j)
                continue;
            if (RANK(from) == RANK(j) && !OUTERFILE(j))
                mRookTbl[from].mask |= BITSET(j);
            if (FILE(from) == FILE(j) && !PROMOTERANK(j))
                mRookTbl[from].mask |= BITSET(j);
            if (abs(RANK(from) - RANK(j)) == abs(FILE(from) - FILE(j)) && !OUTERFILE(j) && !PROMOTERANK(j))
                mBishopTbl[from].mask |= BITSET(j);
        }

        // Search for magic numbers and initialize the attack tables
        while (true)
        {
            bool magicOk = true;
            
            // Clear attack bitmaps to detect hash collisions
            for (int j = 0; j < (1 << BISHOPINDEXBITS); j++)
                mBishopAttacks[from][j] = 0ULL;
            
            // Get a random number with few bits set as a possible magic number
            mBishopTbl[from].magic = getMagicCandidate(mBishopTbl[from].mask);

            for (int j = 0; magicOk && j < (1 << BISHOPINDEXBITS); j++) {
                // First get the subset of mask corresponding to j
                U64 occ = getOccupiedFromMBIndex(j, mBishopTbl[from].mask);
                // Now get the attack bitmap for this subset and store to attack table
                U64 attack = (getAttacks(from, occ, -7) | getAttacks(from, occ, 7) | getAttacks(from, occ, -9) | getAttacks(from, occ, 9));
                int hashindex = MAGICBISHOPINDEX(occ, from);
                if (mBishopAttacks[from][hashindex] == 0ULL)
                    mBishopAttacks[from][hashindex] = attack;
                else if (mBishopAttacks[from][hashindex] != attack)
                    magicOk = false;
            }
            if (magicOk)
                break;
        }

        while (true)
        {
            bool magicOk = true;

            // Clear attack bitmaps to detect hash collisions
            for (int j = 0; j < (1 << ROOKINDEXBITS); j++)
                mRookAttacks[from][j] = 0ULL;

            // Get a random number with few bits set as a possible magic number
            mRookTbl[from].magic = getMagicCandidate(mRookTbl[from].mask);

            for (int j = 0; magicOk && j < (1 << ROOKINDEXBITS); j++) {
                // First get the subset of mask corresponding to j
                U64 occ = getOccupiedFromMBIndex(j, mRookTbl[from].mask);
                // Now get the attack bitmap for this subset and store to attack table
                U64 attack = (getAttacks(from, occ, -1) | getAttacks(from, occ, 1) | getAttacks(from, occ, -8) | getAttacks(from, occ, 8));
                int hashindex = MAGICROOKINDEX(occ, from);
                if (mRookAttacks[from][hashindex] == 0ULL)
                    mRookAttacks[from][hashindex] = attack;
                else if (mRookAttacks[from][hashindex] != attack)
                    magicOk = false;
            }
            if (magicOk)
                break;
        }
        printf(".");
        fflush(stdout);
        epthelper[from] = 0ULL;
        if (RANK(from) == 3 || RANK(from) == 4)
        {
            if (RANK(from - 1) == RANK(from))
                epthelper[from] |= BITSET(from - 1);
            if (RANK(from + 1) == RANK(from))
                epthelper[from] |= BITSET(from + 1);
        }
    }
    printf(" ready.\n");
}


chessposition::chessposition()
{
    //init();
}


chessposition::~chessposition()
{
    delete positionvaluetable;
}


bool chessposition::operator==(chessposition p)
{
    return true;
}



void chessposition::BitboardSet(int index, PieceCode p)
{
    int s2m = p & 0x1;
    piece00[p] |= BITSET(index);
    occupied00[s2m] |= BITSET(index);
}


void chessposition::BitboardClear(int index, PieceCode p)
{
    int s2m = p & 0x1;
    piece00[p] ^= BITSET(index);
    occupied00[s2m] ^= BITSET(index);
}


void chessposition::BitboardMove(int from, int to, PieceCode p)
{
    int s2m = p & 0x1;
    piece00[p] ^= (BITSET(from) | BITSET(to));
    occupied00[s2m] ^= (BITSET(from) | BITSET(to));
}


void chessposition::BitboardPrint(U64 b)
{
    for (int r = 7; r >= 0; r--)
    {
        for (int f = 0; f < 8; f++)
        {
            printf("%s", (b & BITSET(r * 8 + f) ? "x" : "."));
        }
        printf("\n");
    }
}


bool chessposition::playMove(chessmove *cm)
{
    int oldcastle = (state & CASTLEMASK);
    int s2m = state & S2MMASK;
    int from = GETFROM(cm->code);
    int to = GETTO(cm->code);
    PieceCode pfrom = GETPIECE(cm->code);
    PieceType ptype = (pfrom >> 1);
    int eptnew = GETEPT(cm->code);

    PieceCode promote = GETPROMOTION(cm->code);
    PieceCode capture = GETCAPTURE(cm->code);

    movestack[mstop].ept = ept;
    movestack[mstop].hash = hash;
    movestack[mstop].pawnhash = pawnhash;
    movestack[mstop].materialhash = materialhash;
    movestack[mstop].state = state;
    movestack[mstop].kingpos[0] = kingpos[0];
    movestack[mstop].kingpos[1] = kingpos[1];
    movestack[mstop].fullmovescounter = fullmovescounter;
    movestack[mstop].halfmovescounter = halfmovescounter;
    movestack[mstop].isCheck = isCheck;

    halfmovescounter++;

    // Fix hash regarding capture
    if (capture != BLANK && !GETEPCAPTURE(cm->code))
    {
        hash ^= zb.boardtable[(to << 4) | capture];
        if ((capture >> 1) == PAWN)
            pawnhash ^= zb.boardtable[(to << 4) | capture];
        BitboardClear(to, capture);
        materialhash ^= zb.boardtable[(POPCOUNT(piece00[capture]) << 4) | capture];
        halfmovescounter = 0;
    }

    if (promote == BLANK)
    {
        mailbox[to] = pfrom;
        BitboardMove(from, to, pfrom);
    }
    else {
        mailbox[to] = promote;
        BitboardClear(from, pfrom);
        materialhash ^= zb.boardtable[(POPCOUNT(piece00[pfrom]) << 4) | pfrom];
        materialhash ^= zb.boardtable[(POPCOUNT(piece00[promote]) << 4) | promote];
        BitboardSet(to, promote);
        // just double the hash-switch for target to make the pawn vanish
        pawnhash ^= zb.boardtable[(to << 4) | mailbox[to]];
    }

    hash ^= zb.boardtable[(to << 4) | mailbox[to]];
    hash ^= zb.boardtable[(from << 4) | pfrom];

    mailbox[from] = BLANK;

    /* PAWN specials */
    if (ptype == PAWN)
    {
        pawnhash ^= zb.boardtable[(to << 4) | mailbox[to]];
        pawnhash ^= zb.boardtable[(from << 4) | pfrom];
        halfmovescounter = 0;

        if (ept && to == ept)
        {
            int epfield = (from & 0x38) | (to & 0x07);
            BitboardClear(epfield, (pfrom ^ S2MMASK));
            mailbox[epfield] = BLANK;
            hash ^= zb.boardtable[(epfield << 4) | (pfrom ^ S2MMASK)];
            pawnhash ^= zb.boardtable[(epfield << 4) | (pfrom ^ S2MMASK)];
            materialhash ^= zb.boardtable[(POPCOUNT(piece00[(pfrom ^ S2MMASK)]) << 4) | (pfrom ^ S2MMASK)];
        }
    }

    if (ptype == KING)
        kingpos[s2m] = to;

    // Here we can test the move for being legal
    if (isAttacked(kingpos[s2m]))
    {
        // Move is illegal; just do the necessary subset of unplayMove
        hash = movestack[mstop].hash;
        pawnhash = movestack[mstop].pawnhash;
        materialhash = movestack[mstop].materialhash;
        kingpos[s2m] = movestack[mstop].kingpos[s2m];
        halfmovescounter = movestack[mstop].halfmovescounter;
        mailbox[from] = pfrom;
        if (promote != BLANK)
        {
            BitboardClear(to, mailbox[to]);
            BitboardSet(from, pfrom);
        }
        else {
            BitboardMove(to, from, pfrom);
        }

        if (capture != BLANK)
        {
            if (ept && to == ept)
            {
                // special ep capture
                int epfield = (from & 0x38) | (to & 0x07);
                BitboardSet(epfield, capture);
                mailbox[epfield] = capture;
                mailbox[to] = BLANK;
            }
            else
            {
                BitboardSet(to, capture);
                mailbox[to] = capture;
            }
        }
        else {
            mailbox[to] = BLANK;
        }
        return false;
    }

    // remove castle rights
    if (to == 0x00 || from == 0x00)
        state &= ~WQCMASK;
    if (to == 0x07 || from == 0x07)
        state &= ~WKCMASK;
    if (to == 0x38 || from == 0x38)
        state &= ~BQCMASK;
    if (to == 0x3f || from == 0x3f)
        state &= ~BKCMASK;

    if (ptype == KING)
    {
        /* handle castle */
        state &= (s2m ? ~(BQCMASK | BKCMASK) : ~(WQCMASK | WKCMASK));
        int c = castleindex[from][to];
        if (c)
        {
            int rookfrom = castlerookfrom[c];
            int rookto = castlerookto[c];

            BitboardMove(rookfrom, rookto, (PieceCode)(WROOK | s2m));
            mailbox[rookto] = (PieceCode)(WROOK | s2m);

            hash ^= zb.boardtable[(rookto << 4) | (PieceCode)(WROOK | s2m)];
            hash ^= zb.boardtable[(rookfrom << 4) | (PieceCode)(WROOK | s2m)];

            mailbox[rookfrom] = BLANK;
        }
    }

    state ^= S2MMASK;
    isCheck = isAttacked(kingpos[state & S2MMASK]);

    hash ^= zb.s2m;

    if (!(state & S2MMASK))
        fullmovescounter++;

    // Fix hash regarding ept
    hash ^= zb.ept[ept];
    ept = eptnew;
    hash ^= zb.ept[ept];

    // Fix hash regarding castle rights
    oldcastle ^= (state & CASTLEMASK);
    hash ^= zb.cstl[oldcastle];

    ply++;
    rp.addPosition(hash);
    actualpath.move[actualpath.length++] = *cm;
    mstop++;

    return true;
}


void chessposition::unplayMove(chessmove *cm)
{
    int from = GETFROM(cm->code);
    int to = GETTO(cm->code);
    PieceCode pto = mailbox[to];
    PieceCode promote = GETPROMOTION(cm->code);
    PieceCode capture = GETCAPTURE(cm->code);
    int s2m;

    actualpath.length--;
    rp.removePosition(hash);
    ply--;

    mstop--;
    ept = movestack[mstop].ept;
    hash = movestack[mstop].hash;
    pawnhash = movestack[mstop].pawnhash;
    materialhash = movestack[mstop].materialhash;
    state = movestack[mstop].state;
    kingpos[0] = movestack[mstop].kingpos[0];
    kingpos[1] = movestack[mstop].kingpos[1];
    fullmovescounter = movestack[mstop].fullmovescounter;
    halfmovescounter = movestack[mstop].halfmovescounter;
    isCheck = movestack[mstop].isCheck;

    s2m = state & S2MMASK;
    if (promote != BLANK)
    {
        mailbox[from] = (PieceCode)(WPAWN | s2m);
        BitboardClear(to, pto);
        BitboardSet(from, (PieceCode)(WPAWN | s2m));
    }
    else {
        BitboardMove(to, from, pto);
        mailbox[from] = pto;
    }

    if (capture != BLANK)
    {
        if (ept && to == ept)
        {
            // special ep capture
            int epfield = (from & 0x38) | (to & 0x07);
            BitboardSet(epfield, capture);
            mailbox[epfield] = capture;
            mailbox[to] = BLANK;
        }
        else
        {
            BitboardSet(to, capture);
            mailbox[to] = capture;
        }
    }
    else {
        mailbox[to] = BLANK;
    }

    if ((pto >> 1) == KING)
    {
        int c = castleindex[from][to];
        if (c)
        {
            int rookfrom = castlerookfrom[c];
            int rookto = castlerookto[c];

            BitboardMove(rookto, rookfrom, (PieceCode)(WROOK | s2m));
            mailbox[rookfrom] = (PieceCode)(WROOK | s2m);
            mailbox[rookto] = BLANK;
        }
    }
}


template <MoveType Mt> inline void appendMoveToList(chessposition *pos, chessmove **m, int from, int to, PieceCode piece)
{
    **m = chessmove(from, to, piece);
    if (!(Mt & CAPTURE))
    {
        (*m)->value = pos->history[piece >> 1][to];
    }
    else if (!(Mt & QUIET))
    {
        PieceCode capture = pos->mailbox[to];
        (*m)->code |= capture << 16;
        (*m)->value = (mvv[capture >> 1] | lva[piece >> 1]);
    } else {
        PieceCode capture = pos->mailbox[to];
        if (capture)
        {
            (*m)->code |= capture << 16;
            (*m)->value = (mvv[capture >> 1] | lva[piece >> 1]);
        }
        else {
            (*m)->value = pos->history[piece >> 1][to];
        }
    }
    (*m)++;
}

#if 0
inline void chessposition::testMove_old(chessmovelist *movelist, int from, int to, PieceCode promote, PieceCode capture, int ept, PieceCode piece)
{
    chessmove cm(from, to, promote, capture, ept, piece);
    if (capture != BLANK)
    {
        cm.value = (mvv[capture >> 1] | lva[piece >> 1]);
    }
    else {
        cm.value = history[piece >> 1][to];
    }
    movelist->move[movelist->length++] = cm;
}
#endif


template <MoveType Mt> chessmove* CreateMovelist(chessposition *pos, chessmove* m)
{
    int s2m = pos->state & S2MMASK;
    U64 occupiedbits = (pos->occupied00[0] | pos->occupied00[1]);
    U64 emptybits = ~occupiedbits;
    U64 targetbits = 0ULL;
    U64 frombits, tobits;
    int from, to;
    PieceCode pc;


    if (Mt & QUIET)
        targetbits |= emptybits;
    if (Mt & CAPTURE)
        targetbits |= pos->occupied00[s2m ^ S2MMASK];

    for (int p = PAWN; p <= KING; p++)
    {
        pc = (PieceCode)((p << 1) | s2m);
        frombits = pos->piece00[pc];
        switch (p)
        {
        case PAWN:
            while (LSB(from, frombits))
            {
                tobits = 0ULL;
                if (Mt & QUIET)
                {
                    tobits |= (pawn_attacks_free[from][s2m] & emptybits & ~PROMOTERANKBB);
                    if (tobits)
                        tobits |= (pawn_attacks_free_double[from][s2m] & emptybits);
                }
                if (Mt & PROMOTE)
                {
                    // get promoting pawns
                    tobits |= (pawn_attacks_free[from][s2m] & PROMOTERANKBB & emptybits);
                }
                if (Mt & CAPTURE)
                {
                    /* FIXME: ept & EPTSIDEMASK[s2m] is a quite ugly test for correct side respecting null move pruning */
                    tobits |= (pawn_attacks_occupied[from][s2m] & (pos->occupied00[s2m ^ 1] | ((pos->ept & EPTSIDEMASK[s2m]) ? BITSET(pos->ept) : 0ULL)));
                }
                while (LSB(to, tobits))
                {
                    if ((Mt & CAPTURE) && pos->ept && pos->ept == to)
                    {
                        //testMove(result, from, to, BLANK, (PieceCode)(WPAWN | (s2m ^ 1)), ISEPCAPTURE, pc);
                        // treat ep capture as a quiet move and correct code and value manually
                        appendMoveToList<QUIET>(pos, &m, from, to, pc);
                        (m - 1)->code |= (ISEPCAPTURE << 20) | ((WPAWN | (s2m ^ 1)) << 16);
                        (m - 1)->value = (mvv[PAWN] | lva[PAWN]);
                    }
                    else if (PROMOTERANK(to))
                    {
                        //testMove(result, from, to, (PieceCode)((QUEEN << 1) | s2m), mailbox[to], pc);
                        //testMove(result, from, to, (PieceCode)((ROOK << 1) | s2m), mailbox[to], pc);
                        //testMove(result, from, to, (PieceCode)((BISHOP << 1) | s2m), mailbox[to], pc);
                        //testMove(result, from, to, (PieceCode)((KNIGHT << 1) | s2m), mailbox[to], pc);
                        appendMoveToList<Mt>(pos, &m, from, to, pc);
                        (m - 1)->code |= ((QUEEN << 1) | s2m) << 12;
                        (m - 1)->value = mvv[QUEEN];
                        appendMoveToList<Mt>(pos, &m, from, to, pc);
                        (m - 1)->code |= ((ROOK << 1) | s2m) << 12;
                        (m - 1)->value = mvv[ROOK];
                        appendMoveToList<Mt>(pos, &m, from, to, pc);
                        (m - 1)->code |= ((BISHOP << 1) | s2m) << 12;
                        (m - 1)->value = mvv[BISHOP];
                        appendMoveToList<Mt>(pos, &m, from, to, pc);
                        (m - 1)->code |= ((KNIGHT << 1) | s2m) << 12;
                        (m - 1)->value = mvv[KNIGHT];
                    }
                    else if ((Mt & QUIET) && !((from ^ to) & 0x8) && (epthelper[to] & pos->piece00[pc ^ 1]))
                    {
                        // EPT possible for opponent; set EPT field manually
                        appendMoveToList<QUIET>(pos, &m, from, to, pc);
                        (m - 1)->code |= (from + to) << 19;
                    }
                    else {
                        appendMoveToList<Mt>(pos, &m, from, to, pc);
                    }
                    if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                    {
                        pos->unplayMove(m - 1);
                        return m;
                    }
                    tobits ^= BITSET(to);
                }
                frombits ^= BITSET(from);
            }
            break;
        case KNIGHT:
            while (LSB(from, frombits))
            {
                tobits = (knight_attacks[from] & targetbits);
                while (LSB(to, tobits))
                {
                    appendMoveToList<Mt>(pos, &m, from, to, pc);
                    if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                    {
                        pos->unplayMove(m - 1);
                        return m;
                    }
                    tobits ^= BITSET(to);
                }
                frombits ^= BITSET(from);
            }
            break;
        case KING:
            from = pos->kingpos[s2m];
            tobits = (king_attacks[from] & targetbits);
            while (LSB(to, tobits))
            {
                appendMoveToList<Mt>(pos, &m, from, to, pc);
                if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                {
                    pos->unplayMove(m - 1);
                    return m;
                }
                tobits ^= BITSET(to);
            }
            if (Mt & QUIET)
            {
                if (pos->state & QCMASK[s2m])
                {
                    /* queen castle */
                    if (!(occupiedbits & (s2m ? 0x0e00000000000000 : 0x000000000000000e))
                        && !pos->isAttacked(from) && !pos->isAttacked(from - 1) && !pos->isAttacked(from - 2))
                    {
                        appendMoveToList<Mt>(pos, &m, from, from - 2, pc);
                        if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                        {
                            pos->unplayMove(m - 1);
                            return m;
                        }
                    }
                }
                if (pos->state & KCMASK[s2m])
                {
                    /* kink castle */
                    if (!(occupiedbits & (s2m ? 0x6000000000000000 : 0x0000000000000060))
                        && !pos->isAttacked(from) && !pos->isAttacked(from + 1) && !pos->isAttacked(from + 2))
                    {
                        appendMoveToList<Mt>(pos, &m, from, from + 2, pc);
                        if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                        {
                            pos->unplayMove(m - 1);
                            return m;
                        }
                    }
                }
            }
            break;
        default:
            tobits = 0ULL;
            while (LSB(from, frombits))
            {
                if (shifting[p] & 0x1)
                {
                    tobits |= (MAGICBISHOPATTACKS(occupiedbits, from) & targetbits);
                }
                if (shifting[p] & 0x2)
                {
                    tobits |= (MAGICROOKATTACKS(occupiedbits, from) & targetbits);
                }
                while (LSB(to, tobits))
                {
                    appendMoveToList<Mt>(pos, &m, from, to, pc);
                    if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                    {
                        pos->unplayMove(m - 1);
                        return m;
                    }
                    tobits ^= BITSET(to);
                }
                frombits ^= BITSET(from);
            }
            break;
        }
    }
    return m;
}


// Default movelist generator; ordered captures before quiets
int chessposition::getMoves(chessmove *m, MoveType t)
{
    chessmove *mlast = m;
    switch (t)
    {
        case QUIET:
            mlast = CreateMovelist<QUIET>(this, mlast);
            break;
        case CAPTURE:
            mlast = CreateMovelist<CAPTURE>(this, mlast);
            break;
        case TACTICAL:
            mlast = CreateMovelist<TACTICAL>(this, mlast);
            break;
        case QUIETWITHCHECK:
            mlast = CreateMovelist<QUIETWITHCHECK>(this, mlast);
            break;
        default:
            mlast = CreateMovelist<ALL>(this, mlast);
    }

    return (int)(mlast - m);
}



chessmovelist* chessposition::getMoves_old()
{
    int s2m = state & S2MMASK;
    U64 occupiedbits = (occupied00[0] | occupied00[1]);
    U64 emptybits = ~occupiedbits;
    U64 opponentorfreebits = ~occupied00[s2m];
    U64 frombits, tobits;
    int from, to;
    PieceCode pc;

    chessmovelist* result = (chessmovelist*)malloc(sizeof(chessmovelist));
    result->length = 0;
    for (int p = PAWN; p <= KING; p++)
    {
        pc = (PieceCode)((p << 1) | s2m);
        frombits = piece00[pc];
        switch (p)
        {
        case PAWN:
            while (LSB(from, frombits))
            {
                tobits = (pawn_attacks_free[from][s2m] & emptybits);
                if (tobits)
                    tobits |= (pawn_attacks_free_double[from][s2m] & emptybits);
                /* FIXME: ept & EPTSIDEMASK[s2m] is a quite ugly test for correct side respecting null move pruning */
                tobits |= (pawn_attacks_occupied[from][s2m] & (occupied00[s2m ^ 1] | ((ept & EPTSIDEMASK[s2m]) ? BITSET(ept) : 0ULL)));
                while (LSB(to, tobits))
                {
                    if (ept && ept == to)
                    {
                        testMove_old(result, from, to, BLANK, (PieceCode)(WPAWN | (s2m ^ 1)), ISEPCAPTURE, pc);
                    }
                     else if (PROMOTERANK(to))
                    {
                        testMove_old(result, from, to, (PieceCode)((QUEEN << 1) | s2m), mailbox[to], pc);
                        testMove_old(result, from, to, (PieceCode)((ROOK << 1) | s2m), mailbox[to], pc);
                        testMove_old(result, from, to, (PieceCode)((BISHOP << 1) | s2m), mailbox[to], pc);
                        testMove_old(result, from, to, (PieceCode)((KNIGHT << 1) | s2m), mailbox[to], pc);
                    }
                    else if (!((from ^ to) & 0x8) && epthelper[to] & piece00[pc ^ 1])
                    {
                        // EPT possible for opponent
                        testMove_old(result, from, to, BLANK, BLANK, (from + to) >> 1, pc);
                    }
                    else {
                        testMove_old(result, from, to, BLANK, mailbox[to], pc);
                    }
                    tobits ^= BITSET(to);
                }
                frombits ^= BITSET(from);
            }
            break;
        case KNIGHT:
            while (LSB(from, frombits))
            {
                tobits = (knight_attacks[from] & opponentorfreebits);
                while (LSB(to, tobits))
                {
                    testMove_old(result, from, to, BLANK, mailbox[to], pc);
                    tobits ^= BITSET(to);
                }
                frombits ^= BITSET(from);
            }
            break;
        case KING:
            from = kingpos[s2m];
            tobits = (king_attacks[from] & opponentorfreebits);
            while (LSB(to, tobits))
            {
                testMove_old(result, from, to, BLANK, mailbox[to], pc);
                tobits ^= BITSET(to);
            }
            if (state & QCMASK[s2m])
            {
                /* queen castle */
                if (!(occupiedbits & (s2m ? 0x0e00000000000000 : 0x000000000000000e))
                    && !isAttacked(from) && !isAttacked(from - 1) && !isAttacked(from - 2))
                {
                    testMove_old(result, from, from - 2, BLANK, BLANK, pc);
                }
            }
            if (state & KCMASK[s2m])
            {
                /* kink castle */
                if (!(occupiedbits & (s2m ? 0x6000000000000000 : 0x0000000000000060))
                    && !isAttacked(from) && !isAttacked(from + 1) && !isAttacked(from + 2))
                {
                    testMove_old(result, from, from + 2, BLANK, BLANK, pc);
                }
            }
            break;
        default:
            tobits = 0ULL;
            while (LSB(from, frombits))
            {
                if (shifting[p] & 0x1)
                {
                    tobits |= (MAGICBISHOPATTACKS(occupiedbits, from) & opponentorfreebits);
                }
                if (shifting[p] & 0x2)
                {
                    tobits |= (MAGICROOKATTACKS(occupiedbits, from) & opponentorfreebits);
                }
                while (LSB(to, tobits))
                {
                    testMove_old(result, from, to, BLANK, mailbox[to], pc);
                    tobits ^= BITSET(to);
                }
                frombits ^= BITSET(from);
            }
            break;
        }
    }
    return result;
}


#if 0 // not used anymore
U64 chessposition::attacksTo(int index, int side)
{
    return (knight_attacks[index] & piece00[(KNIGHT << 1) | side])
        | (king_attacks[index] & piece00[(KING << 1) | side])
        | (pawn_attacks_occupied[index][side ^ S2MMASK] & piece00[(PAWN << 1) | side])
        | (MAGICROOKATTACKS(occupied00[0] | occupied00[1], index) & (piece00[(ROOK << 1) | side] | piece00[(QUEEN << 1) | side]))
        | (MAGICBISHOPATTACKS(occupied00[0] | occupied00[1], index) & (piece00[(BISHOP << 1) | side] | piece00[(QUEEN << 1) | side]));
}
#endif

bool chessposition::isAttacked(int index)
{
    int opponent = (state & S2MMASK) ^ 1;

    return knight_attacks[index] & piece00[(KNIGHT << 1) | opponent]
        || king_attacks[index] & piece00[(KING << 1) | opponent]
        || pawn_attacks_occupied[index][state & S2MMASK] & piece00[(PAWN << 1) | opponent]
        || MAGICROOKATTACKS(occupied00[0] | occupied00[1], index) & (piece00[(ROOK << 1) | opponent] | piece00[(QUEEN << 1) | opponent])
        || MAGICBISHOPATTACKS(occupied00[0] | occupied00[1], index) & (piece00[(BISHOP << 1) | opponent] | piece00[(QUEEN << 1) | opponent]);
}



int chessposition::phase()
{
    int p = max(0, (24 - POPCOUNT(piece00[4]) - POPCOUNT(piece00[5]) - POPCOUNT(piece00[6]) - POPCOUNT(piece00[7]) - (POPCOUNT(piece00[8]) << 1) - (POPCOUNT(piece00[9]) << 1) - (POPCOUNT(piece00[10]) << 2) - (POPCOUNT(piece00[11]) << 2)));
    return (p * 255 + 12) / 24;
}



int chessposition::getLeastValuablePieceIndex(int to, unsigned int bySide, PieceCode *piece)
{
    int i;
    if (LSB(i, pawn_attacks_occupied[to][bySide ^ S2MMASK] & piece00[(PAWN << 1) | bySide]))
    {
        *piece = WPAWN + bySide;
        return i;
    }
    if (LSB(i, knight_attacks[to] & piece00[(KNIGHT << 1) | bySide]))
    {
        *piece = WKNIGHT + bySide;
        return i;
    }
    if (LSB(i, MAGICBISHOPATTACKS(occupied00[0] | occupied00[1], to) & (piece00[(BISHOP << 1) | bySide])))
    {
        *piece = WBISHOP + bySide;
        return i;
    }
    if (LSB(i, MAGICROOKATTACKS(occupied00[0] | occupied00[1], to) & (piece00[(ROOK << 1) | bySide])))
    {
        *piece = WROOK + bySide;
        return i;
    }
    if (LSB(i, (MAGICBISHOPATTACKS(occupied00[0] | occupied00[1], to) & (piece00[(QUEEN << 1) | bySide])) | (MAGICROOKATTACKS(occupied00[0] | occupied00[1], to) & (piece00[(QUEEN << 1) | bySide]))))
    {
        *piece = WQUEEN + bySide;
        return i;
    }
    if (LSB(i, king_attacks[to] & piece00[(KING << 1) | bySide]))
    {
        *piece = WKING + bySide;
        return i;
    }
    return -1;
}


int chessposition::see(int from, int to)
{
    PieceType bPiece = mailbox[to];
    if (bPiece == BLANK)
        // ep capture is always okay
        return 0;
    PieceCode aPiece = mailbox[from];
    int gain[32], d = 0;
    int side = (aPiece & S2MMASK);
    int fromlist[32];

    gain[0] = materialvalue[bPiece >> 1];
    do {
        d++;
        gain[d] = materialvalue[aPiece >> 1] - gain[d - 1];
        if (max(-gain[d - 1], gain[d]) < 0)
            break;

        fromlist[d] = from;
        BitboardClear(from, aPiece);
        side ^= S2MMASK;
        bPiece = aPiece;
        from = getLeastValuablePieceIndex(to, side, &aPiece);
        if (from < 0)
            BitboardSet(fromlist[d], bPiece);
    } while (from >= 0);
    while (--d)
    {
        gain[d - 1] = -max(-gain[d - 1], gain[d]);
        BitboardSet(fromlist[d], mailbox[fromlist[d]]);
    }
    return gain[0];
}


void MoveSelector::SetPreferredMoves(uint32_t hshm, uint32_t kllm1, uint32_t kllm2, uint32_t rftm)
{
    hashmove = hshm;
    killermove1 = kllm1;
    killermove2 = kllm2;
    refutemove = rftm;
}


MoveSelector::~MoveSelector()
{
    if (captures)
        delete captures;

    if (quiets)
        delete quiets;
}




engine::engine()
{
#ifdef DEBUG
    int p = GetCurrentProcessId();
    char s[256];
    sprintf_s(s, "RubiChess-debug-%d.txt", p);
    fdebug.open(s, fstream::out | fstream::app);
#endif

    tp.pos = &pos;
    pwnhsh.pos = &pos;
    initBitmaphelper();

    setOption("hash", "150");
    setOption("Move Overhead", "50");
    setOption("MultiPV", "1");
    setOption("Ponder", "false");
    setOption("SyzygyPath", "<empty>");
    //setOption("SyzygyPath", "C:\\_Lokale_Daten_ungesichert\\tb");
    //setOption("SyzygyPath", "C:\\tb");

    setOption("Syzygy50MoveRule", "true");


#ifdef _WIN32
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    frequency = f.QuadPart;
#else
    frequency = 1000000000LL;
#endif
}


void engine::setOption(string sName, string sValue)
{
    bool resetTp = false;
    int newint;
    string lValue = sValue;
    transform(sName.begin(), sName.end(), sName.begin(), ::tolower);
    transform(lValue.begin(), lValue.end(), lValue.begin(), ::tolower);
    if (sName == "clear hash")
        tp.clean();
    if (sName == "ponder")
    {
        ponder = (lValue == "true");
    }

    if (sName == "multipv")
    {
        newint = stoi(sValue);
        if (newint >= 1 && newint <= MAXMULTIPV)
            MultiPV = newint;
    }
    if (sName == "hash")
    {
        newint = stoi(sValue);
        if (newint < 1)
            // at least a small hash table is required
            return;
        if (sizeOfTp != newint)
        {
            sizeOfTp = newint;
            resetTp = true;
        }
    }
    if (resetTp)
    {
        int restMb = max (16, tp.setSize(sizeOfTp));
        pwnhsh.setSize(restMb);
    }
    if (sName == "move overhead")
    {
        newint = stoi(sValue);
        if (newint < 0 || newint > 5000)
            return;
        moveOverhead = newint;
    }
    if (sName == "syzygypath")
    {
        SyzygyPath = sValue;
        init_tablebases((char *)SyzygyPath.c_str());
    }
    if (sName == "syzygy50moverule")
    {
        Syzygy50MoveRule = (lValue == "true");
    }
}


#ifdef DEBUG
extern int aspirationdelta[MAXDEPTH][2000];
#endif

void engine::communicate(string inputstring)
{
    string fen;
    vector<string> moves;
    vector<string> searchmoves;
    vector<string> commandargs;
    GuiToken command = UNKNOWN;
    size_t ci, cs;
    bool bGetName, bGetValue;
    string sName, sValue;
    bool bMoves;
    thread *searchthread = nullptr;
    bool pendingisready = false;
    bool pendingposition = false;
    do
    {
        if (pendingisready || pendingposition)
        {
            if (pendingposition)
            {
                // new position first stops current search
                if (stopLevel == ENGINERUN)
                    stopLevel = ENGINESTOPIMMEDIATELY;
                if (searchthread)
                {
                    if (searchthread->joinable())
                        searchthread->join();
                    delete searchthread;
                    searchthread = nullptr;
                }

                pos.getFromFen(fen.c_str());
                for (vector<string>::iterator it = moves.begin(); it != moves.end(); ++it)
                {
                    if (!pos.applyMove(*it))
                        printf("info string Alarm! Zug %s nicht anwendbar (oder Enginefehler)\n", (*it).c_str());
                }
                pos.ply = 0;
                pos.getRootMoves();
                pos.tbFilterRootMoves();
                if (debug)
                {
                    pos.print();
                }
                pendingposition = false;
            }
            if (pendingisready)
            {
                myUci->send("readyok\n");
                pendingisready = false;
            }
        }
        else {
            commandargs.clear();
            command = myUci->parse(&commandargs, inputstring);
            ci = 0;
            cs = commandargs.size();
            switch (command)
            {
            case UCIDEBUG:
                if (ci < cs)
                {
                    if (commandargs[ci] == "on")
                        debug = true;
                    else if (commandargs[ci] == "off")
                        debug = false;
#ifdef DEBUG
                    else if (commandargs[ci] == "this")
                        pos.debughash = pos.hash;
#endif
                }
                break;
            case UCI:
                myUci->send("id name %s\n", name);
                myUci->send("id author %s\n", author);
                myUci->send("option name Clear Hash type button\n");
                myUci->send("option name Hash type spin default 150 min 1 max 1048576\n");
                myUci->send("option name Move Overhead type spin default 50 min 0 max 5000\n");
                myUci->send("option name MultiPV type spin default 1 min 1 max %d\n", MAXMULTIPV);
                myUci->send("option name Ponder type check default false\n");
                myUci->send("option name SyzygyPath type string default <empty>\n");
                myUci->send("option name Syzygy50MoveRule type check default true\n");
                myUci->send("uciok\n", author);
                break;
            case SETOPTION:
                if (en.stopLevel < ENGINESTOPPED)
                {
                    myUci->send("info string Changing option while searching is not supported.\n");
                    break;
                }
                bGetName = bGetValue = false;
                sName = sValue = "";
                while (ci < cs)
                {
                    if (commandargs[ci] == "name")
                    {
                        setOption(sName, sValue);
                        bGetName = true;
                        bGetValue = false;
                        sName = "";
                    }
                    else if (commandargs[ci] == "value")
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
                setOption(sName, sValue);
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
                    fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
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
                resetPonder();
                searchmoves.clear();
                wtime = btime = winc = binc = movestogo = mate = maxdepth = 0;
                infinite = false;
                while (ci < cs)
                {
                    if (commandargs[ci] == "searchmoves")
                    {
                        while (++ci < cs && AlgebraicToIndex(commandargs[ci], 0x88) != 0x88 && AlgebraicToIndex(&commandargs[ci][2], 0x88) != 0x88)
                            searchmoves.push_back(commandargs[ci]);
                    }

                    else if (commandargs[ci] == "wtime")
                    {
                        if (++ci < cs)
                            wtime = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "btime")
                    {
                        if (++ci < cs)
                            btime = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "winc")
                    {
                        if (++ci < cs)
                            winc = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "binc")
                    {
                        if (++ci < cs)
                            binc = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "movetime")
                    {
                        movestogo = 1;
                        winc = binc = 0;
                        if (++ci < cs)
                            wtime = btime = stoi(commandargs[ci++]);
                    }
                    else if (commandargs[ci] == "movestogo")
                    {
                        if (++ci < cs)
                            movestogo = stoi(commandargs[ci++]);
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
                isWhite = (pos.w2m());
                searchthread = new thread(&searchguide);
                if (inputstring != "")
                {
                    // bench mode; wait for end of search
                    searchthread->join();
                    delete searchthread;
                    searchthread = nullptr;
                }
                break;
            case PONDERHIT:
                HitPonder();
                break;
            case STOP:
            case QUIT:
                stopLevel = ENGINESTOPIMMEDIATELY;
                if (searchthread && searchthread->joinable())
                {
                    searchthread->join();
                    delete searchthread;
                    searchthread = nullptr;
                }
                break;
            case EVAL:
                getValueTrace(&pos);
                break;
            default:
                break;
            }
        }
    } while (command != QUIT && (inputstring == "" || pendingposition));

#ifdef DEBUG
    char s[16384];
    cout << "Score delta table:\n";
    int depth = 2;
    int mind, maxd;
    do
    {
        mind = 2000;
        maxd = -1;
        for (int i = 0; i < 2000; i++)
        {
            if (aspirationdelta[depth][i] > 0)
            {
                if (mind > i)
                    mind = i;
                maxd = i;
            }
        }
        for (int i = mind; i <= maxd; i++)
        {
            sprintf_s(s, "%d;%d;%d\n", depth, i - 1000, aspirationdelta[depth][i]);
            en.fdebug << s;
        }
        depth++;
    } while (maxd >= 0 && depth < 20);
#endif
}

zobrist zb;
chessposition pos;
engine en;
