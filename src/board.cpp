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

U64 knight_attacks[64];
U64 king_attacks[64];
U64 pawn_moves_to[64][2];          // bitboard of target square a pawn on index squares moves to
U64 pawn_moves_to_double[64][2];   // bitboard of target square a pawn on index squares moves to with a double push
U64 pawn_attacks_to[64][2];        // bitboard of (occupied) target squares a pawn on index square is attacking/capturing to
U64 pawn_moves_from[64][2];        // bitboard of source squares a pawn pushes to index square
U64 pawn_moves_from_double[64][2]; // bitboard of source squares a pawn double-pushes to index square
U64 pawn_attacks_from[64][2];      // bitboard of source squares a pawn is attacking/capturing on (occupied) index square
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
U64 lineMask[64][64];
int castleindex[64][64] = { { 0 } };
U64 castlekingto[64][2] = { { 0ULL } };
int castlerights[64];
int squareDistance[64][64];  // decreased by 1 for directly indexing evaluation arrays
int psqtable[14][64];


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


char PieceChar(PieceCode c, bool lower = false)
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
    if (!lower && !color)
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

    sprintf_s(s, "%c%d%c%d%c", (from & 0x7) + 'a', ((from >> 3) & 0x7) + 1, (to & 0x7) + 'a', ((to >> 3) & 0x7) + 1, PieceChar(promotion, true));
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

// Sorting for MoveSelector
chessmove* chessmovelist::getNextMove(int minval = INT_MIN)
{
    int current = -1;
    for (int i = 0; i < length; i++)
    {
        if (move[i].value > minval)
        {
            minval = move[i].value;
            current = i;
        }
    }

    if (current >= 0)
        return &move[current];

    return nullptr;
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

    psqval = 0;

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
        int i = AlgebraicToIndex(s);
        if (i < 64)
        {
            ept = i;
        }
    }

    /* half moves */
    if (numToken > 4)
    {
        try {
            halfmovescounter = stoi(token[4]);
        }
        catch (...) {}
    }

    /* full moves */
    if (numToken > 5)
    {
        try {
            fullmovescounter = stoi(token[5]);
        }
        catch (...) {}
    }

    isCheckbb = isAttackedBy<OCCUPIED>(kingpos[state & S2MMASK], (state & S2MMASK) ^ S2MMASK);
    updatePins();

    hash = zb.getHash(this);
    pawnhash = zb.getPawnHash(this);
    materialhash = zb.getMaterialHash(this);
    mstop = 0;
    rootheight = 0;
    lastnullmove = -1;
    return 0;
}



bool chessposition::applyMove(string s)
{
    unsigned int from, to;
    PieceType promtype;
    PieceCode promotion;

    from = AlgebraicToIndex(s);
    to = AlgebraicToIndex(&s[2]);

    if (s.size() > 4 && (promtype = GetPieceType(s[4])) != BLANKTYPE)
        promotion = (PieceCode)((promtype << 1) | (state & S2MMASK));
    else
        promotion = BLANK;

    chessmove m = chessmove(from, to, promotion, BLANK, BLANK);
    m.code = shortMove2FullMove((uint16_t)m.code);

    prepareStack();

    if (playMove(&m))
    {
        if (halfmovescounter == 0)
        {
            // Keep the list short, we have to keep below MAXMOVELISTLENGTH
            mstop = 0;
        }
        return true;
    }
    return false;
}


template <MoveType Mt>
void evaluateMoves(chessmovelist *ml, chessposition *pos, int16_t **cmptr)
{
    for (int i = 0; i < ml->length; i++)
    {
        uint32_t mc = ml->move[i].code;
        PieceCode piece = GETPIECE(mc);
        if (Mt == CAPTURE || (Mt == ALL && GETCAPTURE(mc)))
        {
            PieceCode capture = GETCAPTURE(mc);
            ml->move[i].value = (mvv[capture >> 1] | lva[piece >> 1]);
        }
        if (Mt == QUIET || (Mt == ALL && !GETCAPTURE(mc)))
        {
            ml->move[i].value = pos->history[piece & S2MMASK][GETFROM(mc)][GETTO(mc)];
            if (cmptr)
            {
                for (int j = 0; j < CMPLIES && cmptr[j]; j++)
                {
                    ml->move[i].value += cmptr[j][piece * 64 + GETTO(mc)];
                }
            }

        }
        if (GETPROMOTION(mc))
            ml->move[i].value += mvv[GETPROMOTION(mc) >> 1] - mvv[PAWN];
    }
}


void chessposition::getRootMoves()
{
    chessmovelist movelist;
    prepareStack();
    movelist.length = getMoves(&movelist.move[0]);
    evaluateMoves<ALL>(&movelist, this, NULL);

    int bestval = SCOREBLACKWINS;
    rootmovelist.length = 0;
    defaultmove.code = 0;

    uint16_t moveTo3fold = 0;
    bool bImmediate3fold = false;
    int ttscore, tteval;
    uint16_t tthashmovecode;
    bool tthit = tp.probeHash(hash, &ttscore, &tteval, &tthashmovecode, 0, SHRT_MIN + 1, SHRT_MAX, 0);

    excludemovestack[0] = 0; // FIXME: Not very nice; is it worth to do do singular testing in root search?
    for (int i = 0; i < movelist.length; i++)
    {
        if (playMove(&movelist.move[i]))
        {
            if (tthit)
            {
                // Test for immediate or possible 3fold to fix a possibly wrong hash entry
                if (testRepetiton() >= 2)
                {
                    // This move triggers 3fold; remember move to update hash
                    bImmediate3fold = true;
                    moveTo3fold = movelist.move[i].code;
                }
                else if ((uint16_t)movelist.move[i].code == tthashmovecode)
                {
                    // Test if this move makes a 3fold possible for opponent
                    prepareStack();
                    chessmovelist followupmovelist;
                    followupmovelist.length = getMoves(&followupmovelist.move[0]);
                    for (int j = 0; j < followupmovelist.length; j++)
                    {
                        if (playMove(&followupmovelist.move[j]))
                        {
                            if (testRepetiton() >= 2)
                                // 3fold for opponent is possible
                                moveTo3fold = movelist.move[i].code;

                            unplayMove(&followupmovelist.move[j]);
                        }
                    }
                }
            }

            rootmovelist.move[rootmovelist.length++] = movelist.move[i];
            unplayMove(&movelist.move[i]);
            if (bestval < movelist.move[i].value)
            {
                defaultmove = movelist.move[i];
                bestval = movelist.move[i].value;
            }
        }
    }
    if (moveTo3fold)
        // Hashmove triggers 3fold immediately or with following move; fix hash
        tp.addHash(hash, SCOREDRAW, tteval, bImmediate3fold ? HASHBETA : HASHALPHA, MAXDEPTH, moveTo3fold);
}


void chessposition::tbFilterRootMoves()
{
    useTb = min(TBlargest, en.SyzygyProbeLimit);
    tbPosition = 0;
    useRootmoveScore = 0;
    if (POPCOUNT(occupied00[0] | occupied00[1]) <= useTb)
    {
        if ((tbPosition = root_probe_dtz(this))) {
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
            defaultmove = rootmovelist.move[0];
        }
    }
}


// test the actual move for three-fold-repetition
// maybe this could be fixed in the future by using cuckoo tables like SF does it
// https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf
int chessposition::testRepetiton()
{
    int hit = 0;
    int lastrepply = max(mstop - halfmovescounter, lastnullmove + 1);
    for (int i = mstop - 4; i >= lastrepply; i -= 2)
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
    materialhash = zb.getMaterialHash(this);
    updatePins();
}


void chessposition::prepareStack()
{
    myassert(mstop >= 0 && mstop < MAXMOVESEQUENCELENGTH, this, 1, mstop);
    // copy stack related data directly to stack
    memcpy(&movestack[mstop], &state, sizeof(chessmovestack));
}


void chessposition::playNullMove()
{
    lastnullmove = mstop;
    movestack[mstop++].movecode = 0;
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
    lastnullmove = movestack[--mstop].lastnullmove;
    myassert(mstop >= 0, this, 1, mstop);
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

    int myept = 0;
    if (p == PAWN)
    {
        if (FILE(from) != FILE(to) && capture == BLANK)
        {
            // ep capture
            capture = pc ^ S2MMASK;
            myept = ISEPCAPTURE;
        }
        else if ((from ^ to) == 16 && (epthelper[to] & piece00[pc ^ 1]))
        {
            // double push enables epc
            myept = (from + to) / 2;
        }
    }

    uint32_t fc = (pc << 28) | (myept << 20) | (capture << 16) | c;
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
    unsigned int piececol = (pc & S2MMASK);

    myassert(pc >= WPAWN && pc <= BKING, this, 1, pc);

    // correct piece?
    if (mailbox[from] != pc)
        return false;

    // correct capture?
    if (mailbox[to] != capture && !GETEPCAPTURE(c))
        return false;

    // correct color of capture? capturing the king is illegal
    if (capture && (piececol == (capture & S2MMASK) || capture >= WKING))
        return false;

    myassert(capture >= BLANK && capture <= BQUEEN, this, 1, capture);

    // correct target for type of piece?
    if (!(movesTo(pc, from) & BITSET(to)) && (!ept || to != ept || p != PAWN))
        return false;

    // correct s2m?
    if (piececol != (state & S2MMASK))
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
            int myept = GETEPT(c);
            {
                if (!myept == (bool)(epthelper[to] & piece00[pc ^ 1]))
                    return false;
            }
        }
        else
        {
            // wrong ep capture
            if (GETEPCAPTURE(c) && ept != to)
                return false;

            // missing promotion
            if (RRANK(to, piececol) == 7 && !GETPROMOTION(c))
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


void chessposition::updatePins()
{
    for (int me = WHITE; me <= BLACK; me++)
    {
        int you = me ^ S2MMASK;
        int k = kingpos[me];
        kingPinned[me] = 0ULL;
        U64 occ = occupied00[you];
        U64 attackers = mRookAttacks[k][MAGICROOKINDEX(occ, k)] & (piece00[WROOK | you] | piece00[WQUEEN | you]);
        attackers |= mBishopAttacks[k][MAGICBISHOPINDEX(occ, k)] & (piece00[WBISHOP | you] | piece00[WQUEEN | you]);
        
        while (attackers)
        {
            int index = pullLsb(&attackers);
            U64 potentialPinners = betweenMask[index][k] & occupied00[me];
            if (ONEORZERO(potentialPinners))
                kingPinned[me] |= potentialPinners;
        }
    }
}


bool chessposition::moveGivesCheck(uint32_t c)
{
    int pc = GETPIECE(c);

    // We assume that king moves don't give check; this missed some discovered checks but is faster
    if ((pc >> 1) == KING)
        return false;

    int me = pc & S2MMASK;
    int you = me ^ S2MMASK;
    int yourKing = kingpos[you];

    // test if moving piece gives check
    if (movesTo(pc, GETTO(c)) & BITSET(yourKing))
        return true;

    // test for discovered check
    if (isAttackedByMySlider(yourKing, (occupied00[0] | occupied00[1]) ^ BITSET(GETTO(c)) ^ BITSET(GETFROM(c)), me))
        return true;

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
    *os << "Hash: 0x" << hex << hash << " (should be 0x" << hex << zb.getHash(this) << ")\n";
    *os << "Pawn Hash: 0x" << hex << pawnhash << " (should be 0x" << hex << zb.getPawnHash(this) << ")\n";
    *os << "Material Hash: 0x" << hex << materialhash << " (should be 0x" << hex << zb.getMaterialHash(this) << ")\n";
    *os << "Value: " + to_string(getEval<NOTRACE>()) + "\n";
    *os << "Repetitions: " + to_string(testRepetiton()) + "\n";
    *os << "Phase: " + to_string(phase()) + "\n";
    *os << "Pseudo-legal Moves: " + pseudolegalmoves.toStringWithValue() + "\n";
#if defined(STACKDEBUG) || defined(SDEBUG)
    *os << "Moves in current search: " + movesOnStack() + "\n";
#endif
    *os << "mstop: " + to_string(mstop) + "\n";
    *os << "Ply: " + to_string(ply) + "\n";
    *os << "rootheight: " + to_string(rootheight) + "\n";
    stringstream ss;
    ss << hex << bestmove.code;
    *os << "bestmove[0].code: 0x" + ss.str() + "\n";
}


#if defined(STACKDEBUG) || defined(SDEBUG)
string chessposition::movesOnStack()
{
    string s = "";
    for (int i = 0; i < mstop; i++)
    {
        chessmove cm;
        cm.code = movestack[i].movecode;
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


void chessposition::updateMultiPvTable(int pvindex, uint32_t mc)
{
    uint32_t *table = (pvindex ? multipvtable[pvindex] : pvtable[0]);
    table[0] = mc;
    int i = 0;
    while (pvtable[1][i])
    {
        table[i + 1] = pvtable[1][i];
        i++;
    }
    table[i + 1] = 0;
}


void chessposition::updatePvTable(uint32_t mc, bool recursive)
{
    pvtable[ply][0] = mc;
    int i = 0;
    if (recursive)
    {
        while (pvtable[ply + 1][i])
        {
            pvtable[ply][i + 1] = pvtable[ply + 1][i];
            i++;
        }
    }
    pvtable[ply][i + 1] = 0;
}

string chessposition::getPv(uint32_t *table)
{
    string s = "";
    for (int i = 0; table[i]; i++)
    {
        chessmove cm;
        cm.code = table[i];
        s += cm.toString() + " ";
    }
    return s;
}

#ifdef SDEBUG
bool chessposition::triggerDebug(chessmove* nextmove)
{
    if (pvdebug[0] == 0)
        return false;

    int j = 0;

    while (j + rootheight < mstop && pvdebug[j])
    {
        if ((movestack[j + rootheight].movecode & 0xefff) != pvdebug[j])
            return false;
        j++;
    }
    nextmove->code = pvdebug[j];
 
    return true;
}


const char* PvAbortStr[] = {
    "unknown", "pv from tt", "different move in tt", "razor-pruned", "reverse-futility-pruned", "nullmove-pruned", "probcut-pruned", "late-move-pruned",
    "futility-pruned", "bad-see-pruned", "bad-history-pruned", "multicut-pruned", "bestmove", "not best move", "omitted", "betacut", "below alpha"
};


void chessposition::pvdebugout()
{
    printf("====================================\nMove  Num Dep   Val  Reason\n------------------------------------\n");
    for (int i = 0; pvdebug[i]; i++)
    {
        chessmove m;
        m.code = pvdebug[i];
        printf("%s %s%2d  %2d  %4d  %s\n", m.toString().c_str(), pvmovenum[i] < 0 ? ">" : " ", abs(pvmovenum[i]), pvdepth[i], pvabortval[i], PvAbortStr[pvaborttype[i]]);
        if (pvaborttype[i + 1] == PVA_UNKNOWN || pvaborttype[i] == PVA_OMITTED)
            break;
    }
    printf("====================================\n\n");
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
    while (mask)
    {
        k = pullLsb(&mask);
        if (j & BITSET(i))
            occ |= (1ULL << k);
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
const U64 bishopmagics[] = {
    0x1002004102008200, 0x1002004102008200, 0x4310002248214800, 0x402010c110014208, 0xa000a06240114001, 0xa000a06240114001, 0x402010c110014208, 0xa000a06240114001,
    0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x100c009840001000, 0x4310002248214800, 0xa000a06240114001, 0x4310002248214800,
    0x4310002248214800, 0x822143005020a148, 0x0001901c00420040, 0x0880504024308060, 0x0100201004200002, 0xa000a06240114001, 0x822143005020a148, 0x1002004102008200,
    0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x2008080100820102, 0x1481010004104010, 0x0002052000100024, 0xc880221002060081, 0xc880221002060081,
    0x4310002248214800, 0xc880221002060081, 0x0001901c00420040, 0x8400208020080201, 0x000e008400060020, 0x00449210e3902028, 0x402010c110014208, 0xc880221002060081,
    0x100c009840001000, 0xc880221002060081, 0x1000820800c00060, 0x2803101084008800, 0x2200608200100080, 0x0040900130840090, 0x0024010008800a00, 0x0400110410804810,
    0x402010c110014208, 0xa000a06240114001, 0xa000a06240114001, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200,
    0xa000a06240114001, 0x4310002248214800, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200, 0x1002004102008200
};

const U64 rookmagics[] = {
    0x8200108041020020, 0x8200108041020020, 0xc880221002060081, 0x0009100804021000, 0x0500010004107800, 0x0024010008800a00, 0x0400110410804810, 0x8300038100004222,
    0x004a800182c00020, 0x0009100804021000, 0x3002200010c40021, 0x0020100104000208, 0x01021001a0080020, 0x0884020010082100, 0x1000820800c00060, 0x8020480110020020,
    0x0002052000100024, 0x0200190040088100, 0x0030802001a00800, 0x8010002004000202, 0x0040010100080010, 0x2200608200100080, 0x0001901c00420040, 0x0001400a24008010,
    0x1400a22008001042, 0x8200108041020020, 0x2004500023002400, 0x8105100028001048, 0x8010024d00014802, 0x8000820028030004, 0x402010c110014208, 0x8300038100004222,
    0x0001804002800124, 0x0084022014041400, 0x0030802001a00800, 0x0110a01001080008, 0x0b10080850081100, 0x000010040049020c, 0x0024010008800a00, 0x014c800040100426,
    0x1100400010208000, 0x0009100804021000, 0x0010024871202002, 0x8014001028c80801, 0x1201082010a00200, 0x0002008004102009, 0x8300038100004222, 0x0000401001a00408,
    0x4520920010210200, 0x0400110410804810, 0x8105100028001048, 0x8105100028001048, 0x0802801009083002, 0x8200108041020020, 0x8200108041020020, 0x4000a12400848110,
    0x2000804026001102, 0x2000804026001102, 0x800040a010040901, 0x80001802002c0422, 0x0010b018200c0122, 0x200204802a080401, 0x8880604201100844, 0x80000cc281092402
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
        // initialize psqtable for faster evaluation
        for (int pc = 0; pc <= BKING; pc++)
        {
            int p = pc >> 1;
            int s2m = pc & S2MMASK;
            psqtable[pc][from] = S2MSIGN(s2m) * (eps.eMaterialvalue[p] + eps.ePsqt[p][PSQTINDEX(from, s2m)]);
        }

        king_attacks[from] = knight_attacks[from] = 0ULL;
        pawn_moves_to[from][0] = pawn_attacks_to[from][0] = pawn_moves_to_double[from][0] = 0ULL;
        pawn_moves_to[from][1] = pawn_attacks_to[from][1] = pawn_moves_to_double[from][1] = 0ULL;
        pawn_moves_from[from][0] = pawn_attacks_from[from][0] = pawn_moves_from_double[from][0] = 0ULL;
        pawn_moves_from[from][1] = pawn_attacks_from[from][1] = pawn_moves_from_double[from][1] = 0ULL;
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
            squareDistance[from][j] = max(abs(RANK(from) - RANK(j)), abs(FILE(from) - FILE(j))) - 1;
            betweenMask[from][j] = 0ULL;
            lineMask[from][j] = 0ULL;
            
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
                for (int i = 0; i < 8; i++)
                    lineMask[from][j] |= BITSET(INDEX(i, FILE(from)));
            }
            if (RANK(from) == RANK(j))
            {
                rankMask[from] |= BITSET(j);
                for (int i = min(FILE(from), FILE(j)) + 1; i < max(FILE(from), FILE(j)); i++)
                    betweenMask[from][j] |= BITSET(INDEX(RANK(from), i));
                for (int i = 0; i < 8; i++)
                    lineMask[from][j] |= BITSET(INDEX(RANK(from), i));
            }
            if (from != j && abs(RANK(from) - RANK(j)) == abs(FILE(from) - FILE(j)))
            {
                int dx = (FILE(from) < FILE(j) ? 1 : -1);
                int dy = (RANK(from) < RANK(j) ? 1 : -1);
                for (int i = 1; FILE(from) +  i * dx != FILE(j); i++)
                    betweenMask[from][j] |= BITSET(INDEX(RANK(from) + i * dy, FILE(from) + i * dx));

                for (int i = -7; i < 8; i++)
                {
                    int r = RANK(from) + i * dy;
                    int f = FILE(from) + i * dx;
                    if (r >= 0 && r < 8 && f >= 0 && f < 8)
                        lineMask[from][j] |= BITSET(INDEX(r, f));
                }
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
            if (RRANK(from, s) < 7)
                pawn_moves_to[from][s] |= BITSET(from + S2MSIGN(s) * 8);
            if (RRANK(from, s) > 0)
                pawn_moves_from[from][s] |= BITSET(from - S2MSIGN(s) * 8);

            if (RRANK(from, s) == 1)
            {
                // Double move
                pawn_moves_to_double[from][s] |= BITSET(from + S2MSIGN(s) * 16);
            }
            if (RRANK(from, s) == 3)
            {
                // Double move
                pawn_moves_from_double[from][s] |= BITSET(from - S2MSIGN(s) * 16);
            }
            // Captures
            for (int d = -1; d <= 1; d++)
            {
                to = from - S2MSIGN(s) * 8 + d;
                if (d && abs(FILE(from) - FILE(to)) <= 1 && to >= 0 && to < 64)
                    pawn_attacks_from[from][s] |= BITSET(to);

                to = from + S2MSIGN(s) * 8 + d;
                if (abs(FILE(from) - FILE(to)) <= 1 && to >= 0 && to < 64)
                {
                    if (d)
                        pawn_attacks_to[from][s] |= BITSET(to);
                    for (int r = to; r >= 0 && r < 64; r += S2MSIGN(s) * 8)
                    {
                        passedPawnMask[from][s] |= BITSET(r);
                        if (!d)
                            filebarrierMask[from][s] |= BITSET(r);
                        if (abs(RANK(from) - RANK(r)) <= 1
                            || (RRANK(from,s) == 0 && RRANK(r,s) == 2))
                            kingshieldMask[from][s] |= BITSET(r);
                    }
                }
            }
            kingdangerMask[from][s] = king_attacks[from] | BITSET(from);
        }

        // Slider attacks
        // Fill the mask
        mBishopTbl[from].mask = 0ULL;
        mRookTbl[from].mask = 0ULL;
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

        // mBishopTbl[from].magic = getMagicCandidate(mBishopTbl[from].mask);
        mBishopTbl[from].magic = bishopmagics[from];

        for (int j = 0; j < (1 << BISHOPINDEXBITS); j++) {
            // First get the subset of mask corresponding to j
            U64 occ = getOccupiedFromMBIndex(j, mBishopTbl[from].mask);
            // Now get the attack bitmap for this subset and store to attack table
            U64 attack = (getAttacks(from, occ, -7) | getAttacks(from, occ, 7) | getAttacks(from, occ, -9) | getAttacks(from, occ, 9));
            int hashindex = MAGICBISHOPINDEX(occ, from);
            mBishopAttacks[from][hashindex] = attack;
        }

        // mRookTbl[from].magic = getMagicCandidate(mRookTbl[from].mask);
        mRookTbl[from].magic = rookmagics[from];

        for (int j = 0; j < (1 << ROOKINDEXBITS); j++) {
            // First get the subset of mask corresponding to j
            U64 occ = getOccupiedFromMBIndex(j, mRookTbl[from].mask);
            // Now get the attack bitmap for this subset and store to attack table
            U64 attack = (getAttacks(from, occ, -1) | getAttacks(from, occ, 1) | getAttacks(from, occ, -8) | getAttacks(from, occ, 8));
            int hashindex = MAGICROOKINDEX(occ, from);
            mRookAttacks[from][hashindex] = attack;
        }

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
    myassert(index >= 0 && index < 64, this, 1, index);
    myassert(p >= BLANK && p <= BKING, this, 1, p);
    int s2m = p & 0x1;
    piece00[p] |= BITSET(index);
    occupied00[s2m] |= BITSET(index);
    psqval += psqtable[p][index];
}


void chessposition::BitboardClear(int index, PieceCode p)
{
    myassert(index >= 0 && index < 64, this, 1, index);
    myassert(p >= BLANK && p <= BKING, this, 1, p);
    int s2m = p & 0x1;
    piece00[p] ^= BITSET(index);
    occupied00[s2m] ^= BITSET(index);
    psqval -= psqtable[p][index];
}


void chessposition::BitboardMove(int from, int to, PieceCode p)
{
    myassert(from >= 0 && from < 64, this, 1, from);
    myassert(to >= 0 && to < 64, this, 1, to);
    myassert(p >= BLANK && p <= BKING, this, 1, p);
    int s2m = p & 0x1;
    piece00[p] ^= (BITSET(from) | BITSET(to));
    occupied00[s2m] ^= (BITSET(from) | BITSET(to));
    psqval += psqtable[p][to] - psqtable[p][from];
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
    
    PREFETCH(&mh.table[materialhash & MATERIALHASHMASK]);

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
    isCheckbb = isAttackedBy<OCCUPIED>(kingpos[s2m ^ S2MMASK], s2m);

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
    movestack[mstop++].movecode = cm->code;
    myassert(mstop < MAXMOVESEQUENCELENGTH, this, 1, mstop);
    updatePins();
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


inline void appendMoveToList(chessmove **m, int from, int to, PieceCode piece, PieceCode capture)
{
    **m = chessmove(from, to, capture, piece);
    (*m)++;
}


inline void appendPromotionMove(chessposition *pos, chessmove **m, int from, int to, int col, PieceType promote)
{
    appendMoveToList(m, from, to, WPAWN | col, pos->mailbox[to]);
    (*m - 1)->code |= ((promote << 1) | col) << 12;
}


template <MoveType Mt> int CreateMovelist(chessposition *pos, chessmove* mstart)
{
    int me = pos->state & S2MMASK;
    int you = me ^ S2MMASK;
    U64 occupiedbits = (pos->occupied00[0] | pos->occupied00[1]);
    U64 emptybits = ~occupiedbits;
    U64 targetbits = 0ULL;
    U64 frombits, tobits;
    int from, to;
    PieceCode pc;
    chessmove* m = mstart;

    if (Mt == EVASION)
    {
        int king = pos->kingpos[me];
        // moving the king is alway a possibe evasion
        targetbits = king_attacks[king] & ~pos->occupied00[me];
        while (targetbits)
        {
            to = pullLsb(&targetbits);
            if (!pos->isAttackedBy<OCCUPIEDANDKING>(to, you) && !pos->isAttackedByMySlider(to, occupiedbits ^ BITSET(king), you))
            {
                appendMoveToList(&m, king, to, WKING | me, pos->mailbox[to]);
            }
        }

        if (POPCOUNT(pos->isCheckbb) == 1)
        {
            // only one attacker => capture or block the attacker is a possible evasion
            int attacker;
            GETLSB(attacker, pos->isCheckbb);
            // special case: attacker is pawn and can be captured enpassant
            if (pos->ept && pos->ept == attacker + S2MSIGN(me) * 8)
            {
                frombits = pawn_attacks_from[pos->ept][me] & pos->piece00[WPAWN | me];
                while (frombits)
                {
                    from = pullLsb(&frombits);
                    // treat ep capture as normal move and correct code manually
                    appendMoveToList(&m, from, attacker + S2MSIGN(me) * 8, WPAWN | me, WPAWN | you);
                    (m - 1)->code |= (ISEPCAPTURE << 20);
                }
            }
            // now normal captures of the attacker
            to = attacker;
            frombits = pos->isAttackedBy<OCCUPIED>(to, me);
            // later: blockers; targetbits will contain empty squares between king and attacking slider
            targetbits = betweenMask[king][attacker];
            while (true)
            {
                frombits = frombits & ~pos->kingPinned[me];
                while (frombits)
                {
                    from = pullLsb(&frombits);
                    pc = pos->mailbox[from];
                    if ((pc >> 1) == PAWN)
                    {
                        if (PROMOTERANK(to))
                        {
                            appendPromotionMove(pos, &m, from, to, me, QUEEN);
                            appendPromotionMove(pos, &m, from, to, me, ROOK);
                            appendPromotionMove(pos, &m, from, to, me, BISHOP);
                            appendPromotionMove(pos, &m, from, to, me, KNIGHT);
                            continue;
                        }
                        else if (!((from ^ to) & 0x8) && (epthelper[to] & pos->piece00[pc ^ S2MMASK]))
                        {
                            // EPT possible for opponent; set EPT field manually
                            appendMoveToList(&m, from, to, pc, BLANK);
                            (m - 1)->code |= (from + to) << 19;
                            continue;
                        }
                    }
                    appendMoveToList(&m, from, to, pc, pos->mailbox[to]);
                }
                if (!targetbits)
                    break;
                to = pullLsb(&targetbits);
                frombits = pos->isAttackedBy<FREE>(to, me);  // <FREE> is needed here as the target fields are empty and pawns move normal
            }
        }
        return (int)(m - mstart);
    }

    // Now the "normal" move generations
    if (Mt & QUIET)
        targetbits |= emptybits;
    if (Mt & CAPTURE)
        targetbits |= pos->occupied00[me ^ S2MMASK];

    for (int p = PAWN; p <= KING; p++)
    {
        pc = (PieceCode)((p << 1) | me);
        frombits = pos->piece00[pc];
        switch (p)
        {
        case PAWN:
            while (frombits)
            {
                from = pullLsb(&frombits);
                tobits = 0ULL;
                if (Mt & QUIET)
                {
                    tobits |= (pawn_moves_to[from][me] & emptybits & ~PROMOTERANKBB);
                    if (tobits)
                        tobits |= (pawn_moves_to_double[from][me] & emptybits);
                }
                if (Mt & PROMOTE)
                {
                    // get promoting pawns
                    tobits |= (pawn_moves_to[from][me] & PROMOTERANKBB & emptybits);
                }
                if (Mt & CAPTURE)
                {
                    /* FIXME: ept & EPTSIDEMASK[me] is a quite ugly test for correct side respecting null move pruning */
                    tobits |= (pawn_attacks_to[from][me] & (pos->occupied00[you] | ((pos->ept & EPTSIDEMASK[me]) ? BITSET(pos->ept) : 0ULL)));
                }
                while (tobits)
                {
                    to = pullLsb(&tobits);
                    if ((Mt & CAPTURE) && pos->ept && pos->ept == to)
                    {
                        // treat ep capture as a normal move and correct code manually
                        appendMoveToList(&m, from, to, pc, WPAWN | you);
                        (m - 1)->code |= (ISEPCAPTURE << 20);
                    }
                    else if (PROMOTERANK(to))
                    {
                        appendPromotionMove(pos, &m, from, to, me, QUEEN);
                        appendPromotionMove(pos, &m, from, to, me, ROOK);
                        appendPromotionMove(pos, &m, from, to, me, BISHOP);
                        appendPromotionMove(pos, &m, from, to, me, KNIGHT);
                    }
                    else if ((Mt & QUIET) && !((from ^ to) & 0x8) && (epthelper[to] & pos->piece00[pc ^ 1]))
                    {
                        // EPT possible for opponent; set EPT field manually
                        appendMoveToList(&m, from, to, pc, BLANK);
                        (m - 1)->code |= (from + to) << 19;
                    }
                    else {
                        appendMoveToList(&m, from, to, pc, pos->mailbox[to]);
                    }
                    if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                    {
                        pos->unplayMove(m - 1);
                        return 1;
                    }
                }
            }
            break;
        case KNIGHT:
            while (frombits)
            {
                from = pullLsb(&frombits);
                tobits = (knight_attacks[from] & targetbits);
                while (tobits)
                {
                    to = pullLsb(&tobits);
                    appendMoveToList(&m, from, to, pc, pos->mailbox[to]);
                    if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                    {
                        pos->unplayMove(m - 1);
                        return 1;
                    }
                }
            }
            break;
        case KING:
            from = pos->kingpos[me];
            tobits = (king_attacks[from] & targetbits);
            while (tobits)
            {
                to = pullLsb(&tobits);
                appendMoveToList(&m, from, to, pc, pos->mailbox[to]);
                if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                {
                    pos->unplayMove(m - 1);
                    return 1;
                }
            }
            if (Mt & QUIET)
            {
                if (pos->state & QCMASK[me])
                {
                    /* queen castle */
                    if (!(occupiedbits & (me ? 0x0e00000000000000 : 0x000000000000000e))
                        && !pos->isAttacked(from) && !pos->isAttacked(from - 1) && !pos->isAttacked(from - 2))
                    {
                        appendMoveToList(&m, from, from - 2, pc, BLANK);
                        if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                        {
                            pos->unplayMove(m - 1);
                            return 1;
                        }
                    }
                }
                if (pos->state & KCMASK[me])
                {
                    /* kink castle */
                    if (!(occupiedbits & (me ? 0x6000000000000000 : 0x0000000000000060))
                        && !pos->isAttacked(from) && !pos->isAttacked(from + 1) && !pos->isAttacked(from + 2))
                    {
                        appendMoveToList(&m, from, from + 2, pc, BLANK);
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
            while (frombits)
            {
                from = pullLsb(&frombits);
                if (p == BISHOP || p == QUEEN)
                {
                    tobits |= (MAGICBISHOPATTACKS(occupiedbits, from) & targetbits);
                }
                if (p == ROOK || p == QUEEN)
                {
                    tobits |= (MAGICROOKATTACKS(occupiedbits, from) & targetbits);
                }
                while (tobits)
                {
                    to = pullLsb(&tobits);
                    appendMoveToList(&m, from, to, pc, pos->mailbox[to]);
                    if (Mt == QUIETWITHCHECK && pos->playMove(m - 1))
                    {
                        pos->unplayMove(m - 1);
                        return 1;
                    }
                }
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
        case EVASION:
            return CreateMovelist<EVASION>(this, m);
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
        return ((pawn_moves_to[from][s2m] | pawn_moves_to_double[from][s2m]) & ~occ)
                | (pawn_attacks_to[from][s2m] & occ);
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


template <PieceType Pt>
U64 chessposition::pieceMovesTo(int from)
{
    U64 occ = occupied00[0] | occupied00[1];
    switch (Pt)
    {
    case KNIGHT:
        return knight_attacks[from];
    case BISHOP:
        return MAGICBISHOPATTACKS(occ, from);
    case ROOK:
        return MAGICROOKATTACKS(occ, from);
    case QUEEN:
        return MAGICBISHOPATTACKS(occ, from) | MAGICROOKATTACKS(occ, from);
    default:
        return 0ULL;
    }
}


// this is only used for king attacks, so opponent king attacks can be left out
template <AttackType At> U64 chessposition::isAttackedBy(int index, int col)
{
    U64 occ = occupied00[0] | occupied00[1];
    return (knight_attacks[index] & piece00[WKNIGHT | col])
        | (MAGICROOKATTACKS(occ, index) & (piece00[WROOK | col] | piece00[WQUEEN | col]))
        | (MAGICBISHOPATTACKS(occ, index) & (piece00[WBISHOP | col] | piece00[WQUEEN | col]))
        | (piece00[WPAWN | col] & (At != FREE ?
            pawn_attacks_from[index][col] :
            pawn_moves_from[index][col] | (pawn_moves_from_double[index][col] & PAWNPUSH(col ^ S2MMASK, ~occ))))
        | (At == OCCUPIEDANDKING ? (king_attacks[index] & piece00[WKING | col]) : 0ULL);
}


bool chessposition::isAttacked(int index)
{
    int opponent = (state & S2MMASK) ^ 1;

    return knight_attacks[index] & piece00[WKNIGHT | opponent]
        || king_attacks[index] & piece00[WKING | opponent]
        || pawn_attacks_to[index][state & S2MMASK] & piece00[(PAWN << 1) | opponent]
        || MAGICROOKATTACKS(occupied00[0] | occupied00[1], index) & (piece00[WROOK | opponent] | piece00[WQUEEN | opponent])
        || MAGICBISHOPATTACKS(occupied00[0] | occupied00[1], index) & (piece00[WBISHOP | opponent] | piece00[WQUEEN | opponent]);
}

// used for checkevasion test, could be usefull for discovered check test
U64 chessposition::isAttackedByMySlider(int index, U64 occ, int me)
{
    return (MAGICROOKATTACKS(occ, index) & (piece00[WROOK | me] | piece00[WQUEEN | me]))
        | (MAGICBISHOPATTACKS(occ, index) & (piece00[WBISHOP | me] | piece00[WQUEEN | me]));
}


U64 chessposition::attackedByBB(int index, U64 occ)
{
    return (knight_attacks[index] & (piece00[WKNIGHT] | piece00[BKNIGHT]))
        | (king_attacks[index] & (piece00[WKING] | piece00[BKING]))
        | (pawn_attacks_to[index][1] & piece00[WPAWN])
        | (pawn_attacks_to[index][0] & piece00[BPAWN])
        | (MAGICROOKATTACKS(occ, index) & (piece00[WROOK] | piece00[BROOK] | piece00[WQUEEN] | piece00[BQUEEN]))
        | (MAGICBISHOPATTACKS(occ, index) & (piece00[WBISHOP] | piece00[BBISHOP] | piece00[WQUEEN] | piece00[BQUEEN]));
}


int chessposition::phase()
{
    // minor ~ 10-11    rook ~ 21-22    queen ~ 42-43
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

    value -= materialvalue[nextPiece];

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
        GETLSB(attackerIndex, nextAttacker & piece00[(nextPiece << 1) | s2m]);
        seeOccupied ^= BITSET(attackerIndex);

        // Add new shifting attackers but exclude already moved attackers using current seeOccupied
        if ((nextPiece & 0x1) || nextPiece == KING)  // pawn, bishop, queen, king
            attacker |= (MAGICBISHOPATTACKS(seeOccupied, to) & potentialBishopAttackers);
        if (nextPiece == ROOK || nextPiece == QUEEN || nextPiece == KING)
            attacker |= (MAGICROOKATTACKS(seeOccupied, to) & potentialRookAttackers);

        // Remove attacker
        attacker &= seeOccupied;

        s2m ^= S2MMASK;

        value = -value - 1 - materialvalue[nextPiece];

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
        captureval += materialvalue[QUEEN];
    else if (piece00[WROOK | you])
        captureval += materialvalue[ROOK];
    else if (piece00[WKNIGHT | you] || piece00[WBISHOP | you])
        captureval += materialvalue[KNIGHT];
    else if (piece00[WPAWN | you])
        captureval += materialvalue[PAWN];

    // promotion
    if (piece00[WPAWN | me] & RANK7(me))
        captureval += materialvalue[QUEEN] - materialvalue[PAWN];

    return captureval;
}



// MoveSelector for quiescence search
void MoveSelector::SetPreferredMoves(chessposition *p)
{
    pos = p;
    hashmove.code = 0;
    killermove1.code = killermove2.code = 0;
    if (!p->isCheckbb)
    {
        onlyGoodCaptures = true;
        if (!ISTACTICAL(hashmove.code))
            state = TACTICALINITSTATE;
    }
    else
    {
        state = EVASIONINITSTATE;
        pos->getCmptr(&cmptr[0]);
    }
    captures = &pos->captureslist[pos->ply];
    quiets = &pos->quietslist[pos->ply];
}

// MoveSelector for alphabeta search
void MoveSelector::SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, uint32_t counter, int excludemove)
{
    pos = p;
    hashmove.code = p->shortMove2FullMove(hshm);
    if (kllm1 != hshm)
        killermove1.code = kllm1;
    if (kllm2 != hshm)
        killermove2.code = kllm2;
    if (counter != hshm && counter != kllm1 && counter != kllm2)
        countermove.code = counter;
    pos->getCmptr(&cmptr[0]);
    if (!excludemove)
    {
        captures = &pos->captureslist[pos->ply];
        quiets = &pos->quietslist[pos->ply];
    }
    else
    {
        captures = &pos->singularcaptureslist[pos->ply];
        quiets = &pos->singularquietslist[pos->ply];
    }
    if (p->isCheckbb)
        state = EVASIONINITSTATE;
}


chessmove* MoveSelector::next()
{
    chessmove *m;
    switch (state)
    {
    case INITSTATE:
        state++;
        // fall through
    case HASHMOVESTATE:
        state++;
        if (hashmove.code)
        {
            return &hashmove;
        }
        // fall through
    case TACTICALINITSTATE:
        state++;
        captures->length = CreateMovelist<TACTICAL>(pos, &captures->move[0]);
        evaluateMoves<CAPTURE>(captures, pos, &cmptr[0]);
        // fall through
    case TACTICALSTATE:
        while ((m = captures->getNextMove(0)))
        {
            if (!pos->see(m->code, onlyGoodCaptures))
            {
                m->value |= BADTACTICALFLAG;
            }
            else {
                m->value = INT_MIN;
                if (m->code != hashmove.code)
                    return m;
            }
        }
        state++;
        if (onlyGoodCaptures)
            return nullptr;
        // fall through
    case KILLERMOVE1STATE:
        state++;
        if (pos->moveIsPseudoLegal(killermove1.code))
        {
            return &killermove1;
        }
        // fall through
    case KILLERMOVE2STATE:
        state++;
        if (pos->moveIsPseudoLegal(killermove2.code))
        {
            return &killermove2;
        }
        // fall through
    case COUNTERMOVESTATE:
        state++;
        if (pos->moveIsPseudoLegal(countermove.code))
        {
            return &countermove;
        }
        // fall through
    case QUIETINITSTATE:
        state++;
        quiets->length = CreateMovelist<QUIET>(pos, &quiets->move[0]);
        evaluateMoves<QUIET>(quiets, pos, &cmptr[0]);
        // fall through
    case QUIETSTATE:
        while ((m = quiets->getNextMove()))
        {
            m->value = INT_MIN;
            if (m->code != hashmove.code
                && m->code != killermove1.code
                && m->code != killermove2.code
                && m->code != countermove.code)
                return m;
        }
        state++;
        // fall through
    case BADTACTICALSTATE:
        while ((m = captures->getNextMove()))
        {
            bool bBadTactical = (m->value & BADTACTICALFLAG);
            m->value = INT_MIN;
            if (bBadTactical)
                return m;
        }
        state++;
        // fall through
    case BADTACTICALEND:
        return nullptr;
    case EVASIONINITSTATE:
        state++;
        captures->length = CreateMovelist<EVASION>(pos, &captures->move[0]);
        evaluateMoves<ALL>(captures, pos, &cmptr[0]);
        // fall through
    case EVASIONSTATE:
        while ((m = captures->getNextMove()))
        {
            m->value = INT_MIN;
            return m;
        }
        state++;
        // fall through
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
    rootposition.pwnhsh = new Pawnhash(1);  // some dummy pawnhash just to make the prefetch in playMove happy
    setOption("Threads", "1");  // order is important as the pawnhash depends on Threads > 0
    setOption("hash", to_string(DEFAULTHASH));
    setOption("Move Overhead", "50");
    setOption("MultiPV", "1");
    setOption("Ponder", "false");
    setOption("SyzygyPath", "<empty>");
    setOption("Syzygy50MoveRule", "true");
    setOption("SyzygyProbeLimit", "7");

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
    delete rootposition.pwnhsh;
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
    prepareThreads();
    resetStats();
}


void engine::prepareThreads()
{
    for (int i = 0; i < Threads; i++)
    {
        // copy new position to the threads copy but keep old history data
        memcpy((void*)&sthread[i].pos, &rootposition, offsetof(chessposition, history));
        sthread[i].pos.threadindex = i;
        // early reset of variables that are important for bestmove selection
        sthread[i].pos.bestmovescore[0] = NOSCORE;
        sthread[i].pos.bestmove.code = 0;
        sthread[i].pos.nodes = 0;
        sthread[i].pos.nullmoveply = 0;
        sthread[i].pos.nullmoveside = 0;
    }
}

void engine::resetStats()
{
    for (int i = 0; i < Threads; i++)
    {
        memset(sthread[i].pos.history, 0, sizeof(chessposition::history));
        memset(sthread[i].pos.counterhistory, 0, sizeof(chessposition::counterhistory));
        memset(sthread[i].pos.countermove, 0, sizeof(chessposition::countermove));
    }
}


U64 engine::getTotalNodes()
{
    U64 nodes = 0;
    for (int i = 0; i < Threads; i++)
        nodes += sthread[i].pos.nodes;

    return nodes;
}


void engine::setOption(string sName, string sValue)
{
    bool resetTp = false;
    bool resetTh = false;
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
            resetTh = true;
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
    if (resetTp && sizeOfTp)
    {
        int newRestSizeTp = tp.setSize(sizeOfTp);
        if (restSizeOfTp != newRestSizeTp)
        {
            restSizeOfTp = newRestSizeTp;
            resetTh = true;
        }
    }
    if (resetTh)
    {
        sizeOfPh = min(128, max(16, restSizeOfTp / Threads));
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
        init_tablebases((char*)SyzygyPath.c_str());
    }
    if (sName == "syzygyprobelimit")
    {
        newint = stoi(sValue);
        if (newint >= 0 && newint <= 7)
            SyzygyProbeLimit = newint;
    }
    if (sName == "syzygy50moverule")
    {
        bool newSyzygy50MoveRule = (lValue == "true");
        if (Syzygy50MoveRule != newSyzygy50MoveRule)
        {
            Syzygy50MoveRule = newSyzygy50MoveRule;
            tp.clean();
        }
    }
}


void engine::communicate(string inputstring)
{
    string fen = STARTFEN;
    vector<string> moves;
    vector<string> searchmoves;
    vector<string> commandargs;
    GuiToken command = UNKNOWN;
    size_t ci, cs;
    bool bGetName, bGetValue;
    string sName, sValue;
    bool bMoves;
    bool pendingisready = false;
    bool pendingposition = (inputstring == "");
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
                rootposition.getFromFen(fen.c_str());
                for (vector<string>::iterator it = moves.begin(); it != moves.end(); ++it)
                {
                    if (!rootposition.applyMove(*it))
                        printf("info string Alarm! Zug %s nicht anwendbar (oder Enginefehler)\n", (*it).c_str());
                }
                rootposition.rootheight = rootposition.mstop;
                rootposition.ply = 0;
                rootposition.getRootMoves();
                rootposition.tbFilterRootMoves();
                prepareThreads();
                if (debug)
                {
                    rootposition.print();
                }
                pendingposition = false;
            }
            if (pendingisready)
            {
                send("readyok\n");
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
                        while (++ci < cs)
                        {
                            string s = commandargs[ci];
                            if (s.size() < 4)
                                continue;
                            int from = AlgebraicToIndex(s);
                            int to = AlgebraicToIndex(&s[2]);
                            int promotion = (s.size() <= 4) ? BLANK : (GetPieceType(s[4]) << 1); // Remember: S2m is missing here
                            rootposition.pvdebug[i++] = to | (from << 6) | (promotion << 12);
                        }
                        rootposition.pvdebug[i] = 0;
                        prepareThreads();
                    }
#endif
                }
                break;
            case UCI:
                send("id name %s\n", name);
                send("id author %s\n", author);
                send("option name Clear Hash type button\n");
                send("option name Hash type spin default %d min 1 max 1048576\n", DEFAULTHASH);
                send("option name Move Overhead type spin default 50 min 0 max 5000\n");
                send("option name MultiPV type spin default 1 min 1 max %d\n", MAXMULTIPV);
                send("option name Ponder type check default false\n");
                send("option name SyzygyPath type string default <empty>\n");
                send("option name Syzygy50MoveRule type check default true\n");
                send("option name SyzygyProbeLimit type spin default 7 min 0 max 7\n");
                send("option name Threads type spin default 1 min 1 max %d\n", MAXTHREADS);
                send("uciok\n", author);
                break;
            case UCINEWGAME:
                // invalidate hash and history
                tp.clean();
                resetStats();
                sthread[0].pos.lastbestmovescore = NOSCORE;
                break;
            case SETOPTION:
                if (en.stopLevel != ENGINETERMINATEDSEARCH)
                {
                    send("info string Changing option while searching is not supported. stopLevel = %d\n", en.stopLevel);
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
                        setOption(sName, sValue);
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
                resetPonder();
                searchmoves.clear();
                wtime = btime = winc = binc = movestogo = mate = maxdepth = 0;
                maxnodes = 0ULL;
                infinite = false;
                while (ci < cs)
                {
                    if (commandargs[ci] == "searchmoves")
                    {
                        while (++ci < cs && AlgebraicToIndex(commandargs[ci]) < 64 && AlgebraicToIndex(&commandargs[ci][2]) < 64)
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
                        wtime = btime = 0;
                        if (++ci < cs)
                            winc = binc = stoi(commandargs[ci++]);
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
                isWhite = (sthread[0].pos.w2m());
                stopLevel = ENGINERUN;
                searchStart();
                if (inputstring != "")
                {
                    // bench mode; wait for end of search
                    searchWaitStop(false);
                }
                break;
            case PONDERHIT:
                HitPonder();
                break;
            case STOP:
            case QUIT:
                if (stopLevel < ENGINESTOPIMMEDIATELY)
                    stopLevel = ENGINESTOPIMMEDIATELY;
                break;
            case EVAL:
                sthread[0].pos.getEval<TRACE>();
                break;
            default:
                break;
            }
        }
    } while (command != QUIT && (inputstring == "" || pendingposition));
    if (inputstring == "")
        searchWaitStop();
}

// Explicit template instantiation
// This avoids putting these definitions in header file
template U64 chessposition::pieceMovesTo<KNIGHT>(int);
template U64 chessposition::pieceMovesTo<BISHOP>(int);
template U64 chessposition::pieceMovesTo<ROOK>(int);
template U64 chessposition::pieceMovesTo<QUEEN>(int);


// Some global objects
evalparamset eps;
zobrist zb;
engine en;

// Explicit template instantiation
// This avoids putting these definitions in header file
template U64 chessposition::isAttackedBy<OCCUPIED>(int index, int col);
