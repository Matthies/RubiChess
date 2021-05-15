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
// Get files in folder
//
static size_t getBinFilesInFolder(vector<string> *filenames, string sFolder)
{
#ifdef _WIN32

    vector<string> names;
    string search_path = sFolder + "/*.bin";
    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(search_path.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            filenames->push_back(sFolder + "\\" + fd.cFileName);
    } while (FindNextFile(hFind, &fd));
    FindClose(hFind);
    return filenames->size();
#endif
}

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
    psqval = 0;
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
    ply = 0;
    rootheight = 0;
    lastnullmove = -1;
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



enum SfenFormat { no, bin, plain };

void convert(vector<string> args)
{
    string inputfile;
    string outputfile;
    int evalstats = 0;
    int generalstats = 0;
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
    int rookfiles[2] = { -1, -1 };
    int kingfile = -1;
    int lastfullmove = INT_MAX;

    U64 posWithPieces[32] = { 0ULL };

    while (is.peek() != ios::traits_type::eof())
    {
        is.read((char*)psvbuffer, sfenchunknums * sfenchunksize * sizeof(PackedSfenValue));
        U64 psvread = is.gcount() / sizeof(PackedSfenValue);

        for (PackedSfenValue* psv = psvbuffer; psv < psvbuffer + psvread; psv++)
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
    }

    cerr << "\nFinished converting.\n";

    if (generalstats)
    {
        cerr << "Distribution depending on number of pieces:\n";
        for (int i = 2; i < 33; i++)
            cerr << setw(2) << to_string(i) << " pieces: " << 100.0 * posWithPieces[i] / (double)n << "%\n";
    }
}




//
// Here starts the difficult part: The trainer
// 

// Interface to the Network in nnue.cpp
extern NnueInputSlice *NnueIn;
extern NnueClippedRelu *NnueCl1, *NnueCl2;
extern NnueNetworkLayer *NnueOut, *NnueHd1, *NnueHd2;
extern NnueFeatureTransformer *NnueFt;

const int weightScaleBits = 6;
const float ponanzaConstant = 600.0; //WTF??
const float FV_SCALE = 16;          // FIXME: This is the SF scaling factor; needs to be merged with NnueValueScale

// Classes for the different parts of the trainer network
class TrainerLayer
{
public:
    TrainerLayer* previous;
    NnueLayer* target;
    float activationScale;
    float biasScale;
    float weightScale;

    TrainerLayer(TrainerLayer* prev, NnueLayer* tar) { previous = prev; target = tar; }
};

// Trainer network using floats
class TrainerNetworkLayer : public TrainerLayer
{
public:
    unsigned int inputdims;
    unsigned int outputdims;

    float* bias;
    float* bias_diff;
    float* weight;
    float* weight_diff;
    TrainerNetworkLayer(TrainerLayer* prev, NnueLayer* tar, int id, int od);
    ~TrainerNetworkLayer();
};

TrainerNetworkLayer::TrainerNetworkLayer(TrainerLayer* prev, NnueLayer* tar, int id, int od) : TrainerLayer(prev, tar)
{
    inputdims = id;
    outputdims = od;
    size_t allocsize = od * sizeof(float);
    bias = (float*)allocalign64(allocsize);
    bias_diff = (float*)allocalign64(allocsize);
    allocsize = (size_t)id * (size_t)od * sizeof(float);
    weight = (float*)allocalign64(allocsize);
    weight_diff = (float*)allocalign64(allocsize);

    activationScale = std::numeric_limits<std::int8_t>::max();
    biasScale = (outputdims > 1 ? (1 << weightScaleBits) * activationScale : ponanzaConstant * FV_SCALE);
    weightScale = biasScale / activationScale;

    NnueNetworkLayer* Nl = (NnueNetworkLayer*)target;
    for (int i = 0; i < od; i++)
    {
        bias[i] = Nl->bias[i] / biasScale;
        bias_diff[i] = 0.0;
        int offset = i * Nl->outputdims;
        for (int j = 0; j < id; j++)
        {
            weight[offset + j] = Nl->weight[i] / weightScale;
            weight_diff[offset + j] = 0.0;
        }
    }
}

TrainerNetworkLayer::~TrainerNetworkLayer()
{
    freealigned64(bias);
    freealigned64(bias_diff);
    freealigned64(weight);
    freealigned64(weight_diff);
}


class TrainerInputSlice : public TrainerLayer
{
public:
    const int outputdims = 512;

    TrainerInputSlice();
};

TrainerInputSlice::TrainerInputSlice() : TrainerLayer(NULL, NULL)
{

}

class TrainerClippedRelu : public TrainerLayer
{
public:
    int dims;
    float* min_activations;
    float* max_activations;
    TrainerClippedRelu(TrainerLayer* prev, NnueLayer* tar, int d);
    ~TrainerClippedRelu();
};

TrainerClippedRelu::TrainerClippedRelu(TrainerLayer* prev, NnueLayer *tar, int d) : TrainerLayer(prev, tar)
{
    dims = d;
    size_t allocsize = d * sizeof(float);
    min_activations = (float*)allocalign64(allocsize);
    max_activations = (float*)allocalign64(allocsize);
    for (int i = 0; i < d; i++)
    {
        min_activations[i] = std::numeric_limits<float>::max();
        max_activations[i] = std::numeric_limits<float>::lowest();
    }
}

TrainerClippedRelu::~TrainerClippedRelu()
{
    freealigned64(min_activations);
    freealigned64(max_activations);
}


// Trainer FeatureTransformer
class TrainerFeatureTransformer : public TrainerLayer
{
public:
    float* bias;
    float* bias_diff;
    float* weight;
    TrainerFeatureTransformer(NnueLayer *tar);
    ~TrainerFeatureTransformer();
};


TrainerFeatureTransformer::TrainerFeatureTransformer(NnueLayer* tar) : TrainerLayer(NULL, tar)
{
    size_t allocsize = NnueFtHalfdims * sizeof(float);
    bias = (float*)allocalign64(allocsize);
    bias_diff = (float*)allocalign64(allocsize);
    allocsize = (size_t)NnueFtHalfdims * (size_t)NnueFtInputdims * sizeof(float);
    weight = (float*)allocalign64(allocsize);

    activationScale = std::numeric_limits<std::int8_t>::max();
    biasScale = activationScale;
    weightScale = activationScale;

    NnueFeatureTransformer* Ft = (NnueFeatureTransformer*)target;
    for (int i = 0; i < NnueFtHalfdims; i++)
    {
        bias[i] = Ft->bias[i] / biasScale;
        bias_diff[i] = 0.0;
    }
    for (int i = 0; i < NnueFtHalfdims * NnueFtInputdims; i++)
        weight[i] = Ft->weight[i] / weightScale;
}

TrainerFeatureTransformer::~TrainerFeatureTransformer()
{
    freealigned64(bias);
    freealigned64(bias_diff);
    freealigned64(weight);
}


TrainerInputSlice *TrainerIn;
TrainerClippedRelu *TrainerCl1, *TrainerCl2;
TrainerNetworkLayer *TrainerOut, *TrainerHd1, *TrainerHd2;
TrainerFeatureTransformer *TrainerFt;

void TrainerInit()
{
    TrainerFt = new TrainerFeatureTransformer(NnueFt);
    TrainerIn = new TrainerInputSlice();
    TrainerHd1 = new TrainerNetworkLayer(TrainerIn, NnueHd1, 512, 32);
    TrainerCl1 = new TrainerClippedRelu(TrainerHd1, NnueCl1, 32);
    TrainerHd2 = new TrainerNetworkLayer(TrainerCl1, NnueHd2, 32, 32);
    TrainerCl2 = new TrainerClippedRelu(TrainerHd2, NnueCl2, 32);
    TrainerOut = new TrainerNetworkLayer(TrainerCl2, NnueOut, 32, 1);
}



// global parameters for training
//learn targetdir C : \Entwicklung\nnue\training loop 100 batchsize 1000000 use_draw_in_training 1 use_draw_in_validation 1
//   eta 1.0 lambda 0.5 eval_limit 32000 nn_batch_size 1000 newbob_decay 0.5 newbob_num_trials 6
//   eval_save_interval 10000000 loss_output_interval 10000000 mirror_percentage 0 validation_set_file_name C : \Entwicklung\nnue\gensfen - 12 - 26.03.2021 - 5.bin
string training_dir;
string validation_set_file_name;
int loop = 1;
const bool save_only_once = false;
const bool no_shuffle = false;
int mini_batch_size = 1000000;
uint64_t nn_batch_size = 1000;
const string nn_options = "";
bool use_draw_in_training = false;
bool use_draw_in_validation = false;
double eta1 = 0.0;
const double eta2 = 0.0;
const double eta3 = 0.0;
const uint64_t eta1_epoch = 0; // eta2 is not applied by default
const uint64_t eta2_epoch = 0; // eta3 is not applied by default
double newbob_decay = 1.0;
double newbob_scale = 1.0;
int newbob_num_trials = 2;
const double discount_rate = 0.0;
const int reduction_gameply = 1;
double elmo_lambda = 0.33;
const double elmo_lambda2 = 0.33;
const int elmo_lambda_limit = 32000;
uint64_t mirror_percentage = 0;

uint64_t eval_save_interval = 1000000000ULL;
uint64_t loss_output_interval = 0;

vector<string> filenames;
thread thrFilereader;


struct SfenReader
{
    static const uint64_t READ_SFEN_HASH_SIZE = 64 * 1024 * 1024;
    static const int SFEN_CHUNKS = 1000;
    static const size_t SFEN_READ_SIZE = 10 * 1000 * 1000;
    static const size_t SFEN_THREAD_SIZE = SFEN_READ_SIZE / SFEN_CHUNKS;
    vector<U64> hash;
    ifstream fd;
    PackedSfenValue* buffer;
    vector<PackedSfenValue> sfen_for_mse;
    int nextFree;
    int nextAvail;
    mutex requestmutex;
    int chunkRequestByThread;
    atomic<int> chunkNumForThread;




};

SfenReader sr;
float global_LR;


void readSfens()
{
    ranctx rnd;
    raninit(&rnd, getTime());

    vector<string>myFilenames;
    sr.buffer =  (PackedSfenValue*)allocalign64(sr.SFEN_READ_SIZE * sizeof(PackedSfenValue));
    sr.nextFree = 0;
    sr.nextAvail = sr.SFEN_CHUNKS - 1;
    sr.chunkRequestByThread = -1;

    while (1)
    {
        // Test if we can read SFEN_CHUNKS / 2 chunks to the buffer
        if ((sr.nextFree > sr.nextAvail && sr.nextFree + sr.SFEN_CHUNKS / 2 > sr.nextAvail)
            || (sr.nextFree <= sr.nextAvail && sr.nextFree + sr.SFEN_CHUNKS / 2 <= sr.nextAvail))
        {
            // We can read SFEN_CHUNKS / 2 chunks to the buffer
            PackedSfenValue* psfptr = sr.buffer + sr.nextFree;
            PackedSfenValue* endptr = sr.buffer + sr.nextFree + sr.SFEN_CHUNKS / 2 * sr.SFEN_THREAD_SIZE;
            while (psfptr < endptr)
            {
                if (!myFilenames.size())
                    myFilenames = filenames;

                if (sr.fd.peek() == ios::traits_type::eof())
                    sr.fd.close();

                if (!sr.fd.is_open())
                {
                    string fs = *myFilenames.rbegin();
                    myFilenames.pop_back();
                    sr.fd.open(fs, ios::binary);
                    if (!sr.fd.is_open())
                    {
                        cout << "Error opening file " << fs << ". Exiting...\n";
                        return;
                    }
                }

                streamsize bytestoread = (endptr - psfptr) * sizeof(PackedSfenValue);
                sr.fd.read((char*)psfptr, bytestoread);
                streamsize count = sr.fd.gcount();
                psfptr += count / sizeof(PackedSfenValue);
            }

            U64 size = sr.SFEN_CHUNKS / 2 * sr.SFEN_THREAD_SIZE;
            for (size_t i = 0; i < size; ++i)
                swap(sr.buffer[sr.nextFree + i], sr.buffer[sr.nextFree + ranval(&rnd) % (size - i) + i]);

            sr.nextFree = (sr.nextFree + sr.SFEN_CHUNKS / 2) % sr.SFEN_CHUNKS;

        }

        if (sr.chunkRequestByThread >= 0)
        {
            // A thread wants the next avail chunk of sfens
            int nextAvail = (sr.nextAvail + 1) % sr.SFEN_CHUNKS;
            cout << "Reader: Next avail = " << nextAvail << "\n";
            if (nextAvail != sr.nextFree)
            {
                sr.nextAvail = nextAvail;
                sr.chunkNumForThread = nextAvail;
                // Wait for the requesting thread to clear
                while (sr.chunkNumForThread >= 0) Sleep(100);
            }
        }
        Sleep(100);
    }
}


bool readValidationSfens()
{
    sr.sfen_for_mse.clear();
    ifstream ifval;
    ifval.open(validation_set_file_name, ios::binary);
    if (!ifval.is_open())
    {
        cout << "Failed to read validation data from " << validation_set_file_name << "\n";
        return false;
    }

    while (ifval.peek() != ios::traits_type::eof())
    {
        PackedSfenValue p;
        ifval.read((char*)&p, sizeof(PackedSfenValue));
        sr.sfen_for_mse.push_back(p);
    }
    cout << "Found " << sr.sfen_for_mse.size() << " positions in validation set.\n";
    return true;
}


void calcLoss(searchthread* thr)
{
    chessposition* pos = &thr->pos;
    for (auto p = sr.sfen_for_mse.begin(); p != sr.sfen_for_mse.end(); p++)
    {
        //PackedSfenValue *p = (PackedSfenValue*)it;
        //p->score = 0;
        pos->getFromSfen(&p->sfen);
        if (pos->toFen() == "b2r1r1k/2p3bp/p2q4/2p1pp1n/2P5/PN1P1PP1/1BQ2RNP/2R3K1 b - - 4 24")
            cout << "debug qsearch\n";
        int roots2m = pos->state & S2MMASK;
        int score = pos->alphabeta<NoPrune>(SCOREBLACKWINS, SCOREWHITEWINS, 0);
        int pvi = 0;
        uint32_t mc;
        string spv = "";
        while (mc = pos->pvtable[0][pvi++])
        {
            pos->prepareStack();
            pos->playMove(mc);
            spv = spv + moveToString(mc) + " ";
        }
        pvi--;
        int rempvi = pvi;
        int shallow_value = pos->getEval<NOTRACE>();
        if (roots2m != (pos->state & S2MMASK))
            shallow_value = -shallow_value;
        while (pvi > 0)
            pos->unplayMove(pos->pvtable[0][--pvi]);
        cout << "pv has " << rempvi << " moves   " << pos->toFen() << "  PV: " << spv << "\n";

    }

}


void learnWorker(searchthread* thr)
{
    // Some sfen sync testing testing
    int i = thr->index;
    for (int j = 0; j < 100; j++)
    {
        sr.requestmutex.lock();
        sr.chunkRequestByThread = i % 16;
        while (sr.chunkNumForThread < 0) Sleep(10);
        int myChunk = sr.chunkNumForThread;
        cout << "thread#" << i << ":  Got chunk " << myChunk << "\n";
        sr.chunkNumForThread = -1;
        sr.chunkRequestByThread = -1;
        sr.requestmutex.unlock();
        Sleep(2000 + 100 * i);
    }
    cout << "Thread#" << i << " terminated.\n";
}


void learn(vector<string> args)
{
    size_t cs = args.size();
    size_t ci = 0;

    while (ci < cs)
    {
        string cmd = args[ci++];

        if (cmd == "training_dir" && ci < cs)
            training_dir = args[ci++];
        if (cmd == "validation_set_file_name" && ci < cs)
            validation_set_file_name = args[ci++];
        if (cmd == "loop" && ci < cs)
            loop = stoi(args[ci++]);
        if (cmd == "eval_limit" && ci < cs)
            eval_limit = stoi(args[ci++]);
        if (cmd == "mini_batch_size" && ci < cs)
            mini_batch_size = stoi(args[ci++]);
        if (cmd == "nn_batch_size" && ci < cs)
            nn_batch_size = stoi(args[ci++]);
        if (cmd == "use_draw_in_training" && ci < cs)
            use_draw_in_training = stoi(args[ci++]);
        if (cmd == "use_draw_in_validation" && ci < cs)
            use_draw_in_validation = stoi(args[ci++]);
        if (cmd == "eta" && ci < cs)
            eta1 = stof(args[ci++]);
        if (cmd == "newbob_decay" && ci < cs)
            newbob_decay = stof(args[ci++]);
        if (cmd == "newbob_num_trials" && ci < cs)
            newbob_num_trials = stoi(args[ci++]);
        if (cmd == "lambda" && ci < cs)
            elmo_lambda = stof(args[ci++]);
        if (cmd == "mirror_percentage" && ci < cs)
            mirror_percentage = stoi(args[ci++]);
        if (cmd == "eval_save_interval" && ci < cs)
            eval_save_interval = stoi(args[ci++]);
        if (cmd == "loss_output_interval" && ci < cs)
            loss_output_interval = stoi(args[ci++]);



    }

    if (!getBinFilesInFolder(&filenames, training_dir))
    {
        cout << "No bin file found in training dir. Exiting...\n";
        return;
    }
    cout << "Found " << filenames.size() << " bin files in traing dir: ";
    for (auto fi = filenames.begin(); fi != filenames.end(); fi++)
        cout << *fi << " ";

    cout << "\nLearning with these parameters:\n";
    cout << "training_dir:              " << training_dir << "\n";
    cout << "validation_set_file_name:  " << validation_set_file_name << "\n";
    cout << "loop:                      " << loop << "\n";
    cout << "eval_limit:                " << eval_limit << "\n";
    cout << "save_only_once:            " << (save_only_once ? "true" : "false") << "\n";
    cout << "no_shuffle:                " << (no_shuffle ? "true" : "false") << "\n";
    cout << "Loss Function:             " << "ELMO_METHOD(WCSC27)\n";
    cout << "mini_batch_size:           " << mini_batch_size << "\n";
    cout << "nn_batch_size:             " << nn_batch_size << "\n";
    cout << "nn_options:                " << nn_options << "\n";
    cout << "learning rate:             " << showpoint << fixed << setprecision(1) << eta1 << " , " << eta2 << " , " << eta3 << "\n";
    cout << "eta_epoch:                 " << eta1_epoch << " , " << eta2_epoch << "\n";
    cout << "newbob_decay:              " << newbob_decay << " with " << newbob_num_trials << " trials\n";
    cout << "discount_rate:             " << showpoint << fixed << setprecision(1) << discount_rate << "\n";
    cout << "reduction_gameply:         " << reduction_gameply << "\n";
    cout << "lambda:                    " << showpoint << fixed << setprecision(1) << elmo_lambda << "\n";
    cout << "lambda2:                   " << showpoint << fixed << setprecision(1) << elmo_lambda2 << "\n";
    cout << "lambda_limit:              " << elmo_lambda_limit << "\n";
    cout << "mirror_percentage:         " << mirror_percentage << "\n";
    cout << "eval_save_interval:        " << eval_save_interval << "\n";
    cout << "loss_output_interval:      " << loss_output_interval << "\n";

    cout << "  use_draw_in_training:    " << use_draw_in_training << "\n";
    cout << "  use_draw_in_validation:  " << use_draw_in_validation << "\n";

    TrainerInit();
    //Eval::NNUE::InitializeTraining(eta1, eta1_epoch, eta2, eta2_epoch, eta3);
    //Eval::NNUE::SetBatchSize(nn_batch_size);
    //Eval::NNUE::SetOptions(nn_options);

    if (!readValidationSfens())
        return;
    thrFilereader = thread(&readSfens);

    // Initial loss
    en.sthread[0].thr = thread(&calcLoss, &en.sthread[0]);
    en.sthread[0].thr.join();
    return;

    // Start workers
    for (int tnum = 0; tnum < en.Threads; tnum++)
    {
        en.sthread[tnum].index = tnum;
        en.sthread[tnum].thr = thread(&learnWorker, &en.sthread[tnum]);
    }

    for (int tnum = 0; tnum < en.Threads; tnum++)
    {
        if (en.sthread[tnum].thr.joinable())
            en.sthread[tnum].thr.join();
    }

    cout << "All threads joined.\n";
}

#endif
