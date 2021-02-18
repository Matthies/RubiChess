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

const U64 sfenchunksize = 0x4000;
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
    memset(occupied00, 0, sizeof(occupied00));
    psqval = 0;
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
    mstop = 0;
    rootheight = 0;
    lastnullmove = -1;
    accumulator->computationState = false;

    return 0;
}


void chessposition::toSfen(PackedSfen *sfen)
{
    // 1 + 6 + 6 + 32*1 + 32*5 + 4 + 7 + 7 + 8 = 
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


// convert rubi move code to SF/sfen code
inline uint16_t sfMoveCode(uint32_t c)
{
    uint16_t sfc = (uint16_t)(c & 0xfff);
    uint16_t p;
    if ((p = GETPROMOTION(c)))
        sfc |= ((p >> 1) + 2) << 12;
    else if (ISEPCAPTURE(c))
        sfc |= (2 << 14);
    else if (ISCASTLE(c))
        sfc |= (3 << 14);

    return sfc;
}


// convert sfen move code to Rubi short code
inline uint16_t shortCode(uint16_t c)
{
    uint16_t rc = c & 0xfff;
    if ((c & 0xc000) == 0x4000)
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
    cout << " Book has " << booklen << " entries.\n";
    return true;
}

static void freeBookPositions()
{
    while (booklen)
        delete bookfen[--booklen];

    freealigned64(bookfen);
}


static void gensfenthread(searchthread* thr)
{
    ranctx rnd;
    U64 key;
    U64 hash_index;
    U64 key2;
    U64 rndseed = time(NULL);
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


        for (int ply = 0; ; ++ply)
        {
            if (ply > maxply) // default: 200; SF: 400
            {
                if (generate_draw) flush_psv(0, thr);
                break;
            }
            pos->ply = 0;
            movelist.length = CreateMovelist<ALL>(pos, &movelist.move[0]);
                
            if (movelist.length == 0)
            {
                if (pos->isCheckbb) // Mate
                    flush_psv(-S2MSIGN(pos->state & S2MMASK), thr);
                else if (generate_draw)
                    flush_psv(0, thr); // Stalemate
                break;
            }
            
            int nextdepth = depth + ranval(&rnd) % depthvariance;
            //cout << "ply=" << ply << "   depth=" << nextdepth << "  :  ";

            int score = pos->alphabeta<NoPrune>(SCOREBLACKWINS, SCOREWHITEWINS, nextdepth);

            if (POPCOUNT(pos->occupied00[0] | pos->occupied00[1]) <= pos->useTb) // TB adjudication
            {
                flush_psv((score > SCOREDRAW) ? S2MSIGN(pos->state & S2MMASK) : (score < SCOREDRAW ? -S2MSIGN(pos->state & S2MMASK) : 0), thr);
                break;
            }

            if (abs(score) >= eval_limit) // win adjudication; default: 32000
            {
                flush_psv((score >= eval_limit) ? S2MSIGN(pos->state & S2MMASK) : -S2MSIGN(pos->state & S2MMASK), thr);
                break;
            }

            if (score == SCOREDRAW && (pos->halfmovescounter>= 100 || pos->testRepetiton() >= 2))
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
            psvnums++;

            if (psvnums % sfenchunksize == 0)
            {
                int thischunk = (int)(psvnums / sfenchunksize - 1);
                int nextchunk = (thischunk + 1) % sfenchunknums;
                while (thr->chunkstate[nextchunk] != CHUNKFREE)
                    Sleep(10);
                thr->chunkstate[nextchunk] = CHUNKINUSE;
                psvnums = psvnums % (sfenchunknums * sfenchunksize);
            }
            
SKIP_SAVE:
            // preset move for next ply with the pv move
            nmc = pos->pvtable[pos->ply][0];
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
                        //cout << "normal random\n";
                        i = ranval(&rnd) % movelist.length;
                        nmc = movelist.move[i].code;
                    }
                    else
                    {
                        pos->getRootMoves();
                        int cur_multi_pv = min(pos->rootmovelist.length, (ply < random_opening_ply ? 8 : random_multi_pv));
                        int cur_multi_pv_diff = (ply < random_opening_ply ? 100 : random_multi_pv_diff);
                        //cout << "multi-pv with depth=" << random_multi_pv_depth << " num=" << cur_multi_pv << " and diff=" << cur_multi_pv_diff << "\n";
                        // random multi pv
                        pos->rootsearch<MultiPVSearch>(SCOREBLACKWINS, SCOREWHITEWINS, random_multi_pv_depth, 1, cur_multi_pv);
                        int s = min(pos->rootmovelist.length, cur_multi_pv);
                        // Exclude moves with score outside of allowed margin
                        while (cur_multi_pv_diff && pos->bestmovescore[0] > pos->bestmovescore[s - 1] + cur_multi_pv_diff)
                            s--;

                        nmc = pos->multipvtable[ranval(&rnd) % s][0];
                    }
                }
                else {
                    //cout << "best move\n";
                }

                bool legal = pos->playMove(nmc);

                if (legal)
                {
                    if (pos->halfmovescounter == 0)
                        pos->mstop = 0;
                    break;
                }
                else {
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
}


void gensfen(vector<string> args)
{
    U64 fensnum = 10000;
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
            fensnum = stoi(args[ci++]);
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
        if (cmd == "book" && ci < cs)
            book = args[ci++];
    }

    int tnum;
    gensfenstop = false;

    cout << "Generating sfnes with these parameters:\n";
    cout << "output_file_name:      " << outputfile << "\n";
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
    cout << "book:                  " << book << "\n";

    const U64 chunksneeded = fensnum / sfenchunksize + 1;
    ofstream os(outputfile, ios::binary | fstream::app);
    if (!os)
    {
        cout << "Cannot open file " << outputfile << "\n";
        return;
    }

    if (!getBookPositions())
        return;

    for (tnum = 0; tnum < en.Threads; tnum++)
    {
        en.sthread[tnum].index = tnum;
        en.sthread[tnum].chunkstate[0] = CHUNKINUSE;
        en.sthread[tnum].chunkstate[1] = CHUNKFREE;
        en.sthread[tnum].psvbuffer = (PackedSfenValue*)allocalign64(sfenchunknums * sfenchunksize * sizeof(PackedSfenValue));
        en.sthread[tnum].thr = thread(&gensfenthread, &en.sthread[tnum]);
    }

    U64 chunkswritten = 0;
    tnum = 0;
    while (chunkswritten < chunksneeded)
    {
        searchthread* thr = &en.sthread[tnum];
        Sleep(10);
        for (int i = 0; i < sfenchunknums; i++)
        {
            if (thr->chunkstate[i] == CHUNKFULL)
            {
                os.write((char*)(thr->psvbuffer + i * sfenchunksize), sfenchunksize * sizeof(PackedSfenValue));
                chunkswritten++;
                thr->chunkstate[i] = CHUNKFREE;
                cout << chunkswritten * sfenchunksize << " sfens written. (" << thr->index << ")\n";
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
    cout << "gensfen finished.\n";
    en.MultiPV = old_multipv;

    freeBookPositions();
    for (tnum = 0; tnum < en.Threads; tnum++)
        freealigned64(en.sthread[tnum].psvbuffer);
}



enum SfenFormat { no, bin, plain };

void convert(vector<string> args)
{
    string inputfile;
    string outputfile;
    int evalstats = 0;
    SfenFormat outformat = no;
    size_t cs = args.size();
    size_t ci = 0;
    U64 evalsum[2][7] = { 0 };  // evalsum for hce and NNUE classified by at  0 / <0.5 / <1 / <3 / <20 / 1-4 / different prefer
    U64 evaln[7] = { 0 };

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
            outformat = (args[ci] == "bin" ? bin : args[ci] == "plain" ? plain : no);
            ci++;
        }
        if (cmd == "evalstats" && ci < cs)
        {
            evalstats = (NnueReady != NnueDisabled) * stoi(args[ci++]);
            if (evalstats < 1 || evalstats > 2)
                evalstats = 0;
        }
    }

    ifstream is(inputfile, ios::binary);
    if (!is)
    {
        cout << "Cannot open input file " << inputfile << ".\n";
        return;
    }

    ostream *os = &cout;
    ofstream ofs;
    if (outputfile != "")
    {
        ofs.open(outputfile, ios::binary);
        if (!ofs)
        {
            cout << "Cannot open output file.\n";
            return;
        }
        os = &ofs;
    }

    chessposition* pos = &en.sthread[0].pos;
    PackedSfenValue* psvbuffer = (PackedSfenValue*)allocalign64(sfenchunknums * sfenchunksize * sizeof(PackedSfenValue));

    U64 n = 0;
    while (is.peek() != ios::traits_type::eof())
    {
        is.read((char*)psvbuffer, sfenchunknums * sfenchunksize * sizeof(PackedSfenValue));
        U64 psvread = is.gcount() / sizeof(PackedSfenValue);

        for (PackedSfenValue* psv = psvbuffer; psv < psvbuffer + psvread; psv++)
        {
            pos->getFromSfen(&psv->sfen);
            chessmove cm;
            cm.code = pos->shortMove2FullMove(shortCode(psv->move));
            if (!cm.code || ISEPCAPTUREORCASTLE(cm.code))
            {
                // Fix the incompatible move coding
                cm.code = pos->shortMove2FullMove(psv->move);
                if (cm.code)
                    psv->move = sfMoveCode(cm.code);
            }

            if (!cm.code)
                continue;

            n++;

            if (outformat == plain)
            {
                *os << "fen " << pos->toFen() << "\n";
                *os << "move " << cm.toString() << "\n";
                *os << "score " << psv->score << "\n";
                *os << "ply " << to_string((pos->fullmovescounter - 1) * 2 + (pos->state & S2MMASK)) << "\n";
                *os << "result " << to_string(psv->game_result) << "\n";
                *os << "e\n";
            }
            else if (outformat == bin)
            {
                os->write((char*)psv, sizeof(PackedSfenValue));
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
    }

    cerr << "\nFinished converting.\n";
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
