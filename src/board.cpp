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
int squareDistance[64][64];  // decreased by 1 for directly indexing evaluation arrays
alignas(64) int psqtable[14][64];

const string strCpuFeatures[] = STRCPUFEATURELIST;


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


inline string moveToString(uint32_t mc)
{
    char s[8];

    if (mc == 0)
        return "(none)";

    int from, to;
    PieceCode promotion;
    from = GETFROM(mc);
    if (!en.chess960)
        to = GETCORRECTTO(mc);
    else
        to = GETTO(mc);
    promotion = GETPROMOTION(mc);

    sprintf_s(s, "%c%d%c%d", (from & 0x7) + 'a', ((from >> 3) & 0x7) + 1, (to & 0x7) + 'a', ((to >> 3) & 0x7) + 1);
    if (promotion)
    {
        string ps(1, PieceChar(promotion, true));
        return s + ps;
    }

    return s;
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

chessmove::chessmove(uint32_t c)
{
    code = c;
}

string chessmove::toString()
{
    return moveToString(code);
}

void chessmove::print()
{
    guiCom << toString();
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

// Sorting for MoveSelector; keep the move in the list
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

// Sorting for MoveSelector; remove it from the list
uint32_t chessmovelist::getAndRemoveNextMove()
{
    int minval = INT_MIN;
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
    {
        int mc = move[current].code;
        SDEBUGDO(true, lastvalue = move[current].value;)
        move[current] = move[--length];
        return mc;
    }

    return 0;
}


bool chessposition::w2m()
{
    return !(state & S2MMASK);
}


void chessposition::initCastleRights(int rookfiles[2][2], int kingfile[2])
{
    for (int from = 0; from < 64; from++)
    {
        castlerights[from] = ~0;
        int f = FILE(from);
        int r = RANK(from);
        int color = (r == 0 ? 0 : r == 7 ? 1 : -1);
        if (color >= 0 && (f == rookfiles[color][0] || f == kingfile[color]))
            castlerights[from] &= ~QCMASK[color];
        if (color >= 0 && (f == rookfiles[color][1] || f == kingfile[color]))
            castlerights[from] &= ~KCMASK[color];
    }

    for (int i = 0; i < 4; i++)
    {
        int col = i / 2;
        int gCastle = i % 2;
        castlerookfrom[i] = rookfiles[col][gCastle] + 56 * col;
        castleblockers[i] = betweenMask[kingfile[col] + 56 * col][castlekingto[i]]
            | betweenMask[castlerookfrom[i]][castlerookto[i]]
            | BITSET(castlerookto[i]) | BITSET(castlekingto[i]);

        castlekingwalk[i] = betweenMask[kingfile[col] + 56 * col][castlekingto[i]] | BITSET(castlekingto[i]);
    }
}


int chessposition::getFromFen(const char* sFen)
{
    string s;
    vector<string> token = SplitString(sFen);
    int numToken = (int)token.size();

    psqval = 0;
    phcount = 0;

    memset(piece00, 0, sizeof(piece00));
    memset(mailbox, 0, sizeof(mailbox));

    // At least two token are needed
    if (numToken < 2)
        return -1;

    while (numToken < 4)
    {
        numToken++;
        token.push_back("-");
    }

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

    // Some basic checks for legal positions
    if ((piece00[WPAWN] | piece00[BPAWN]) & PROMOTERANKBB)
        return -1;
    if (POPCOUNT(piece00[WKING]) != 1 || POPCOUNT(piece00[BKING]) != 1)
        return -1;
    if (squareDistance[kingpos[WHITE]][kingpos[BLACK]] < 1)
        return -1;

    state = 0;
    /* side to move */
    if (token[1] == "b")
        state |= S2MMASK;

    /* castle rights */
    int rookfiles[2][2] = {{ -1, -1 }, { -1, -1 }};
    int kingfile[2] = { FILE(kingpos[0]), FILE(kingpos[1]) };
    s = token[2];
    const string usualcastles = "QKqk";
    const string castles960 = "ABCDEFGHabcdefgh";
    for (unsigned int i = 0; i < s.length(); i++)
    {
        bool gCastle;
        int col;
        int rookfile = -1;
        char c = s[i];
        size_t castleindex;
        if ((castleindex = usualcastles.find(c)) != string::npos)
        {
            col = (int)castleindex / 2;
            gCastle = castleindex % 2;
            U64 rookbb = (piece00[WROOK | col] & RANK1(col)) >> (56 * col);
            myassert(rookbb, this, 1, rookbb);
            if (gCastle)
                GETMSB(rookfile, rookbb);
            else
                GETLSB(rookfile, rookbb);
        }
        else if ((castleindex = (int)castles960.find(c)) != string::npos)
        {
            col = (int)castleindex / 8;
            rookfile = castleindex % 8;
            gCastle = (rookfile > FILE(kingpos[col]));
            castleindex = col * 2 + gCastle;
        }
        if (rookfile >= 0)
        {
            state |= SETCASTLEFILE(rookfile, castleindex);
            // FIXME: Maybe some sanity check if castle flag fits to rook placement
            rookfiles[col][gCastle] = rookfile;
            // Set chess960 if non-standard rook/king setup is found
            if (kingfile[col] != 4 || rookfiles[col][gCastle] != gCastle * 7)
                en.chess960 = true;
        }
    }
    initCastleRights(rookfiles, kingfile);

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
    halfmovescounter = 0;
    if (numToken > 4)
    {
        try {
            halfmovescounter = stoi(token[4]);
        }
        catch (...) {}
    }

    /* full moves */
    fullmovescounter = 1;
    if (numToken > 5)
    {
        try {
            fullmovescounter = stoi(token[5]);
        }
        catch (...) {}
    }

    isCheckbb = isAttackedBy<OCCUPIED>(kingpos[state & S2MMASK], (state & S2MMASK) ^ S2MMASK);
    kingPinned = 0ULL;
    updatePins<WHITE>();
    updatePins<BLACK>();

    hash = zb.getHash(this);
    pawnhash = zb.getPawnHash(this);
    materialhash = zb.getMaterialHash(this);
    lastnullmove = -1;
    ply = 0;
    useTb = min(TBlargest, en.SyzygyProbeLimit);
    return 0;
}



uint32_t chessposition::applyMove(string s, bool resetMstop)
{
    int from, to;
    PieceType promtype;
    PieceCode promotion;

    if (s.size() < 4)
        return 0;

    from = AlgebraicToIndex(s);
    to = AlgebraicToIndex(&s[2]);

    if (s.size() > 4 && (promtype = GetPieceType(s[4])) != BLANKTYPE)
        promotion = (PieceCode)((promtype << 1) | (state & S2MMASK));
    else
        promotion = BLANK;

    // Change normal castle moves (e.g. e1g1) to the more general chess960 format "king captures rook" (e1h1)
    if ((mailbox[from] >> 1) == KING && mailbox[to] == BLANK && abs(from - to) == 2)
        to = to > from ? to + 1 : to - 2;

    chessmove m = chessmove(from, to, promotion, BLANK, BLANK);
    m.code = shortMove2FullMove((uint16_t)m.code);

    prepareStack();

    if (playMove(m.code))
    {
        if (resetMstop && halfmovescounter == 0)
        {
            // Keep the list short, we have to keep below MAXMOVELISTLENGTH
            ply = 0;
        }
        return m.code;
    }
    return 0;
}


template <MoveType Mt>
void chessposition::evaluateMoves(chessmovelist *ml)
{
    for (int i = 0; i < ml->length; i++)
    {
        uint32_t mc = ml->move[i].code;
        PieceCode piece = GETPIECE(mc);
        if (Mt == CAPTURE || (Mt == ALL && GETCAPTURE(mc)))
        {
            PieceCode capture = GETCAPTURE(mc);
            ml->move[i].value = (mvv[capture >> 1] | lva[piece >> 1]) + tacticalhst[piece >> 1][GETTO(mc)][capture >> 1];
        }
        if (Mt == QUIET || (Mt == ALL && !GETCAPTURE(mc)))
        {
            int to = GETCORRECTTO(mc);
            ml->move[i].value = history[piece & S2MMASK][threatSquare][GETFROM(mc)][to];
            int16_t** cptr = cmptr[ply];
            if (cptr)
            {
                for (int j = 0; j < CMPLIES && cptr[j]; j++)
                {
                    ml->move[i].value += cptr[j][piece * 64 + to];
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

    if (state & S2MMASK)
        updateThreats<BLACK>();
    else
        updateThreats<WHITE>();
    prepareStack();
    movelist.length = CreateMovelist<ALL>(&movelist.move[0]);
    evaluateMoves<ALL>(&movelist);

    int bestval = SCOREBLACKWINS;
    rootmovelist.length = 0;
    defaultmove = 0;

    uint16_t moveTo3fold = 0;
    bool bImmediate3fold = false;
    int ttscore, tteval;
    uint16_t tthashmovecode;
    bool tthit = tp.probeHash(hash, &ttscore, &tteval, &tthashmovecode, 0, SHRT_MIN + 1, SHRT_MAX, 0);
    bool bSearchmoves = (en.searchmoves.size() > 0);

    excludemovestack[0] = 0; // FIXME: Not very nice; is it worth to do do singular testing in root search?
    for (int i = 0; i < movelist.length; i++)
    {
        if (bSearchmoves)
        {
            string strMove = moveToString(movelist.move[i].code);
            if (en.searchmoves.find(strMove) == en.searchmoves.end())
                continue;
        }
        if (playMove(movelist.move[i].code))
        {
            if (tthit)
            {
                // Test for immediate or possible 3fold to fix a possibly wrong hash entry
                if (testRepetition() >= 2)
                {
                    // This move triggers 3fold; remember move to update hash
                    bImmediate3fold = true;
                    moveTo3fold = movelist.move[i].code;
                }
                else if ((uint16_t)movelist.move[i].code == tthashmovecode)
                {
                    movelist.move[i].value = PVVAL;
                    // Test if this move makes a 3fold possible for opponent
                    prepareStack();
                    chessmovelist followupmovelist;
                    followupmovelist.length = CreateMovelist<ALL>(&followupmovelist.move[0]);
                    for (int j = 0; j < followupmovelist.length; j++)
                    {
                        if (playMove(followupmovelist.move[j].code))
                        {
                            if (testRepetition() >= 2)
                                // 3fold for opponent is possible
                                moveTo3fold = movelist.move[i].code;

                            unplayMove(followupmovelist.move[j].code);
                        }
                    }
                }
            }

            rootmovelist.move[rootmovelist.length++] = movelist.move[i];
            unplayMove(movelist.move[i].code);
            if (bestval < movelist.move[i].value)
            {
                defaultmove = movelist.move[i].code;
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
    tbPosition = 0;
    useRootmoveScore = 0;
    if (POPCOUNT(occupied00[0] | occupied00[1]) > useTb)
        return;

    if ((tbPosition = root_probe_dtz())) {
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
        defaultmove = rootmovelist.move[0].code;
    }
}


// test the actual move for three-fold-repetition
// maybe this could be fixed in the future by using cuckoo tables like SF does it
// https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf
int chessposition::testRepetition()
{
    int hit = 0;
    int lastrepply = max(ply - halfmovescounter, lastnullmove + 1);
    int i = ply - 4;
    while (i >= lastrepply)
    {
        if (hash == (&movestack[0] + i)->hash)
        {
            hit++;
            if (i > 0)
            {
                hit++;
                break;
            }
        }
        i -= 2;
    }
    return hit;
}

void chessposition::prepareStack()
{
    myassert(ply >= 0 && ply < MAXDEPTH, this, 1, ply);
    // copy stack related data directly to stack
    memcpy(&movestack[ply], &state, sizeof(chessmovestack));
}


void chessposition::playNullMove()
{
    lastnullmove = ply;
    movestack[ply++].movecode = 0;
    state ^= S2MMASK;
    hash ^= zb.s2m ^ zb.ept[ept];
    ept = 0;
    myassert(ply <= MAXDEPTH, this, 1, ply);
#ifdef NNUE
    DirtyPiece* dp = &dirtypiece[ply];
    dp->dirtyNum = 0;
    dp->pc[0] = 0; // don't break search for updatable positions on stack
    accumulator[ply].computationState[WHITE] = false;
    accumulator[ply].computationState[BLACK] = false;
#endif
}


void chessposition::unplayNullMove()
{
    state ^= S2MMASK;
    lastnullmove = movestack[--ply].lastnullmove;
    ept = movestack[ply].ept;
    hash ^= zb.s2m^ zb.ept[ept];
    myassert(ply >= 0, this, 1, ply);
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

    int myeptcastle = 0;
    if (p == PAWN)
    {
        if (FILE(from) != FILE(to) && capture == BLANK)
        {
            // ep capture
            capture = pc ^ S2MMASK;
            myeptcastle = EPCAPTUREFLAG;
        }
        else if ((from ^ to) == 16 && (epthelper[to] & piece00[pc ^ 1]))
        {
            // double push enables epc
            myeptcastle = ((from + to) / 2) << 20;
        }
    }

    if (p == KING && capture == (WROOK | (pc & S2MMASK)))
    {
        // castle move
        int cstli = (to > from) + (pc & S2MMASK) * 2;
        myeptcastle = CASTLEFLAG | (cstli << 20);
        capture = 0;
    }

    uint32_t fc = (pc << 28) | myeptcastle | (capture << 16) | c;
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

    // special test for castles; FIXME: This code is almost duplicate to the move generation
    if (ISCASTLE(c))
    {
        int s2m = state & S2MMASK;

        // king in check => castle is illegal
        if (isAttacked(from, s2m))
            return false;

        // test if move is on first rank
        if (RRANK(from, s2m) || RRANK(to, s2m))
            return false;

        int cstli = GETCASTLEINDEX(c);

        // Test for correct rook
        if (castlerookfrom[cstli] != to)
            return false;

        // test for castle right
        if (!(state & (WQCMASK << cstli)))
            return false;

        // test if path to target is occupied
        if (castleblockers[cstli] & ((occupied00[0] | occupied00[1]) ^ BITSET(from) ^ BITSET(to)))
            return false;

        // test if king path is attacked
        BitboardClear(to, (PieceType)(WROOK | s2m));
        U64 kingwalkbb = castlekingwalk[cstli];
        bool attacked = false;
        while (!attacked && kingwalkbb)
        {
            int walkto = pullLsb(&kingwalkbb);
            attacked = isAttacked(walkto, s2m);
        }
        BitboardSet(to, (PieceType)(WROOK | s2m));
        return !attacked;
    }

    // correct capture?
    if (mailbox[to] != capture && !ISEPCAPTUREORCASTLE(c))
        return false;

    // correct color of capture? capturing the king is illegal
    if (capture && (piececol == (capture & S2MMASK) || capture >= WKING))
        return false;

    myassert(capture >= BLANK && capture <= BQUEEN, this, 1, capture);

    // correct target for type of piece?
    if (!(movesTo(pc, from) & BITSET(to)))
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
            if (ISEPCAPTURE(c) && ept != to)
                return false;

            // missing promotion
            if (RRANK(to, piececol) == 7 && !GETPROMOTION(c))
                return false;
        }
    }



    return true;
}


// This is mainly for detecting discovered attacks on the queen so we exclude enemy queen from the test
template <int Me> bool chessposition::sliderAttacked(int index, U64 occ)
{
    const int You = Me ^ S2MMASK;
    U64 ppr = ROOKATTACKS(occ, index);
    U64 pdr = ROOKATTACKS(occ & ~ppr, index) & piece00[WROOK | You];
    U64 ppb = BISHOPATTACKS(occ, index);
    U64 pdb = BISHOPATTACKS(occ & ~ppb, index) & piece00[WBISHOP | You];

    return pdr || pdb;
}


template <int Me> void chessposition::updatePins()
{
    const int You = Me ^ S2MMASK;
    int k = kingpos[Me];
    U64 occ = occupied00[You];
    U64 attackers = ROOKATTACKS(occ, k) & (piece00[WROOK | You] | piece00[WQUEEN | You]);
    attackers |= BISHOPATTACKS(occ, k) & (piece00[WBISHOP | You] | piece00[WQUEEN | You]);

    while (attackers)
    {
        int index = pullLsb(&attackers);
        U64 potentialPinners = betweenMask[index][k] & occupied00[Me];
        if (ONEORZERO(potentialPinners))
            kingPinned |= potentialPinners;
    }
    // 'Reset' attack vector to make getBestPossibleCapture work even if evaluation was skipped
    attackedBy[Me][0] = 0xffffffffffffffff;
}


template <int Me> void chessposition::updateThreats()
{
    const int You = Me ^ S2MMASK;

    U64 threatsByPawns = PAWNATTACK(You, piece00[WPAWN | You]) & (occupied00[Me] ^ piece00[WPAWN | Me]);

    U64 threatsByMinors = 0Ull;
    U64 yourBishops = piece00[WBISHOP | You];
    while (yourBishops) {
        int from = pullLsb(&yourBishops);
        threatsByMinors |= pieceMovesTo<BISHOP>(from);
    }
    U64 yourKnights = piece00[WKNIGHT | You];
    while (yourKnights) {
        int from = pullLsb(&yourKnights);
        threatsByMinors |= pieceMovesTo<KNIGHT>(from);
    }
    threatsByMinors &= (piece00[WROOK | Me] | piece00[WQUEEN | Me]);

    U64 threatsByRooks = 0ULL;
    U64 yourRooks = piece00[WROOK | You];
    while (yourRooks) {
        int from = pullLsb(&yourRooks);
        threatsByRooks |= pieceMovesTo<ROOK>(from);
    }
    threatsByRooks &= piece00[WQUEEN | Me];

    threats = threatsByPawns | threatsByMinors | threatsByRooks;
    if (threats)
        GETLSB(threatSquare, threats);
    else
        threatSquare = 64;
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
    pseudolegalmoves.length = CreateMovelist<ALL>(&pseudolegalmoves.move[0]);
    *os << "\nFEN: " + toFen() + "\n";
    *os << "State: " << std::hex << state << "\n";
    *os << "EPT: " + to_string(ept) + "\n";
    *os << "Halfmoves: " + to_string(halfmovescounter) + "\n";
    *os << "Fullmoves: " + to_string(fullmovescounter) + "\n";
    *os << "Hash: 0x" << hex << hash << " (should be 0x" << hex << zb.getHash(this) << ")\n";
    *os << "Pawn Hash: 0x" << hex << pawnhash << " (should be 0x" << hex << zb.getPawnHash(this) << ")\n";
    *os << "Material Hash: 0x" << hex << materialhash << " (should be 0x" << hex << zb.getMaterialHash(this) << ")\n";
    *os << "Value: " + to_string(getEval<NOTRACE>()) + "\n";
    *os << "Repetitions: " + to_string(testRepetition()) + "\n";
    *os << "Phase: " + to_string(phcount) + "\n";
    *os << "Pseudo-legal Moves: " + pseudolegalmoves.toStringWithValue() + "\n";
    *os << "Moves in current search: " + movesOnStack() + "\n";
    *os << "Ply: " + to_string(ply) + "\n";
    stringstream ss;
    ss << hex << bestmove;
    *os << "bestmove[0].code: 0x" + ss.str() + "\n";
}


string chessposition::movesOnStack()
{
    string s = "";
    for (int i = 0; i < ply; i++)
    {
        chessmove cm;
        cm.code = movestack[i].movecode;
        s = s + cm.toString() + " ";
    }
    return s;
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
            s += en.chess960 ? 'A' + GETCASTLEFILE(state, 1) : 'K';
        if (state & WQCMASK)
            s += en.chess960 ? 'A' + GETCASTLEFILE(state, 0) : 'Q';
        if (state & BKCMASK)
            s += en.chess960 ? 'a' + GETCASTLEFILE(state, 3) : 'k';
        if (state & BQCMASK)
            s += en.chess960 ? 'a' + GETCASTLEFILE(state, 2) : 'q';
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
    uint32_t *table = multipvtable[pvindex];
    table[0] = mc;
    int i = 0;
    while (pvtable[1][i])
    {
        table[i + 1] = pvtable[1][i];
        i++;
    }
    table[++i] = 0;

    // replicate best multipv to pvtable
    if (pvindex == 0)
        memcpy(pvtable[0], table, (i + 1) * sizeof(uint32_t));
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

int chessposition::applyPv(uint32_t* table)
{
    chessmove cm;
    int i = 0;

    while ((cm.code = table[i++]))
    {
        prepareStack();
        if (!playMove(cm.code))
            printf("Alarm! Wrong move %s in PV.\n", cm.toString().c_str());
    }

    return i - 1;
}

void chessposition::reapplyPv(uint32_t* table, int num)
{
    chessmove cm;

    while (num)
    {
        cm.code = table[--num];
        unplayMove(cm.code);
    }
}


#ifdef SDEBUG
bool chessposition::triggerDebug(chessmove* nextmove)
{
    if (pvmovecode[0] == 0)
        return false;

    int j = 0;

    while (j < ply && pvmovecode[j])
    {
        if ((movestack[j].movecode) != pvmovecode[j])
            return false;
        j++;
    }
    nextmove->code = pvmovecode[j];

    return true;
}


const char* PvAbortStr[] = {
    "unknown", "pv from tt", "different move in tt", "razor-pruned", "reverse-futility-pruned", "nullmove-pruned", "probcut-pruned", "late-move-pruned",
    "futility-pruned", "bad-see-pruned", "bad-history-pruned", "multicut-pruned", "bestmove", "not best move", "omitted", "betacut", "below alpha", "checkmate", "stalemate"
};


void chessposition::pvdebugout()
{
    printf("=======================================================================\n  Window       Move   MoveVal   Num Dep    Score        Reason\n-----------------------------------------------------------------------\n");
    for (int i = 0; pvmovecode[i]; i++)
    {
        chessmove m;
        m.code = pvmovecode[i];

        printf("%6d/%6d  %s  %8x  %s%2d  %2d  %5d  %23s  %s\n",
            pvalpha[i], pvbeta[i], m.toString().c_str(), pvmovevalue[i], pvmovenum[i] < 0 ? ">" : " ",
            abs(pvmovenum[i]), pvdepth[i], pvabortscore[i], PvAbortStr[pvaborttype[i]], pvadditionalinfo[i].c_str());
        if (pvaborttype[i + 1] == PVA_UNKNOWN || pvaborttype[i] == PVA_OMITTED)
            break;
    }
    printf("=======================================================================\n\n");
}

#endif

// shameless copy from http://chessprogramming.wikispaces.com/Magic+Bitboards#Plain
alignas(64) U64 mBishopAttacks[64][1 << BISHOPINDEXBITS];
alignas(64) U64 mRookAttacks[64][1 << ROOKINDEXBITS];

alignas(64) SMagic mBishopTbl[64];
alignas(64) SMagic mRookTbl[64];



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

    initPsqtable();
    for (int from = 0; from < 64; from++)
    {
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
            int hashindex = BISHOPINDEX(occ, from);
            mBishopAttacks[from][hashindex] = attack;
        }

        // mRookTbl[from].magic = getMagicCandidate(mRookTbl[from].mask);
        mRookTbl[from].magic = rookmagics[from];

        for (int j = 0; j < (1 << ROOKINDEXBITS); j++) {
            // First get the subset of mask corresponding to j
            U64 occ = getOccupiedFromMBIndex(j, mRookTbl[from].mask);
            // Now get the attack bitmap for this subset and store to attack table
            U64 attack = (getAttacks(from, occ, -1) | getAttacks(from, occ, 1) | getAttacks(from, occ, -8) | getAttacks(from, occ, 8));
            int hashindex = ROOKINDEX(occ, from);
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
    phcount += phasefactor[p >> 1];
}


void chessposition::BitboardClear(int index, PieceCode p)
{
    myassert(index >= 0 && index < 64, this, 1, index);
    myassert(p >= BLANK && p <= BKING, this, 1, p);
    int s2m = p & 0x1;
    piece00[p] ^= BITSET(index);
    occupied00[s2m] ^= BITSET(index);
    psqval -= psqtable[p][index];
    phcount -= phasefactor[p >> 1];
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


bool chessposition::playMove(uint32_t mc)
{
    int s2m = state & S2MMASK;
    int eptnew = 0;
    int oldcastle = (state & CASTLEMASK);

#ifdef NNUE
    DirtyPiece* dp = &dirtypiece[ply + 1];
    dp->dirtyNum = 0;
    accumulator[ply + 1].computationState[WHITE] = false;
    accumulator[ply + 1].computationState[BLACK] = false;
#endif

    halfmovescounter++;

    // Castle has special play
    if (ISCASTLE(mc))
    {
        // Get castle squares and move king and rook
        int kingfrom = GETFROM(mc);
        int rookfrom = GETTO(mc);
        int cstli = GETCASTLEINDEX(mc);
        int kingto = castlekingto[cstli];
        int rookto = castlerookto[cstli];
        PieceCode kingpc = (PieceCode)(WKING | s2m);
        PieceCode rookpc = (PieceCode)(WROOK | s2m);

        mailbox[kingfrom] = BLANK;
        mailbox[rookfrom] = BLANK;
        mailbox[kingto] = kingpc;
        mailbox[rookto] = rookpc;

        if (kingfrom != kingto)
        {
            kingpos[s2m] = kingto;
            BitboardMove(kingfrom, kingto, kingpc);
            hash ^= zb.boardtable[(kingfrom << 4) | kingpc] ^ zb.boardtable[(kingto << 4) | kingpc];
            pawnhash ^= zb.boardtable[(kingfrom << 4) | kingpc] ^ zb.boardtable[(kingto << 4) | kingpc];
#ifdef NNUE
            dp->pc[0] = kingpc;
            dp->from[0] = kingfrom;
            dp->to[0] = kingto;
            dp->dirtyNum = 1;
#endif
        }
        if (rookfrom != rookto)
        {
            BitboardMove(rookfrom, rookto, rookpc);
            hash ^= zb.boardtable[(rookfrom << 4) | rookpc] ^ zb.boardtable[(rookto << 4) | rookpc];
#ifdef NNUE
            int di = dp->dirtyNum;
            dp->pc[di] = rookpc;
            dp->from[di] = rookfrom;
            dp->to[di] = rookto;
            dp->dirtyNum++;
#endif
        }
        state &= (s2m ? ~(BQCMASK | BKCMASK) : ~(WQCMASK | WKCMASK));
    }
    else
    {
        int from = GETFROM(mc);
        int to = GETTO(mc);
        PieceCode pfrom = GETPIECE(mc);
        PieceType ptype = (pfrom >> 1);
        PieceCode promote = GETPROMOTION(mc);
        PieceCode capture = GETCAPTURE(mc);

#ifdef NNUE
        dp->pc[0] = pfrom;
        dp->from[0] = from;
        dp->to[0] = to;
        dp->dirtyNum = 1;
#endif

        myassert(!promote || (ptype == PAWN && RRANK(to, s2m) == 7), this, 4, promote, ptype, to, s2m);
        myassert(pfrom == mailbox[from], this, 3, pfrom, from, mailbox[from]);
        myassert(ISEPCAPTURE(mc) || capture == mailbox[to], this, 2, capture, mailbox[to]);

        // Fix hash regarding capture
        if (capture != BLANK && !ISEPCAPTURE(mc))
        {
            hash ^= zb.boardtable[(to << 4) | capture];
            if ((capture >> 1) == PAWN)
                pawnhash ^= zb.boardtable[(to << 4) | capture];
            BitboardClear(to, capture);
            materialhash ^= zb.boardtable[(POPCOUNT(piece00[capture]) << 4) | capture];
            halfmovescounter = 0;
#ifdef NNUE
            dp->pc[1] = capture;
            dp->from[1] = to;
            dp->to[1] = -1;
            dp->dirtyNum = 2;
#endif
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
            pawnhash ^= zb.boardtable[(to << 4) | promote];
#ifdef NNUE
            int di = dp->dirtyNum;
            dp->to[0] = -1; // remove promoting pawn;
            dp->from[di] = -1;
            dp->to[di] = to;
            dp->pc[di] = promote;
            dp->dirtyNum++;
#endif
        }

        hash ^= zb.boardtable[(to << 4) | mailbox[to]];
        hash ^= zb.boardtable[(from << 4) | pfrom];

        mailbox[from] = BLANK;

        /* PAWN specials */
        if (ptype == PAWN)
        {
            eptnew = GETEPT(mc);
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
#ifdef NNUE
                dp->pc[1] = (pfrom ^ S2MMASK);
                dp->from[1] = epfield;
                dp->to[1] = -1;
                dp->dirtyNum++;
#endif
            }
        }

        if (ptype == KING)
            kingpos[s2m] = to;

        // Here we can test the move for being legal
        if (isAttacked(kingpos[s2m], s2m))
        {
            // Move is illegal; just do the necessary subset of unplayMove
            hash = movestack[ply].hash;
            pawnhash = movestack[ply].pawnhash;
            materialhash = movestack[ply].materialhash;
            kingpos[s2m] = movestack[ply].kingpos[s2m];
            halfmovescounter = movestack[ply].halfmovescounter;
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

        // remove castle rights depending on from and to square
        state &= (castlerights[from] & castlerights[to]);
        if (ptype == KING)
        {
            // Store king position in pawn hash
            pawnhash ^= zb.boardtable[(from << 4) | pfrom] ^ zb.boardtable[(to << 4) | pfrom];
        }
    }

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

    movestack[ply++].movecode = mc;
    myassert(ply <= MAXDEPTH, this, 1, ply);
    kingPinned = 0ULL;
    updatePins<WHITE>();
    updatePins<BLACK>();
    nodes++;

    return true;
}


void chessposition::unplayMove(uint32_t mc)
{
    ply--;
    myassert(ply >= 0, this, 1, ply);
    // copy data from stack back to position
    memcpy(&state, &movestack[ply], sizeof(chessmovestack));

    // Castle has special undo
    if (ISCASTLE(mc))
    {
        // Get castle squares and undo king and rook move
        int kingfrom = GETFROM(mc);
        int rookfrom = GETTO(mc);
        int cstli = GETCASTLEINDEX(mc);
        int kingto = castlekingto[cstli];
        int rookto = castlerookto[cstli];
        int s2m = cstli / 2;
        PieceCode kingpc = (PieceCode)(WKING | s2m);
        PieceCode rookpc = (PieceCode)(WROOK | s2m);

        mailbox[kingto] = BLANK;
        mailbox[rookto] = BLANK;
        mailbox[kingfrom] = kingpc;
        mailbox[rookfrom] = rookpc;

        if (kingfrom != kingto)
            BitboardMove(kingto, kingfrom, kingpc);

        if (rookfrom != rookto)
            BitboardMove(rookto, rookfrom, rookpc);
    }
    else
    {
        int from = GETFROM(mc);
        int to = GETTO(mc);
        PieceCode pfrom = GETPIECE(mc);

        PieceCode promote = GETPROMOTION(mc);
        PieceCode capture = GETCAPTURE(mc);

        mailbox[from] = pfrom;
        if (promote != BLANK)
        {
            BitboardClear(to, promote);
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
    }
}


U64 chessposition::nextHash(uint32_t mc)
{
    U64 newhash = hash;
    int from = GETFROM(mc);
    int to = GETTO(mc);
    int pc = GETPIECE(mc);
    int capture = GETCAPTURE(mc);

    newhash ^= zb.s2m;
    newhash ^= zb.boardtable[(to << 4) | pc];
    newhash ^= zb.boardtable[(from << 4) | pc];

    if (capture)
        newhash ^= zb.boardtable[(to << 4) | capture];

    return newhash;
}


inline void appendMoveToList(chessmove **m, int from, int to, PieceCode piece, PieceCode capture)
{
    **m = chessmove(from, to, capture, piece);
    (*m)++;
}


inline void appendPromotionMove(chessmove **m, int from, int to, int col, PieceType promote, PieceCode capture)
{
    appendMoveToList(m, from, to, WPAWN | col, capture);
    (*m - 1)->code |= ((promote << 1) | col) << 12;
}


template <PieceType Pt, Color me> int chessposition::CreateMovelistPiece(chessmove* mstart, U64 occ, U64 targets)
{
    const PieceCode pc = (PieceCode)((Pt << 1) | me);
    U64 frombits = piece00[pc];
    U64 tobits = 0ULL;
    chessmove *m = mstart;

    while (frombits)
    {
        int from = pullLsb(&frombits);
        if (Pt == KNIGHT)
            tobits = (knight_attacks[from] & targets);
        if (Pt == BISHOP || Pt == QUEEN)
            tobits |= (BISHOPATTACKS(occ, from) & targets);
        if (Pt == ROOK || Pt == QUEEN)
            tobits |= (ROOKATTACKS(occ, from) & targets);
        if (Pt == KING)
            tobits = (king_attacks[from] & targets);
        while (tobits)
        {
            int to = pullLsb(&tobits);
            appendMoveToList(&m, from, to, pc, mailbox[to]);
        }
    }

    return (int)(m - mstart);
}


template <Color me> inline int chessposition::CreateMovelistCastle(chessmove* mstart)
{
    if (isCheckbb)
        return 0;

    chessmove *m = mstart;
    U64 occupiedbits = (occupied00[0] | occupied00[1]);

    for (int cstli = me * 2; cstli < 2 * me + 2; cstli++)
    {
        if ((state & (WQCMASK << cstli)) == 0)
            continue;
        const int kingfrom = kingpos[me];
        const int rookfrom = castlerookfrom[cstli];
        if (castleblockers[cstli] & (occupiedbits ^ BITSET(rookfrom) ^ BITSET(kingfrom)))
            continue;

        BitboardClear(rookfrom, (PieceType)(WROOK | me));
        U64 kingwalkbb = castlekingwalk[cstli];
        bool attacked = false;
        while (!attacked && kingwalkbb)
        {
            int to = pullLsb(&kingwalkbb);
            attacked = isAttacked(to, me);
        }
        BitboardSet(rookfrom, (PieceType)(WROOK | me));

        if (attacked)
            continue;

        // Create castle move 'king captures rook' and add castle flag manually
        appendMoveToList(&m, kingfrom, rookfrom, WKING | me, BLANK);
        (m - 1)->code |= (CASTLEFLAG | cstli << 20);
    }

    return (int)(m - mstart);
}


template <MoveType Mt, Color me> inline int chessposition::CreateMovelistPawn(chessmove* mstart)
{
    chessmove *m = mstart;
    const int you = 1 - me;
    const PieceCode pc = (PieceCode)(WPAWN | me);
    const U64 occ = occupied00[0] | occupied00[1];
    U64 frombits, tobits;
    int from, to;

    if (Mt & QUIET)
    {
        U64 push = PAWNPUSH(you, ~occ & ~PROMOTERANKBB);
        U64 pushers = push & piece00[pc];
        U64 doublepushers = PAWNPUSH(you, push) & (RANK2(me) & pushers);
        while (pushers)
        {
            from = pullLsb(&pushers);
            appendMoveToList(&m, from, PAWNPUSHINDEX(me, from), pc, BLANK);
        }
        while (doublepushers)
        {
            from = pullLsb(&doublepushers);
            to = PAWNPUSHDOUBLEINDEX(me, from);
            appendMoveToList(&m, from, to, pc, BLANK);
            if (epthelper[to] & piece00[WPAWN | you])
                // EPT possible for opponent; set EPT field manually
                (m - 1)->code |= (from + to) << 19;
        }
    }

    if (Mt & CAPTURE)
    {
        frombits = piece00[pc] & ~RANK7(me);
        while (frombits)
        {
            from = pullLsb(&frombits);
            tobits = (pawn_attacks_to[from][me] & occupied00[you]);
            while (tobits)
            {
                to = pullLsb(&tobits);
                appendMoveToList(&m, from, to, pc, mailbox[to]);
            }
        }
        if (ept)
        {
            frombits = piece00[pc] & pawn_attacks_to[ept][you];
            while (frombits)
            {
                from = pullLsb(&frombits);
                appendMoveToList(&m, from, ept, pc, WPAWN | you);
                (m - 1)->code |= EPCAPTUREFLAG;
            }

        }
    }

    if (Mt & PROMOTE)
    {
        frombits = piece00[pc] & RANK7(me);
        while (frombits)
        {
            from = pullLsb(&frombits);
            tobits = (pawn_attacks_to[from][me] & occupied00[you]) | (pawn_moves_to[from][me] & ~occ);
            while (tobits)
            {
                to = pullLsb(&tobits);
                PieceCode cap = mailbox[to];
                appendPromotionMove(&m, from, to, me, QUEEN, cap);
                appendPromotionMove(&m, from, to, me, ROOK, cap);
                appendPromotionMove(&m, from, to, me, BISHOP, cap);
                appendPromotionMove(&m, from, to, me, KNIGHT, cap);
            }
        }
    }

    return (int)(m - mstart);
}


int chessposition::CreateEvasionMovelist(chessmove* mstart)
{
    chessmove* m = mstart;
    int me = state & S2MMASK;
    int you = me ^ S2MMASK;
    U64 targetbits;
    U64 frombits;
    int from, to;
    PieceCode pc;
    int king = kingpos[me];
    U64 occupiedbits = (occupied00[0] | occupied00[1]);

    // moving the king is alway a possibe evasion
    targetbits = king_attacks[king] & ~occupied00[me];
    while (targetbits)
    {
        to = pullLsb(&targetbits);
        if (!isAttackedBy<OCCUPIEDANDKING>(to, you) && !isAttackedByMySlider(to, occupiedbits ^ BITSET(king), you))
        {
            appendMoveToList(&m, king, to, WKING | me, mailbox[to]);
        }
    }

    if (POPCOUNT(isCheckbb) == 1)
    {
        // only one attacker => capture or block the attacker is a possible evasion
        int attacker;
        GETLSB(attacker, isCheckbb);
        // special case: attacker is pawn and can be captured enpassant
        if (ept && ept == attacker + S2MSIGN(me) * 8)
        {
            frombits = pawn_attacks_from[ept][me] & piece00[WPAWN | me];
            while (frombits)
            {
                from = pullLsb(&frombits);
                // treat ep capture as normal move and correct code manually
                appendMoveToList(&m, from, attacker + S2MSIGN(me) * 8, WPAWN | me, WPAWN | you);
                (m - 1)->code |= EPCAPTUREFLAG;
            }
        }
        // now normal captures of the attacker
        to = attacker;
        frombits = isAttackedBy<OCCUPIED>(to, me);
        // later: blockers; targetbits will contain empty squares between king and attacking slider
        targetbits = betweenMask[king][attacker];
        while (true)
        {
            frombits = frombits & ~kingPinned;
            while (frombits)
            {
                from = pullLsb(&frombits);
                pc = mailbox[from];
                PieceCode cap = mailbox[to];
                if ((pc >> 1) == PAWN)
                {
                    if (PROMOTERANK(to))
                    {
                        appendPromotionMove(&m, from, to, me, QUEEN, cap);
                        appendPromotionMove(&m, from, to, me, ROOK, cap);
                        appendPromotionMove(&m, from, to, me, BISHOP, cap);
                        appendPromotionMove(&m, from, to, me, KNIGHT, cap);
                        continue;
                    }
                    else if (!((from ^ to) & 0x8) && (epthelper[to] & piece00[pc ^ S2MMASK]))
                    {
                        // EPT possible for opponent; set EPT field manually
                        appendMoveToList(&m, from, to, pc, BLANK);
                        (m - 1)->code |= (from + to) << 19;
                        continue;
                    }
                }
                appendMoveToList(&m, from, to, pc, mailbox[to]);
            }
            if (!targetbits)
                break;
            to = pullLsb(&targetbits);
            frombits = isAttackedBy<FREE>(to, me);  // <FREE> is needed here as the target fields are empty and pawns move normal
        }
    }
    return (int)(m - mstart);
}


template <MoveType Mt> int chessposition::CreateMovelist(chessmove* mstart)
{
    int me = state & S2MMASK;
    U64 occupiedbits = (occupied00[0] | occupied00[1]);
    U64 emptybits = ~occupiedbits;
    U64 targetbits = 0ULL;
    chessmove* m = mstart;

    if (Mt & QUIET)
        targetbits |= emptybits;
    if (Mt & CAPTURE)
        targetbits |= occupied00[me ^ S2MMASK];
    if (me)
    {
        m += CreateMovelistPawn<Mt, BLACK>(m);
        m += CreateMovelistPiece<KNIGHT, BLACK>(m, occupiedbits, targetbits);
        m += CreateMovelistPiece<BISHOP, BLACK>(m, occupiedbits, targetbits);
        m += CreateMovelistPiece<ROOK, BLACK>(m, occupiedbits, targetbits);
        m += CreateMovelistPiece<QUEEN, BLACK>(m, occupiedbits, targetbits);
        m += CreateMovelistPiece<KING, BLACK>(m, occupiedbits, targetbits);
        if (Mt & QUIET)
            m += CreateMovelistCastle<BLACK>(m);
    }
    else {
        m += CreateMovelistPawn<Mt, WHITE>(m);
        m += CreateMovelistPiece<KNIGHT, WHITE>(m, occupiedbits, targetbits);
        m += CreateMovelistPiece<BISHOP, WHITE>(m, occupiedbits, targetbits);
        m += CreateMovelistPiece<ROOK, WHITE>(m, occupiedbits, targetbits);
        m += CreateMovelistPiece<QUEEN, WHITE>(m, occupiedbits, targetbits);
        m += CreateMovelistPiece<KING, WHITE>(m, occupiedbits, targetbits);
        if (Mt & QUIET)
            m += CreateMovelistCastle<WHITE>(m);
    }

    return (int)(m - mstart);
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
                | (pawn_attacks_to[from][s2m] & (occ | (ept ? BITSET(ept) : 0ULL)));
    case KNIGHT:
        return knight_attacks[from];
    case BISHOP:
        return BISHOPATTACKS(occ, from);
    case ROOK:
        return ROOKATTACKS(occ, from);
    case QUEEN:
        return BISHOPATTACKS(occ, from) | ROOKATTACKS(occ, from);
    case KING:
        return king_attacks[from];
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
        return BISHOPATTACKS(occ, from);
    case ROOK:
        return ROOKATTACKS(occ, from);
    case QUEEN:
        return BISHOPATTACKS(occ, from) | ROOKATTACKS(occ, from);
    default:
        return 0ULL;
    }
}


// this is only used for king attacks, so opponent king attacks can be left out
template <AttackType At> U64 chessposition::isAttackedBy(int index, int col)
{
    U64 occ = occupied00[0] | occupied00[1];
    return (knight_attacks[index] & piece00[WKNIGHT | col])
        | (ROOKATTACKS(occ, index) & (piece00[WROOK | col] | piece00[WQUEEN | col]))
        | (BISHOPATTACKS(occ, index) & (piece00[WBISHOP | col] | piece00[WQUEEN | col]))
        | (piece00[WPAWN | col] & (At != FREE ?
            pawn_attacks_from[index][col] :
            pawn_moves_from[index][col] | (pawn_moves_from_double[index][col] & PAWNPUSH(col ^ S2MMASK, ~occ))))
        | (At == OCCUPIEDANDKING ? (king_attacks[index] & piece00[WKING | col]) : 0ULL);
}


bool chessposition::isAttacked(int index, int Me)
{
    int opponent = Me ^ S2MMASK;

    return knight_attacks[index] & piece00[WKNIGHT | opponent]
        || king_attacks[index] & piece00[WKING | opponent]
        || pawn_attacks_to[index][state & S2MMASK] & piece00[(PAWN << 1) | opponent]
        || ROOKATTACKS(occupied00[0] | occupied00[1], index) & (piece00[WROOK | opponent] | piece00[WQUEEN | opponent])
        || BISHOPATTACKS(occupied00[0] | occupied00[1], index) & (piece00[WBISHOP | opponent] | piece00[WQUEEN | opponent]);
}

// used for checkevasion test, could be usefull for discovered check test
U64 chessposition::isAttackedByMySlider(int index, U64 occ, int me)
{
    return (ROOKATTACKS(occ, index) & (piece00[WROOK | me] | piece00[WQUEEN | me]))
        | (BISHOPATTACKS(occ, index) & (piece00[WBISHOP | me] | piece00[WQUEEN | me]));
}


U64 chessposition::attackedByBB(int index, U64 occ)
{
    return (knight_attacks[index] & (piece00[WKNIGHT] | piece00[BKNIGHT]))
        | (king_attacks[index] & (piece00[WKING] | piece00[BKING]))
        | (pawn_attacks_to[index][1] & piece00[WPAWN])
        | (pawn_attacks_to[index][0] & piece00[BPAWN])
        | (ROOKATTACKS(occ, index) & (piece00[WROOK] | piece00[BROOK] | piece00[WQUEEN] | piece00[BQUEEN]))
        | (BISHOPATTACKS(occ, index) & (piece00[WBISHOP] | piece00[BBISHOP] | piece00[WQUEEN] | piece00[BQUEEN]));
}


// more advanced see respecting a variable threshold, quiet and promotion moves and faster xray attack handling
bool chessposition::see(uint32_t move, int threshold)
{
    int from = GETFROM(move);
    int to = GETCORRECTTO(move);

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
            attacker |= (BISHOPATTACKS(seeOccupied, to) & potentialBishopAttackers);
        if (nextPiece == ROOK || nextPiece == QUEEN || nextPiece == KING)
            attacker |= (ROOKATTACKS(seeOccupied, to) & potentialRookAttackers);

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
    U64 msk;
#ifdef NNUE
    // attack bitboard not set in NNUE mode
    if (NnueReady)
        msk = 0xffffffffffffffff;
    else
        msk = attackedBy[me][0];
#else
    msk = attackedBy[me][0];
#endif
    if (piece00[WQUEEN | you] & msk)
        captureval += materialvalue[QUEEN];
    else if (piece00[WROOK | you] & msk)
        captureval += materialvalue[ROOK];
    else if ((piece00[WKNIGHT | you] | piece00[WBISHOP | you]) & msk)
        captureval += materialvalue[KNIGHT];
    else if (piece00[WPAWN | you] & msk)
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
    if (!p->isCheckbb)
    {
        onlyGoodCaptures = true;
        state = TACTICALINITSTATE;
        margin = 1;
    }
    else
    {
        onlyGoodCaptures = false;
        state = EVASIONINITSTATE;
    }
    captures = &pos->captureslist[pos->ply];
    quiets = &pos->quietslist[pos->ply];
}

// MoveSelector for probcut
void MoveSelector::SetPreferredMoves(chessposition* p, int m, int excludemove)
{
    pos = p;
    margin = m;
    onlyGoodCaptures = true;
    state = TACTICALINITSTATE;
    if (!excludemove)
        captures = &pos->captureslist[pos->ply];
    else
        captures = &pos->singularcaptureslist[pos->ply];
}

// MoveSelector for alphabeta search
void MoveSelector::SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, uint32_t counter, int excludemove)
{
    pos = p;
    hashmove = p->shortMove2FullMove(hshm);
    if (kllm1 != hashmove)
        killermove1 = kllm1;
    if (kllm2 != hashmove)
        killermove2 = kllm2;
    if (counter != hashmove && counter != kllm1 && counter != kllm2)
        countermove = counter;
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


uint32_t MoveSelector::next()
{
    chessmove *m;
    switch (state)
    {
    case INITSTATE:
        state++;
        // fall through
    case HASHMOVESTATE:
        state++;
        if (hashmove)
        {
            SDEBUGDO(true, value = PVVAL;)
            return hashmove;
        }
        // fall through
    case TACTICALINITSTATE:
        state++;
        captures->length = pos->CreateMovelist<TACTICAL>(&captures->move[0]);
        pos->evaluateMoves<CAPTURE>(captures);
        // fall through
    case TACTICALSTATE:
        while ((m = captures->getNextMove(0)))
        {
            SDEBUGDO(true, value = m->value;)
            if (!pos->see(m->code, margin))
            {
                m->value |= BADTACTICALFLAG;
            }
            else {
                m->value = INT_MIN;
                if (m->code != hashmove)
                    return m->code;
            }
        }
        state++;
        if (onlyGoodCaptures)
            return 0;
        // fall through
    case KILLERMOVE1STATE:
        state++;
        if (pos->moveIsPseudoLegal(killermove1))
        {
            SDEBUGDO(true, value = KILLERVAL1;)
            return killermove1;
        }
        // fall through
    case KILLERMOVE2STATE:
        state++;
        if (pos->moveIsPseudoLegal(killermove2))
        {
            SDEBUGDO(true, value = KILLERVAL2;)
            return killermove2;
        }
        // fall through
    case COUNTERMOVESTATE:
        state++;
        if (pos->moveIsPseudoLegal(countermove))
        {
            SDEBUGDO(true, value = NMREFUTEVAL;)
            return countermove;
        }
        // fall through
    case QUIETINITSTATE:
        state++;
        quiets->length = pos->CreateMovelist<QUIET>(&quiets->move[0]);
        pos->evaluateMoves<QUIET>(quiets);
        // fall through
    case QUIETSTATE:
        uint32_t mc;
        while ((mc = quiets->getAndRemoveNextMove()))
        {
            SDEBUGDO(true, value = quiets->lastvalue;)
            if (mc != hashmove
                && mc != killermove1
                && mc != killermove2
                && mc != countermove)
                return mc;
        }
        state++;
        // fall through
    case BADTACTICALSTATE:
        while ((m = captures->getNextMove()))
        {
            SDEBUGDO(true, value = m->value;)
            bool bBadTactical = (m->value & BADTACTICALFLAG);
            m->value = INT_MIN;
            if (bBadTactical) {
                STATISTICSDO(if (m->code == hashmove) STATISTICSINC(moves_bad_hash));
                return m->code;
            }
        }
        state++;
        // fall through
    case BADTACTICALEND:
        return 0;
    case EVASIONINITSTATE:
        state++;
        captures->length = pos->CreateEvasionMovelist(&captures->move[0]);
        pos->evaluateMoves<ALL>(captures);
        // fall through
    case EVASIONSTATE:
        while ((mc = captures->getAndRemoveNextMove()))
        {
            SDEBUGDO(true, value = captures->lastvalue;)
            return mc;
        }
        state++;
        // fall through
    default:
        return 0;
    }
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

static void uciSetSyzygyPath()
{
    init_tablebases((char*)en.SyzygyPath.c_str());
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

#ifdef UCILOGGING
void uciSetLogFile()
{
    string filename = (en.LogFile == "" ? "" : en.ExecPath + en.LogFile);
    string sLogging;
    if (!guiCom.openLog(filename, en.frequency))
        sLogging = "Cannot open Logfile " + filename;
    else
        sLogging = (filename == "" ? "No logging." : "Logging to " + filename);

    guiCom << en.name() + " (Build " + BUILD + ")\n";
    guiCom << "UCI compatible chess engine by " + en.author + "\n";
    guiCom << "----------------------------------------------------------------------------------------\n";
    guiCom << "System: " + cinfo.SystemName() + "\n";
    guiCom << "CPU-Features of system: " + cinfo.PrintCpuFeatures(cinfo.machineSupports) + "\n";
    guiCom << "CPU-Features of binary: " + cinfo.PrintCpuFeatures(cinfo.binarySupports) + "\n";
    guiCom << "========================================================================================\n";
    guiCom << "info string " + sLogging + "\n";
}
#endif

#ifdef NNUE
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
#endif

compilerinfo::compilerinfo()
{
    GetSystemInfo();
}

string compilerinfo::SystemName()
{
    return system;
}


engine::engine(compilerinfo *c)
{
    compinfo = c;
    initBitmaphelper();
#ifdef NNUE
    NnueInit();
#endif
    rootposition.pwnhsh.setSize(1);  // some dummy pawnhash just to make the prefetch in playMove happy
    // default castle rights
    int rf[2][2] = { { 0, 7 },{0,7} };
    int kf[2] = { 4,4 };
    rootposition.initCastleRights(rf, kf);

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
    ucioptions.Set("SyzygyPath", "<empty>");
    Threads = 0;
    allocThreads();
    rootposition.pwnhsh.remove();
    rootposition.mtrlhsh.remove();
#ifdef NNUE
    NnueRemove();
#endif
}


void engine::registerOptions()
{
#ifdef UCILOGGING
    en.ucioptions.Register(&en.LogFile, "LogFile", ucistring, "", 0, 0, uciSetLogFile);
#endif
#ifdef _WIN32
    ucioptions.Register(&allowlargepages, "Allow Large Pages", ucicheck, "true", 0, 0, uciAllowLargePages);
#endif
    ucioptions.Register(&Threads, "Threads", ucispin, "1", 1, MAXTHREADS, uciSetThreads);  // order is important as the pawnhash depends on Threads > 0
    ucioptions.Register(&Hash, "Hash", ucispin, to_string(DEFAULTHASH), 1, MAXHASH, uciSetHash);
    ucioptions.Register(&moveOverhead, "Move_Overhead", ucispin, "100", 0, 5000, nullptr);
    ucioptions.Register(&MultiPV, "MultiPV", ucispin, "1", 1, MAXMULTIPV, nullptr);
    ucioptions.Register(&ponder, "Ponder", ucicheck, "false");
    ucioptions.Register(&SyzygyPath, "SyzygyPath", ucistring, "<empty>", 0, 0, uciSetSyzygyPath);
    ucioptions.Register(&Syzygy50MoveRule, "Syzygy50MoveRule", ucicheck, "true");
    ucioptions.Register(&SyzygyProbeLimit, "SyzygyProbeLimit", ucispin, "7", 0, 7, nullptr);
    ucioptions.Register(&BookFile, "BookFile", ucistring, "<empty>", 0, 0, uciSetBookFile);
    ucioptions.Register(&BookBestMove, "BookBestMove", ucicheck, "true");
    ucioptions.Register(&BookDepth, "BookDepth", ucispin, "255", 0, 255);
    ucioptions.Register(&chess960, "UCI_Chess960", ucicheck, "false");
    ucioptions.Register(nullptr, "Clear Hash", ucibutton, "", 0, 0, uciClearHash);
#ifdef NNUE
#ifndef NNUEINCLUDED
    ucioptions.Register(&NnueNetpath, "NNUENetpath", ucistring, "<Default>", 0, 0, uciSetNnuePath);
#endif
    ucioptions.Register(&usennue, "Use_NNUE", ucicheck, "true", 0, 0, uciSetNnuePath);
#endif
}


void engine::allocThreads()
{
    // first cleanup the old searchthreads memory
    for (int i = 0; i < oldThreads; i++)
    {
        sthread[i].pos.mtrlhsh.remove();
        sthread[i].pos.pwnhsh.remove();
    }

    freealigned64(sthread);

    oldThreads = Threads;

    if (!Threads)
        return;

    size_t size = Threads * sizeof(searchthread);
    myassert(size % 64 == 0, nullptr, 1, size % 64);

    sthread = (searchthread*) allocalign64(size);
    memset((void*)sthread, 0, size);
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
#ifdef NNUE
        pos->accumulator[0].computationState[WHITE] = false;
        pos->accumulator[0].computationState[BLACK] = false;
#endif
    }
}


void chessposition::resetStats()
{
    memset(history, 0, sizeof(chessposition::history));
    memset(tacticalhst, 0, sizeof(chessposition::tacticalhst));
    memset(counterhistory, 0, sizeof(chessposition::counterhistory));
    memset(countermove, 0, sizeof(chessposition::countermove));
    he_yes = 0ULL;
    he_all = 0ULL;
    he_threshold = 8100;
}


void engine::resetStats()
{
    for (int i = 0; i < Threads; i++)
    {
        chessposition* pos = &sthread[i].pos;
        pos->resetStats();
    }
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
    string fen = STARTFEN;
    vector<string> moves;
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
                ponderhit = (lastopponentsmove && lastopponentsmove == rootposition.pondermove);
                // Preserve hashes of earlier position up to last halfmove counter reset for repetition detection
                rootposition.prerootmovenum = rootposition.ply;
                int i = 0;
                int j = PREROOTMOVES - rootposition.ply;
                while (i < rootposition.ply)
                    rootposition.prerootmovestack[j++] = rootposition.movestack[i++];

                rootposition.lastnullmove = -rootposition.ply - 1;
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
                            rootposition.pvmovecode[i++] = rootposition.movestack[rootposition.ply - 1].movecode;
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
                guiCom << "id name " + name() + "\n";
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
#ifdef NNUE
                if (en.usennue && !NnueReady)
                    break;
#endif
                pondersearch = NO;
                searchmoves.clear();
                wtime = btime = winc = binc = movestogo = mate = maxdepth = 0;
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
                searchStart();
                if (inputstring != "")
                {
                    // bench mode; wait for end of search
                    searchWaitStop(false);
                }
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
                    maxdepth = stoi(commandargs[ci++]);
                    perft(maxdepth, true);
                }
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
    if (inputstring == "")
        searchWaitStop();
}


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



// Some global objects
alignas(64) compilerinfo cinfo;
alignas(64) evalparamset eps;
alignas(64) zobrist zb;
alignas(64) engine en(&cinfo);


// Explicit template instantiation
// This avoids putting these definitions in header file
template U64 chessposition::isAttackedBy<OCCUPIED>(int index, int col);
template U64 chessposition::pieceMovesTo<KNIGHT>(int);
template U64 chessposition::pieceMovesTo<BISHOP>(int);
template U64 chessposition::pieceMovesTo<ROOK>(int);
template U64 chessposition::pieceMovesTo<QUEEN>(int);
template bool chessposition::sliderAttacked<WHITE>(int index, U64 occ);
template bool chessposition::sliderAttacked<BLACK>(int index, U64 occ);
template void chessposition::updatePins<WHITE>();
template void chessposition::updatePins<BLACK>();
template void chessposition::updateThreats<WHITE>();
template void chessposition::updateThreats<BLACK>();
