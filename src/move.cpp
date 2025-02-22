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

void chessmovelist::print()
{
    printf("%s", toString().c_str());
}

// Sorting for MoveSelector; keep the move in the list
chessmove* chessmovelist::getNextMove(int minval)
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


uint32_t chessposition::applyMove(string s, bool resetMstop)
{
    int from, to;
    PieceCode promotion = BLANK;

    size_t slen = s.size();

    if (s[0] == 'O' || s[0] == '0') {
        if (slen >= 3 && s[0] == s[2] && (slen < 5 || s[0] == s[4])) {
            guiCom << "info string Warning! Your GUI uses wrong encoding " + s + " for castle move. But I am gracious for now.\n";
            int col = state & S2MMASK;
            from = kingpos[col];
            to = castlerookfrom[col * 2 + (slen == 3)];
        }
        else {
            return 0;
        }
    }
    else {
        if (slen < 4 || slen > 5)
            return 0;

        from = AlgebraicToIndex(s);
        to = AlgebraicToIndex(&s[2]);
        if (slen == 5)
            promotion = (PieceCode)((GetPieceType(s[4]) << 1) | (state & S2MMASK));

        // Change normal castle moves (e.g. e1g1) to the more general chess960 format "king captures rook" (e1h1)
        if ((mailbox[from] >> 1) == KING && mailbox[to] == BLANK && abs(from - to) == 2)
            to = to > from ? to + 1 : to - 2;
    }

    chessmove m = chessmove(from, to, promotion, BLANK, BLANK);
    m.code = shortMove2FullMove((uint16_t)m.code);

    prepareStack();

    if (playMove<false>(m.code))
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
            int pieceTo = piece * 64 + to;
            ml->move[i].value += (conthistptr[ply - 1][pieceTo] + conthistptr[ply - 2][pieceTo] + (conthistptr[ply - 4][pieceTo] + conthistptr[ply - 6][pieceTo]) / 2);
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

    bool tthit;
    ttentry* tte = tp.probeHash(hash, &tthit);
    bool bSearchmoves = (en.searchmoves.size() > 0);

    excludemovestack[0] = 0; // FIXME: Not very nice; is it worth to do do singular testing in root search?
    for (int i = 0; i < movelist.length; i++)
    {
        chessmove* m = &movelist.move[i];
        if (!see(m->code, 0))
            m->code |= BADSEEFLAG;
        if (bSearchmoves)
        {
            string strMove = moveToString(m->code);
            if (en.searchmoves.find(strMove) == en.searchmoves.end())
                continue;
        }
        if (playMove<false>(m->code))
        {
            if (tthit)
            {
                // Test for immediate or possible 3fold to fix a possibly wrong hash entry
                if (testRepetition() >= 2)
                {
                    // This move triggers 3fold; remember move to update hash
                    bImmediate3fold = true;
                    moveTo3fold = m->code;
                }
                else if ((uint16_t)m->code == tte->movecode)
                {
                    // Test if this move makes a 3fold possible for opponent
                    prepareStack();
                    chessmovelist followupmovelist;
                    followupmovelist.length = CreateMovelist<ALL>(&followupmovelist.move[0]);
                    for (int j = 0; j < followupmovelist.length; j++)
                    {
                        if (playMove<false>(followupmovelist.move[j].code))
                        {
                            if (testRepetition() >= 2)
                                // 3fold for opponent is possible
                                moveTo3fold = m->code;

                            unplayMove<false>(followupmovelist.move[j].code);
                        }
                    }
                }
            }

            rootmovelist.move[rootmovelist.length++] = *m;
            unplayMove<false>(m->code);
            if (bestval < m->value)
            {
                defaultmove = m->code;
                bestval = m->value;
            }
        }
    }
    if (moveTo3fold)
        // Hashmove triggers 3fold immediately or with following move; fix hash
        tp.addHash(tte, hash, SCOREDRAW, tte->staticeval, bImmediate3fold ? HASHBETA : HASHALPHA, 250 + TTDEPTH_OFFSET, moveTo3fold);
}


void chessposition::tbFilterRootMoves()
{
    tbPosition = 0;
    useRootmoveScore = 0;
    if (!useTb || POPCOUNT(occupied00[0] | occupied00[1]) > useTb)
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
            // double push enables ept
            myeptcastle = ADDEPT(from, to);
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

    if (kingPinned & BITSET(from) && !(BITSET(to) & lineMask[kingpos[piececol]][from]))
        return false;

    return true;
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


void chessposition::playNullMove()
{
    lastnullmove = ply;
    conthistptr[ply] = (int16_t*)counterhistory[0][0];
    movecode[ply++] = 0;
    state ^= S2MMASK;
    hash ^= zb.s2m ^ zb.ept[ept];
    ept = 0;
    myassert(ply <= MAXDEPTH, this, 1, ply);
    DirtyPiece* dp = &dirtypiece[ply];
    dp->dirtyNum = 0;
    dp->pc[0] = 0; // don't break search for updatable positions on stack
    computationState[ply][WHITE] = false;
    computationState[ply][BLACK] = false;
    
}


void chessposition::unplayNullMove()
{
    state ^= S2MMASK;
    lastnullmove = movestack[--ply].lastnullmove;
    ept = movestack[ply].ept;
    hash ^= zb.s2m^ zb.ept[ept];
    myassert(ply >= 0, this, 1, ply);
}

// Do all updates for a move played
// LiteMode: ommit updated for several things(*) when you don't need to evaluate position
// (*): accumulator, dirtypiece, halfmovescounter, fullmovescounter, hash, pawnhash, piececount, conthistptr, nodes, updatePins()
template <bool LiteMode>
bool chessposition::playMove(uint32_t mc)
{
    int s2m = state & S2MMASK;
    int eptnew = 0;
    int oldcastle;
    DirtyPiece* dp;

    if (!LiteMode) {
        oldcastle = (state & CASTLEMASK);
        dp = &dirtypiece[ply + 1];
        dp->dirtyNum = 0;
        computationState[ply + 1][WHITE] = false;
        computationState[ply + 1][BLACK] = false;
    }

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
            if (!LiteMode) {
                hash ^= zb.boardtable[(kingfrom << 4) | kingpc] ^ zb.boardtable[(kingto << 4) | kingpc];
                pawnhash ^= zb.boardtable[(kingfrom << 4) | kingpc] ^ zb.boardtable[(kingto << 4) | kingpc];
                dp->pc[0] = kingpc;
                dp->from[0] = kingfrom;
                dp->to[0] = kingto;
                dp->dirtyNum = 1;
            }
        }
        if (rookfrom != rookto)
        {
            BitboardMove(rookfrom, rookto, rookpc);
            if (!LiteMode) {
                hash ^= zb.boardtable[(rookfrom << 4) | rookpc] ^ zb.boardtable[(rookto << 4) | rookpc];
                int di = dp->dirtyNum;
                dp->pc[di] = rookpc;
                dp->from[di] = rookfrom;
                dp->to[di] = rookto;
                dp->dirtyNum++;
            }
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

        if (!LiteMode) {
            dp->pc[0] = pfrom;
            dp->from[0] = from;
            dp->to[0] = to;
            dp->dirtyNum = 1;
        }

        myassert(!promote || (ptype == PAWN && RRANK(to, s2m) == 7), this, 4, promote, ptype, to, s2m);
        myassert(pfrom == mailbox[from], this, 3, pfrom, from, mailbox[from]);
        myassert(ISEPCAPTURE(mc) || capture == mailbox[to], this, 2, capture, mailbox[to]);

        // Fix hash regarding capture
        if (capture != BLANK && !ISEPCAPTURE(mc))
        {
            BitboardClear(to, capture);
            materialhash ^= zb.boardtable[(POPCOUNT(piece00[capture]) << 4) | capture];
            if (!LiteMode) {
                hash ^= zb.boardtable[(to << 4) | capture];
                if ((capture >> 1) == PAWN)
                    pawnhash ^= zb.boardtable[(to << 4) | capture];
                dp->pc[1] = capture;
                dp->from[1] = to;
                dp->to[1] = -1;
                dp->dirtyNum = 2;
                piececount--;
            }
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
            if (!LiteMode) {
                // just double the hash-switch for target to make the pawn vanish
                pawnhash ^= zb.boardtable[(to << 4) | promote];
                int di = dp->dirtyNum;
                dp->to[0] = -1; // remove promoting pawn;
                dp->from[di] = -1;
                dp->to[di] = to;
                dp->pc[di] = promote;
                dp->dirtyNum++;
            }
        }

        if (!LiteMode) {
            hash ^= zb.boardtable[(to << 4) | mailbox[to]];
            hash ^= zb.boardtable[(from << 4) | pfrom];
        }
        mailbox[from] = BLANK;

        /* PAWN specials */
        if (ptype == PAWN)
        {
            eptnew = GETEPT(mc);
            if (!LiteMode) {
                pawnhash ^= zb.boardtable[(to << 4) | mailbox[to]];
                pawnhash ^= zb.boardtable[(from << 4) | pfrom];
            }
            halfmovescounter = 0;

            if (ept && to == ept)
            {
                int epfield = (from & 0x38) | (to & 0x07);
                BitboardClear(epfield, (pfrom ^ S2MMASK));
                mailbox[epfield] = BLANK;
                materialhash ^= zb.boardtable[(POPCOUNT(piece00[(pfrom ^ S2MMASK)]) << 4) | (pfrom ^ S2MMASK)];
                if (!LiteMode) {
                    hash ^= zb.boardtable[(epfield << 4) | (pfrom ^ S2MMASK)];
                    pawnhash ^= zb.boardtable[(epfield << 4) | (pfrom ^ S2MMASK)];
                    dp->pc[1] = (pfrom ^ S2MMASK);
                    dp->from[1] = epfield;
                    dp->to[1] = -1;
                    dp->dirtyNum++;
                    piececount--;
                }
            }
        }

        if (ptype == KING)
            kingpos[s2m] = to;

        // Here we can test the move for being legal
        if (isAttacked(kingpos[s2m], s2m))
        {
            materialhash = movestack[ply].materialhash;
            // Move is illegal; just do the necessary subset of unplayMove
            if (!LiteMode) {
                hash = movestack[ply].hash;
                pawnhash = movestack[ply].pawnhash;
            }
            halfmovescounter = movestack[ply].halfmovescounter;
            kingpos[s2m] = movestack[ply].kingpos[s2m];
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
                if (!LiteMode)
                    piececount++;
            }
            else {
                mailbox[to] = BLANK;
            }
            return false;
        }

        // remove castle rights depending on from and to square
        state &= (castlerights[from] & castlerights[to]);
        if (!LiteMode && ptype == KING)
        {
            // Store king position in pawn hash
            pawnhash ^= zb.boardtable[(from << 4) | pfrom] ^ zb.boardtable[(to << 4) | pfrom];
        }
    }

    state ^= S2MMASK;
    isCheckbb = isAttackedBy<OCCUPIED>(kingpos[s2m ^ S2MMASK], s2m);

    if (!LiteMode) {
        hash ^= zb.s2m;

        if (!(state & S2MMASK))
            fullmovescounter++;

        // Fix hash regarding ept
        hash ^= zb.ept[ept];
    }

    ept = eptnew;

    if (!LiteMode) {
        hash ^= zb.ept[ept];

        // Fix hash regarding castle rights
        oldcastle ^= (state & CASTLEMASK);
        hash ^= zb.cstl[oldcastle];

        PREFETCH(&tp.table[hash & tp.sizemask]);

        conthistptr[ply] = (int16_t*)counterhistory[GETPIECE(mc)][GETCORRECTTO(mc)];
        myassert(piececount == POPCOUNT(occupied00[WHITE] | occupied00[BLACK]), this, 1, piececount);
    }
    movecode[ply++] = mc;
    myassert(ply <= MAXDEPTH, this, 1, ply);
    kingPinned = 0ULL;

    if (!LiteMode) {
        updatePins<WHITE>();
        updatePins<BLACK>();
        nodes++;
    }

    return true;
}

template <bool LiteMode>
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
            if (!LiteMode)
                piececount++;
        }
        else {
            mailbox[to] = BLANK;
        }
    }
}


//
// MoveSelector
//
// Constructor for quiescence search
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
        margin = 0;
    }
    hashmove = 0;   // FIXME: maybe worth to give a hashmove here?
    captures = &pos->captureslist[pos->ply];
    quiets = &pos->quietslist[pos->ply];
}

// Constructor for probcut
void MoveSelector::SetPreferredMoves(chessposition* p, int m, int excludemove)
{
    pos = p;
    margin = m;
    onlyGoodCaptures = true;
    hashmove = 0;
    state = TACTICALINITSTATE;
    if (!excludemove)
        captures = &pos->captureslist[pos->ply];
    else
        captures = &pos->singularcaptureslist[pos->ply];
}

// Constructor for alphabeta search
void MoveSelector::SetPreferredMoves(chessposition *p, uint16_t hshm, uint32_t kllm1, uint32_t kllm2, uint32_t counter, int excludemove)
{
    pos = p;
    hashmove = p->shortMove2FullMove(hshm);
    killermove1 = (kllm1 != hashmove ? kllm1 : 0);
    killermove2 = (kllm2 != hashmove ? kllm2 : 0);
    countermove = (counter != hashmove && counter != kllm1 && counter != kllm2 ? counter : 0);
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
    state = (p->isCheckbb ? EVASIONINITSTATE : HASHMOVESTATE);
    onlyGoodCaptures = false;
    margin = 0;
}


uint32_t MoveSelector::next()
{
    chessmove *m;
    switch (state)
    {
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
        STATISTICSDO(numOfCaptures = captures->length);
        STATISTICSDO(if (numOfCaptures) statistics.ms_tactic_stage[PvNode][depth][numOfCaptures]++ && statistics.ms_tactic_stage[PvNode][depth][0]++);
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
                if (m->code != hashmove) {
                    STATISTICSINC(ms_tactic_moves[PvNode][depth][numOfCaptures]);
                    STATISTICSINC(ms_tactic_moves[PvNode][depth][0]);
                    return m->code;
                }
            }
        }
        state++;
        if (onlyGoodCaptures)
            return 0;
        // fall through
    case KILLERMOVE1STATE:
        STATISTICSINC(ms_spcl_stage[PvNode][depth]);
        state++;
        if (pos->moveIsPseudoLegal(killermove1))
        {
            SDEBUGDO(true, value = KILLERVAL1;)
            STATISTICSINC(ms_spcl_moves[PvNode][depth]);
            return killermove1;
        }
        // fall through
    case KILLERMOVE2STATE:
        state++;
        if (pos->moveIsPseudoLegal(killermove2))
        {
            SDEBUGDO(true, value = KILLERVAL2;)
            STATISTICSINC(ms_spcl_moves[PvNode][depth]);
            return killermove2;
        }
        // fall through
    case COUNTERMOVESTATE:
        state++;
        if (pos->moveIsPseudoLegal(countermove))
        {
            SDEBUGDO(true, value = NMREFUTEVAL;)
            STATISTICSINC(ms_spcl_moves[PvNode][depth]);
            return countermove;
        }
        // fall through
    case QUIETINITSTATE:
        state++;
        quiets->length = pos->CreateMovelist<QUIET>(&quiets->move[0]);
        STATISTICSDO(numOfQuiets = min(MAXSTATMOVES - 1, quiets->length));
        STATISTICSDO(if (numOfQuiets) statistics.ms_quiet_stage[PvNode][depth][numOfQuiets]++ && statistics.ms_quiet_stage[PvNode][depth][0]++);
        pos->evaluateMoves<QUIET>(quiets);
        // fall through
    case QUIETSTATE:
        uint32_t mc;
        while ((mc = quiets->getAndRemoveNextMove()))
        {
            SDEBUGDO(true, value = quiets->lastvalue;);
            if (mc != hashmove
                && mc != killermove1
                && mc != killermove2
                && mc != countermove)
            {
                STATISTICSINC(ms_quiet_moves[PvNode][depth][numOfQuiets]);
                STATISTICSINC(ms_quiet_moves[PvNode][depth][0]);
                return mc;
            }
        }
        STATISTICSINC(ms_badtactic_stage[PvNode][depth]);
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
                STATISTICSINC(ms_badtactic_moves[PvNode][depth]);
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
        STATISTICSDO(numOfCaptures = captures->length);
        STATISTICSDO(if (numOfCaptures) statistics.ms_evasion_stage[PvNode][depth][numOfCaptures]++&& statistics.ms_evasion_stage[PvNode][depth][0]++);
        pos->evaluateMoves<ALL>(captures);
        // fall through
    case EVASIONSTATE:
        while ((mc = captures->getAndRemoveNextMove()))
        {
            SDEBUGDO(true, value = captures->lastvalue;)
            STATISTICSINC(ms_evasion_moves[PvNode][depth][numOfCaptures]);
            STATISTICSINC(ms_evasion_moves[PvNode][depth][0]);
            return mc;
        }
        state++;
        // fall through
    default:
        return 0;
    }
}


//
// Move generation stuff
//
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
                (m - 1)->code |= ADDEPT(from, to);
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
                        (m - 1)->code |= ADDEPT(from, to);
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



// Explicit template instantiation
// This avoids putting these definitions in header file
template void chessposition::evaluateMoves<QUIET>(chessmovelist*);
template void chessposition::evaluateMoves<CAPTURE>(chessmovelist*);
template bool chessposition::playMove<true>(uint32_t);
template void chessposition::unplayMove<true>(uint32_t);

} // namespace rubichess
