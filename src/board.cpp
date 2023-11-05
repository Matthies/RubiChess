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

using namespace rubichess;

namespace rubichess {

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
    if (POPCOUNT(occupied00[WHITE] | occupied00[BLACK]) > 32)
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
    piececount = POPCOUNT(occupied00[WHITE] | occupied00[BLACK]);
    return 0;
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
    *os << "Pseudo-legal Moves: " + pseudolegalmoves.toString() + "\n";
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
        cm.code = movecode[i];
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
        if (!playMove<false>(cm.code))
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
        unplayMove<false>(cm.code);
    }
}


#ifdef SDEBUG
bool chessposition::triggerDebug(uint16_t* nextmove)
{
    if (pvmovecode[0] == 0)
        return false;

    int j = 0;

    while (j < ply && pvmovecode[j])
    {
        if ((movecode[j] & 0xffff) != (pvmovecode[j] & 0xffff))
            return false;
        j++;
    }
    *nextmove = pvmovecode[j];

    return true;
}


const char* PvAbortStr[] = {
    "unknown", "pv from tt", "different move in tt", "razor-pruned", "reverse-futility-pruned", "nullmove-pruned", "probcut-pruned", "late-move-pruned",
    "futility-pruned", "bad-see-pruned", "bad-history-pruned", "multicut-pruned", "bestmove", "not best move", "omitted", "betacut", "below alpha", "checkmate", "stalemate"
};


void chessposition::pvdebugout()
{
    guiCom.log("[SDEBUG] =======================================================================\n");
    guiCom.log("[SDEBUG]   Window       Move   MoveVal   Num Dep  Score          Reason         \n");
    guiCom.log("[SDEBUG] -----------------------------------------------------------------------\n");
    for (int i = 0; pvmovecode[i]; i++)
    {
        chessmove m;
        m.code = pvmovecode[i];
        stringstream ss;
        ss << "[SDEBUG] " << setw(6) << pvalpha[i] << "/" << setw(6) << pvbeta[i] << "  " << m.toString() << "  " << setw(8) << hex << pvmovevalue[i] << dec << "  " << (pvmovenum[i] < 0 ? ">" : " ")
            << setw(2) << abs(pvmovenum[i]) << "  " << setw(2) << pvdepth[i] << "  " << setw(5) << pvabortscore[i] << "  " << setw(23) << PvAbortStr[pvaborttype[i]] << "  " << pvadditionalinfo[i] << "\n";
        guiCom.log(ss.str());

        if (pvaborttype[i + 1] == PVA_UNKNOWN || pvaborttype[i] == PVA_OMITTED)
            break;
    }
    guiCom.log("[SDEBUG] =======================================================================\n");
}

#endif


// shameless copy from https://www.chessprogramming.org/Magic_Bitboards
alignas(64) U64 mBishopAttacks[64][1 << BISHOPINDEXBITS];
alignas(64) U64 mRookAttacks[64][1 << ROOKINDEXBITS];

alignas(64) SMagic mBishopTbl[64];
alignas(64) SMagic mRookTbl[64];



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
    // attack bitboard not set in NNUE mode
    if (NnueReady)
        msk = 0xffffffffffffffff;
    else
        msk = attackedBy[me][0];
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


// Some global objects
alignas(64) compilerinfo cinfo;
alignas(64) EPSCONST evalparamset eps;
alignas(64) zobrist zb;
alignas(64) engine en(&cinfo);
alignas(64) SPSCONST searchparamset sps;
alignas(64) GuiCommunication guiCom;

// Explicit template instantiation
// This avoids putting these definitions in header file
template U64 chessposition::isAttackedBy<FREE>(int, int col);
template U64 chessposition::isAttackedBy<OCCUPIED>(int, int);
template U64 chessposition::isAttackedBy<OCCUPIEDANDKING>(int, int);
template U64 chessposition::pieceMovesTo<KNIGHT>(int);
template U64 chessposition::pieceMovesTo<BISHOP>(int);
template U64 chessposition::pieceMovesTo<ROOK>(int);
template U64 chessposition::pieceMovesTo<QUEEN>(int);
template bool chessposition::sliderAttacked<WHITE>(int, U64);
template bool chessposition::sliderAttacked<BLACK>(int, U64);
template void chessposition::updatePins<WHITE>();
template void chessposition::updatePins<BLACK>();
template void chessposition::updateThreats<WHITE>();
template void chessposition::updateThreats<BLACK>();

} // namespace rubichess
