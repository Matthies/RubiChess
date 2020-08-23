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

//
// Global objects
//
bool NnueReady = false;
NnueInputSlice* NnueIn;
NnueClippedRelu *NnueCl1, *NnueCl2;
NnueNetworkLayer *NnueOut, *NnueHd1, *NnueHd2;
NnueFeatureTransformer *NnueFt;


//
// FeatureTransformer
//
NnueFeatureTransformer::NnueFeatureTransformer() : NnueLayer(NULL)
{
    size_t allocsize = halfdims * sizeof(int16_t);
    bias = (int16_t*)allocalign64(allocsize);
    allocsize = (size_t)halfdims * (size_t)inputdims * sizeof(int16_t);
    weight = (int16_t*)allocalign64(allocsize);
}

NnueFeatureTransformer::~NnueFeatureTransformer()
{
    freealigned64(bias);
    freealigned64(weight);
}

bool NnueFeatureTransformer::ReadWeights(ifstream *is)
{
    int i;
    for (i = 0; i < halfdims; ++i)
        is->read((char*)&bias[i], sizeof(int16_t));
    for (i = 0; i < halfdims * inputdims; ++i)
        is->read((char*)&weight[i], sizeof(int16_t));

    return !is->fail();

    return true;
}

uint32_t NnueFeatureTransformer::GetHash()
{
    return NNUEFEATUREHASH ^ outputdims;
}


//
// NetworkLayer
//
NnueNetworkLayer::NnueNetworkLayer(NnueLayer* prev, int id, int od) : NnueLayer(prev)
{
    inputdims = id;
    outputdims = od;
    size_t allocsize = od * sizeof(int32_t);
    bias = (int32_t*)allocalign64(allocsize);
    allocsize = (size_t)id * (size_t)od * sizeof(int8_t);
    weight = (int8_t*)allocalign64(allocsize);
}

NnueNetworkLayer::~NnueNetworkLayer()
{
    freealigned64(bias);
    freealigned64(weight);
}

bool NnueNetworkLayer::ReadWeights(ifstream* is)
{
    int i;

    if (previous)
        previous->ReadWeights(is);
    for (i = 0; i < outputdims; ++i)
        is->read((char*)&bias[i], sizeof(int32_t));
    for (i = 0; i < outputdims * inputdims; ++i)
        is->read((char*)&weight[i], sizeof(int8_t));

    return !is->fail();

    return true;
}

uint32_t NnueNetworkLayer::GetHash()
{
    return (NNUENETLAYERHASH + outputdims) ^ (previous->GetHash() >> 1) ^ (previous->GetHash() << 31);
}


//
// ClippedRelu
//
NnueClippedRelu::NnueClippedRelu(NnueLayer* prev) : NnueLayer(prev)
{
}

bool NnueClippedRelu::ReadWeights(ifstream* is)
{
    if (previous) return previous->ReadWeights(is);
    return true;
};

uint32_t NnueClippedRelu::GetHash()
{
    return NNUECLIPPEDRELUHASH + previous->GetHash();
}


//
// InputSlice
//
NnueInputSlice::NnueInputSlice() : NnueLayer(NULL)
{
}

bool NnueInputSlice::ReadWeights(ifstream* is)
{
    if (previous) return previous->ReadWeights(is);
    return true;
};

uint32_t NnueInputSlice::GetHash()
{
    return NNUEINPUTSLICEHASH ^ outputdims;
}


//
// Global Interface
//
void NnueInit()
{
    NnueFt = new NnueFeatureTransformer();
    NnueIn = new NnueInputSlice();
    NnueHd1 = new NnueNetworkLayer(NnueIn, 512, 32);
    NnueCl1 = new NnueClippedRelu(NnueHd1);
    NnueHd2 = new NnueNetworkLayer(NnueCl1, 32, 32);
    NnueCl2 = new NnueClippedRelu(NnueHd2);
    NnueOut = new NnueNetworkLayer(NnueCl2, 32, 1);
}

void NnueRemove()
{
    delete NnueFt;
    delete NnueIn;
    delete NnueHd1;
    delete NnueCl1;
    delete NnueHd2;
    delete NnueCl2;
    delete NnueOut;
}

void NnueReadNet(string path)
{
    uint32_t fthash = NnueFt->GetHash();
    uint32_t nethash = NnueOut->GetHash();
    uint32_t filehash = (fthash ^ nethash);

    ifstream is(path, ios::binary);
    if (!is) return;
    
    uint32_t version, hash, size;
    string sarchitecture;
    
    is.read((char*)&version, sizeof(uint32_t));
    is.read((char*)&hash, sizeof(uint32_t));
    is.read((char*)&size, sizeof(uint32_t));
    if (size)
    {
        sarchitecture.resize(size);
        is.read((char*)&sarchitecture[0], size);
    }

    if (version != NNUEFILEVERSION) return;

    if (hash != filehash) return;

    is.read((char*)&hash, sizeof(uint32_t));
    if (hash != fthash) return;
    if (!NnueFt->ReadWeights(&is)) return;
    is.read((char*)&hash, sizeof(uint32_t));
    if (hash != nethash) return;
    if (!NnueOut->ReadWeights(&is)) return;
    if (is.peek() != ios::traits_type::eof())
        return;

    NnueReady = true;
}
#endif