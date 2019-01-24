
#include "RubiChess.h"

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
U64 betweenMask[64][64];
int castleindex[64][64] = { 0 };
U64 castlekingto[64][2] = { 0ULL };
int castlerights[64];


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

void chessmovelist::sort(int limit, const unsigned int refutetarget)
{
    for (int i = 0; i < length - 1; i++)
    {
        if (refutetarget < BOARDSIZE && GETFROM(move[i].code) == refutetarget)
            move[i].value |= NMREFUTEVAL;
        for (int j = i + 1; j < length; j++)
            if (move[i].value < move[j].value)
                swap(move[i], move[j]);
        if (move[i].value < limit)
            break;
    }
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

    hash = zb.getHash(this);
    pawnhash = zb.getPawnHash(this);
    materialhash = zb.getMaterialHash(this);
    rp.clean();
    rp.addPosition(hash);
    memset(history, 0, sizeof(history));
    memset(killer, 0, sizeof(killer));
    mstop = 0;
    rootheight = 0;
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
    rootheight = mstop;
    return retval;
}


void chessposition::getRootMoves()
{
    // Precalculating the list of legal moves didn't work well for some unknown reason but we need the number of legal moves in MultiPV mode
    chessmovelist movelist;
    prepareStack();
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
    if (POPCOUNT(occupied00[0] | occupied00[1]) <= TBlargest)
    {
        if ((tbPosition = root_probe(this))) {
            en.tbhits++;
            // The current root position is in the tablebases.
            // RootMoves now contains only moves that preserve the draw or win.

            // Do not probe tablebases during the search.
            useTb = 0;
        }
        else // If DTZ tables are missing, use WDL tables as a fallback
        {
            // Filter out moves that do not preserve a draw or win
            tbPosition = root_probe_wdl(this);
            // useRootmoveScore is set within root_probe_wdl
        }

        if (tbPosition)
        {
            // Sort the moves
            for (int i = 0; i < rootmovelist.length; i++)
            {
                for (int j = i + 1; j < rootmovelist.length; j++)
                {
                    if (rootmovelist.move[i] < rootmovelist.move[j])
                    {
                        swap(rootmovelist.move[i], rootmovelist.move[j]);
                    }
                }
            }
        }
    }
}


/* test the actualmove for three-fold-repetition as the repetition table may give false positive due to table collisions */
int chessposition::testRepetiton()
{
    int hit = 0;
    for (int i = mstop - 1; i >= 0; i--)
    {
        if (hash == movestack[i].hash)
        {
            hit++;
            if (i > rootheight)
            {
                hit++;
                break;
            }
        }
        if (movestack[i].halfmovescounter == 0)
            break;
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

    pawnhash ^= zb.boardtable[(kingpos[0] << 4) | WKING] ^ zb.boardtable[((kingpos[1] ^ RANKMASK) << 4) | WKING];
    pawnhash ^= zb.boardtable[(kingpos[1] << 4) | BKING] ^ zb.boardtable[((kingpos[0] ^ RANKMASK) << 4) | BKING];
    int kingpostemp = kingpos[0];
    kingpos[0] = kingpos[1] ^ RANKMASK;
    kingpos[1] = kingpostemp ^ RANKMASK;
}


void chessposition::prepareStack()
{
    myassert(mstop >= 0 && mstop < MAXMOVESEQUENCELENGTH, this, 1, mstop);
    // copy stack related data directly to stack
    memcpy(&movestack[mstop], &state, sizeof(chessmovestack));
}


void chessposition::playNullMove()
{
#ifdef STACKDEBUG
    movecodestack[mstop] = 0;
#endif
    mstop++;
    state ^= S2MMASK;
    hash ^= zb.s2m;
    ply++;
    myassert(mstop < MAXMOVESEQUENCELENGTH, this, 1, mstop);
}


void chessposition::unplayNullMove()
{
    state ^= S2MMASK;
    hash ^= zb.s2m;
    ply--;
    mstop--;
    myassert(mstop >= 0, this, 1, mstop);
}


void chessposition::getpvline(int depth, int pvnum)
{
    chessmove cm;
    uint16_t movecode;
    pvline.length = 0;
    while (depth > 0)
    {
        if (pvline.length == 0 && bestmove[pvnum].code != 0)
        {
            cm = bestmove[pvnum];
        }
        else if ((movecode = tp.getMoveCode(hash)))
        {
            cm.code = shortMove2FullMove(movecode);
            if (!cm.code)
                break;
        }
        else
        {
            break;
        }

        prepareStack();
        if (!playMove(&cm))
        {
            printf("info string Alarm - Illegaler Zug %s in pvline\n", cm.toString().c_str());
            print();
            tp.printHashentry(hash);
        }
        pvline.move[pvline.length++] = cm;
        depth--;
    }
    for (int i = pvline.length; i;)
        unplayMove(&(pvline.move[--i]));
}


uint32_t chessposition::shortMove2FullMove(uint16_t c)
{
    if (!c)
        return 0;

    int from = GETFROM(c);
    int to = GETTO(c);
    PieceCode pc = mailbox[from];
    if (!pc) // short move is wrong
        return 0;
    PieceCode capture = mailbox[to];
    PieceType p = pc >> 1;

    myassert(capture >= BLANK && capture <= BKING, this, 1, capture);
    myassert(pc >= WPAWN && pc <= BKING, this, 1, pc);

    int ept = 0;
    if (p == PAWN)
    {
        if (FILE(from) != FILE(to) && capture == BLANK)
        {
            // ep capture
            capture = pc ^ S2MMASK;
            ept = ISEPCAPTURE;
        }
        else if ((from ^ to) == 16 && (epthelper[to] & piece00[pc ^ 1]))
        {
            // double push enables epc
            ept = (from + to) / 2;
        }
    }

    uint32_t fc = (pc << 28) | (ept << 20) | (capture << 16) | c;
    if (moveIsPseudoLegal(fc))
        return fc;
    else
        return 0;
}


// FIXME: moveIsPseudoLegal gets more and more complicated with making it "thread safe"; maybe using 32bit for move in tp would be better?
bool chessposition::moveIsPseudoLegal(uint32_t c)
{
    if (!c)
        return false;

    int from = GETFROM(c);
    int to = GETTO(c);
    PieceCode pc = GETPIECE(c);
    PieceCode capture = GETCAPTURE(c);
    PieceType p = pc >> 1;
    int s2m = (pc & S2MMASK);

    myassert(pc >= WPAWN && pc <= BKING, this, 1, pc);

    // correct piece?
    if (mailbox[from] != pc)
        return false;

    // correct capture?
    if (mailbox[to] != capture && !GETEPCAPTURE(c))
        return false;

    // correct color of capture? capturing the king is illegal
    if (capture && (s2m == (capture & S2MMASK) || capture >= WKING))
        return false;

    myassert(capture >= BLANK && capture <= BQUEEN, this, 1, capture);

    // correct target for type of piece?
    if (!(movesTo(pc, from) & BITSET(to)))
        return false;

    // correct s2m?
    if (s2m != (state & S2MMASK))
        return false;

    // only pawn can promote
    if (GETPROMOTION(c) && p != PAWN)
        return false;

    if (p == PAWN)
    {
        // pawn specials
        if (((from ^ to) == 16))
        {
            // double push
            if (betweenMask[from][to] & (occupied00[0] | occupied00[1]))
                // blocked
                return false;

            // test if "making ep capture possible" is both true or false
            int ept = GETEPT(c);
            {
                if (!ept == (bool)(epthelper[to] & piece00[pc ^ 1]))
                    return false;
            }
        }
        else
        {
            // wrong ep capture
            if (GETEPCAPTURE(c) && ept != to)
                return false;

            // missing promotion
            if (RRANK(to, s2m) == 7 && !GETPROMOTION(c))
                return false;
        }
    }


    if (p == KING && (((from ^ to) & 3) == 2))
    {
        // test for correct castle
        if (isAttacked(from))
            return false;

        U64 occupied = occupied00[0] | occupied00[1];
        int s2m = state & S2MMASK;

        if (from > to)
        {
            //queen castle
            if (occupied & (s2m ? 0x0e00000000000000 : 0x000000000000000e)
                || isAttacked(from - 1)
                || isAttacked(from - 2)
                || !(state & QCMASK[s2m]))
            {
                return false;
            }
        }
        else
        {
            // king castle
            if (occupied & (s2m ? 0x6000000000000000 : 0x0000000000000060)
                || isAttacked(from + 1)
                || isAttacked(from + 2)
                || !(state & KCMASK[s2m]))
            {
                return false;
            }
        }
    }

    return true;
}


bool chessposition::moveGivesCheck(uint32_t c)
{
    int pc = GETPIECE(c);
    int me = pc & S2MMASK;
    int you = me ^ S2MMASK;
    int yourKing = kingpos[you];

    // test if moving piece gives check
    if (movesTo(pc, GETTO(c)) & BITSET(yourKing))
        return true;

#if 0
    // test for discovered check; seems a good idea but doesn't work, maybe too expensive for too few positives
    if (isAttackedByMySlider(yourKing, (occupied00[0] | occupied00[1]) ^ BITSET(GETTO(c)) ^ BITSET(GETFROM(c)), me))
        return true;
#endif

    return false;
}


void chessposition::print(ostream* os)
{

    *os << "Board:\n";
    for (int r = 7; r >= 0; r--)
    {
        for (int f = 0; f < 8; f++)
        {
            int mb = mailbox[INDEX(r, f)];
            char pc = PieceChar(mb);
            if (pc == 0)
                pc = '.';
            if (mb < BLANK || mb > BKING)
            {
                *os << "Illegal Mailbox entry " + to_string(mb);
                    pc = 'X';
            }
            *os << char(pc);
        }
        *os << "\n";
    }
    chessmovelist pseudolegalmoves;
    pseudolegalmoves.length = getMoves(&pseudolegalmoves.move[0]);
    *os << "\nFEN: " + toFen() + "\n";
    *os << "State: " + to_string(state) + "\n";
    *os << "EPT: " + to_string(ept) + "\n";
    *os << "Halfmoves: " + to_string(halfmovescounter) + "\n";
    *os << "Fullmoves: " + to_string(fullmovescounter) + "\n";
    *os << "Hash: " + to_string(hash) + " (should be " + to_string(zb.getHash(this)) +  ")\n";
    *os << "Pawn Hash: " + to_string(pawnhash) + " (should be " + to_string(zb.getPawnHash(this)) + ")\n";
    *os << "Value: " + to_string(getValue()) + "\n";
#ifdef EVALTUNE
    getPositionTuneSet(&pts);
    *os << "info string Value from gradients: " + getGradientString() + " " + to_string(NEWTAPEREDEVAL(getGradientValue(&this->pts), ph)) + "\n";
#endif
    *os << "Repetitions: " + to_string(rp.getPositionCount(hash)) + "\n";
    *os << "Phase: " + to_string(phase()) + "\n";
    *os << "Pseudo-legal Moves: " + pseudolegalmoves.toStringWithValue() + "\n";
#ifdef STACKDEBUG
    *os << "Moves in current search: " + movesOnStack() + "\n";
#endif
    *os << "mstop: " + to_string(mstop) + "\n";
    *os << "Ply: " + to_string(ply) + "\n";
    *os << "rootheight: " + to_string(rootheight) + "\n";
    stringstream ss;
    ss << hex << bestmove[0].code;
    *os << "bestmove[0].code: 0x" + ss.str() + "\n";
}


#ifdef STACKDEBUG
string chessposition::movesOnStack()
{
    string s = "";
    for (int i = 0; i < mstop; i++)
    {
        chessmove cm;
        cm.code = movecodestack[i];
        s = s + cm.toString() + " ";
    }
    return s;
}
#endif


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
            switch (mailbox[index] >> 1)
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
                    c = (char)(c + ('A' - 'a'));
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


#ifdef DEBUGEVAL
void chessposition::debugeval(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stdout, format, argptr);
    va_end(argptr);
}
#endif

#ifdef SDEBUG
void chessposition::updatePvTable(uint32_t movecode)
{
    pvtable[ply][0] = movecode;
    int i = 0;
    while (pvtable[ply + 1][i])
    {
        pvtable[ply][i + 1] = pvtable[ply + 1][i];
        i++;
    }
    pvtable[ply][i + 1] = 0;

}

string chessposition::getPv()
{
    string s = "PV:";
    for (int i = 0; pvtable[0][i]; i++)
    {
        chessmove cm;
        cm.code = pvtable[0][i];
        s += " " + cm.toString();
    }
    return s;
}

bool chessposition::triggerDebug(chessmove* nextmove)
{
    if (pvdebug[0] == 0)
        return false;

    int j = 0;

    while (j + rootheight < mstop && pvdebug[j])
    {
        if ((movecodestack[j + rootheight] & 0xefff) != pvdebug[j])
            return false;
        j++;
    }
    nextmove->code = pvdebug[j];
 
    if (debugOnlySubtree)
        return (pvdebug[j] == 0);

    if (debugRecursive)
        return true;

    return (j + rootheight == mstop);
}

void chessposition::sdebug(int indent, const char* format, ...)
{
    fprintf(stderr, "%*s", indent, "");
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
    fprintf(stderr, "\n");
}

#endif

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

#if 0  // Code to generate magics below, don't remove
U64 getMagicCandidate(U64 mask)
{
    U64 magic;
    do {
        magic = zb.getRnd() & zb.getRnd() & zb.getRnd();
    } while (POPCOUNT((mask * magic) & 0xFF00000000000000ULL) < 6);
    return magic;
}
#endif

// Use precalculated macigs for better to save time at startup
const U64 magics[] = {
    0x1002004102008200, 0x8200108041020020, 0xa000a06240114001, 0x100c009840001000, 0x4310002248214800, 0xc880221002060081, 0x402010c110014208, 0x0009100804021000,
    0x100840404a000001, 0x0500010004107800, 0x010e801178050000, 0x0024010008800a00, 0x1000820800c00060, 0x0400110410804810, 0x822143005020a148, 0x8300038100004222,
    0x1880200432802081, 0x004a800182c00020, 0x0400118068050815, 0x00004102a0080040, 0x0000008200454000, 0x3002200010c40021, 0x8000049702100010, 0x0020100104000208,
    0x20a0112570110004, 0x01021001a0080020, 0x00000402c0401000, 0x0884020010082100, 0x0010000500414000, 0x844008004019000c, 0x0026002101006002, 0x8020480110020020,
    0x06c5202004001049, 0x0002052000100024, 0x0040900130840090, 0x0200190040088100, 0x0001901c00420040, 0x0030802001a00800, 0x0880504024308060, 0x8010002004000202,
    0x0100201004200002, 0x0040010100080010, 0x2200608200100080, 0x5000409000801100, 0x2041100211002010, 0x8002214020400100, 0x4240090980080200, 0x0001400a24008010,
    0x41010100ca00480d, 0x1400a22008001042, 0x2803101084008800, 0x8008608810008240, 0x82200800040925a0, 0x2004500023002400, 0x2008080100820102, 0x8105100028001048,
    0x1481010004104010, 0x8010024d00014802, 0x0084022014041400, 0x8000820028030004, 0x0a04000a00212400, 0x0080094010018101, 0x1002008130184802, 0xc000021002002008,
    0x024020200c050111, 0x0001804002800124, 0x0422100042410300, 0x0400288202004100, 0x00449210e3902028, 0x0601840014020504, 0x8400208020080201, 0x0110a01001080008,
    0x000e008400060020, 0x0b10080850081100, 0x2c01001020090391, 0x000010040049020c, 0x10c2403028010804, 0x800040a010040901, 0x000900200c000202, 0x014c800040100426,
    0x400200808c042080, 0x1100400010208000, 0x0000401001a00408, 0x0014002001112018, 0x1002088010002100, 0x0010024871202002, 0x0008000208548081, 0x8014001028c80801,
    0x9002804504001841, 0x1201082010a00200, 0x0001081108100900, 0x0002008004102009, 0x1020040310901040, 0x0041012003400810, 0x0042409114040082, 0x080002034000a004,
    0x4540801108200010, 0x4520920010210200, 0x0020a82210040046, 0x1000408021020042, 0x4240915088424400, 0x0000242008026004, 0x000a022080a38000, 0x4010100024008401,
    0x9092002008085520, 0x0802801009083002, 0x00810420b1820008, 0x0000008040081002, 0x6000d40040840000, 0x42008000421000a0, 0x2622002009210022, 0x4000a12400848110,
    0x09102004d0005049, 0x2000804026001102, 0x444009a020100210, 0x404000510060400d, 0x208041c220040308, 0x0604200008041041, 0x0000914070082200, 0x80001802002c0422,
    0x028100002220148a, 0x0010b018200c0122, 0x080002b804049120, 0x200204802a080401, 0x8200804c30c42810, 0x8880604201100844, 0x00410304000a2021, 0x80000cc281092402
};


void initBitmaphelper()
{
    int to;
    castleindex[4][2] = WQC;
    castleindex[4][6] = WKC;
    castleindex[60][58] = BQC;
    castleindex[60][62] = BKC;
    castlekingto[4][0] = BITSET(2) | BITSET(6);
    castlekingto[60][1] = BITSET(58) | BITSET(62);

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
        castlerights[from] = ~0;
        if (from == 0x00)
            castlerights[from] &= ~WQCMASK;
        if (from == 0x07)
            castlerights[from] &= ~WKCMASK;
        if (from == 0x38)
            castlerights[from] &= ~BQCMASK;
        if (from == 0x3f)
            castlerights[from] &= ~BKCMASK;

        for (int j = 0; j < 64; j++)
        {
            betweenMask[from][j] = 0ULL;
            if (abs(FILE(from) - FILE(j)) == 1)
            {
                neighbourfilesMask[from] |= BITSET(j);
                if (RANK(from) == RANK(j))
                    phalanxMask[from] |= BITSET(j);
            }
            if (FILE(from) == FILE(j))
            {
                fileMask[from] |= BITSET(j);
                for (int i = min(RANK(from), RANK(j)) + 1; i < max(RANK(from), RANK(j)); i++)
                    betweenMask[from][j] |= BITSET(INDEX(i, FILE(from)));
            }
            if (RANK(from) == RANK(j))
            {
                rankMask[from] |= BITSET(j);
                for (int i = min(FILE(from), FILE(j)) + 1; i < max(FILE(from), FILE(j)); i++)
                    betweenMask[from][j] |= BITSET(INDEX(RANK(from), i));
            }
            if (from != j && abs(RANK(from) - RANK(j)) == abs(FILE(from) - FILE(j)))
            {
                int dx = (FILE(from) < FILE(j) ? 1 : -1);
                int dy = (RANK(from) < RANK(j) ? 1 : -1);
                for (int i = 1; FILE(from) +  i * dx != FILE(j); i++)
                    betweenMask[from][j] |= BITSET(INDEX(RANK(from) + i * dy, FILE(from) + i * dx));
            }
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
        int magicnum = 0;
        for (int j = 0; j < 64; j++)
        {
            if (from == j)
                continue;
            if (RANK(from) == RANK(j) && !ISOUTERFILE(j))
                mRookTbl[from].mask |= BITSET(j);
            if (FILE(from) == FILE(j) && !PROMOTERANK(j))
                mRookTbl[from].mask |= BITSET(j);
            if (abs(RANK(from) - RANK(j)) == abs(FILE(from) - FILE(j)) && !ISOUTERFILE(j) && !PROMOTERANK(j))
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
            // mBishopTbl[from].magic = getMagicCandidate(mBishopTbl[from].mask);
            mBishopTbl[from].magic = magics[magicnum++];

            for (int j = 0; magicOk && j < (1 << BISHOPINDEXBITS); j++) {
                // First get the subset of mask corresponding to j
                U64 occ = getOccupiedFromMBIndex(j, mBishopTbl[from].mask);
                // Now get the attack bitmap for this subset and store to attack table
                U64 attack = (getAttacks(from, occ, -7) | getAttacks(from, occ, 7) | getAttacks(from, occ, -9) | getAttacks(from, occ, 9));
                int hashindex = MAGICBISHOPINDEX(occ, from);
                // this test is obsolete now that the magics are precalculated
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
            // mRookTbl[from].magic = getMagicCandidate(mRookTbl[from].mask);
            mRookTbl[from].magic = magics[magicnum++];

            for (int j = 0; magicOk && j < (1 << ROOKINDEXBITS); j++) {
                // First get the subset of mask corresponding to j
                U64 occ = getOccupiedFromMBIndex(j, mRookTbl[from].mask);
                // Now get the attack bitmap for this subset and store to attack table
                U64 attack = (getAttacks(from, occ, -1) | getAttacks(from, occ, 1) | getAttacks(from, occ, -8) | getAttacks(from, occ, 8));
                int hashindex = MAGICROOKINDEX(occ, from);
                // this test is obsolete now that the magics are precalculated
                if (mRookAttacks[from][hashindex] == 0ULL)
                    mRookAttacks[from][hashindex] = attack;
                else if (mRookAttacks[from][hashindex] != attack)
                    magicOk = false;
            }
            if (magicOk)
                break;
        }
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
}


void chessposition::BitboardSet(int index, PieceCode p)
{
    myassert(index >= 0 && index < 64, this, 0);
    myassert(p >= BLANK && p <= BKING, this, 0);
    int s2m = p & 0x1;
    piece00[p] |= BITSET(index);
    occupied00[s2m] |= BITSET(index);
}


void chessposition::BitboardClear(int index, PieceCode p)
{
    myassert(index >= 0 && index < 64, this, 0);
    myassert(p >= BLANK && p <= BKING, this, 0);
    int s2m = p & 0x1;
    piece00[p] ^= BITSET(index);
    occupied00[s2m] ^= BITSET(index);
}


void chessposition::BitboardMove(int from, int to, PieceCode p)
{
    myassert(from >= 0 && from < 64, this, 0);
    myassert(to >= 0 && to < 64, this, 0);
    myassert(p >= BLANK && p <= BKING, this, 0);
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
    int s2m = state & S2MMASK;
    int from = GETFROM(cm->code);
    int to = GETTO(cm->code);
    PieceCode pfrom = GETPIECE(cm->code);
    PieceType ptype = (pfrom >> 1);
    int eptnew = GETEPT(cm->code);

    PieceCode promote = GETPROMOTION(cm->code);
    PieceCode capture = GETCAPTURE(cm->code);

    myassert(!promote || (ptype == PAWN && RRANK(to,s2m)==7), this, 4, promote, ptype, to, s2m);
    myassert(pfrom == mailbox[from], this, 3, pfrom, from, mailbox[from]);
    myassert(GETEPCAPTURE(cm->code) || capture == mailbox[to], this, 2, capture, mailbox[to]);

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
    int oldcastle = (state & CASTLEMASK);
    state &= (castlerights[from] & castlerights[to]);
    if (ptype == KING)
    {
        // Store king position in pawn hash
        pawnhash ^= zb.boardtable[(from << 4) | pfrom] ^ zb.boardtable[(to << 4) | pfrom];

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

    PREFETCH(&pwnhsh->table[pawnhash & pwnhsh->sizemask]);

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

    PREFETCH(&tp.table[hash & tp.sizemask]);

    ply++;
    rp.addPosition(hash);
#ifdef STACKDEBUG
    movecodestack[mstop] = cm->code;
#endif
    mstop++;
    myassert(mstop < MAXMOVESEQUENCELENGTH, this, 1, mstop);

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

    rp.removePosition(hash);
    ply--;

    mstop--;
    myassert(mstop >= 0, this, 1, mstop);
    // copy data from stack back to position
    memcpy(&state, &movestack[mstop], sizeof(chessmovestack));

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
    myassert(from >= 0 && from < 64, pos, 1, from);
    myassert(to >= 0 && to < 64, pos, 1, to);
    myassert(piece >= WPAWN && piece <= BKING, pos, 1, piece);

    **m = chessmove(from, to, piece);
    if (!(Mt & CAPTURE))
    {
        (*m)->value = pos->history[piece & S2MMASK][from][to];
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
            (*m)->value = pos->history[piece & S2MMASK][from][to];
        }
    }
    (*m)++;
}


template <MoveType Mt> int CreateMovelist(chessposition *pos, chessmove* mstart)
{
    int s2m = pos->state & S2MMASK;
    U64 occupiedbits = (pos->occupied00[0] | pos->occupied00[1]);
    U64 emptybits = ~occupiedbits;
    U64 targetbits = 0ULL;
    U64 frombits, tobits;
    int from, to;
    PieceCode pc;
    chessmove* m = mstart;

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
                        // treat ep capture as a quiet move and correct code and value manually
                        appendMoveToList<QUIET>(pos, &m, from, to, pc);
                        (m - 1)->code |= (ISEPCAPTURE << 20) | ((WPAWN | (s2m ^ 1)) << 16);
                        (m - 1)->value = (mvv[PAWN] | lva[PAWN]);
                    }
                    else if (PROMOTERANK(to))
                    {
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
                        return 1;
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
                        return 1;
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
                    return 1;
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
                            return 1;
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
                            return 1;
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
                        return 1;
                    }
                    tobits ^= BITSET(to);
                }
                frombits ^= BITSET(from);
            }
            break;
        }
    }
    return (int)(m - mstart);
}


int chessposition::getMoves(chessmove *m, MoveType t)
{
    switch (t)
    {
        case QUIET:
            return CreateMovelist<QUIET>(this, m);
        case CAPTURE:
            return CreateMovelist<CAPTURE>(this, m);
            break;
        case TACTICAL:
            return CreateMovelist<TACTICAL>(this, m);
            break;
        case QUIETWITHCHECK:
            return CreateMovelist<QUIETWITHCHECK>(this, m);
            break;
        default:
            return CreateMovelist<ALL>(this, m);
    }
}


U64 chessposition::movesTo(PieceCode pc, int from)
{
    PieceType p = (pc >> 1) ;
    int s2m = pc & S2MMASK;
    U64 occ = occupied00[0] | occupied00[1];
    switch (p)
    {
    case PAWN:
        return ((pawn_attacks_free[from][s2m] | pawn_attacks_free_double[from][s2m]) & ~occ)
                | (pawn_attacks_occupied[from][s2m] & occ);
    case KNIGHT:
        return knight_attacks[from];
    case BISHOP:
        return MAGICBISHOPATTACKS(occ, from);
    case ROOK:
        return MAGICROOKATTACKS(occ, from);
    case QUEEN:
        return MAGICBISHOPATTACKS(occ, from) | MAGICROOKATTACKS(occ, from);
    case KING:
        return king_attacks[from] | castlekingto[from][s2m];
    default:
        return 0ULL;
    }
}


bool chessposition::isAttacked(int index)
{
    int opponent = (state & S2MMASK) ^ 1;

    return knight_attacks[index] & piece00[(KNIGHT << 1) | opponent]
        || king_attacks[index] & piece00[(KING << 1) | opponent]
        || pawn_attacks_occupied[index][state & S2MMASK] & piece00[(PAWN << 1) | opponent]
        || MAGICROOKATTACKS(occupied00[0] | occupied00[1], index) & (piece00[(ROOK << 1) | opponent] | piece00[(QUEEN << 1) | opponent])
        || MAGICBISHOPATTACKS(occupied00[0] | occupied00[1], index) & (piece00[(BISHOP << 1) | opponent] | piece00[(QUEEN << 1) | opponent]);
}

// not needed as the discovered check test is disable but maybe we can use it later
bool chessposition::isAttackedByMySlider(int index, U64 occ, int me)
{
    return MAGICROOKATTACKS(occ, index) & (piece00[(ROOK << 1) | me] | piece00[(QUEEN << 1) | me])
        || MAGICBISHOPATTACKS(occ, index) & (piece00[(BISHOP << 1) | me] | piece00[(QUEEN << 1) | me]);
}


U64 chessposition::attackedByBB(int index, U64 occ)
{
    return (knight_attacks[index] & (piece00[WKNIGHT] | piece00[BKNIGHT]))
        | (king_attacks[index] & (piece00[WKING] | piece00[BKING]))
        | (pawn_attacks_occupied[index][1] & piece00[WPAWN])
        | (pawn_attacks_occupied[index][0] & piece00[BPAWN])
        | (MAGICROOKATTACKS(occ, index) & (piece00[WROOK] | piece00[BROOK] | piece00[WQUEEN] | piece00[BQUEEN]))
        | (MAGICBISHOPATTACKS(occ, index) & (piece00[WBISHOP] | piece00[BBISHOP] | piece00[WQUEEN] | piece00[BQUEEN]));
}


int chessposition::phase()
{
    int p = max(0, (24 - POPCOUNT(piece00[4]) - POPCOUNT(piece00[5]) - POPCOUNT(piece00[6]) - POPCOUNT(piece00[7]) - (POPCOUNT(piece00[8]) << 1) - (POPCOUNT(piece00[9]) << 1) - (POPCOUNT(piece00[10]) << 2) - (POPCOUNT(piece00[11]) << 2)));
    return (p * 255 + 12) / 24;
}




// more advanced see respecting a variable threshold, quiet and promotion moves and faster xray attack handling
bool chessposition::see(uint32_t move, int threshold)
{
    int from = GETFROM(move);
    int to = GETTO(move);

    int value = GETTACTICALVALUE(move) - threshold;

    if (value < 0)
        // the move itself is not good enough to reach the threshold
        return false;

    int nextPiece = (ISPROMOTION(move) ? GETPROMOTION(move) : GETPIECE(move)) >> 1;

    value -= prunematerialvalue[nextPiece];

    if (value >= 0)
        // the move is good enough even if the piece is recaptured
        return true;

    // Now things get a little more complicated...
    U64 seeOccupied = ((occupied00[0] | occupied00[1]) ^ BITSET(from)) | BITSET(to);
    U64 potentialRookAttackers = (piece00[WROOK] | piece00[BROOK] | piece00[WQUEEN] | piece00[BQUEEN]);
    U64 potentialBishopAttackers = (piece00[WBISHOP] | piece00[BBISHOP] | piece00[WQUEEN] | piece00[BQUEEN]);

    // Get attackers excluding the already moved piece
    U64 attacker = attackedByBB(to, seeOccupied) & seeOccupied;

    int s2m = (state & S2MMASK) ^ S2MMASK;

    while (true)
    {
        U64 nextAttacker = attacker & occupied00[s2m];
        // No attacker left => break
        if (!nextAttacker)
            break;

        // Find attacker with least value
        nextPiece = PAWN;
        while (!(nextAttacker & piece00[(nextPiece << 1) | s2m]))
            nextPiece++;

        // Simulate the move
        int attackerIndex;
        LSB(attackerIndex, nextAttacker & piece00[(nextPiece << 1) | s2m]);
        seeOccupied ^= BITSET(attackerIndex);

        // Add new shifting attackers but exclude already moved attackers using current seeOccupied
        if (nextPiece == PAWN || shifting[nextPiece] & 0x1 || nextPiece == KING)
            attacker |= (MAGICBISHOPATTACKS(seeOccupied, to) & potentialBishopAttackers);
        if (shifting[nextPiece] & 0x2 || nextPiece == KING)
            attacker |= (MAGICROOKATTACKS(seeOccupied, to) & potentialRookAttackers);

        // Remove attacker
        attacker &= seeOccupied;

        s2m ^= S2MMASK;

        value = -value - 1 - prunematerialvalue[nextPiece];

        if (value >= 0)
            break;
    }

    return (s2m ^ (state & S2MMASK));
}



int chessposition::getBestPossibleCapture()
{
    int me = state & S2MMASK;
    int you = me ^ S2MMASK;
    int captureval = 0;

    if (piece00[WQUEEN | you])
        captureval += prunematerialvalue[QUEEN];
    else if (piece00[WROOK | you])
        captureval += prunematerialvalue[ROOK];
    else if (piece00[WKNIGHT | you] || piece00[WBISHOP | you])
        captureval += prunematerialvalue[KNIGHT];
    else if (piece00[WPAWN | you])
        captureval += prunematerialvalue[PAWN];

    // promotion
    if (piece00[WPAWN | me] & RANK7(me))
        captureval += prunematerialvalue[QUEEN] - prunematerialvalue[PAWN];

    return captureval;
}



MoveSelector::~MoveSelector()
{
        delete captures;
        delete quiets;
}

void MoveSelector::SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, int nmrfttarget)
{
    pos = p;
    hashmove.code = p->shortMove2FullMove(hshm);
    if (kllm1 != hshm)
        killermove1.code = kllm1;
    if (kllm2 != hshm)
        killermove2.code = kllm2;
    refutetarget = nmrfttarget;
}

chessmove* MoveSelector::next()
{
    switch (state)
    {
    case INITSTATE:
        state++;
    case HASHMOVESTATE:
        state++;
        if (hashmove.code)
        {
            return &hashmove;
        }
    case TACTICALINITSTATE:
        state++;
        captures = new chessmovelist;
        captures->length = CreateMovelist<TACTICAL>(pos, &captures->move[0]);
        captures->sort(0);
        capturemovenum = 0;
    case TACTICALSTATE:
        while (capturemovenum < captures->length
            && (captures->move[capturemovenum].code == hashmove.code 
                || !pos->see(captures->move[capturemovenum].code, 0)))
        {
            // mark the move for BADTACTICALSTATE
            captures->move[capturemovenum].value |= (1 << 31);
            capturemovenum++;
        }
        if (capturemovenum < captures->length)
        {
            return &captures->move[capturemovenum++];
        }
        state++;
    case KILLERMOVE1STATE:
        state++;
        if (pos->moveIsPseudoLegal(killermove1.code))
        {
            return &killermove1;
        }
    case KILLERMOVE2STATE:
        state++;
        if (pos->moveIsPseudoLegal(killermove2.code))
        {
            return &killermove2;
        }
    case QUIETINITSTATE:
        state++;
        quiets = new chessmovelist;
        quiets->length = CreateMovelist<QUIET>(pos, &quiets->move[0]);
        quiets->sort(0, refutetarget);
        quietmovenum = 0;
    case QUIETSTATE:
        while (quietmovenum < quiets->length
            && (quiets->move[quietmovenum].code == hashmove.code
                || quiets->move[quietmovenum].code == killermove1.code
                || quiets->move[quietmovenum].code == killermove2.code))
        {
            quietmovenum++;
        }
        if (quietmovenum < quiets->length)
        {
            return &quiets->move[quietmovenum++];
        }
        state++;
        capturemovenum = 0;
    case BADTACTICALSTATE:
        while (capturemovenum < captures->length
            && (captures->move[capturemovenum].code == hashmove.code 
                || !(captures->move[capturemovenum].value & (1 << 31))))
        {
            capturemovenum++;
        }
        if (capturemovenum < captures->length)
        {
            return &captures->move[capturemovenum++];
        }
        state++;
    default:
        return nullptr;
    }
}


searchthread::searchthread()
{
    pwnhsh = NULL;
}

searchthread::~searchthread()
{
    delete pwnhsh;
}


engine::engine()
{
    initBitmaphelper();

    setOption("Threads", "1");  // order is important as the pawnhash depends on Threads > 0
    setOption("hash", "256");
    setOption("Move Overhead", "50");
    setOption("MultiPV", "1");
    setOption("Ponder", "false");
    setOption("SyzygyPath", "<empty>");
    setOption("Syzygy50MoveRule", "true");

#ifdef _WIN32
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    frequency = f.QuadPart;
#else
    frequency = 1000000000LL;
#endif
}

engine::~engine()
{
    setOption("SyzygyPath", "<empty>");
    delete[] sthread;
}

void engine::allocPawnhash()
{
    for (int i = 0; i < Threads; i++)
    {
        delete sthread[i].pwnhsh;
        sthread[i].pos.pwnhsh = sthread[i].pwnhsh = new Pawnhash(sizeOfPh);
    }
}


void engine::allocThreads()
{
    delete[] sthread;
    sthread = new searchthread[Threads];
    for (int i = 0; i < Threads; i++)
    {
        sthread[i].index = i;
        sthread[i].searchthreads = sthread;
        sthread[i].numofthreads = Threads;
    }
    allocPawnhash();
}


void engine::prepareThreads()
{
    sthread[0].pos.bestmovescore[0] = NOSCORE;
    sthread[0].pos.bestmove[0].code = 0;
    for (int i = 1; i < Threads; i++)
    {
        sthread[i].pos = sthread[0].pos;
        sthread[i].pos.pwnhsh = sthread[i].pwnhsh;
        sthread[i].pos.threadindex = i;
        // early reset of variables that are important for bestmove selection
        sthread[i].pos.bestmovescore[0] = NOSCORE;
        sthread[i].pos.bestmove[0].code = 0;
    }
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
    if (sName == "threads")
    {
        newint = stoi(sValue);
        if (newint >= 1 && newint <= MAXTHREADS && newint != Threads)
        {
            Threads = newint;
            resetTp = true;
        }
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
        sizeOfPh = max(16, tp.setSize(sizeOfTp) / Threads);
        allocThreads();
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

static void waitForSearchGuide(thread **th)
{
    if (*th)
    {
        if ((*th)->joinable())
            (*th)->join();
        delete *th;
    }
    *th = nullptr;
}

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
    thread *searchguidethread = nullptr;
    bool pendingisready = false;
    bool pendingposition = false;
    do
    {
        if (stopLevel >= ENGINESTOPIMMEDIATELY)
        {
            waitForSearchGuide(&searchguidethread);
        }
        if (pendingisready || pendingposition)
        {
            if (pendingposition)
            {
                // new position first stops current search
                if (stopLevel < ENGINESTOPIMMEDIATELY)
                {
                    stopLevel = ENGINESTOPIMMEDIATELY;
                    waitForSearchGuide(&searchguidethread);
                }
                chessposition *rootpos = &sthread[0].pos;
                rootpos->getFromFen(fen.c_str());
                for (vector<string>::iterator it = moves.begin(); it != moves.end(); ++it)
                {
                    if (!rootpos->applyMove(*it))
                        printf("info string Alarm! Zug %s nicht anwendbar (oder Enginefehler)\n", (*it).c_str());
                }
                rootpos->ply = 0;
                rootpos->getRootMoves();
                rootpos->tbFilterRootMoves();
                prepareThreads();
                if (debug)
                {
                    rootpos->print();
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
            command = myUci->parse(&commandargs, inputstring);  // blocking!!
            ci = 0;
            cs = commandargs.size();
            switch (command)
            {
            case UCIDEBUG:
                if (ci < cs)
                {
#ifdef SDEBUG
                    chessposition *rootpos = &sthread[0].pos;
#endif
                    if (commandargs[ci] == "on")
                        debug = true;
                    else if (commandargs[ci] == "off")
                        debug = false;
#ifdef SDEBUG
                    else if (commandargs[ci] == "this")
                        rootpos->debughash = rootpos->hash;
                    else if (commandargs[ci] == "pv")
                    {
                        rootpos->debugOnlySubtree = false;
                        rootpos->debugRecursive = false;
                        int i = 0;
                        while (++ci < cs)
                        {
                            string s = commandargs[ci];
                            if (s == "recursive")
                            {
                                rootpos->debugRecursive = true;
                                continue;
                            }
                            if (s == "sub")
                            {
                                rootpos->debugOnlySubtree = true;
                                continue;
                            }
                            if (s.size() < 4)
                                continue;
                            int from = AlgebraicToIndex(s, BOARDSIZE);
                            int to = AlgebraicToIndex(&s[2], BOARDSIZE);
                            int promotion = (s.size() <= 4) ? BLANK : (GetPieceType(s[4]) << 1); // Remember: S2m is missing here
                            rootpos->pvdebug[i++] = to | (from << 6) | (promotion << 12);
                        }
                        rootpos->pvdebug[i] = 0;
                    }
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
                myUci->send("option name Threads type spin default 1 min 1 max 64\n");
                myUci->send("uciok\n", author);
                break;
            case UCINEWGAME:
                // invalidate hash
                tp.clean();
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
                isWhite = (sthread[0].pos.w2m());
                stopLevel = ENGINERUN;
                searchguidethread = new thread(&searchguide);
                if (inputstring != "")
                {
                    // bench mode; wait for end of search
                    waitForSearchGuide(&searchguidethread);
                }
                break;
            case PONDERHIT:
                HitPonder();
                break;
            case STOP:
            case QUIT:
                stopLevel = ENGINESTOPIMMEDIATELY;
                break;
            case EVAL:
                // removed for now getValueTrace(&sthread[0].pos);
                break;
            default:
                break;
            }
        }
    } while (command != QUIT && (inputstring == "" || pendingposition));
    waitForSearchGuide(&searchguidethread);
}

zobrist zb;
engine en;
