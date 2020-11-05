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

#if 0
// this is what gensfen oofers in SF:
    std::cout << "gensfen : " << endl
        << "  search_depth = " << search_depth << " to " << search_depth2 << endl
        << "  nodes = " << nodes << endl
        << "  loop_max = " << loop_max << endl
        << "  eval_limit = " << eval_limit << endl
        << "  thread_num (set by USI setoption) = " << thread_num << endl
        << "  book_moves (set by USI setoption) = " << Options["BookMoves"] << endl
        << "  random_move_minply     = " << random_move_minply << endl
        << "  random_move_maxply     = " << random_move_maxply << endl
        << "  random_move_count      = " << random_move_count << endl
        << "  random_move_like_apery = " << random_move_like_apery << endl
        << "  random_multi_pv        = " << random_multi_pv << endl
        << "  random_multi_pv_diff   = " << random_multi_pv_diff << endl
        << "  random_multi_pv_depth  = " << random_multi_pv_depth << endl
        << "  write_minply           = " << write_minply << endl
        << "  write_maxply           = " << write_maxply << endl
        << "  output_file_name       = " << output_file_name << endl
        << "  use_eval_hash          = " << use_eval_hash << endl
        << "  save_every             = " << save_every << endl
        << "  random_file_name       = " << random_file_name << endl;


    struct PackedSfen { uint8_t data[32]; };

    struct PackedSfenValue
    {
        // phase
        PackedSfen sfen;

        // Evaluation value returned from Learner::search()
        int16_t score;

        // PV first move
        // Used when finding the match rate with the teacher
        uint16_t move;

        // Trouble of the phase from the initial phase.
        uint16_t gamePly;

        // 1 if the player on this side ultimately wins the game. -1 if you are losing.
        // 0 if a draw is reached.
        // The draw is in the teacher position generation command gensfen,
        // Only write if LEARN_GENSFEN_DRAW_RESULT is enabled.
        int8_t game_result;

        // When exchanging the file that wrote the teacher aspect with other people
        //Because this structure size is not fixed, pad it so that it is 40 bytes in any environment.
        uint8_t padding;

        // 32 + 2 + 2 + 2 + 1 + 1 = 40bytes
    };

    // Folgendes ist in search.cpp ganz unten definiert
    extern Learner::ValueAndPV  search(Position& pos, int depth , size_t multiPV = 1 , uint64_t NodesLimit = 0);
    extern Learner::ValueAndPV qsearch(Position& pos);

#endif


// Play the pv, evaluate and go back to original position
// assume that the pv is valid and has no null moves
// questionable if this is worth the effort
#if 0
int evaluate_leaf(chessposition *pos , movelist pv)
{
    int rootcolor = pos->state & S2MMASK;
    int ply2 = ply;
    for (i = 0; i < pv.length; i++)
    {
        chessmove *m = &movelist[i];
        pos->playMove(m);//, states[ply2++]);
#if defined(EVAL_NNUE)  // Hmmmm. Zwischenbewertungen für Beschleunigung??
        if (depth < 8)
            Eval::evaluate_with_no_return(pos);
#endif  // defined(EVAL_NNUE)
    }
    int v = pos->getEval();
    if (rootColor != pos->state & S2MMASK)
        v = -v;

    for (i = pv.length; --i >= 0;)
    {
        chessmove *m = &movelist[i];
        pos->unplayMove(m);//, states[ply2++]);
    }

    return v;
}
#endif

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
    if (p = GETPROMOTION(c))
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
    //cerr << "flush thread " << thr->index << " called with result " << result << "\n";
    while (true)
    {
        //cout << "flush: offset=" << offset << "\n";
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
        //cerr << "flush: thread " << thr->index << " marked chunk " << fullchunk << " as full.\n";
    }

}


// global parameters for gensfen variation
int depth = 4;
int random_multi_pv = 0;
int random_multi_pv_depth = 4;
int random_multi_pv_diff = 32000;
int random_move_count = 5;
int random_move_minply = 1;
int random_move_maxply = 24;
int write_minply = 16;
int maxply = 400;
int generate_draw = 1;
U64 nodes = 0;
int eval_limit = 32000;


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
    chessmove nextmove;
    chessposition* pos = &thr->pos;
    pos->he_yes = 0ULL;
    pos->he_all = 0ULL;
    pos->he_threshold = 8100;
    memset(pos->history, 0, sizeof(chessposition::history));
    memset(pos->counterhistory, 0, sizeof(chessposition::counterhistory));
    memset(pos->countermove, 0, sizeof(chessposition::countermove));

    while (true)
    {
        if (gensfenstop)
            return;

        pos->getFromFen(STARTFEN);
        
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

        for (int ply = 0; ; ++ply)
        {
            if (ply > maxply) // default: 200; SF: 400
            {
                if (generate_draw) flush_psv(0, thr);
                break;
            }
            pos->ply = 0;
            //pos->prepareStack();
            movelist.length = CreateMovelist<ALL>(pos, &movelist.move[0]);
                
            if (movelist.length == 0)
            {
                if (pos->isCheckbb) // Mate
                    flush_psv(-S2MSIGN(pos->state & S2MMASK), thr);
                else if (generate_draw)
                    flush_psv(0, thr); // Stalemate
                break;
            }
            
            int score = pos->alphabeta<NoPrune>(SCOREBLACKWINS, SCOREWHITEWINS, depth);

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
            
            // Die ersten plies nicht schreiben, da sie "zu ähnlich" sind
            if (ply < write_minply - 1) // default: 16
            {
                //a_psv.clear();
                goto SKIP_SAVE;
            }

            // Position schon im Hash? Dann überspringen
            key = pos->hash;
            hash_index = key & (GENSFEN_HASH_SIZE - 1);
            key2 = sfenhash[hash_index];
            if (key == key2)
            {
                //a_psv.clear();
                goto SKIP_SAVE;
            }
            sfenhash[hash_index] = key; // Replace with the current key.
            
            //cout << pos->toFen() << "   value: " << value1 << "\n";

            // sfen packen
            thr->psv = thr->psvbuffer + psvnums;
            pos->toSfen(&thr->psv->sfen);
            // Stellung bewerten; Was macht evaluate_leaf anders als learnersearch/value1?
            // Laut noob ist der Sinn und Zweck von evaluate_leaf tatsächlich zweifelhaft
            //leaf_value = evaluate_leaf(pos, pv1);
            //psv.score = leaf_value == VALUE_NONE ? search_value : leaf_value;
            // Ply speichern
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
                //cerr << "gensfen thread " << thr->index << " switches to chunk " << nextchunk << "\n";
                psvnums = psvnums % (sfenchunknums * sfenchunksize);
            }
            
SKIP_SAVE:
            // preset move for next ply with the pv move
            nextmove.code = pos->pvtable[pos->ply][0];
            if (!nextmove.code)
            {
                // No move in pv => mate or stalemate
                if (pos->isCheckbb) // Mate
                    flush_psv(-S2MSIGN(pos->state & S2MMASK), thr);
                else if (generate_draw)
                    flush_psv(0, thr); // Stalemate
                break;
            }

            pos->prepareStack();
            bool chooserandom = !nextmove.code || (random_move_count && ply >= random_move_minply && ply <= random_move_maxply && random_move_flag[ply]);

            while (true)
            {
                int i = 0;
                if (chooserandom)
                {
                    if (random_multi_pv == 0)
                    {
                        i = ranval(&rnd) % movelist.length;
                        nextmove.code = movelist.move[i].code;
                    }
                    else
                    {
                        // random multi pv
                        pos->getRootMoves();
                        pos->rootsearch<MultiPVSearch>(SCOREBLACKWINS, SCOREWHITEWINS, random_multi_pv_depth, 1);
                        int s = min(pos->rootmovelist.length, random_multi_pv);
                        // Exclude moves with score outside of allowed margin
                        while (random_multi_pv_diff && pos->bestmovescore[0] > pos->bestmovescore[s - 1] + random_multi_pv_diff)
                            s--;

                        nextmove.code = pos->multipvtable[ranval(&rnd) % s][0];
                    }
                }

                bool legal = pos->playMove(&nextmove);

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
    string outputfile;
    size_t cs = args.size();
    size_t ci = 0;

    int old_multipv = en.MultiPV;
    while (ci < cs)
    {
        string cmd = args[ci++];
        if (cmd == "depth" && ci < cs)
            depth = stoi(args[ci++]);
        if (cmd == "loop" && ci < cs)
            fensnum = stoi(args[ci++]);
        if (cmd == "output_file_name" && ci < cs)
            outputfile = args[ci++];
        if (cmd == "random_multi_pv" && ci < cs)
            random_multi_pv = en.MultiPV = stoi(args[ci++]);
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
    }

    const U64 chunksneeded = fensnum / sfenchunksize + 1;
    ofstream os(outputfile, ios::binary);
    if (!os)
    {
        cout << "Cannot open file " << outputfile << "\n";
        return;
    }

    int tnum;
    gensfenstop = false;

    cout << "Generating sfnes with these parameters:\n";
    cout << "depth:                 " << depth << "\n";
    cout << "maxply:                " << maxply << "\n";
    cout << "write_minply:          " << write_minply << "\n";
    cout << "generate_draw:         " << generate_draw << "\n";
    cout << "nodes:                 " << nodes << "\n";
    cout << "eval_limit:            " << eval_limit << "\n";
    cout << "random_move_count:     " << random_move_count << "\n";
    cout << "random_multi_pv_depth: " << random_multi_pv_depth << "\n";
    cout << "random_move_maxply:    " << random_move_maxply << "\n";
    cout << "random_multi_pv:       " << random_multi_pv << "\n";
    cout << "random_multi_pv_depth: " << random_multi_pv_depth << "\n";
    cout << "random_multi_pv_diff:  " << random_multi_pv_diff << "\n";

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
                //cerr << "gensfen: thread " << thr->index << " writing chunk " << i << "\n";
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
        //cerr << "Waiting for thread " << tnum << "\n";
        if (en.sthread[tnum].thr.joinable())
            en.sthread[tnum].thr.join();
    }
    cout << "gensfen finished.\n";
    en.MultiPV = old_multipv;
}



enum SfenFormat { bin, plain };

void convert(vector<string> args)
{
    string inputfile;
    string outputfile;
    SfenFormat outformat = plain;
    size_t cs = args.size();
    size_t ci = 0;

    while (ci < cs)
    {
        string cmd = args[ci++];

        if (cmd == "input_file_name")
            if (ci < cs)
            {
                inputfile = args[ci++];
                inputfile.erase(remove(inputfile.begin(), inputfile.end(), '\"'), inputfile.end());
            }
        if (cmd == "output_file_name")
            if (ci < cs)
            {
                outputfile = args[ci++];
                outputfile.erase(remove(outputfile.begin(), outputfile.end(), '\"'), outputfile.end());
            }
        if (cmd == "output_format")
            if (ci < cs)
            {
                outformat = (args[ci] == "bin" ? bin : plain);
                ci++;
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

            if (outformat == plain)
            {
                *os << "fen " << pos->toFen() << "\n";
                *os << "move " << cm.toString() << "\n";
                *os << "score " << psv->score << "\n";
                *os << "ply " << to_string((pos->fullmovescounter - 1) * 2 + (pos->state & S2MMASK)) << "\n";
                *os << "result " << to_string(psv->game_result) << "\n";
                *os << "e\n";
            }
            else
            {
                os->write((char*)psv, sizeof(PackedSfenValue));
            }
        }
        cerr << ".";
    }

    cerr << "Finished converting.\n";
}


void learn(vector<string> args)
{

}

#endif