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

#ifdef NNUE

bool NnueReady = false;

void NnueReadNet(string path)
{
    cout << "Reading " << path << "\n";
    ifstream s(path, ios::binary);
    if (!s) return;
    
    uint32_t nnueVersion, nnueHash, nnueSize;
    string nnueArchitecture;
    
    s.read((char*)&nnueVersion, sizeof(uint32_t));
    s.read((char*)&nnueHash, sizeof(uint32_t));
    s.read((char*)&nnueSize, sizeof(uint32_t));
    if (nnueSize)
    {
        nnueArchitecture.resize(nnueSize);
        s.read((char*)&nnueArchitecture[0], nnueSize);
    }
        
    cout << "Version: " << hex << nnueVersion << "  Hash: " << nnueHash << "  Architecture: " << nnueArchitecture << "\n";
    
    if (nnueVersion != NNUEFILEVERSION) return;
    // FIXME: Missing check for correct hash
    
    
    
    NnueReady = true;
}
#endif