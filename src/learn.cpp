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

#ifdef NNUELEARN

//
// Generate fens for training
//

#define GENSFEN_HASH_SIZE 0x1000000
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
#define HUFFMAN2RUBI(x) (((x) >> 3) | ((((x) & 7) + 1) * 2))

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

    isCheckbb = isAttackedBy<OCCUPIED>(kingpos[state & S2MMASK], (state & S2MMASK) ^ S2MMASK);
    kingPinned = 0ULL;
    updatePins<WHITE>();
    updatePins<BLACK>();

    hash = zb.getHash(this);
    pawnhash = zb.getPawnHash(this);
    materialhash = zb.getMaterialHash(this);
    lastnullmove = -1;
    ply = 0;
    accumulator[0].computationState[WHITE] = false;
    accumulator[0].computationState[BLACK] = false;

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
}


/*
    Summary about different 16bit move encoding:
    Rubi:
        ppppfffffftttttt
        pppp                Promotion piece code (e.g. 11 = 1011 for promotion to black queen)
            ffffff          from square
                  tttttt    to square
    Castle moves are encoded 'king captures rook'
    Ep captures are detected by 'diagonal pawn move with empty target square'

    SF/sfen/binpack:
        ttppfffffftttttt
        tt                  type (00=normal, 01=promotion, 10=ep capture, 11=castle)
          pp                promotion piece (00=knight, 01=bishop, 10=rook, 11=queen)
            ffffff          from square
                  tttttt    to square
    Castle moves are encoded 'king moves to its target' (true for FRC?)
*/

// get SF/sfen code from a long Rubi move code
inline uint16_t sfMoveCode(uint32_t c)
{
    uint16_t sfc = (uint16_t)(c & 0xfff);
    uint16_t p;
    if ((p = GETPROMOTION(c)))
        sfc |= ((p >> 1) + 2) << 12;
    else if (ISEPCAPTURE(c))
        sfc |= (2 << 14);
    else if (ISCASTLE(c))
    {
        sfc = (sfc & 0xffc0) | GETCORRECTTO(sfc);
        sfc |= (3 << 14);
    }
    return sfc;
}


// get Rubi short move code from a SF/sfen code
inline uint16_t rubiMoveCode(uint16_t c)
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

    return rc;
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


#define SHORTFROMBIGENDIAN(c) ((uint8_t)(c)[1] | ((uint8_t)(c)[0] << 8))
#define LONGLONGFROMBIGENDIAN(c) ((U64)((uint8_t)(c)[7]) | ((U64)((uint8_t)(c)[6]) << 8) | ((U64)((uint8_t)(c)[5]) << 16) | (((U64)((uint8_t)(c)[4])) << 24ULL) | ((U64)((uint8_t)(c)[3]) << 32) | ((U64)((uint8_t)(c)[2]) << 40) | ((U64)((uint8_t)(c)[1]) << 48) | ((U64)((uint8_t)(c)[0]) << 56))

void drawBytes(char *b, int n)
{
#if 0
    for (int i = 0; i < n; i++)
        printf("%02x ", *(uint8_t*)(b + i));
    printf("\n");
#endif
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

inline int indexBits(U64 c) {
    if (c <= 1)  return 0;
    int n;
    GETMSB(n, c - 1);
    return n + 1;
}

// bitnum <= 8
inline uint8_t getNextBits(Binpack *bp, int bitnum)
{
    uint8_t byte = *(uint8_t*)*bp->data;
    byte = byte << bp->consumedBits;
    byte = byte >> (8 - bitnum);
    bp->consumedBits += bitnum;

    if (bp->consumedBits >= 8)
    {
        int bitsleft = bp->consumedBits - 8;
        uint8_t byte2 = *(uint8_t*)(*bp->data + 1) >> (8 - bitsleft);
        byte |= byte2;
        bp->consumedBits -= 8;
        (*bp->data)++;
    }
    //printf("getNextBits(%d): %02x  aktuelles Byte: %02x(%d)\n", bitnum, byte, *(uint8_t*)*bp->data, bp->consumedBits);
    return byte;
}

inline uint16_t getNextBlocks(Binpack* bp, int blocksize)
{
    uint8_t mask = BITSET(blocksize) - 1;
    uint16_t val = 0;
    int bitnum = 0;
    while (1)
    {
        uint8_t byte = getNextBits(bp, blocksize + 1);
        val |= (byte & mask) << bitnum;
        if (!(byte >> blocksize))
            break;
        bitnum += blocksize;
    }
    return val;
}

inline int getNthBitIndex(U64 occ,unsigned int n)
{
    // FIXME: make this faster
    int index;
    for (unsigned int i = 0; i <= n; i++)
        index = pullLsb(&occ);
    return index;
}

void chessposition::getPosFromBinpack(Binpack* bp)
{
    drawBytes(*bp->data, 8);
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
            drawBytes(*bp->data, 1);
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

    drawBytes(*bp->data, 16 - bytesConsumed);
    drawBytes(*bp->data + 16 - bytesConsumed, 16);
    (*bp->data) += 16 - bytesConsumed;

}


int chessposition::getFromBinpack(Binpack *bp)
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
        // get the pieces
        getPosFromBinpack(bp);
        // get the move
        uint16_t bpmc = SHORTFROMBIGENDIAN(*bp->data);
        *bp->data += sizeof(uint16_t);
        uint16_t type = (bpmc >> 14);
        uint16_t from = (bpmc >> 8) & 0x3f;
        uint16_t to = (bpmc >> 2) & 0x3f;
        uint16_t shortmc = (type << 14) | (from << 6) | to;
        if (type == 1)
            // promotion
            shortmc |= ((bpmc & 0x3) << 12);
        bp->move = shortmc;
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
        playMove(bp->fullmove);
        int Me = state & S2MMASK;
        if (ept)
        {
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
                bp->move = ((4 + (toId % 4)) << 12) | (from << 6) | to;
            }
            else {
                bitnum = indexBits(targetnum);
                toId = getNextBits(bp, bitnum);
                to = getNthBitIndex(targetbb, toId);
                bp->move = (ept && ept == to ? (2 << 14) : 0) | (from << 6) | to;
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
                // castle move; Rubi codes it 'king captures rook'
                to = castlerookfrom[getNthBitIndex(mycastlemask, toId - targetnum) - 1];
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
        int16_t scorediff = toSigned(getNextBlocks(bp, 4));
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

    bp->fullmove = shortMove2FullMove(rubiMoveCode(bp->move));
    if (!bp->fullmove) {
        printf("Illegal move %04x. Something wrong here. Exit...\n", bp->move);
        print();
        return -1;
    }

    return 0;
}


// global parameters for gensfen variation
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
    pos->he_yes = 0ULL;
    pos->he_all = 0ULL;
    pos->he_threshold = 8100;
    memset(pos->history, 0, sizeof(chessposition::history));
    memset(pos->counterhistory, 0, sizeof(chessposition::counterhistory));
    memset(pos->countermove, 0, sizeof(chessposition::countermove));
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
                pos->alphabeta<NoPrune>(SCOREBLACKWINS, SCOREWHITEWINS, nextdepth)
                : pos->alphabeta<Prune>(SCOREBLACKWINS, SCOREWHITEWINS, nextdepth));

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
            thr->psv->move = (uint16_t)sfMoveCode(pos->pvtable[0][0]);
            thr->psv->game_result = 2 * S2MSIGN(pos->state & S2MMASK); // not yet known

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

                bool legal = pos->playMove(nmc);

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

    const unsigned int chunksneeded = (unsigned int)(loop / sfenchunksize) + 1;
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



enum SfenFormat { no, bin, binpack, plain };

void convert(vector<string> args)
{
    string inputfile;
    string outputfile;
    int evalstats = 0;
    int generalstats = 0;
    SfenFormat outformat = no;
    SfenFormat informat = no;
    size_t cs = args.size();
    size_t ci = 0;
    //U64 evalsum[2][7] = { 0 };  // evalsum for hce and NNUE classified by at  0 / <0.5 / <1 / <3 / <20 / 1-4 / different prefer
    //U64 evaln[7] = { 0 };

    while (ci < cs)
    {
        string cmd = args[ci++];

        if (cmd == "input_file_name" && ci < cs)
        {
            inputfile = args[ci++];
            inputfile.erase(remove(inputfile.begin(), inputfile.end(), '\"'), inputfile.end());
        }
        if (cmd == "output_file_name" && ci < cs)
        {
            outputfile = args[ci++];
            outputfile.erase(remove(outputfile.begin(), outputfile.end(), '\"'), outputfile.end());
        }
        if (cmd == "output_format" && ci < cs)
        {
            outformat = (args[ci] == "bin" ? bin : args[ci] == "plain" ? plain : args[ci] == "binpack" ? binpack : no);
            ci++;
        }
        if (cmd == "evalstats" && ci < cs)
        {
            evalstats = (NnueReady != NnueDisabled) * stoi(args[ci++]);
            if (evalstats < 1 || evalstats > 2)
                evalstats = 0;
        }
        if (cmd == "generalstats")
        {
            generalstats = (ci < cs ? stoi(args[ci++]) : 1);
        }
    }

    ifstream is(inputfile, ios::binary);
    if (!is)
    {
        cout << "Cannot open input file " << inputfile << ".\n";
        return;
    }

    informat = (inputfile.find(".binpack") != string::npos ? binpack : inputfile.find(".bin") != string::npos ? bin : plain);

    ostream *os = &cout;
    ofstream ofs;
    if (outputfile != "")
    {
        if (outformat == no)
            outformat = (outputfile.find(".binpack") != string::npos ? binpack : outputfile.find(".bin") != string::npos ? bin : plain);
        ofs.open(outputfile, outformat == plain ? ios::out : ios::binary);
        if (!ofs)
        {
            cout << "Cannot open output file.\n";
            return;
        }
        os = &ofs;
    }

    chessposition* pos = &en.sthread[0].pos;
    char* buffer = nullptr;
    size_t buffersize;
    //size_t buffernum;
    //size_t bufferreserve;
    //size_t currentbuffer;
    //uint32_t chunksize;
    Binpack bp;

    if (informat == bin)
    {
        buffersize = sfenchunknums * sfenchunksize * sizeof(PackedSfenValue);
        //bufferreserve = 0;
    }
    else if (informat == binpack)
    {
        buffersize = 0;
        //bufferreserve = 64;
    } else  // informat == plain
    {
        buffersize = 1024 * 1024;
    }

    U64 n = 0;
    //int rookfiles[2] = { -1, -1 };
    //int kingfile = -1;
    //int lastfullmove = INT_MAX;

    U64 posWithPieces[32] = { 0ULL };
    bool okay = true;
    size_t nextbuffersize = buffersize;

    while (okay && is.peek() != ios::traits_type::eof())
    {
        int16_t score;
        int8_t result;
        uint16_t move;
        uint16_t gameply;

        if (informat == binpack)
        {
            char hd[8];
            is.read(hd, 8);
            if (strncmp(hd, "BINP", 4) != 0)
            {
                cout << "BINP Header missing. Exit.\n";
                return;
            }
            nextbuffersize = *(uint32_t*)&hd[4];
        }

        if (nextbuffersize != buffersize)
        {
            if (buffer)
            {
                freealigned64(buffer);
                buffer = nullptr;
            }
            buffersize = nextbuffersize;
        }

        if (!buffer)
        {
            buffer = (char*)allocalign64(buffersize);
        }

        char* bptr = buffer;
        size_t restdata = buffersize;
        is.read((char*)buffer, buffersize);
        if (!is)
            restdata = is.gcount();

        while (okay && bptr < buffer + restdata)
        {
            if (informat == bin)
            {
                PackedSfenValue* psv = (PackedSfenValue*)bptr;
                pos->getFromSfen(&psv->sfen);
                score = psv->score;
                result = psv->game_result;
                move = psv->move;
                gameply = psv->gamePly;
                bptr += sizeof(PackedSfenValue);
            }
            else if (informat == binpack)
            {
                if (!bp.data)
                {
                    bp.data = &bptr;
                    bp.consumedBits = 0;
                }
                okay = (pos->getFromBinpack(&bp) == 0);
                score = bp.score;
                result = bp.gameResult;
                move = bp.move;
                gameply = bp.gamePly;
            }
            else // informat == plain
            {
                score = NOSCORE;
                result = 0;
                move = 0;
                gameply = 0;
            }

            uint32_t rubimovecode = pos->shortMove2FullMove(rubiMoveCode(move));
            uint32_t sfmovecode = rubimovecode;
            if (!rubimovecode || ISEPCAPTUREORCASTLE(rubimovecode))
            {
                // Fix the incompatible move coding
                sfmovecode = pos->shortMove2FullMove(move & 0xfff);
                if (sfmovecode)
                    move = sfMoveCode(sfmovecode);
                if (!rubimovecode)
                {
                    // FIXME: Can this still happen??
                    printf("Alarm. Movecode: %04x: %s\n", move, pos->toFen().c_str());
                    rubimovecode = pos->shortMove2FullMove(move);
                }
            }



            if (outformat == plain)
            {
                *os << "fen " << pos->toFen() << endl;
                *os << "move " << moveToString(rubimovecode) << endl;
                *os << "score " << score << endl;
                *os << "ply " << to_string(gameply) << endl;
                *os << "result " << to_string(result) << endl;
                *os << "e" << endl;;
            }
            else if (outformat == bin)
            {
                PackedSfenValue psv;
                psv.score = score;
                psv.game_result = result;
                psv.gamePly = gameply;
                psv.move = move; // FIXME: revert to SF format needed
                pos->toSfen(&psv.sfen);
                os->write((char*)&psv, sizeof(PackedSfenValue));
            }

            n++;

        }

#if 0
        U64 psvread = is.gcount() / sizeof(PackedSfenValue);
        for (PackedSfenValue* psv = (PackedSfenValue*) buffer; psv < buffer + psvread; psv++)
        {
            pos->getFromSfen(&psv->sfen);
            // Test and reset the castle related mebers in case of chess960
            // FIXME: This is not bulletproof; a rook could have walked in the plies not recorded and lead to wrong castle rights
            int castlestate = pos->state & CASTLEMASK;
            if (castlestate && pos->fullmovescounter < lastfullmove)
            {
                int whiteCanCastle = castlestate & (WQCMASK | WKCMASK);
                int castleQueenside = castlestate & (WQCMASK | BQCMASK);
                int castleKingside = castlestate & (WKCMASK | BKCMASK);
                kingfile = (whiteCanCastle ? FILE(pos->kingpos[WHITE]) : FILE(pos->kingpos[BLACK]));

                rookfiles[0] = -1;
                if (castleQueenside)
                {
                    U64 rooksbb;
                    if (whiteCanCastle & castleQueenside)
                        rooksbb = (pos->piece00[WROOK] & RANK1(WHITE));
                    else
                        rooksbb = (pos->piece00[BROOK] & RANK1(BLACK));
                    rookfiles[0] = FILE(pullLsb(&rooksbb));
                }

                rookfiles[1] = -1;
                if (castleKingside)
                {
                    U64 rooksbb;
                    if (whiteCanCastle & castleKingside)
                        rooksbb = (pos->piece00[WROOK] & RANK1(WHITE));
                    else
                        rooksbb = (pos->piece00[BROOK] & RANK1(BLACK));
                    rookfiles[1] = FILE(pullMsb(&rooksbb));
                }
                pos->initCastleRights(rookfiles, kingfile);
            }

            lastfullmove = pos->fullmovescounter;

            uint32_t rubimovecode = pos->shortMove2FullMove(shortCode(psv->move));
            uint32_t sfmovecode = rubimovecode;
            if (!rubimovecode || ISEPCAPTUREORCASTLE(rubimovecode))
            {
                // Fix the incompatible move coding
                sfmovecode = pos->shortMove2FullMove(psv->move);
                if (sfmovecode)
                    psv->move = sfMoveCode(sfmovecode);
                if (!rubimovecode)
                {
                    // FIXME: Can this still happen??
                    printf("Alarm. Movecode: %04x: %s\n", psv->move, pos->toFen().c_str());
                    rubimovecode = pos->shortMove2FullMove(psv->move);
                }
            }

            if (!sfmovecode)
                continue;

            n++;

            if (outformat == plain)
            {
                *os << "fen " << pos->toFen() << "\n";
                *os << "move " << moveToString(rubimovecode) << "\n";
                *os << "score " << psv->score << "\n";
                *os << "ply " << to_string((pos->fullmovescounter - 1) * 2 + (pos->state & S2MMASK)) << "\n";
                *os << "result " << to_string(psv->game_result) << "\n";
                *os << "e\n";
            }
            else if (outformat == bin)
            {
                os->write((char*)psv, sizeof(PackedSfenValue));
            }

            if (generalstats)
            {
                posWithPieces[POPCOUNT(pos->occupied00[WHITE] | pos->occupied00[BLACK])]++;
            }

            if (evalstats)
            {
                NnueType origNnue = NnueReady;
                NnueReady = NnueDisabled;
                int ev[2];
                int absev[2];
                ev[evalstats - 1] = pos->getEval<NOTRACE>();
                absev[evalstats - 1] = abs(ev[evalstats - 1]);
                NnueReady = origNnue;
                ev[2 - evalstats] = pos->getEval<NOTRACE>();
                absev[2 - evalstats] = abs(ev[2 - evalstats]);
                int i = (ev[0] == 0 ? 0 : (absev[0] < 50 ? 1 : (absev[0] < 100 ? 2 : (absev[0] < 300 ? 3 : (absev[0] > 2000 ? -1 : 4)))));
                if (ev[0] * ev[1] < 0)
                    i = 6;

                if (i < 0) break;

                evalsum[0][i] += absev[0];
                evalsum[1][i] += absev[1];
                evaln[i]++;
                if (i >= 1 && i <= 4)
                {
                    evalsum[0][5] += absev[0];
                    evalsum[1][5] += absev[1];
                    evaln[5]++;
                }

                if (n % 10000 == 0)
                {
                    cerr << "======================================================================\n";
                    for (i = 0; i < 7; i++)
                    {
                        double avh = evalsum[0][i] / (double)evaln[i];
                        double avn = evalsum[1][i] / (double)evaln[i];
                        cerr << setprecision(3) << fixed << i << setw(8) << evaln[i] << setw(14) << avh << setw(14) << avn << setw(14) << avh / avn << "\n";
                    }
                }

            }
        }
        if (!evalstats) cerr << ".";
#endif
    }

    if (!okay)
        cerr << "\nAn error occured while reading input data.\n";
    cerr << "\nFinished converting. " << n << " positions found.\n";

    if (generalstats && n > 0)
    {
        cerr << "Distribution depending on number of pieces:\n";
        for (int i = 2; i < 33; i++)
            cerr << setw(2) << to_string(i) << " pieces: " << 100.0 * posWithPieces[i] / (double)n << "%\n";
    }
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

#endif
