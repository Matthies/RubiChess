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

			
void gensfen(U64 fensnum)
{
    while (true)
    {
        pos.getFromFen(STARTFEN);
        
        // Initialisierung der random moves
        // minimum ply, maximum ply and number of random moves
        int random_move_minply = 1;
        int random_move_maxply = 24;
        int random_move_count = 5;
		vector<bool> random_move_flag;
        random_move_flag.resize((size_t)random_move_maxply + random_move_count);
        vector<int> a;
        a.reserve((size_t)random_move_maxply);
        for (int i = std::max(random_move_minply - 1 , 0) ; i < random_move_maxply; ++i)
            a.push_back(i);
        for (int i = 0 ; i < std::min(random_move_count, (int)a.size()) ; ++i)
		{
			swap(a[i], a[prng.rand((uint64_t)a.size() - i) + i]);
			random_move_flag[a[i]] = true;
		}


        for (int ply = 0; ; ++ply)
		{
            if (draw || ply > maxply) // default: 400
            {
                if (generate_draw) flush_psv(0);
                break;
            }
            
            movelist.length = CreateMovelist<ALL>(pos, &movelist.move[0]);
      			
            if (movelist.length == 0)
			{
                if (pos.incheckbb) // Mate
                    flush_psv(-1);
				else if (generate_draw)
					flush_psv(0); // Stalemate
				break;
			}
            
            // Diese Suche ist primär für die Ermittlung des PV; der Score (value1) wird "nur" zum Erkennen des Siegs verwendet
            pv_value1 = pos.learnersearch(depth, 1, nodes);
			value1 = pv_value1.first;
			pv1 = pv_value1.second;
            if (abs(value1) >= eval_limit) // win adjudication; default: 32000
            {
                flush_psv((value1 >= eval_limit) ? 1 : -1);
				break;
			}

            if (3-fold or 50-moves-draw)
            {
                if (generate_draw) flush_psv(0);
                break;
            }
            
            // Die ersten plies nicht schreiben, da sie "zu ähnlich" sind
            if (ply < write_minply - 1) // default: 16
            {
                a_psv.clear();
                goto SKIP_SAVE;
			}

            // Position schon im Hash? Dann überspringen
			key = pos.hash;
			hash_index = key & (GENSFEN_HASH_SIZE - 1);
			key2 = hash[hash_index];
			if (key == key2)
			{
				a_psv.clear();
				goto SKIP_SAVE;
			}
			hash[hash_index] = key; // Replace with the current key.
            
            // sfen packen
            pos.sfen_pack(psv.sfen);
            // Stellung bewerten; Was macht evaluate_leaf anders als learnersearch/value1?
            // Laut noob ist der Sinn und Zweck von evaluate_leaf tatsächlich zweifelhaft
			leaf_value = evaluate_leaf(pos, pv1);
            psv.score = leaf_value == VALUE_NONE ? search_value : leaf_value;
            // Ply speichern 
			psv.gamePly = ply;
            Move pv_move1 = pv1[0];
            psv.move = pv_move1;
            
SKIP_SAVE:;
            // preset move for next ply with the pv move
            next_move = pv_move1;
            
			if (
				// 1. Random move of random_move_count times from random_move_minply to random_move_maxply
				(random_move_minply != -1 && ply <(int)random_move_flag.size() && random_move_flag[ply]) ||
				// 2. A mode to perform random move of random_move_count times after leaving the track
				(random_move_minply == -1 && random_move_c <random_move_count))
			{
				++random_move_c;

            


        
        }
    }
}