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

#ifdef NNUELEARN
#include <atomic>
#include <mutex>

namespace rubichess {

using namespace rubichess;

/*
    Summary about different 16bit move encoding:
    Rubi:
        ppppfffffftttttt
        pppp                Promotion piece code (e.g. 11 = 1011 for promotion to black queen)
            ffffff          from square
                  tttttt    to square
    Castle moves are encoded 'king captures rook'
    Ep captures are detected by 'diagonal pawn move with empty target square'

    SF/sfen:
        ttppfffffftttttt
        tt                  type (00=normal, 01=promotion, 10=ep capture, 11=castle)
          pp                promotion piece (00=knight, 01=bishop, 10=rook, 11=queen)
            ffffff          from square
                  tttttt    to square
    Castle moves are encoded 'king captures rook'

    binpack:
        ttppfffffftttttt
        tt                  type (00=normal, 01=promotion, 10=castle, 11=ep capture)
          pp                promotion piece (00=knight, 01=bishop, 10=rook, 11=queen)
            ffffff          from square
                  tttttt    to square
    Castle moves are encoded 'king captures rook'

*/

// get SF/sfen code from a long Rubi move code
inline uint16_t sfFromRubi(uint32_t c)
{
    uint16_t sfc = (uint16_t)(c & 0xfff);
    uint16_t p;
    if ((p = GETPROMOTION(c)))
        sfc |= ((p >> 1) + 2) << 12;
    else if (ISCASTLE(c))
        sfc |= (3 << 14);
    else if (ISEPCAPTURE(c))
        sfc |= (2 << 14);
    return sfc;
}

// get SF/sfen code from a long Rubi move code
inline uint16_t binpackFromRubi(uint32_t c)
{
    //printf("%08x\n", c);
    uint16_t sfc = (GETFROM(c) << 8) | (GETTO(c) << 2);
    if (ISPROMOTION(c))
        sfc |= ((GETPROMOTION(c) >> 1) - 2) | (1 << 14);
    else if (ISCASTLE(c))
        sfc |= (2 << 14);
    else if (ISEPCAPTURE(c))
        sfc |= (3 << 14);
    //printf("%08x\n", sfc);
    return sfc;
}

// get Rubi full move code from a SF/sfen code
inline uint32_t rubiFromSf(chessposition *pos, uint16_t c)
{
    uint16_t rc = c & 0xfff;
    uint8_t type = (c >> 14);
    if (type == 1) // promotion
    {
        int p = (((c >> 12) - 2) << 1);
        if (RANK(GETTO(c)) == 0)
            // black promotion
            p++;
        rc |= (p << 12);
    }
    uint32_t fullmove = pos->shortMove2FullMove(rc);
    return fullmove;
}

// get Rubi full move code from a binpack code
inline uint32_t rubiFromBinpack(chessposition* pos, uint16_t c)
{
    uint16_t rc = c & 0xfff;
    uint8_t type = (c >> 14);
    if (type == 1) // promotion
    {
        int p = (((c >> 12) - 2) << 1);
        if (RANK(GETTO(c)) == 0)
            // black promotion
            p++;
        rc |= (p << 12);
    }
    uint32_t fullmove = pos->shortMove2FullMove(rc);
    return fullmove;
}


//
// Sfen/bin related code
//

constexpr size_t GENSFEN_HASH_SIZE = 0x1000000;
alignas(64) U64 sfenhash[GENSFEN_HASH_SIZE];

const unsigned int sfenchunksize = 0x1000;
const int sfenchunknums = 2;
bool gensfenstop;

enum { CHUNKFREE, CHUNKINUSE, CHUNKFULL };

struct HuffmanedPiece
{
    int code; // how it will be coded
    int bits; // How many bits do you have
};

constexpr HuffmanedPiece huffman_table[] =
{
  {0x00,1}, // NO_PIECE
  {0x00,1}, // NO_PIECE
  {0x01,5}, // WPAWN
  {0x11,5}, // BPAWN
  {0x03,5}, // WKNIGHT
  {0x13,5}, // BKNIGHT
  {0x05,5}, // WBISHOP
  {0x15,5}, // BBISHOP
  {0x07,5}, // WROOK
  {0x17,5}, // BROOK
  {0x09,5}, // WQUEEN
  {0x19,5}, // BQUEEN
};

// FIXME: This is ugly but compatible with SF sfens
inline uint8_t HUFFMAN2RUBI(uint8_t x) {
    return (((x) >> 3) | ((((x) & 7) + 1) * 2));
}

inline void write_n_bit(uint8_t *buffer, uint8_t data, int n, int *totalbits)
{
    while (n)
    {
        int bitnum = *totalbits % 8;
        int bytenum = *totalbits / 8;
        uint8_t* b = buffer + bytenum;
        uint8_t olddata = *b;
        int bits = min(n, 8 - bitnum);
        uint8_t datamask = (1 << bits) - 1;
        *b = olddata | ((data & datamask) << bitnum);
        data = data >> bits;
        *totalbits += bits;
        n -= bits;
    }
}

inline void read_n_bit(uint8_t* buffer, uint8_t* data, int n, int* totalbits)
{
    int databits = 0;
    *data = 0;
    while (n)
    {
        int bitnum = *totalbits % 8;
        int bytenum = *totalbits / 8;
        uint8_t* b = buffer + bytenum;
        int bits = min(n, 8 - bitnum);
        uint8_t datamask = ((1 << bits) - 1) << bitnum;
        *data |= ((*b & datamask) >> bitnum) << databits;
        databits += bits;
        *totalbits += bits;
        n -= bits;
    }
}

inline void read_piece(uint8_t* buffer, uint8_t* piece, int* totalbits)
{
    read_n_bit(buffer, piece, 1, totalbits);
    if (*piece == 0)
        return;
    read_n_bit(buffer, piece, 4, totalbits);
    *piece = HUFFMAN2RUBI(*piece);
}


int chessposition::getFromSfen(PackedSfen* sfen)
{
    uint8_t b;
    memset(piece00, 0, sizeof(piece00));
    psqval = 0;
    phcount = 0;
    int bitnum = 0;
    uint8_t* buffer = &sfen->data[0];
    // side to move
    read_n_bit(buffer, &b, 1, &bitnum);
    state = b;
    //  king positions
    read_n_bit(buffer, &b, 6, &bitnum);
    kingpos[WHITE] = b;
    mailbox[b] = WKING;
    BitboardSet(b, WKING);
    read_n_bit(buffer, &b, 6, &bitnum);
    kingpos[BLACK] = b;
    mailbox[b] = BKING;
    BitboardSet(b, BKING);
    U64 kingbb = piece00[WKING] | piece00[BKING];

    // pieces in the order of fen
    for (int i = 0; i < 64; i++)
    {
        int sq = i ^ RANKMASK;
        if (BITSET(sq) & kingbb) continue;

        read_piece(buffer, &b, &bitnum);
        mailbox[sq] = b;
        if (b)
            BitboardSet(sq, b);
    }
    // Castles
    read_n_bit(buffer, &b, 4, &bitnum);
    if (b & 1) state |= WKCMASK;
    if (b & 2) state |= WQCMASK;
    if (b & 4) state |= BKCMASK;
    if (b & 8) state |= BQCMASK;

    // en passant
    read_n_bit(buffer, &b, 1, &bitnum);
    if (b)
        read_n_bit(buffer, (uint8_t*)&ept, 6, &bitnum);
    else
        ept = 0;

    // halfmove counter; FIXME: This is compatible with SF but buggy as the upper bit is missing and counter is cut at 64
    read_n_bit(buffer, (uint8_t*)&halfmovescounter, 6, &bitnum);

    // fullmoves counter
    read_n_bit(buffer, (uint8_t*)&fullmovescounter, 8, &bitnum);

    // Fix for overflow of fullmoves (256), comaptible with latest SF and old nodchip
    read_n_bit(buffer, &b, 8, &bitnum);
    fullmovescounter += (b << 8);

    // Fix for overflow of halfmoves (64), comaptible with latest SF and old nodchip
    read_n_bit(buffer, &b, 1, &bitnum);
    halfmovescounter += (b << 6);

    isCheckbb = isAttackedBy<OCCUPIED>(kingpos[state & S2MMASK], (state & S2MMASK) ^ S2MMASK);
    kingPinned = 0ULL;
    updatePins<WHITE>();
    updatePins<BLACK>();

    zb.getAllHashes(this);
    lastnullmove = -1;
    ply = 0;
    piececount = POPCOUNT(occupied00[WHITE] | occupied00[BLACK]);
    computationState[0][WHITE] = false;
    computationState[0][BLACK] = false;
    threatSquare = 64;
    return 0;
}


void chessposition::toSfen(PackedSfen *sfen)
{
    memset(&sfen->data, 0, sizeof(sfen->data));
    int bitnum = 0;
    uint8_t* buffer = &sfen->data[0];
    // side to move
    write_n_bit(buffer, state & S2MMASK, 1, &bitnum);
    //  king positions
    write_n_bit(buffer, kingpos[WHITE], 6, &bitnum);
    write_n_bit(buffer, kingpos[BLACK], 6, &bitnum);

    // pieces in the order of fen
    for (int i = 0; i < 64; i++)
    {
        int sq = i ^ RANKMASK;
        PieceCode pc = mailbox[sq];
        if ((pc >> 1) == KING) continue;
        write_n_bit(buffer, huffman_table[pc].code, huffman_table[pc].bits, &bitnum);
    }
    // Castles
    write_n_bit(buffer, (bool)(state & WKCMASK), 1, &bitnum);
    write_n_bit(buffer, (bool)(state & WQCMASK), 1, &bitnum);
    write_n_bit(buffer, (bool)(state & BKCMASK), 1, &bitnum);
    write_n_bit(buffer, (bool)(state & BQCMASK), 1, &bitnum);

    // en passant
    if (ept)
        write_n_bit(buffer, ept * 2 + 1, 7, &bitnum);
    else
        write_n_bit(buffer, 0, 1, &bitnum);

    // halfmove counter; FIXME: This is compatible with SF but buggy as the upper bit is missing and counter is cut at 64
    write_n_bit(buffer, halfmovescounter, 6, &bitnum);

    // fullmoves counter
    write_n_bit(buffer, fullmovescounter, 8, &bitnum);

    // Fix for overflow of fullmoves (256), comaptible with latest SF and old nodchip
    write_n_bit(buffer, fullmovescounter >> 8, 8, &bitnum);

    // Fix for overflow of halfmoves (64), comaptible with latest SF and old nodchip
    write_n_bit(buffer, halfmovescounter >> 6, 1, &bitnum);
}


void flush_psv(int result, searchthread* thr)
{
    PackedSfenValue* p;
    int fullchunk = -1;

    if (!thr->psv)
        // Not a single position of this game stored
        return;

    U64 offset = thr->psv - thr->psvbuffer;
    while (true)
    {
        p = thr->psvbuffer + offset;
        if (abs(p->game_result) != 2)
            break;
        p->game_result = p->game_result / 2 * result;
        if (offset % sfenchunksize == 0)
        {
            fullchunk = ((int)(offset / sfenchunksize) + sfenchunknums - 1) % sfenchunknums;
            if (offset == 0)
                offset = sfenchunknums * sfenchunksize;
        }
        offset--;
    }
    if (fullchunk >= 0 && thr->chunkstate[fullchunk] == CHUNKINUSE)
    {
        thr->chunkstate[fullchunk] = CHUNKFULL;
    }
}



//
// BINPACK related code
//

const size_t maxBinpackChunkSize = 1 * 1024 * 1024;
size_t maxContinuationSize;

inline uint16_t SHORTFROMBIGENDIAN(char* c) {
    return ((uint8_t)(c)[1] | ((uint8_t)(c)[0] << 8));
}
inline U64 LONGLONGFROMBIGENDIAN(char* c) {
    return  ((U64)((uint8_t)(c)[7]) | ((U64)((uint8_t)(c)[6]) << 8) | ((U64)((uint8_t)(c)[5]) << 16) | (((U64)((uint8_t)(c)[4])) << 24ULL) | ((U64)((uint8_t)(c)[3]) << 32) | ((U64)((uint8_t)(c)[2]) << 40) | ((U64)((uint8_t)(c)[1]) << 48) | ((U64)((uint8_t)(c)[0]) << 56));
}

inline int  GETBITINDEX(U64 b, int i) {
    return (POPCOUNT((b) & (((BITSET(i) - 1) << 1) + 1)) - 1);
}

void drawBytes(char *b, int n)
{
    for (int i = 0; i < n; i++)
        printf("%02x ", *(uint8_t*)(b + i));
    printf("\n");
}

inline int16_t toSigned(uint16_t u)
{
    u = (u << 15) | (u >> 1);
    if (u & 0x8000)
        u ^= 0x7fff;
    int16_t s;
    memcpy(&s, &u, sizeof(uint16_t));
    return s;
}

inline uint16_t toUnsigned(int16_t s)
{
    uint16_t u;
    memcpy(&u, &s, sizeof(int16_t));
    if (u & 0x8000)
        u ^= 0x7fff;
    u = (u << 1) | (u >> 15);
    return u;
}

inline int indexBits(U64 c) {
    if (c <= 1)  return 0;
    int n;
    GETMSB(n, c - 1);
    return n + 1;
}

// bitnum <= 8
inline uint8_t getNextBits(Binpack *bp, int bitnum)
{
    uint8_t by = *(uint8_t*)*bp->data;
    by = by << bp->consumedBits;
    by = by >> (8 - bitnum);
    bp->consumedBits += bitnum;

    if (bp->consumedBits >= 8)
    {
        int bitsleft = bp->consumedBits - 8;
        uint8_t by2 = *(uint8_t*)(*bp->data + 1) >> (8 - bitsleft);
        by |= by2;
        bp->consumedBits -= 8;
        (*bp->data)++;
    }
    return by;
}

inline uint16_t getNextBlocks(Binpack* bp, int blocksize)
{
    uint8_t mask = BITSET(blocksize) - 1;
    uint16_t val = 0;
    int bitnum = 0;
    while (1)
    {
        uint8_t by = getNextBits(bp, blocksize + 1);
        val |= (by & mask) << bitnum;
        if (!(by >> blocksize))
            break;
        bitnum += blocksize;
    }
    return val;
}

inline int getNthBitIndex(U64 occ,unsigned int n)
{
    int index;
    for (unsigned int i = 0; i <= n; i++)
        index = pullLsb(&occ);
    return index;
}

// bitnum <= 8
inline void putNextBits(Binpack *bp, uint8_t by, int bitnum)
{
    if (bp->debug()) printf("putNextBits(%d) before: %02x  aktuelles Byte: %02x(%d)\n", bitnum, by, *(uint8_t*)*bp->data, bp->consumedBits);
    by = by << (8 - bitnum);
    *((uint8_t*)*bp->data) |= (by >> bp->consumedBits);
    bp->consumedBits += bitnum;

    if (bp->consumedBits >= 8)
    {
        if (bp->debug()) printf("putNextBits full byte: %02x\n", *(uint8_t*)*bp->data);
        (*bp->data)++;
        bp->consumedBits -= 8;
        by = by << (bitnum - bp->consumedBits);
        *((uint8_t*)*bp->data) = by;
    }
    if (bp->debug()) printf("putNextBits(%d) after : %02x  aktuelles Byte: %02x(%d)\n", bitnum, by, *(uint8_t*)*bp->data, bp->consumedBits);
}

inline void putNextBlocks(Binpack* bp, int blocksize, uint16_t val)
{
    uint8_t mask = BITSET(blocksize) - 1;
    while (1)
    {
        uint8_t by = val & mask;
        val = val >> blocksize;
        if (val)
            by |= BITSET(blocksize);
        putNextBits(bp, by, blocksize + 1);
        if (!val)
            break;
    }
}


void chessposition::fixEpt()
{
    if (!ept)
        return;

    int Me = state & S2MMASK;
    int You = 1 - Me;
    // the ep flag may be wrong (ep capture illegal); recheck as this is important for indexing the pawn doing the ep capture
    int king = kingpos[Me];
    int doublepusher = PAWNPUSHINDEX(You, ept);
    U64 eppawns = epthelper[doublepusher] & piece00[WPAWN | Me];
    BitboardClear(doublepusher, WPAWN | You);
    bool eptpossible = false;
    while (eppawns) {
        int from = pullLsb(&eppawns);
        BitboardMove(from, ept, WPAWN | Me);
        eptpossible = !isAttacked(king, Me);
        BitboardMove(ept, from, WPAWN | Me);
        if (eptpossible)
            break;
    }
    BitboardSet(doublepusher, WPAWN | You);
    if (!eptpossible)
        ept = 0;
}


void chessposition::copyToLight(chessposition *target)
{
    target->ply = ply;
    target->state = state;
    memcpy(target->piece00, piece00, sizeof(piece00));
    target->ept = ept;
    target->halfmovescounter = halfmovescounter;
    memcpy(target->kingpos, kingpos, sizeof(kingpos));
    memcpy(target->mailbox, mailbox, sizeof(mailbox));
    target->piececount = piececount;
}

// test if this is the follow up position of src playing move mc
bool chessposition::followsTo(chessposition *src, uint32_t mc)
{
    if (!mc)
        return false;

    if (!src->playMove<false>(mc))
    {
        // mc should always be possible in position src as it is the preferred move
        cout << "Alarm! Failed to play " << hex << mc << "(" << moveToString(mc) << ")" << endl;
        return false;
    }

    src->fixEpt();
    src->ply = 0;

    for (int p = WPAWN; p <= BKING; p++)
        if (src->piece00[p] != piece00[p]) return false;

    const U64 statemsk = CASTLEMASK | S2MMASK;
    if ((src->state & statemsk)  != (state & statemsk) || src->ept != ept || src->halfmovescounter != halfmovescounter)
        return false;

    return true;
}

void chessposition::getPosFromBinpack(Binpack* bp)
{
    if (bp->debug()) drawBytes(*bp->data, 8);
    U64 occ = LONGLONGFROMBIGENDIAN(*bp->data);
    *bp->data += sizeof(occ);
    int piecenum = 0;
    uint8_t b = 0;
    while (occ)
    {
        int sq = pullLsb(&occ);
        int p;
        PieceCode pc;
        if (piecenum & 1)
        {
            p = b >> 4;
        }
        else {
            b = *(uint8_t*)*bp->data;
            if (bp->debug()) drawBytes(*bp->data, 1);
            p = b & 0xf;
            (*bp->data)++;
        }

        switch (p) {
        case 12:
            // pawn with ep square behind
            ept = (RANK(sq) == 3 ? sq - 8 : sq + 8);
            pc = (RANK(sq) == 3 ? WPAWN : BPAWN);
            break;
        case 13:
            // white rook with coresponding castling rights
            pc = WROOK;
            state |= SETCASTLEFILE(FILE(sq), (WHITE * 2 + (bool)piece00[WKING]));
            break;
        case 14:
            // black rook with coresponding castling rights
            pc = BROOK;
            state |= SETCASTLEFILE(FILE(sq), (BLACK * 2 + (bool)piece00[BKING]));
            break;
        case 15:
            // black king and black is side to move
            pc = BKING;
            state |= S2MMASK;
            break;
        default:
            pc = p + 2;
        }
        mailbox[sq] = pc;
        BitboardSet(sq, pc);
        if ((pc >> 1) == KING)
            kingpos[pc & S2MMASK] = sq;
        piecenum++;
    }

    int bytesConsumed = (piecenum + 1) / 2;

    if (bp->debug()) drawBytes(*bp->data, 16 - bytesConsumed);
    if (bp->debug()) drawBytes(*bp->data + 16 - bytesConsumed, 16);
    (*bp->data) += 16 - bytesConsumed;
}


int chessposition::getNextFromBinpack(Binpack *bp)
{
    if (bp->compressedmoves == 0)
    {
        // get a full position
        memset(piece00, 0, sizeof(piece00));
        memset(mailbox, 0, sizeof(mailbox));
        psqval = 0;
        state = 0;
        phcount = 0;
        ply = 0;
        ept = 0;
        threatSquare = 64;
        lastnullmove = -1;
        computationState[0][WHITE] = false;
        computationState[0][BLACK] = false;
        // get the pieces
        getPosFromBinpack(bp);
        zb.getAllHashes(this);
        isCheckbb = isAttackedBy<OCCUPIED>(kingpos[state & S2MMASK], (state & S2MMASK) ^ S2MMASK);
        kingPinned = 0ULL;
        updatePins<WHITE>();
        updatePins<BLACK>();
        piececount = POPCOUNT(occupied00[WHITE] | occupied00[BLACK]);
        // get the move
        uint16_t bpmc = SHORTFROMBIGENDIAN(*bp->data);
        *bp->data += sizeof(uint16_t);
        uint16_t type = (bpmc >> 14);
        uint16_t from = (bpmc >> 8) & 0x3f;
        uint16_t to = (bpmc >> 2) & 0x3f;
        bp->move = (type << 14) | (from << 6) | to;
        if (type == 1)
            bp->move |= (bpmc & 3) << 12;
        // get the score
        bp->score = toSigned(SHORTFROMBIGENDIAN(*bp->data));
        *bp->data += sizeof(int16_t);
        //get ply and result
        uint16_t plyandresult = SHORTFROMBIGENDIAN(*bp->data);
        *bp->data += sizeof(uint16_t);
        bp->gamePly = plyandresult & 0x3FFF;
        fullmovescounter = bp->gamePly / 2 + 1;
        bp->gameResult = (int8_t)toSigned(plyandresult >> 14);
        // 50-moves-counter
        halfmovescounter = SHORTFROMBIGENDIAN(*bp->data);
        *bp->data += sizeof(int16_t);
        //number of compressed moves following
        bp->compressedmoves = SHORTFROMBIGENDIAN(*bp->data);
        *bp->data += sizeof(int16_t);
    }
    else {
        // play the last move
        playMove<false>(bp->fullmove);
        ply = 0;
        int Me = state & S2MMASK;
        fixEpt();
        bp->lastScore = -bp->score;
        bp->gameResult = -bp->gameResult;
        // get the next compressed move
        U64 mysquaresbb = occupied00[state & S2MMASK];
        int bitnum = indexBits(POPCOUNT(mysquaresbb));
        int pieceId =  getNextBits(bp, bitnum);
        int from = getNthBitIndex(mysquaresbb, pieceId);
        PieceCode pc = mailbox[from];
        U64 targetbb;
        int targetnum;
        int to;
        int toId;
        switch (pc >> 1) {
        case PAWN:
        {
            U64 occ = occupied00[0] | occupied00[1];
            targetbb = pawn_moves_to[from][Me] & ~occ;
            if (targetbb)
                targetbb |= pawn_moves_to_double[from][Me] & ~occ;
            targetbb |= pawn_attacks_to[from][Me] & (piece00[1 - Me] | (ept ? BITSET(ept) : 0ULL));
            targetnum = POPCOUNT(targetbb);
            if (RRANK(from, Me) == 6)
            {
                // promotion
                bitnum = indexBits(targetnum << 2);
                toId = getNextBits(bp, bitnum);
                to = getNthBitIndex(targetbb, toId / 4);
                bp->move = (1 << 14) | ((toId % 4) << 12) | (from << 6) | to;
            }
            else {
                bitnum = indexBits(targetnum);
                toId = getNextBits(bp, bitnum);
                to = getNthBitIndex(targetbb, toId);
                bp->move = (ept && ept == to ? (3 << 14) : 0) | (from << 6) | to;
            }
        }
            break;
        case KING:
        {
            targetbb = movesTo(pc, from) & ~mysquaresbb;
            targetnum = POPCOUNT(targetbb);
            // Test castle moves
            int mycastlemask = (Me ? BQCMASK | BKCMASK : WQCMASK | WKCMASK) & state;
            bitnum = indexBits(targetnum + POPCOUNT(mycastlemask));
            toId = getNextBits(bp, bitnum);
            if (toId >= targetnum)
                // castle move
                to = (2 << 14) | castlerookfrom[getNthBitIndex(mycastlemask, toId - targetnum) - 1];
            else
                to = getNthBitIndex(targetbb, toId);
            bp->move = (from << 6) | to;
        }
            break;
        default:
            targetbb = movesTo(pc, from) & ~mysquaresbb;
            targetnum = POPCOUNT(targetbb);
            bitnum = indexBits(targetnum);
            toId = getNextBits(bp, bitnum);
            to = getNthBitIndex(targetbb, toId);
            bp->move = (from << 6) | to;
            break;
        }
        // get the score diff
        uint16_t uscore = getNextBlocks(bp, 4);
        int16_t scorediff = toSigned(uscore);
        bp->score = bp->lastScore + scorediff;
        bp->gamePly++;
        bp->compressedmoves--;
        // last compressed move? throw away unused bits
        if (bp->compressedmoves == 0 && bp->consumedBits)
        {
            (*bp->data)++;
            bp->consumedBits = 0;
        }
    }

    bp->fullmove = rubiFromBinpack(this, bp->move);
    if (!bp->fullmove) {
        printf("Illegal move %04x. Something wrong here. Exit...\n", bp->move);
        print();
        return -1;
    }

    return 0;
}



void chessposition::posToBinpack(Binpack* bp)
{
    U64 occ = occupied00[0] | occupied00[1];
    *(U64*)(*bp->data) = LONGLONGFROMBIGENDIAN((char*)&occ);
    if (bp->debug()) drawBytes(*bp->data, 8);
    *bp->data += sizeof(occ);
    int piecenum = 0;
    while (piecenum < 32)
    {
        int pc = 0;
        if (occ)
        {
            int sq = pullLsb(&occ);
            pc = mailbox[sq];
            int s2m = pc & S2MMASK;
            if (ept && ept == (RANK(sq) == 3 ? sq - 8 : sq + 8))
                // pawn with ep square behind
                pc = 12;
            else if ((pc >> 1) == ROOK && RRANK(sq, s2m) == 0 && ISOUTERFILE(sq) && (state & (WQCMASK << (s2m * 2 + (sq > kingpos[s2m])))))
                pc = 13 + s2m;
            else if (pc == BKING && (state & S2MMASK))
                pc = 15;
            else
                pc = pc - 2;
        }
        if (piecenum & 1)
        {
            *((uint8_t*)*bp->data) |= pc << 4;
            (*bp->data)++;
        }
        else {
            *((uint8_t*)*bp->data) = pc;
        }
    piecenum++;
    }

    if (bp->debug()) drawBytes(*bp->data - 16, 16);
}

void prepareNextBinpackPosition(Binpack* bp)
{
    if (bp->debug()) cout << "Compressed moves: " << bp->compressedmoves << "  consumed bits in last byte: " << bp->consumedBits << endl;
    // throw away unused bits of last compressed move
    if (bp->consumedBits)
    {
        (*bp->data)++;
        bp->consumedBits = 0;
    }
    if (bp->compressedmoves) {
        // write number of compressed moves
        *((uint16_t*)bp->compmvsptr) = SHORTFROMBIGENDIAN((char*)&bp->compressedmoves);
        bp->compressedmoves = 0;
    }
}

void chessposition::nextToBinpack(Binpack* bp)
{
    if (!bp->inpos->followsTo(this, bp->lastFullmove))
    {
        // no continuation
        prepareNextBinpackPosition(bp);

        if (*bp->data > bp->base + maxBinpackChunkSize + 8 + maxContinuationSize)
            bp->flushAt = *bp->data;

        bp->inpos->copyToLight(this);
        // write the position
        posToBinpack(bp);
        // write the move
        uint16_t bpmc = binpackFromRubi(bp->fullmove);
        *((uint16_t*)*bp->data) = SHORTFROMBIGENDIAN((char*)&bpmc);
        if (bp->debug()) drawBytes(*bp->data, 2);
        *bp->data += sizeof(uint16_t);
        // write the score
        uint16_t score = toUnsigned(bp->score);
        *((uint16_t*)*bp->data) = SHORTFROMBIGENDIAN((char*)&score);
        if (bp->debug()) drawBytes(*bp->data, 2);
        *bp->data += sizeof(uint16_t);
        // write ply and result
        uint16_t plyandresult = bp->gamePly;
        plyandresult |= (toUnsigned(bp->gameResult) << 14);
        *((uint16_t*)*bp->data) = SHORTFROMBIGENDIAN((char*)&plyandresult);
        if (bp->debug()) drawBytes(*bp->data, 2);
        *bp->data += sizeof(uint16_t);
        // write half moves counter
        *((uint16_t*)*bp->data) = SHORTFROMBIGENDIAN((char*)&halfmovescounter);
        if (bp->debug()) drawBytes(*bp->data, 2);
        *bp->data += sizeof(uint16_t);
        // remember the buffer position where to write the number of compressed for later
        bp->compmvsptr = *bp->data;
        *((uint16_t*)*bp->data) = SHORTFROMBIGENDIAN((char*)&bp->compressedmoves);
        *bp->data += sizeof(uint16_t);
        **bp->data = 0;
    }
    else {
        // we have a continuation
        int Me = state & S2MMASK;
        bp->compressedmoves++;
        fixEpt();
        // get compressed move encoding for next move
        uint32_t mc = bp->fullmove;
        int from = GETFROM(mc);
        U64 mysquaresbb = occupied00[state & S2MMASK];
        int fromlen = indexBits(POPCOUNT(mysquaresbb));
        int fromN = GETBITINDEX(mysquaresbb, from);
        if (bp->debug()) cout << "from=" << from << "  mysquaresbb=" << hex << mysquaresbb << " fromN=" << dec << fromN << endl;
        int pc = mailbox[from];
        int to = GETTO(mc);
        U64 targetbb;
        int targetnum;
        int tolen;
        int toN;

        switch (pc >> 1) {
        case PAWN:
        {
            U64 occ = occupied00[0] | occupied00[1];
            targetbb = pawn_moves_to[from][Me] & ~occ;
            if (targetbb)
                targetbb |= pawn_moves_to_double[from][Me] & ~occ;
            targetbb |= pawn_attacks_to[from][Me] & (piece00[1 - Me] | (ept ? BITSET(ept) : 0ULL));
            targetnum = POPCOUNT(targetbb);
            if (RRANK(from, Me) == 6)
            {
                // promotion
                tolen = indexBits(targetnum << 2);
                toN = GETBITINDEX(targetbb, to) * 4 + GETPROMOTION(mc) / 2 - 2;
            }
            else {
                tolen = indexBits(targetnum);
                toN = GETBITINDEX(targetbb, to);
            }
        }
            break;
        case KING:
        {
            targetbb = movesTo(pc, from) & ~mysquaresbb;
            targetnum = POPCOUNT(targetbb);
            // Test castle moves
            int mycastlemask = (Me ? BQCMASK | BKCMASK : WQCMASK | WKCMASK) & state;
            int myqueenmask = (Me ? BQCMASK : WQCMASK) & state;
            tolen = indexBits(targetnum + POPCOUNT(mycastlemask));
            if (ISCASTLE(mc))
                toN = targetnum + (to > from && myqueenmask);
            else
                toN = GETBITINDEX(targetbb, to);
        }
            break;
        default:
            targetbb = movesTo(pc, from) & ~mysquaresbb;
            targetnum = POPCOUNT(targetbb);
            tolen = indexBits(targetnum);
            toN = GETBITINDEX(targetbb, to);
            break;
        }
        if (bp->debug()) cout << "to = " << to << "  targetbb =" << hex << targetbb << " toN = " << dec << toN << endl;
        putNextBits(bp, fromN, fromlen);
        putNextBits(bp, toN, tolen);
        // Score diff
        int16_t scorediff = bp->score - bp->lastScore;
        uint16_t uscore = toUnsigned(scorediff);
        putNextBlocks(bp, 4, uscore);
    }
}



//
// Stuff related to generating fens for training
//

//
//
// global parameters for gensfen variation
//
//
int depth = 4;
int depth2 = depth;
int random_multi_pv = 0;
int random_multi_pv_depth = 4;
int random_multi_pv_diff = 50;
int random_opening_ply = 8;
int random_move_count = 5;
int random_move_minply = 1;
int random_move_maxply = 24;
int write_minply = 16;
int maxply = 400;
int generate_draw = 1;
U64 nodes = 0;
int eval_limit = 32000;
int disable_prune = 0;
string book = "";

size_t booklen;
string** bookfen;


static bool getBookPositions()
{
    ifstream is;
    string epd;
    booklen = 1;
    if (book != "")
    {
        is.open(book);
        if (!is)
        {
            cout << "Cannot open book file " << book << "\n";
            return false;
        }
        while (getline(is, epd)) booklen++;
    }
    size_t allocsize = booklen * sizeof(string*);
    bookfen = (string**)allocalign64(allocsize);
    bookfen[0] = new string(STARTFEN);
    if (book != "")
    {
        is.clear();
        is.seekg(0);
        booklen = 1;
        while (getline(is, epd)) bookfen[booklen++] = new string(epd);
    }
    cout << " Number of book entries: " << booklen << "\n";
    return true;
}

static void freeBookPositions()
{
    while (booklen)
        delete bookfen[--booklen];

    freealigned64(bookfen);
}


static void gensfenthread(searchthread* thr, U64 rndseed)
{
    ranctx rnd;
    U64 key;
    U64 hash_index;
    U64 key2;
    raninit(&rnd, rndseed);
    chessmovelist movelist;
    U64 psvnums = 0;
    uint32_t nmc;
    chessposition* pos = &thr->pos;
    pos->resetStats();
    const int depthvariance = max(1, depth2 - depth + 1);
    thr->totalchunks = 0;

    while (true)
    {
        if (gensfenstop)
            return;

        string startfen = en.chess960 ? frcStartFen() : *bookfen[ranval(&rnd) % booklen];
        pos->getFromFen(startfen.c_str());

        vector<bool> random_move_flag;
        random_move_flag.resize((size_t)random_move_maxply + random_move_count);
        vector<int> a;
        a.reserve((size_t)random_move_maxply);
        for (int i = max(random_move_minply - 1 , 0) ; i < random_move_maxply; ++i)
            a.push_back(i);
        for (int i = 0 ; i < min(random_move_count, (int)a.size()) ; ++i)
        {
            swap(a[i], a[ranval(&rnd) % (a.size() - i) + i]);
            random_move_flag[a[i]] = true;
        }

        // Opening random
        for (int i = 0; i < random_opening_ply; ++i)
            random_move_flag[i] = true;

        thr->psv = nullptr;

        for (int ply = 0; ; ++ply)
        {
            if (ply > maxply) // default: 200; SF: 400
            {
                if (generate_draw) flush_psv(0, thr);
                break;
            }
            pos->ply = 0;
            movelist.length = pos->CreateMovelist<ALL>(&movelist.move[0]);

            if (movelist.length == 0)
            {
                if (pos->isCheckbb) // Mate
                    flush_psv(-S2MSIGN(pos->state & S2MMASK), thr);
                else if (generate_draw)
                    flush_psv(0, thr); // Stalemate
                break;
            }

            int nextdepth = depth + ranval(&rnd) % depthvariance;

            int score = (disable_prune ?
                pos->alphabeta<NoPrune>(SCOREBLACKWINS, SCOREWHITEWINS, nextdepth, false)
                : pos->alphabeta<Prune>(SCOREBLACKWINS, SCOREWHITEWINS, nextdepth, false));

            if (POPCOUNT(pos->occupied00[0] | pos->occupied00[1]) <= pos->useTb) // TB adjudication; FIXME: bad with incomplete TB sets
            {
                flush_psv((score > SCOREDRAW) ? S2MSIGN(pos->state & S2MMASK) : (score < SCOREDRAW ? -S2MSIGN(pos->state & S2MMASK) : 0), thr);
                break;
            }

            if (abs(score) >= eval_limit) // win adjudication; default: 32000
            {
                flush_psv((score >= eval_limit) ? S2MSIGN(pos->state & S2MMASK) : -S2MSIGN(pos->state & S2MMASK), thr);
                break;
            }

            if (score == SCOREDRAW && (pos->halfmovescounter>= 100 || pos->testRepetition() >= 2))
            {
                if (generate_draw) flush_psv(0, thr);
                break;
            }

            // Skip first plies
            if (ply < write_minply - 1) // default: 16
                goto SKIP_SAVE;

            // Skip position already in hash table
            key = pos->hash;
            hash_index = key & (GENSFEN_HASH_SIZE - 1);
            key2 = sfenhash[hash_index];
            if (key == key2)
                goto SKIP_SAVE;

            sfenhash[hash_index] = key; // Replace with the current key.

            // generate sfen and values
            thr->psv = thr->psvbuffer + psvnums;
            pos->toSfen(&thr->psv->sfen);
            thr->psv->score = score;
            thr->psv->gamePly = ply;
            thr->psv->move = sfFromRubi(pos->pvtable[0][0]);
            thr->psv->game_result = 2 * S2MSIGN(pos->state & S2MMASK); // not yet known
            thr->psv->padding = 0xff;

            if (thr->psv->move)
            {
                psvnums++;

                if (psvnums % sfenchunksize == 0)
                {
                    thr->totalchunks++;
                    int thischunk = (int)(psvnums / sfenchunksize - 1);
                    int nextchunk = (thischunk + 1) % sfenchunknums;
                    while (thr->chunkstate[nextchunk] != CHUNKFREE)
                    {
                        printf("\rThread %d waiting for main thread...", thr->index);
                        Sleep(100);
                    }
                    thr->chunkstate[nextchunk] = CHUNKINUSE;
                    psvnums = psvnums % (sfenchunknums * sfenchunksize);
                }
            }

SKIP_SAVE:
            // preset move for next ply with the pv move
            nmc = pos->pvtable[0][0];
            if (!nmc)
            {
                // No move in pv => mate or stalemate
                if (pos->isCheckbb) // Mate
                    flush_psv(-S2MSIGN(pos->state & S2MMASK), thr);
                else if (generate_draw)
                    flush_psv(0, thr); // Stalemate
                break;
            }

            pos->prepareStack();
            bool chooserandom = !nmc || (ply <= random_move_maxply && random_move_flag[ply]);

            while (true)
            {
                int i = 0;
                if (chooserandom)
                {
                    if (random_multi_pv == 0)
                    {
                        i = ranval(&rnd) % movelist.length;
                        nmc = movelist.move[i].code;
                    }
                    else
                    {
                        // random multi pv
                        pos->getRootMoves();
                        int cur_multi_pv = min(pos->rootmovelist.length, (ply < random_opening_ply ? 8 : random_multi_pv));
                        int cur_multi_pv_diff = (ply < random_opening_ply ? 100 : random_multi_pv_diff);
                        pos->rootsearch<MultiPVSearch>(SCOREBLACKWINS, SCOREWHITEWINS, random_multi_pv_depth, 1, cur_multi_pv);
                        int s = min(pos->rootmovelist.length, cur_multi_pv);
                        // Exclude moves with score outside of allowed margin
                        while (cur_multi_pv_diff && pos->bestmovescore[0] > pos->bestmovescore[s - 1] + cur_multi_pv_diff)
                            s--;

                        nmc = pos->multipvtable[ranval(&rnd) % s][0];
                    }
                }

                bool legal = pos->playMove<false>(nmc);

                if (legal)
                    break;

                movelist.move[i].code = movelist.move[--movelist.length].code;
                if (!movelist.length)
                {
                    if (pos->isCheckbb) // Mate
                        flush_psv(-S2MSIGN(pos->state & S2MMASK), thr);
                    else if (generate_draw)
                        flush_psv(0, thr); // Stalemate
                    break;
                }
            }
        }
    }
}


void gensfen(vector<string> args)
{
    U64 loop = 10000;
    string outputfile = "sfens.bin";
    size_t cs = args.size();
    size_t ci = 0;

    int old_multipv = en.MultiPV;
    while (ci < cs)
    {
        string cmd = args[ci++];
        if (cmd == "depth" && ci < cs)
            depth = stoi(args[ci++]);
        if (cmd == "depth2" && ci < cs)
            depth2 = stoi(args[ci++]);
        if (cmd == "loop" && ci < cs)
            loop = stoi(args[ci++]);
        if (cmd == "output_file_name" && ci < cs)
            outputfile = args[ci++];
        if (cmd == "random_multi_pv" && ci < cs)
            random_multi_pv = stoi(args[ci++]);
        if (cmd == "random_multi_pv_depth" && ci < cs)
            random_multi_pv_depth = stoi(args[ci++]);
        if (cmd == "random_multi_pv_diff" && ci < cs)
            random_multi_pv_diff = stoi(args[ci++]);
        if (cmd == "random_move_count" && ci < cs)
            random_move_count = stoi(args[ci++]);
        if (cmd == "random_move_maxply" && ci < cs)
            random_move_maxply = stoi(args[ci++]);
        if (cmd == "write_minply" && ci < cs)
            write_minply = stoi(args[ci++]);
        if (cmd == "eval_limit" && ci < cs)
            eval_limit = stoi(args[ci++]);
        if (cmd == "disable_prune" && ci < cs)
            disable_prune = stoi(args[ci++]);
        if (cmd == "book" && ci < cs)
            book = args[ci++];
    }

    int tnum;
    gensfenstop = false;

    cout << "Generating sfnes with these parameters:\n";
    cout << "output_file_name:      " << outputfile << "\n";
    cout << "loop:                  " << loop << "\n";
    cout << "depth:                 " << depth << "\n";
    cout << "depth2:                " << depth2 << "\n";
    cout << "maxply:                " << maxply << "\n";
    cout << "write_minply:          " << write_minply << "\n";
    cout << "generate_draw:         " << generate_draw << "\n";
    cout << "nodes:                 " << nodes << "\n";
    cout << "eval_limit:            " << eval_limit << "\n";
    cout << "random_opening_ply:    " << random_opening_ply << "\n";
    cout << "random_move_count:     " << random_move_count << "\n";
    cout << "random_move_minply:    " << random_move_minply << "\n";
    cout << "random_move_maxply:    " << random_move_maxply << "\n";
    cout << "random_multi_pv:       " << random_multi_pv << "\n";
    cout << "random_multi_pv_depth: " << random_multi_pv_depth << "\n";
    cout << "random_multi_pv_diff:  " << random_multi_pv_diff << "\n";
    cout << "disable_prune:         " << disable_prune << "\n";
    cout << "book:                  " << book << "\n";

    // create full chunks but ensure at least loop positions
    const unsigned int chunksneeded = (unsigned int)((loop - 1) / sfenchunksize) + 1;
    ofstream os(outputfile, ios::binary | fstream::app);
    if (!os)
    {
        cout << "Cannot open file " << outputfile << "\n";
        return;
    }

    if (!en.chess960 && !getBookPositions())
        return;

    for (tnum = 0; tnum < en.Threads; tnum++)
    {
        en.sthread[tnum].index = tnum;
        en.sthread[tnum].chunkstate[0] = CHUNKINUSE;
        en.sthread[tnum].chunkstate[1] = CHUNKFREE;
        en.sthread[tnum].psvbuffer = (PackedSfenValue*)allocalign64(sfenchunknums * sfenchunksize * sizeof(PackedSfenValue));
        en.sthread[tnum].thr = thread(&gensfenthread, &en.sthread[tnum], getTime() ^ zb.getRnd() );
    }

    U64 chunkswritten = 0;
    tnum = 0;
    const int charsperline = 100;
    U64 starttime = getTime();
    string sProgress(charsperline,'.');
    cout << "\r  ?h??m??s |" << sProgress << "|";
    const double dotsperthread = (double)charsperline / en.Threads;
    const double chunksperthread = (double)chunksneeded / en.Threads;
    const char finedotchar[] = { '-', '\\', '|', '/', 'X'};
    bool showPps = false;
    U64 lastSwitch = starttime;
    const int minShowSec = 30;
    while (chunkswritten < chunksneeded)
    {
        searchthread* thr = &en.sthread[tnum];
        Sleep(100);
        for (int i = 0; i < sfenchunknums; i++)
        {
            if (thr->chunkstate[i] == CHUNKFULL)
            {
                os.write((char*)(thr->psvbuffer + i * sfenchunksize), sfenchunksize * sizeof(PackedSfenValue));
                chunkswritten++;
                thr->chunkstate[i] = CHUNKFREE;
                // Update progress
                U64 now = getTime();
                stringstream ss;
                if (showPps)
                {
                    int pps = ((now - starttime) < en.frequency) ? 0 : (int)((chunkswritten * sfenchunksize) / (int)((now - starttime) / en.frequency));
                    ss << setfill(' ') << setw(7) << pps << "pps";
                }
                else {
                    U64 totaltime = max(now - starttime, (now - starttime) / chunkswritten * chunksneeded);
                    U64 remainingsecs = (totaltime + starttime - now) / en.frequency;
                    ss  << setfill(' ') << setw(3) << (remainingsecs / (60 * 60)) << "h"
                        << setfill('0') << setw(2) << (remainingsecs / 60) % 60 << "m"
                        << setfill('0') << setw(2) << remainingsecs % 60 << "s";
                }
                if ((now - lastSwitch) / en.frequency > minShowSec)
                {
                    // Switch display
                    showPps = !showPps;
                    lastSwitch = now;
                }

                int firstdot = (int)round((double)thr->index * dotsperthread);
                int lastdot = (int)round((double)(thr->index + 1) * dotsperthread) - 1;
                int numdots = lastdot - firstdot + 1;
                int previousdot = min(charsperline - 1, firstdot + (int)round((double)(thr->totalchunks - 1) * numdots / chunksperthread));
                int currentdot = firstdot + (int)round(thr->totalchunks * numdots / chunksperthread);
                for (int c = previousdot; c < currentdot; c++)
                    sProgress[c] = finedotchar[4];
                sProgress[currentdot] = finedotchar[thr->totalchunks % 4];
                cout << "\r" << ss.str() << " |" << sProgress << "|";
            }
        }
        tnum = (tnum + 1) % en.Threads;
    }
    gensfenstop = true;
    for (tnum = 0; tnum < en.Threads; tnum++)
    {
        if (en.sthread[tnum].thr.joinable())
            en.sthread[tnum].thr.join();
    }
    cout << "\n\ngensfen finished.\n";
    en.MultiPV = old_multipv;

    freeBookPositions();
    for (tnum = 0; tnum < en.Threads; tnum++)
        freealigned64(en.sthread[tnum].psvbuffer);
}


static void flushBinpack(ostream *os, char *buffer, Binpack* bp)
{
    size_t size = *bp->data - buffer;   // default: copy everything up to the end
    if (bp->flushAt) {
        // buffer full, flushAt is the cutting point
        size = bp->flushAt - buffer;
    }

    *((uint32_t*)(buffer + 4)) = (uint32_t)size - 8;
    os->write(buffer, size);
    if (bp->flushAt) {
        size_t wrappedbytes = *bp->data - bp->flushAt;
        memcpy(buffer + 8, bp->flushAt, wrappedbytes + 1);
        *bp->data -= size - 8;
        if (wrappedbytes)
            bp->compmvsptr -= size - 8;
        bp->flushAt = nullptr;
    }
}

enum SfenFormat { no, bin, binpack, plain };

struct conversion_t {
    string outfilename;
    string outfileext;
    ofstream ofs;
    SfenFormat outformat;
    SfenFormat informat;
    SfenFormat cmpformat;
    int rescoreDepth;
    ifstream *is;
    ifstream *cmps;
    ostream *os;
    mutex mtin, mtout, mtcmp;
    bool okay;
    bool stoprequest;
    bool endofinputfile;
    atomic<unsigned long long> numPositions;
    atomic<int> numInChunks;
    atomic<int> numOutChunks;
    atomic<int> chunksWritten;
    int skipChunks;
    int splitChunks;
    int disable_prune;
    int preserveChunks;
} conv;


bool openOutputFile(conversion_t* cv)
{
    string filename;
    if (cv->ofs)
        cv->ofs.close();
    if (cv->splitChunks)
        filename = cv->outfilename + "-" + to_string(cv->numOutChunks / cv->splitChunks) + cv->outfileext;
    else
        filename = cv->outfilename + cv->outfileext;
    cv->ofs.open(filename, conv.outformat == plain ? ios::out : ios::binary);
    if (!cv->ofs)
    {
        cout << "Cannot open output file " << filename << endl;
        return false;
     }
     cv->os = &cv->ofs;
     return true;
}


struct trainingdata {
    int16_t score;
    int8_t result;
    uint32_t move;  // movecode in Rubi long format
    uint16_t gameply;
};


class sfenreader {
    chessposition* pos;
    char* inbuffer;
    size_t inbuffersize;
    size_t inbufferreserve;
    size_t nextinbuffersize;
    size_t restdata;
    int lastreadchunknum;
    int writechunknum;
    Binpack inbp;
    char* inbptr = nullptr;

public:
    sfenreader();
    ~sfenreader();
    void init(conversion_t* cv);
    chessposition* getPos() { return pos; }
    int getReadChunknum() { return lastreadchunknum; }
    int getWriteChunknum() { return writechunknum; }
    bool testAndAllocateBuffer(conversion_t* cv);
    bool endOfFile(conversion_t* cv) { return restdata == 0 && cv->is->peek() == ios::traits_type::eof(); }
    bool endOfBuffer() { return (inbptr >= inbuffer + restdata); }
    bool getTrainingData(conversion_t* cv, trainingdata* td);
    size_t getRestData() { return restdata; }
};


sfenreader::sfenreader()
{
    int rookfiles[2][2] = { { 0 , 7 }, {0 , 7} };
    int kingfile[2] = { 4, 4 };
    memset((void*)&inbp, 0, sizeof(inbp));
    pos = (chessposition*)allocalign64(sizeof(chessposition));
    pos->pwnhsh.setSize(1);
    pos->initCastleRights(rookfiles, kingfile);
    pos->accumulation = NnueCurrentArch ? NnueCurrentArch->CreateAccumulationStack() : nullptr;
    pos->psqtAccumulation = NnueCurrentArch ? NnueCurrentArch->CreatePsqtAccumulationStack() : nullptr;
    if (NnueCurrentArch) {
        NnueCurrentArch->CreateAccumulationCache(pos);
        NnueCurrentArch->ResetAccumulationCache(pos);
    }
    pos->resetStats();
    memset(pos->prerootmovestack, 0xff, sizeof(chessposition::prerootmovestack));
    memset(pos->movestack, 0, sizeof(chessposition::movestack));
    pos->prerootmovenum = 0;
    pos->prerootmovecode[PREROOTMOVES - 1] = 0x00000001; // satisfy 'no nullmove eval available' and 'unused countermove within bounds'
    inbuffer = nullptr;
}


sfenreader::~sfenreader()
{
    pos->pwnhsh.remove();
    freealigned64(inbuffer);
    freealigned64(pos);
}


void sfenreader::init(conversion_t* cv)
{
    if (cv->informat == bin)
    {
        inbuffersize = sfenchunknums * sfenchunksize * sizeof(PackedSfenValue);
        inbufferreserve = 0;
    }
    else if (cv->informat == binpack)
    {
        inbuffersize = 0;
        inbufferreserve = 0;
    }
    else  // informat == plain
    {
        inbuffersize = 0;
        inbufferreserve = 0;
    }
    nextinbuffersize = inbuffersize;
}


bool sfenreader::testAndAllocateBuffer(conversion_t* cv)
{
    writechunknum = lastreadchunknum;
    if (cv->is->peek() == ios::traits_type::eof()) {
        cv->endofinputfile = true;
    }
    if (cv->endofinputfile)
    {
        lastreadchunknum = -1;
        return true;
    }

    if (cv->informat == binpack)
    {
        char hd[8];
        cv->is->read(hd, 8);
        if (strncmp(hd, "BINP", 4) != 0)
        {
            cout << "BINP Header missing. Exit." << endl;
            cv->mtin.unlock();
            return false;
        }
        nextinbuffersize = *(uint32_t*)&hd[4];
    }

    if (nextinbuffersize != inbuffersize)
    {
        if (inbuffer)
        {
            freealigned64(inbuffer);
            inbuffer = nullptr;
        }
        inbuffersize = nextinbuffersize;
    }

    if (!inbuffer)
    {
        inbuffer = (char*)allocalign64(inbuffersize + inbufferreserve);
        inbptr = inbuffer + inbufferreserve;
    }
    else {
        size_t bytesleft = inbuffer + inbuffersize + inbufferreserve - inbptr;
        memcpy(inbuffer + inbufferreserve - bytesleft, inbptr, bytesleft);
        inbptr -= inbuffersize;
    }

    cv->is->read((char*)inbuffer + inbufferreserve, inbuffersize);
    restdata = cv->is->gcount();

    lastreadchunknum = cv->numInChunks++;
    if (cv->numInChunks <= cv->skipChunks)
        restdata = 0;

    return true;
}


bool sfenreader::getTrainingData(conversion_t* cv, trainingdata* td)
{
    bool found = true;
    if (cv->informat == bin)
    {
        PackedSfenValue* psv = (PackedSfenValue*)inbptr;
        pos->getFromSfen(&psv->sfen);
        td->score = psv->score;
        td->result = psv->game_result;
        td->move = rubiFromSf(pos, psv->move);
        td->gameply = psv->gamePly;
        inbptr += sizeof(PackedSfenValue);
    }
    else if (cv->informat == binpack)
    {
        if (!inbp.data)
        {
            inbp.data = &inbptr;
            inbp.consumedBits = 0;
        }
        if (pos->getNextFromBinpack(&inbp) != 0)
            cv->okay = false;
        td->score = inbp.score;
        td->result = inbp.gameResult;
        td->move = inbp.fullmove;
        td->gameply = inbp.gamePly;
        pos->fixEpt();
    }
    else // informat == plain
    {
        string key;
        string value;
        td->move = 0;
        td->gameply = 0;
        td->result = 0;
        td->score = 0;
        found = false;
        while (true) {
            if (cv->is->peek() == ios::traits_type::eof())
                break;
            *cv->is >> key;
            if (key == "e")
            {
                found = true;
                break;
            }
            *cv->is >> ws;
            getline(*cv->is, value);
            if (key == "fen")
                if (pos->getFromFen(value.c_str()) != 0)
                    cv->okay = false;
            if (key == "result")
                td->result = stoi(value);
            if (key == "score")
                td->score = stoi(value);
            if (key == "ply")
                td->gameply = stoi(value);
            if (key == "move" && value.size() >= 4)
            {
                uint16_t mc = 0;
                int from = AlgebraicToIndex(&value[0]);
                int to = AlgebraicToIndex(&value[2]);
                int pc = pos->mailbox[from];
                int s2m = pc & S2MMASK;
                if ((pc >> 1) == KING && ((from ^ to) & 3) == 2)
                    // castle
                    to = (to > from ? to + 1 : to - 2);
                else if ((pc >> 1) == PAWN && RRANK(from, s2m) == 6)
                    // promotion
                    mc = ((GetPieceType(value[4]) << 1) | s2m) << 12;
                mc |= (from << 6) | to;
                td->move = pos->shortMove2FullMove(mc);
            }
        }
    }

    if (found && cv->rescoreDepth)
    {
        int newscore;
        if (cv->disable_prune)
            newscore = pos->alphabeta<NoPrune>(SCOREBLACKWINS, SCOREWHITEWINS, cv->rescoreDepth, false);
        else
            newscore = pos->alphabeta<Prune>(SCOREBLACKWINS, SCOREWHITEWINS, cv->rescoreDepth, false);
        td->score = newscore;
    }

    return found;
}


static void convertthread(searchthread* thr, conversion_t* cv)
{
    sfenreader* inreader;
    sfenreader* cmpreader = nullptr;
    char* outbuffer = nullptr;
    char* outbptr = nullptr;
    Binpack outbp; // output

    inreader = new sfenreader();
    inreader->init(cv);

    if (cv->cmps)
    {
        cmpreader = new sfenreader();
        cmpreader->init(cv);
    }

    chessposition* outpos = nullptr;

    if (cv->outformat == binpack)
    {
        outbptr = outbuffer = (char*)allocalign64(maxBinpackChunkSize + 2 * maxContinuationSize);
        outbp.base = outbuffer;
        outbp.data = &outbptr;
        outbp.consumedBits = 0;
        outbp.inpos = inreader->getPos();
    }

    while (true)
    {
        cv->mtin.lock();

        if (!inreader->testAndAllocateBuffer(cv))
            return;

        if (!cv->okay || cv->stoprequest || cv->endofinputfile)
        {
            cv->mtin.unlock();
            break;
        }

        if (cmpreader && !cmpreader->testAndAllocateBuffer(cv))
            return;

        cv->mtin.unlock();

        if (inreader->getReadChunknum() < 0)
            // no more data in input file
            break;

        while (cv->okay && (cv->informat == plain || !inreader->endOfBuffer()))
        {
            trainingdata intraining;
            trainingdata cmptraining;

            bool found = inreader->getTrainingData(cv, &intraining);

            if (!found) {
                cerr << "Thread#" << thr->index << ": No training data found. Break!\n";
                break;
            }

            if (cmpreader) {
                bool cmpfound = cmpreader->getTrainingData(cv, &cmptraining);

                // now do some comparison
                cout << "Input file:   " <<  intraining.gameply << " " << intraining.move << " " << intraining.result << " " << intraining.score << "\n";
                cout << "Compare file: " << cmptraining.gameply << " " << cmptraining.move << " " << cmptraining.result << " " << cmptraining.score << "\n";
            }

            chessposition* inpos = inreader->getPos();
            if (cv->outformat == bin)
            {
                PackedSfenValue psv;
                psv.score = intraining.score;
                psv.game_result = intraining.result;
                psv.gamePly = intraining.gameply;
                psv.move = sfFromRubi(intraining.move);
                psv.padding = 0xff;
                inpos->toSfen(&psv.sfen);
                cv->mtout.lock();
                cv->os->write((char*)&psv, sizeof(PackedSfenValue));
                cv->mtout.unlock();
            }
            else if (cv->outformat == binpack)
            {
                if (outbp.flushAt) {
                    if (*outbp.data > outbuffer)
                    {
                        // Wait for correct order
                        while (cv->preserveChunks && inreader->getWriteChunknum() >= 0 && inreader->getWriteChunknum() != cv->chunksWritten)
                            Sleep(100);
                        // flush chunk
                        cv->mtout.lock();
                        flushBinpack(cv->os, outbuffer, &outbp);
                        if (cv->splitChunks && cv->numOutChunks % cv->splitChunks == 0)
                            openOutputFile(cv);
                        cv->chunksWritten++;
                        cv->mtout.unlock();
                    }
                    cv->numOutChunks++;
                }

                if (outbptr == outbuffer)
                {
                    // start new chunk
                    strcpy(outbptr, "BINP");
                    outbptr += 8;
                    outbp.fullmove = 0;
                }

                if (outbp.debug())
                {
                    cout << "fen " << inpos->toFen() << endl;
                    cout << "move " << moveToString(intraining.move) << endl;
                    cout << "score " << intraining.score << endl;
                    cout << "ply " << to_string(intraining.gameply) << endl;
                    cout << "result " << to_string(intraining.result) << endl;
                    cout << "e" << endl;
                }
                if (!outpos)
                {
                    outpos = outbp.outpos = (chessposition*)allocalign64(sizeof(chessposition));
                    inpos->copyToLight(outpos);
                    memcpy(outbp.outpos->castlerights, inpos->castlerights, sizeof(inpos->castlerights));
                }

                outbp.lastFullmove = outbp.fullmove;
                outbp.fullmove = intraining.move;
                outbp.lastScore = -outbp.score;
                outbp.score = intraining.score;
                outbp.gamePly = intraining.gameply;
                outbp.gameResult = intraining.result;

                outpos->nextToBinpack(&outbp);

                if (cv->preserveChunks && inreader->endOfBuffer()) {
                    prepareNextBinpackPosition(&outbp);
                    outbp.flushAt = *outbp.data;
                }

                if (outbp.debug())
                {
                    size_t offset = (outbptr - outbuffer);
                    cout << "Current offset: " << hex << offset << dec << endl;
                }

                outbp.fullmove = intraining.move;
            }
            else if (cv->outformat == plain)
            {
                cv->mtout.lock();
                *cv->os << "fen " << inpos->toFen() << endl;
                *cv->os << "move " << moveToString(intraining.move) << endl;
                *cv->os << "score " << intraining.score << endl;
                *cv->os << "ply " << to_string(intraining.gameply) << endl;
                *cv->os << "result " << to_string(intraining.result) << endl;
                *cv->os << "e" << endl;
                cv->mtout.unlock();
            }

            cv->numPositions++;
            if (cv->numPositions % (cv->rescoreDepth ? 0x2000 : 0x20000) == 0) cerr << ".";
        }
    }

    if (cv->outformat == binpack)
    {
        prepareNextBinpackPosition(&outbp);
        if (*outbp.data > outbuffer)
        {
            while (cv->preserveChunks && inreader->getWriteChunknum() >= 0 && inreader->getWriteChunknum() != cv->chunksWritten)
                Sleep(100);
            // flush chunk
            cv->mtout.lock();
            flushBinpack(cv->os, outbuffer, &outbp);
            if (cv->splitChunks && cv->numOutChunks % cv->splitChunks == 0)
                openOutputFile(cv);
            cv->chunksWritten++;
            cv->mtout.unlock();
        }
        cv->numOutChunks++;
    }

    delete inreader;
    if (cmpreader)
        delete cmpreader;
    if (outbuffer)
        freealigned64(outbuffer);
    if (outpos)
        freealigned64(outpos);

    thr->index = -1;
}


void convert(vector<string> args)
{
    string inputfile;
    string comparefile = "";
    string outputfile = "";
    size_t cs = args.size();
    size_t ci = 0;

    conv.outformat = no;
    conv.numInChunks = 0;
    conv.numOutChunks = 0;
    conv.chunksWritten = 0;
    conv.disable_prune = 0;
    conv.skipChunks = 0;
    conv.rescoreDepth = 0;
    conv.splitChunks = 0;
    conv.numPositions = 0;
    conv.preserveChunks = 0;
    conv.stoprequest = false;
    conv.okay = true;

    size_t unnamedParams = 0;
    while (ci < cs)
    {
        string cmd = args[ci++];
        if ((cmd == "input_file_name" && ci < cs) || (unnamedParams == 1 && ci == 1))
        {
            inputfile = (unnamedParams == ci ? args[ci - 1] : args[ci++]);
            inputfile.erase(remove(inputfile.begin(), inputfile.end(), '\"'), inputfile.end());
        }
        else if ((cmd == "output_file_name" && ci < cs) || (unnamedParams == 2 && ci == 2))
        {
            outputfile = (unnamedParams == ci ? args[ci - 1] : args[ci++]);
            outputfile.erase(remove(outputfile.begin(), outputfile.end(), '\"'), outputfile.end());
        }
        else if (cmd == "compare_file_name" && ci < cs)
        {
            comparefile = args[ci++];
            comparefile.erase(remove(comparefile.begin(), comparefile.end(), '\"'), comparefile.end());
        }
        else if (cmd == "output_format" && ci < cs)
        {
            conv.outformat = (args[ci] == "bin" ? bin : args[ci] == "plain" ? plain : args[ci] == "binpack" ? binpack : no);
            ci++;
        }
        else if (cmd == "rescore_depth" && ci < cs)
        {
            conv.rescoreDepth = stoi(args[ci++]);
        }
        else if (cmd == "disable_prune" && ci < cs)
        {
            conv.disable_prune = stoi(args[ci++]);
        }
        else if (cmd == "skip_chunks" && ci < cs)
        {
            conv.skipChunks = stoi(args[ci++]);
        }
        else if (cmd == "split_chunks" && ci < cs)
        {
            conv.splitChunks = stoi(args[ci++]);
        }
        else if (cmd == "preserve_chunks" && ci < cs)
        {
            conv.preserveChunks = stoi(args[ci++]);
        }
        else
        {
            unnamedParams++;
            ci--;
        }
    }

    conv.informat = (inputfile.find(".binpack") != string::npos ? binpack : inputfile.find(".bin") != string::npos ? bin : plain);
    conv.is = new ifstream(inputfile, conv.informat != plain ? ios::binary : ios_base::in);
    if (!conv.is)
    {
        cout << "Cannot open input file " << inputfile << endl;
        return;
    }
    conv.endofinputfile = false;

    conv.cmps = nullptr;
    if (comparefile != "")
    {
        conv.cmpformat = (comparefile.find(".binpack") != string::npos ? binpack : comparefile.find(".bin") != string::npos ? bin : plain);
        conv.cmps = new ifstream(comparefile, conv.cmpformat != plain ? ios::binary : ios_base::in);
        if (!conv.cmps)
        {
            cout << "Cannot open compare file " << comparefile << endl;
            return;
        }
    }

    conv.os = &cout;
    if (outputfile != "")
    {
        size_t iExt = outputfile.find_last_of(".");
        conv.outfilename = outputfile.substr(0, iExt);
        conv.outfileext = outputfile.substr(iExt);
        if (conv.outformat == no)
            conv.outformat = (conv.outfileext.find(".binpack") != string::npos ? binpack : conv.outfileext.find(".bin") != string::npos ? bin : plain);

        if (!openOutputFile(&conv))
            return;
    }

    maxContinuationSize = (conv.preserveChunks ? maxBinpackChunkSize : 10 * 1024);

    for (int tnum = 0; tnum < en.Threads; tnum++)
    {
        en.sthread[tnum].index = tnum;
        en.sthread[tnum].thr = thread(&convertthread, &en.sthread[tnum], &conv);
    }

    int threadsToStop = en.Threads;
    while (threadsToStop)
    {
        Sleep(100);
        if (_kbhit())
        {
            char c = _getch();
            if (c == 'q') {
                conv.stoprequest = true;
                cerr << "Stopping after current chunks";
            }
        }
        for (int tnum = 0; tnum < en.Threads; tnum++)
        {
            if (en.sthread[tnum].index < 0 && en.sthread[tnum].thr.joinable())
            {
                en.sthread[tnum].thr.join();
                threadsToStop--;
            }
        }
    }

    if (conv.ofs)
        conv.ofs.close();

    if (!conv.okay)
        cerr << endl << "An error occured while reading input data." << endl;
    cerr << endl << "Finished converting " << inputfile << ". " << conv.numPositions << " positions found." << endl;
    if (conv.numOutChunks)
        cerr << "Chunks written:  " << conv.chunksWritten << endl << "Last read chunk: " << conv.numInChunks << endl;

    delete conv.is;
    if (conv.cmps)
        delete conv.cmps;
}




void learn(vector<string> args)
{
    size_t cs = args.size();
    size_t ci = 0;

    while (ci < cs)
    {
        string cmd = args[ci++];

        // ToDo...
    }
}

} // namespace rubichess

#endif
