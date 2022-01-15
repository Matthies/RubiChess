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

//
// This implements NNUE based evaluation compatible with halfKP-256-32-32-1 nets.
// NNUE based evaluation was invented by Yu Nasu for Shogi engine and ported to
// Stockfish by Hisayori Noda (nodchip).
// Intrinsic cpu code for better performance is taken from cfish port by Ronald de Man.
//

#include "RubiChess.h"

#ifdef NNUE

//
// Some NNUE related constants and types
//

// PieceSquare indices
enum {
    PS_WPAWN    = 1,
    PS_BPAWN    = 1 * 64 + 1,
    PS_WKNIGHT  = 2 * 64 + 1,
    PS_BKNIGHT  = 3 * 64 + 1,
    PS_WBISHOP  = 4 * 64 + 1,
    PS_BBISHOP  = 5 * 64 + 1,
    PS_WROOK    = 6 * 64 + 1,
    PS_BROOK    = 7 * 64 + 1,
    PS_WQUEEN   = 8 * 64 + 1,
    PS_BQUEEN   = 9 * 64 + 1,
    PS_END      = 10 * 64 + 1
};

// table to translate PieceCode to PieceSquare index for both POVs respecting the piece order special to RubiChess
uint32_t PieceToIndex[2][16] = {
  { 0, 0, PS_WPAWN, PS_BPAWN, PS_WKNIGHT, PS_BKNIGHT, PS_WBISHOP, PS_BBISHOP, PS_WROOK, PS_BROOK, PS_WQUEEN, PS_BQUEEN, 0, 0, 0, 0 },
  { 0, 0, PS_BPAWN, PS_WPAWN, PS_BKNIGHT, PS_WKNIGHT, PS_BBISHOP, PS_WBISHOP, PS_BROOK, PS_WROOK, PS_BQUEEN, PS_WQUEEN, 0, 0, 0, 0 }
};


//
// Global objects
//
NnueType NnueReady = NnueDisabled;

NnueInputSlice* NnueIn;
NnueClippedRelu *NnueCl1, *NnueCl2;
NnueNetworkLayer *NnueOut, *NnueHd1, *NnueHd2;
NnueFeatureTransformer *NnueFt;


//
// NNUE interface in chessposition
//
template <NnueType Nt, Color c> void chessposition::HalfkpAppendActiveIndices(NnueIndexList *active)
{
    const int r = (Nt == NnueRotate) ? 0x3f : 0x3c;
    int k = ORIENT(c, kingpos[c], r);
    U64 nonkingsbb = (occupied00[0] | occupied00[1]) & ~(piece00[WKING] | piece00[BKING]);
    while (nonkingsbb)
    {
        int index = pullLsb(&nonkingsbb);
        active->values[active->size++] = ORIENT(c, index, r) + PieceToIndex[c][mailbox[index]] + PS_END * k;
    }
}


template <NnueType Nt, Color c> void chessposition::HalfkpAppendChangedIndices(DirtyPiece* dp, NnueIndexList* add, NnueIndexList* remove)
{
    const int r = (Nt == NnueRotate) ? 0x3f : 0x3c;
    int k = ORIENT(c, kingpos[c], r);
    for (int i = 0; i < dp->dirtyNum; i++) {
        PieceCode pc = dp->pc[i];
        if ((pc >> 1) == KING)
            continue;
        if (dp->from[i] >= 0)
            remove->values[remove->size++] = ORIENT(c, dp->from[i], r) + PieceToIndex[c][pc] + PS_END * k;
        if (dp->to[i] >= 0)
            add->values[add->size++] = ORIENT(c, dp->to[i], r) + PieceToIndex[c][pc] + PS_END * k;
    }
}

#ifdef USE_AVX512
#define NUM_REGS 8
#define SIMD_WIDTH 512
typedef __m512i vec8_t, vec16_t;
typedef __mmask64 mask_t;
#define vec_add_16(a,b) _mm512_add_epi16(a,b)
#define vec_sub_16(a,b) _mm512_sub_epi16(a,b)
#define vec_packs(a,b) _mm512_packs_epi16(a,b)
#define vec_clip_8(a,b) _mm512_max_epi8(vec_packs(a,b),_mm512_setzero_si512())

#elif defined(USE_AVX2)
#define NUM_REGS 16
#define SIMD_WIDTH 256
typedef __m256i vec8_t, vec16_t;
#define vec_add_16(a,b) _mm256_add_epi16(a,b)
#define vec_sub_16(a,b) _mm256_sub_epi16(a,b)
#define vec_packs(a,b) _mm256_packs_epi16(a,b)
#define vec_clip_8(a,b) _mm256_max_epi8(vec_packs(a,b),_mm256_setzero_si256())

#elif defined(USE_SSE2)
#define NUM_REGS 16
#define SIMD_WIDTH 128
typedef __m128i vec8_t, vec16_t;
#define vec_add_16(a,b) _mm_add_epi16(a,b)
#define vec_sub_16(a,b) _mm_sub_epi16(a,b)
#define vec_packs(a,b) _mm_packs_epi16(a,b)
#if defined(USE_SSSE3)
#define vec_clip_8(a,b) vec_packs(_mm_max_epi16(a,_mm_setzero_si128()),_mm_max_epi16(b,_mm_setzero_si128()))
#else
#define vec_clip_16(a) _mm_min_epi16(_mm_max_epi16(a,_mm_setzero_si128()),_mm_set1_epi16(127))
#endif

#elif defined(USE_NEON)
#define NUM_REGS 16
#define SIMD_WIDTH 128
typedef int16x8_t vec16_t;
typedef int8x16_t vec8_t;
#define vec_add_16(a,b) vaddq_s16(a,b)
#define vec_sub_16(a,b) vsubq_s16(a,b)
#define vec_packs(a,b) vcombine_s8(vqmovn_s16(a),vqmovn_s16(b))
#define vec_clip_8(a,b) vmaxq_s8(vec_packs(a,b),vdupq_n_s8(0))

#endif


#ifdef USE_SIMD
#define TILE_HEIGHT (NUM_REGS * SIMD_WIDTH / 16)
#else
#define TILE_HEIGHT NnueFtHalfdims
#endif


template <NnueType Nt, Color c> void chessposition::UpdateAccumulator()
{
#ifdef USE_SIMD
    vec16_t acc[NUM_REGS];
#endif

    int mslast = ply;
    // A full update needs activation of all pieces excep kings
    int fullupdatecost = POPCOUNT(occupied00[WHITE] | occupied00[BLACK]) - 2;

    while (mslast > 0 && !accumulator[mslast].computationState[c])
    {
        // search for position with computed accu on stack that leads to current position by differential updates
        // break at king move or if the dirty piece updates get too expensive
        DirtyPiece* dp = &dirtypiece[mslast];
        if ((dp->pc[0] >> 1) == KING || (fullupdatecost -= dp->dirtyNum + 1) < 0)
            break;
        mslast--;
    }

    if (mslast >= 0 && accumulator[mslast].computationState[c])
    {
        if (mslast == ply)
            return;

        NnueIndexList removedIndices[2], addedIndices[2];
        removedIndices[0].size = removedIndices[1].size = 0;
        addedIndices[0].size = addedIndices[1].size = 0;
        HalfkpAppendChangedIndices<Nt, c>(&dirtypiece[mslast + 1], &addedIndices[0], &removedIndices[0]);
        for (int ms = mslast + 2; ms <= ply; ms++)
            HalfkpAppendChangedIndices<Nt, c>(&dirtypiece[ms], &addedIndices[1], &removedIndices[1]);

        accumulator[mslast + 1].computationState[c] = true;
        accumulator[ply].computationState[c] = true;

        int pos2update[3] = { mslast + 1, mslast + 1 == ply ? -1 : ply, -1 };
#ifdef USE_SIMD
        for (unsigned int i = 0; i < NnueFtHalfdims / TILE_HEIGHT; i++)
        {
            vec16_t* accTile = (vec16_t*)&accumulator[mslast].accumulation[c][i * TILE_HEIGHT];
            for (unsigned int j = 0; j < NUM_REGS; j++)
                acc[j] = accTile[j];
            for (unsigned int l = 0; pos2update[l] >= 0; l++)
            {
                // Difference calculation for the deactivated features
                for (unsigned int k = 0; k < removedIndices[l].size; k++)
                {
                    unsigned int index = removedIndices[l].values[k];
                    const unsigned int offset = NnueFtHalfdims * index + i * TILE_HEIGHT;
                    vec16_t* column = (vec16_t*)&NnueFt->weight[offset];
                    for (unsigned int j = 0; j < NUM_REGS; j++)
                        acc[j] = vec_sub_16(acc[j], column[j]);
                }

                // Difference calculation for the activated features
                for (unsigned int k = 0; k < addedIndices[l].size; k++)
                {
                    unsigned int index = addedIndices[l].values[k];
                    const unsigned int offset = NnueFtHalfdims * index + i * TILE_HEIGHT;
                    vec16_t* column = (vec16_t*)&NnueFt->weight[offset];
                    for (unsigned int j = 0; j < NUM_REGS; j++)
                        acc[j] = vec_add_16(acc[j], column[j]);
                }

                accTile = (vec16_t*)&accumulator[pos2update[l]].accumulation[c][i * TILE_HEIGHT];
                for (unsigned int j = 0; j < NUM_REGS; j++)
                    accTile[j] = acc[j];
            }
        }
#else
        for (unsigned int l = 0; pos2update[l] >= 0; l++)
        {
            memcpy(&accumulator[pos2update[l]].accumulation[c], &accumulator[mslast].accumulation[c], NnueFtHalfdims * sizeof(int16_t));
            mslast = pos2update[l];

            // Difference calculation for the deactivated features
            for (unsigned int k = 0; k < removedIndices[l].size; k++)
            {
                unsigned int index = removedIndices[l].values[k];
                const unsigned int offset = NnueFtHalfdims * index;

                for (unsigned int j = 0; j < NnueFtHalfdims; j++)
                    accumulator[mslast].accumulation[c][j] -= NnueFt->weight[offset + j];
            }

            // Difference calculation for the activated features
            for (unsigned int k = 0; k < addedIndices[l].size; k++)
            {
                unsigned int index = addedIndices[l].values[k];
                const unsigned int offset = NnueFtHalfdims * index;

                for (unsigned int j = 0; j < NnueFtHalfdims; j++)
                    accumulator[mslast].accumulation[c][j] += NnueFt->weight[offset + j];
            }
        }
#endif
    }
    else {
        // Full update needed
        NnueAccumulator* ac = &accumulator[ply];
        ac->computationState[c] = true;
        NnueIndexList activeIndices;
        activeIndices.size = 0;
        HalfkpAppendActiveIndices<Nt, c>(&activeIndices);
#ifdef USE_SIMD
        for (unsigned int i = 0; i < NnueFtHalfdims / TILE_HEIGHT; i++)
        {
            vec16_t* ft_biases_tile = (vec16_t*)&NnueFt->bias[i * TILE_HEIGHT];
            for (unsigned int j = 0; j < NUM_REGS; j++)
                acc[j] = ft_biases_tile[j];

            for (unsigned int k = 0; k < activeIndices.size; k++)
            {
                unsigned int index = activeIndices.values[k];
                unsigned int offset = NnueFtHalfdims * index + i * TILE_HEIGHT;
                vec16_t* column = (vec16_t*)&NnueFt->weight[offset];
                for (unsigned int j = 0; j < NUM_REGS; j++)
                    acc[j] = vec_add_16(acc[j], column[j]);
            }

            vec16_t* accTile = (vec16_t*)&ac->accumulation[c][i * TILE_HEIGHT];
            for (unsigned int j = 0; j < NUM_REGS; j++)
                accTile[j] = acc[j];
        }

#else
        memcpy(ac->accumulation[c], NnueFt->bias, NnueFtHalfdims * sizeof(int16_t));

        for (unsigned int k = 0; k < activeIndices.size; k++)
        {
            unsigned int index = activeIndices.values[k];
            unsigned int offset = NnueFtHalfdims * index;

            for (unsigned int j = 0; j < NnueFtHalfdims; j++)
                ac->accumulation[c][j] += NnueFt->weight[offset + j];
        }
#endif
    }
}


template <NnueType Nt> void chessposition::Transform(clipped_t *output)
{
    UpdateAccumulator<Nt, WHITE>();
    UpdateAccumulator<Nt, BLACK>();

    int16_t(*acc)[2][256] = &accumulator[ply].accumulation;

    const int perspectives[2] = { state & S2MMASK, !(state & S2MMASK) };
    for (int p = 0; p < 2; p++)
    {
        const unsigned int offset = NnueFtHalfdims * p;

#ifdef USE_SIMD
        const unsigned int numChunks = (16 * NnueFtHalfdims) / SIMD_WIDTH;

#if defined(USE_SSSE3) || defined(USE_NEON)
        vec8_t* out = (vec8_t*)&output[offset];
        for (unsigned int i = 0; i < numChunks / 2; i++) {
            vec16_t s0 = ((vec16_t*)(*acc)[perspectives[p]])[i * 2];
            vec16_t s1 = ((vec16_t*)(*acc)[perspectives[p]])[i * 2 + 1];
            out[i] = vec_clip_8(s0, s1);
        }

#else
        vec16_t* out = (vec16_t*)&output[offset];
        for (unsigned int i = 0; i < numChunks; i++) {
            vec16_t sum = ((vec16_t*)(*acc)[perspectives[p]])[i];
            out[i] = vec_clip_16(sum);
        }

#endif
#else
        for (unsigned int i = 0; i < NnueFtHalfdims; i++) {
            int16_t sum = (*acc)[perspectives[p]][i];
            output[offset + i] = max<int16_t>(0, min<int16_t>(127, sum));
        }

#endif
    }
}


eval NnueValueScale = 64;


template <NnueType Nt> int chessposition::NnueGetEval()
{
    Transform<Nt>(network.input);
    NnueHd1->Propagate(network.input, network.hidden1_values);
    NnueCl1->Propagate(network.hidden1_values, network.hidden1_clipped);
    NnueHd2->Propagate(network.hidden1_clipped, network.hidden2_values);
    NnueCl1->Propagate(network.hidden2_values, network.hidden2_clipped);
    NnueOut->OutLayer(network.hidden2_clipped, &network.out_value);

    return network.out_value * NnueValueScale / 1024;
}


//
// FeatureTransformer
//
NnueFeatureTransformer::NnueFeatureTransformer() : NnueLayer(NULL)
{
    size_t allocsize = NnueFtHalfdims * sizeof(int16_t);
    bias = (int16_t*)allocalign64(allocsize);
    allocsize = (size_t)NnueFtHalfdims * (size_t)NnueFtInputdims * sizeof(int16_t);
    weight = (int16_t*)allocalign64(allocsize);
}

NnueFeatureTransformer::~NnueFeatureTransformer()
{
    freealigned64(bias);
    freealigned64(weight);
}

bool NnueFeatureTransformer::ReadWeights(NnueNetsource_t is)
{
    int i;
    for (i = 0; i < NnueFtHalfdims; ++i)
        NNUEREAD(is, (char*)&bias[i], sizeof(int16_t));
    for (i = 0; i < NnueFtHalfdims * NnueFtInputdims; ++i)
        NNUEREAD(is, (char*)&weight[i], sizeof(int16_t));

    return !NNUEREADFAIL(is);
}

#ifdef EVALOPTIONS
void NnueFeatureTransformer::WriteWeights(ofstream* os)
{
    int i;
    for (i = 0; i < NnueFtHalfdims; ++i)
        os->write((char*)&bias[i], sizeof(int16_t));
    for (i = 0; i < NnueFtHalfdims * NnueFtInputdims; ++i)
        os->write((char*)&weight[i], sizeof(int16_t));
}
#endif

uint32_t NnueFeatureTransformer::GetHash()
{
    return NNUEFEATUREHASH ^ NnueFtOutputdims;
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
    allocsize = (size_t)id * (size_t)od * sizeof(weight_t);
    weight = (weight_t*)allocalign64(allocsize);
}

NnueNetworkLayer::~NnueNetworkLayer()
{
    freealigned64(bias);
    freealigned64(weight);
}


unsigned int bit_shuffle(unsigned int v, int left, int right, unsigned int mask)
{
    unsigned int w = v & mask;
    w = (w << left) | (w >> right);
    return (v & ~mask) | (w & mask);
}


#if defined(USE_AVX512)
inline unsigned int shuffleWeightIndex(unsigned int idx, unsigned int dims, bool outlayer)
{
    if (dims > 32)
        idx = bit_shuffle(idx, 1, 2, 0x38);
    else if (dims == 32) {
        idx = bit_shuffle(idx, 2, 2, 0x14);
        if (!outlayer)
            idx = bit_shuffle(idx, 2, 3, 0x1f0);
    }
    return idx;
}
#else   //AVX2 version
inline unsigned int shuffleWeightIndex(unsigned int idx, unsigned int dims, bool outlayer)
{
    if (dims > 32)
        idx = bit_shuffle(idx, 1, 1, 0x18);
    else if (dims == 32) {
        idx = bit_shuffle(idx, 2, 1, 0x1c);
        if (!outlayer)
            idx = bit_shuffle(idx, 1, 3, 0xf0);
    }
    return idx;
}
#endif

bool NnueNetworkLayer::ReadWeights(NnueNetsource_t is)
{
    if (previous)
        previous->ReadWeights(is);

    for (unsigned int i = 0; i < outputdims; ++i)
        NNUEREAD(is, (char*)&bias[i], sizeof(int32_t));

    size_t buffersize = outputdims * inputdims;
    char* weightbuffer = (char*)calloc(buffersize, sizeof(char));

    if (!weightbuffer)
        return false;

    char* w = weightbuffer;
    NNUEREAD(is, weightbuffer, buffersize);

    for (unsigned int r = 0; r < outputdims; r++)
        for (unsigned int c = 0; c < inputdims; c++)
        {
            unsigned int idx = r * inputdims + c;
#if defined(USE_AVX2)
            idx = shuffleWeightIndex(idx, inputdims, outputdims == 1);
#endif
            weight[idx] = *w++;
        }

    free(weightbuffer);

    return !NNUEREADFAIL(is);
}

#ifdef EVALOPTIONS
#if defined(USE_AVX512)
inline unsigned int reverseShuffleWeightIndex(unsigned int idx, unsigned int dims, bool outlayer)
{
    if (dims > 32)
        idx = bit_shuffle(idx, 2, 1, 0x38);
    else if (dims == 32) {
        if (!outlayer)
            idx = bit_shuffle(idx, 3, 2, 0x1f0);
        idx = bit_shuffle(idx, 2, 2, 0x14);
    }
    return idx;
}
#else   //AVX2 version
inline unsigned int reverseShuffleWeightIndex(unsigned int idx, unsigned int dims, bool outlayer)
{
    if (dims > 32)
        idx = bit_shuffle(idx, 1, 1, 0x18);
    else if (dims == 32) {
        if (!outlayer)
            idx = bit_shuffle(idx, 3, 1, 0xf0);
        idx = bit_shuffle(idx, 1, 2, 0x1c);
    }
    return idx;
}
#endif

void NnueNetworkLayer::WriteWeights(ofstream* os)
{
    if (previous)
        previous->WriteWeights(os);

    for (unsigned int i = 0; i < outputdims; ++i)
        os->write((char*)&bias[i], sizeof(int32_t));

    size_t buffersize = outputdims * inputdims;
    char* weightbuffer = (char*)calloc(buffersize, sizeof(char));
    if (!weightbuffer) {
        cout << "Failed to allocated temporary buffer.\n";
        return;
    }

    char* w = weightbuffer;
    for (unsigned int r = 0; r < outputdims; r++)
        for (unsigned int c = 0; c < inputdims; c++)
        {
            unsigned int idx = r * inputdims + c;
            uint32_t ridx = idx;
#if defined(USE_AVX2)
            ridx = reverseShuffleWeightIndex(idx, inputdims, outputdims == 1);
#endif
            *(w + ridx) = weight[idx];
        }

    os->write(weightbuffer, buffersize);

    free(weightbuffer);
}
#endif

uint32_t NnueNetworkLayer::GetHash()
{
    return (NNUENETLAYERHASH + outputdims) ^ (previous->GetHash() >> 1) ^ (previous->GetHash() << 31);
}


// 32->1 propagation to output value
void NnueNetworkLayer::OutLayer(clipped_t* input, int32_t* output)
{
#if defined(USE_AVX2)
    __m256i* iv = (__m256i*)input;
    __m256i* row = (__m256i*)weight;
    __m256i prod = _mm256_maddubs_epi16(iv[0], row[0]);
    prod = _mm256_madd_epi16(prod, _mm256_set1_epi16(1));
    __m128i sum = _mm_add_epi32(
        _mm256_castsi256_si128(prod), _mm256_extracti128_si256(prod, 1));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
    *output = _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + bias[0];

#elif defined(USE_SSE2)
    __m128i* iv = (__m128i*)input;
    __m128i* row = (__m128i*)weight;
#if defined(USE_SSSE3)
    const __m128i kOnes = _mm_set1_epi16(1);
    __m128i p0 = _mm_madd_epi16(_mm_maddubs_epi16(iv[0], row[0]), kOnes);
    __m128i p1 = _mm_madd_epi16(_mm_maddubs_epi16(iv[1], row[1]), kOnes);
    __m128i sum = _mm_add_epi32(p0, p1);
#else
    __m128i p0 = _mm_madd_epi16(iv[0], row[0]);
    __m128i p1 = _mm_madd_epi16(iv[1], row[1]);
    __m128i p2 = _mm_madd_epi16(iv[2], row[2]);
    __m128i p3 = _mm_madd_epi16(iv[3], row[3]);
    __m128i sum = _mm_add_epi32(_mm_add_epi32(p0, p1), _mm_add_epi32(p2, p3));
#endif
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xb));
#if defined(USE_SSE41)
    return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
#else
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1));
    *output = _mm_cvtsi128_si32(sum) + bias[0];
#endif

#elif defined(USE_NEON)
    int8x8_t* iv = (int8x8_t*)input;
    int8x8_t* row = (int8x8_t*)weight;
    int32x4_t sum = { bias[0] };
    for (unsigned int j = 0; j < 2; j++) {
        int16x8_t prod = vmull_s8(iv[2 * j], row[2 * j]);
        prod = vmlal_s8(prod, iv[2 * j + 1], row[2 * j + 1]);
        sum = vpadalq_s16(sum, prod);
    }
    *output = sum[0] + sum[1] + sum[2] + sum[3];

#else
    *output = bias[0];
    for (unsigned int j = 0; j < 32; j++)
        *output += weight[j] * input[j];
#endif

}


void NnueNetworkLayer::Propagate(clipped_t* input, int32_t* output)
{
    myassert(inputdims == 32 || inputdims % 128 == 0, nullptr, 1, inputdims);
    myassert(outputdims % 8 == 0, nullptr, 1, outputdims);

#if defined(USE_AVX512)
    if (inputdims >= 64) {
        __m128i* outVec = (__m128i*)output;
        __m128i* biasVec = (__m128i*)bias;
        for (unsigned int i = 0; i < outputdims / 4; i++) {
            __m512i* inVec = (__m512i*)input;
            __m512i* w = (__m512i*)&weight[4 * i * inputdims];
            __m512i s0, s1, s2, s3;
            s0 = s1 = s2 = s3 = _mm512_setzero_si512();
            const __m512i kOnes = _mm512_set1_epi16(1);
            __m512i p1, p2;
            for (unsigned int j = 0; j < inputdims / 128; j++) {
                p1 = _mm512_maddubs_epi16(inVec[2 * j], w[0 * inputdims / 64 + 2 * j]);
                p2 = _mm512_maddubs_epi16(inVec[2 * j + 1], w[0 * inputdims / 64 + 2 * j + 1]);
                s0 = _mm512_add_epi32(s0, _mm512_madd_epi16(_mm512_add_epi16(p1, p2), kOnes));
                p1 = _mm512_maddubs_epi16(inVec[2 * j], w[1 * inputdims / 64 + 2 * j]);
                p2 = _mm512_maddubs_epi16(inVec[2 * j + 1], w[1 * inputdims / 64 + 2 * j + 1]);
                s1 = _mm512_add_epi32(s1, _mm512_madd_epi16(_mm512_add_epi16(p1, p2), kOnes));
                p1 = _mm512_maddubs_epi16(inVec[2 * j], w[2 * inputdims / 64 + 2 * j]);
                p2 = _mm512_maddubs_epi16(inVec[2 * j + 1], w[2 * inputdims / 64 + 2 * j + 1]);
                s2 = _mm512_add_epi32(s2, _mm512_madd_epi16(_mm512_add_epi16(p1, p2), kOnes));
                p1 = _mm512_maddubs_epi16(inVec[2 * j], w[3 * inputdims / 64 + 2 * j]);
                p2 = _mm512_maddubs_epi16(inVec[2 * j + 1], w[3 * inputdims / 64 + 2 * j + 1]);
                s3 = _mm512_add_epi32(s3, _mm512_madd_epi16(_mm512_add_epi16(p1, p2), kOnes));
            }
            s0 = _mm512_add_epi32(_mm512_unpacklo_epi32(s0, s1),
                _mm512_unpackhi_epi32(s0, s1));
            s2 = _mm512_add_epi32(_mm512_unpacklo_epi32(s2, s3),
                _mm512_unpackhi_epi32(s2, s3));
            s0 = _mm512_add_epi32(_mm512_unpacklo_epi64(s0, s2),
                _mm512_unpackhi_epi64(s0, s2));
            __m256i sum256 = _mm256_add_epi32(_mm512_castsi512_si256(s0),
                _mm512_extracti64x4_epi64(s0, 1));
            __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum256),
                _mm256_extracti128_si256(sum256, 1));
            outVec[i] = _mm_add_epi32(sum128, biasVec[i]);
        }
    }
    else { // 32 x 32 multiplication
        __m512i* outVec = (__m512i*)output;
        __m512i* biasVec = (__m512i*)bias;
        __m128i* inVec = (__m128i*)input;
        __m512i in0 = _mm512_broadcast_i32x4(inVec[0]);
        __m512i in1 = _mm512_broadcast_i32x4(inVec[1]);
        const __m512i kOnes = _mm512_set1_epi16(1);
        __m512i s0, s1, s2, s3, p;
        for (unsigned int i = 0; i < outputdims / 16; i++) {
            __m512i* w = (__m512i*)&weight[16 * i * 32];
            s0 = _mm512_maddubs_epi16(in0, w[0]); // first half of rows 0,4,8,12
            s0 = _mm512_madd_epi16(s0, kOnes);
            p = _mm512_maddubs_epi16(in1, w[1]); // second half of rows 0,4,8,12
            p = _mm512_madd_epi16(p, kOnes);
            s0 = _mm512_add_epi32(s0, p);
            s1 = _mm512_maddubs_epi16(in0, w[2]); // first half of rows 1,5,9,13
            s1 = _mm512_madd_epi16(s1, kOnes);
            p = _mm512_maddubs_epi16(in1, w[3]);
            p = _mm512_madd_epi16(p, kOnes);
            s1 = _mm512_add_epi32(s1, p);
            s2 = _mm512_maddubs_epi16(in0, w[4]);
            s2 = _mm512_madd_epi16(s2, kOnes);
            p = _mm512_maddubs_epi16(in1, w[5]);
            p = _mm512_madd_epi16(p, kOnes);
            s2 = _mm512_add_epi32(s2, p);
            s3 = _mm512_maddubs_epi16(in0, w[6]);
            s3 = _mm512_madd_epi16(s3, kOnes);
            p = _mm512_maddubs_epi16(in1, w[7]);
            p = _mm512_madd_epi16(p, kOnes);
            s3 = _mm512_add_epi32(s3, p);
            s0 = _mm512_add_epi32(
                _mm512_unpacklo_epi32(s0, s1), _mm512_unpackhi_epi32(s0, s1));
            s2 = _mm512_add_epi32(
                _mm512_unpacklo_epi32(s2, s3), _mm512_unpackhi_epi32(s2, s3));
            s0 = _mm512_add_epi32(
                _mm512_unpacklo_epi64(s0, s2), _mm512_unpackhi_epi64(s0, s2));
            outVec[i] = _mm512_add_epi32(s0, biasVec[i]);
        }
    }

#elif defined(USE_AVX2)
    if (inputdims > 32) {
        __m128i* outVec = (__m128i*)output;
        __m128i* biasVec = (__m128i*)bias;
        __m256i* inVec = (__m256i*)input;
        for (unsigned int i = 0; i < outputdims / 4; i++) {
            __m256i* w = (__m256i*) & weight[4 * i * inputdims];
            __m256i s0, s1, s2, s3;
            s0 = s1 = s2 = s3 = _mm256_setzero_si256();
            const __m256i kOnes = _mm256_set1_epi16(1);
            __m256i p1, p2;
            for (unsigned int j = 0; j < inputdims / 64; j++) {
                p1 = _mm256_maddubs_epi16(inVec[2 * j], w[0 * inputdims / 32 + 2 * j]);
                p2 = _mm256_maddubs_epi16(inVec[2 * j + 1], w[0 * inputdims / 32 + 2 * j + 1]);
                s0 = _mm256_add_epi32(s0, _mm256_madd_epi16(_mm256_add_epi16(p1, p2), kOnes));
                p1 = _mm256_maddubs_epi16(inVec[2 * j], w[1 * inputdims / 32 + 2 * j]);
                p2 = _mm256_maddubs_epi16(inVec[2 * j + 1], w[1 * inputdims / 32 + 2 * j + 1]);
                s1 = _mm256_add_epi32(s1, _mm256_madd_epi16(_mm256_add_epi16(p1, p2), kOnes));
                p1 = _mm256_maddubs_epi16(inVec[2 * j], w[2 * inputdims / 32 + 2 * j]);
                p2 = _mm256_maddubs_epi16(inVec[2 * j + 1], w[2 * inputdims / 32 + 2 * j + 1]);
                s2 = _mm256_add_epi32(s2, _mm256_madd_epi16(_mm256_add_epi16(p1, p2), kOnes));
                p1 = _mm256_maddubs_epi16(inVec[2 * j], w[3 * inputdims / 32 + 2 * j]);
                p2 = _mm256_maddubs_epi16(inVec[2 * j + 1], w[3 * inputdims / 32 + 2 * j + 1]);
                s3 = _mm256_add_epi32(s3, _mm256_madd_epi16(_mm256_add_epi16(p1, p2), kOnes));
            }
            s0 = _mm256_hadd_epi32(s0, s1);
            s2 = _mm256_hadd_epi32(s2, s3);
            s0 = _mm256_hadd_epi32(s0, s2);
            __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(s0),
                _mm256_extracti128_si256(s0, 1));
            outVec[i] = _mm_add_epi32(sum128, biasVec[i]);
        }
    }
    else { // 32x32 multiplication
        __m256i* outVec = (__m256i*)output;
        __m256i* biasVec = (__m256i*)bias;
        __m128i* inVec = (__m128i*)input;
        __m256i in0 = _mm256_broadcastsi128_si256(inVec[0]);
        __m256i in1 = _mm256_broadcastsi128_si256(inVec[1]);
        const __m256i kOnes = _mm256_set1_epi16(1);
        __m256i s0, s1, s2, s3, p;
        for (unsigned int i = 0; i < outputdims / 8; i++) {
            __m256i* w = (__m256i*) & weight[8 * i * 32];
            s0 = _mm256_maddubs_epi16(in0, w[0]); // first half of rows 0,4
            s0 = _mm256_madd_epi16(s0, kOnes);
            p = _mm256_maddubs_epi16(in1, w[1]); // second half of rows 0,4
            p = _mm256_madd_epi16(p, kOnes);
            s0 = _mm256_add_epi32(s0, p);
            s1 = _mm256_maddubs_epi16(in0, w[2]); // first half of rows 1,5
            s1 = _mm256_madd_epi16(s1, kOnes);
            p = _mm256_maddubs_epi16(in1, w[3]); // second half of rows 1,5
            p = _mm256_madd_epi16(p, kOnes);
            s1 = _mm256_add_epi32(s1, p);
            s2 = _mm256_maddubs_epi16(in0, w[4]); // first half of rows 2,6
            s2 = _mm256_madd_epi16(s2, kOnes);
            p = _mm256_maddubs_epi16(in1, w[5]); // second half of rows 2,6
            p = _mm256_madd_epi16(p, kOnes);
            s2 = _mm256_add_epi32(s2, p);
            s3 = _mm256_maddubs_epi16(in0, w[6]); // first half of rows 3,7
            s3 = _mm256_madd_epi16(s3, kOnes);
            p = _mm256_maddubs_epi16(in1, w[7]); // second half of rows 3,7
            p = _mm256_madd_epi16(p, kOnes);
            s3 = _mm256_add_epi32(s3, p);
            s0 = _mm256_hadd_epi32(s0, s1);
            s2 = _mm256_hadd_epi32(s2, s3);
            s0 = _mm256_hadd_epi32(s0, s2);
            outVec[i] = _mm256_add_epi32(s0, biasVec[i]);
        }
    }

#elif defined(USE_SSSE3)
    __m128i* outVec = (__m128i*)output;
    __m128i* biasVec = (__m128i*)bias;
    __m128i* inVec = (__m128i*)input;
    const __m128i kOnes = _mm_set1_epi16(1);
    for (unsigned int i = 0; i < outputdims / 4; i++) {
        __m128i* w = (__m128i*) & weight[4 * i * inputdims], p1, p2, s0, s1, s2, s3;
        s0 = s1 = s2 = s3 = _mm_setzero_si128();
        for (unsigned int j = 0; j < inputdims / 32; j++) {
            p1 = _mm_maddubs_epi16(inVec[2 * j], w[0 * inputdims / 16 + 2 * j]);
            p2 = _mm_maddubs_epi16(inVec[2 * j + 1], w[0 * inputdims / 16 + 2 * j + 1]);
            s0 = _mm_add_epi32(s0, _mm_madd_epi16(_mm_add_epi16(p1, p2), kOnes));
            p1 = _mm_maddubs_epi16(inVec[2 * j], w[1 * inputdims / 16 + 2 * j]);
            p2 = _mm_maddubs_epi16(inVec[2 * j + 1], w[1 * inputdims / 16 + 2 * j + 1]);
            s1 = _mm_add_epi32(s1, _mm_madd_epi16(_mm_add_epi16(p1, p2), kOnes));
            p1 = _mm_maddubs_epi16(inVec[2 * j], w[2 * inputdims / 16 + 2 * j]);
            p2 = _mm_maddubs_epi16(inVec[2 * j + 1], w[2 * inputdims / 16 + 2 * j + 1]);
            s2 = _mm_add_epi32(s2, _mm_madd_epi16(_mm_add_epi16(p1, p2), kOnes));
            p1 = _mm_maddubs_epi16(inVec[2 * j], w[3 * inputdims / 16 + 2 * j]);
            p2 = _mm_maddubs_epi16(inVec[2 * j + 1], w[3 * inputdims / 16 + 2 * j + 1]);
            s3 = _mm_add_epi32(s3, _mm_madd_epi16(_mm_add_epi16(p1, p2), kOnes));
        }
        s0 = _mm_hadd_epi32(s0, s1);
        s2 = _mm_hadd_epi32(s2, s3);
        s0 = _mm_hadd_epi32(s0, s2);
        outVec[i] = _mm_add_epi32(s0, biasVec[i]);
    }

#elif defined(USE_SSE2)
    __m128i* outVec = (__m128i*)output;
    __m128i* biasVec = (__m128i*)bias;
    __m128i* inVec = (__m128i*)input;
    for (unsigned int i = 0; i < outputdims / 4; i++) {
        __m128i* w = (__m128i*) & weight[4 * i * inputdims], p, s0, s1, s2, s3;
        s0 = s1 = s2 = s3 = _mm_setzero_si128();
        for (unsigned int j = 0; j < inputdims / 8; j++) {
            p = _mm_madd_epi16(inVec[j], w[0 * inputdims / 8 + j]);
            s0 = _mm_add_epi32(s0, p);
            p = _mm_madd_epi16(inVec[j], w[1 * inputdims / 8 + j]);
            s1 = _mm_add_epi32(s1, p);
            p = _mm_madd_epi16(inVec[j], w[2 * inputdims / 8 + j]);
            s2 = _mm_add_epi32(s2, p);
            p = _mm_madd_epi16(inVec[j], w[3 * inputdims / 8 + j]);
            s3 = _mm_add_epi32(s3, p);
        }
        s0 = _mm_add_epi32(_mm_unpacklo_epi32(s0, s1), _mm_unpackhi_epi32(s0, s1));
        s2 = _mm_add_epi32(_mm_unpacklo_epi32(s2, s3), _mm_unpackhi_epi32(s2, s3));
        s0 = _mm_add_epi32(_mm_unpacklo_epi64(s0, s2), _mm_unpackhi_epi64(s0, s2));
        outVec[i] = _mm_add_epi32(s0, biasVec[i]);
    }

#elif defined(USE_NEON)
    const unsigned int numChunks = inputdims / 16;
    int8x8_t* inVec = (int8x8_t*)input;
    for (unsigned int i = 0; i < outputdims; ++i) {
        unsigned int offset = i * inputdims;
        int32x4_t sum = { bias[i] };
        int8x8_t* row = (int8x8_t*)&weight[offset];
        for (unsigned int j = 0; j < numChunks; j++) {
            int16x8_t product = vmull_s8(inVec[j * 2], row[j * 2]);
            product = vmlal_s8(product, inVec[j * 2 + 1], row[j * 2 + 1]);
            sum = vpadalq_s16(sum, product);
        }
        output[i] = sum[0] + sum[1] + sum[2] + sum[3];
    }
#else
    for (unsigned int i = 0; i < outputdims; i++) {
        unsigned int offset = i * inputdims;
        int32_t sum = bias[i];
        for (unsigned int j = 0; j < inputdims; j++)
            sum += weight[offset + j] * input[j];
        output[i] = sum;
    }

#endif


}

//
// ClippedRelu
//
NnueClippedRelu::NnueClippedRelu(NnueLayer* prev, int d) : NnueLayer(prev)
{
    dims = d;
}

bool NnueClippedRelu::ReadWeights(NnueNetsource_t is)
{
    if (previous) return previous->ReadWeights(is);
    return true;
}

#ifdef EVALOPTIONS
void NnueClippedRelu::WriteWeights(ofstream* os)
{
    if (previous) return previous->WriteWeights(os);
}
#endif

uint32_t NnueClippedRelu::GetHash()
{
    return NNUECLIPPEDRELUHASH + previous->GetHash();
}

void NnueClippedRelu::Propagate(int32_t *input, clipped_t *output)
{
#if defined(USE_AVX512)
    __m512i* in = (__m512i*)input;
    __m256i* out = (__m256i*)output;
    __m512i words = _mm512_srai_epi16(_mm512_packs_epi32(in[0], in[1]), NnueClippingShift);
    __m256i packed = _mm256_packs_epi16(
        _mm512_castsi512_si256(words), _mm512_extracti64x4_epi64(words, 1));
    out[0] = _mm256_max_epi8(packed, _mm256_setzero_si256());
#elif defined(USE_AVX2)
    const unsigned int numChunks = dims / 32;
    const __m256i kZero = _mm256_setzero_si256();
    __m256i* in = (__m256i*)input;
    __m256i* out = (__m256i*)output;
    for (unsigned int i = 0; i < numChunks; i++) {
        __m256i words0 = _mm256_srai_epi16(_mm256_packs_epi32(
            in[i * 4 + 0], in[i * 4 + 1]), NnueClippingShift);
        __m256i words1 = _mm256_srai_epi16(_mm256_packs_epi32(
            in[i * 4 + 2], in[i * 4 + 3]), NnueClippingShift);
        out[i] = _mm256_max_epi8(_mm256_packs_epi16(words0, words1), kZero);
    }
#elif defined(USE_SSSE3)
    const unsigned int numChunks = dims / 16;
    const __m128i k0x80s = _mm_set1_epi8(-128);
    __m128i* in = (__m128i*)input;
    __m128i* out = (__m128i*)output;
    for (unsigned int i = 0; i < numChunks; i++) {
        __m128i words0 = _mm_srai_epi16(
            _mm_packs_epi32(in[i * 4 + 0], in[i * 4 + 1]), NnueClippingShift);
        __m128i words1 = _mm_srai_epi16(
            _mm_packs_epi32(in[i * 4 + 2], in[i * 4 + 3]), NnueClippingShift);
        __m128i packedbytes = _mm_packs_epi16(words0, words1);
        out[i] = _mm_subs_epi8(_mm_adds_epi8(packedbytes, k0x80s), k0x80s);
    }

#elif defined(USE_SSE2)
    const unsigned int numChunks = dims / 8;
    const __m128i kZero = _mm_setzero_si128();
    const __m128i k0x7f = _mm_set1_epi16(0x7f);
    __m128i* in = (__m128i*)input;
    __m128i* out = (__m128i*)output;
    for (unsigned int i = 0; i < numChunks; i++) {
        __m128i words = _mm_srai_epi16(_mm_packs_epi32(in[i * 2], in[i * 2 + 1]),
            NnueClippingShift);
        out[i] = _mm_min_epi16(_mm_max_epi16(words, kZero), k0x7f);
    }

#elif defined(USE_NEON)
    const unsigned int numChunks = dims / 8;
    const int8x8_t kZero = { 0 };
    int32x4_t* in = (int32x4_t*)input;
    int8x8_t* out = (int8x8_t*)output;
    for (unsigned int i = 0; i < numChunks; i++) {
        int16x8_t shifted = vcombine_s16(
            vqshrn_n_s32(in[i * 2], NnueClippingShift), vqshrn_n_s32(in[i * 2 + 1], NnueClippingShift));
        out[i] = vmax_s8(vqmovn_s16(shifted), kZero);
    }

#else
    for (int i = 0; i < dims; i++)
        output[i] = max(0, min(127, input[i] >> NnueClippingShift));
#endif

}


//
// InputSlice
//
NnueInputSlice::NnueInputSlice() : NnueLayer(NULL)
{
}

bool NnueInputSlice::ReadWeights(NnueNetsource_t is)
{
    if (previous) return previous->ReadWeights(is);
    return true;
}

#ifdef EVALOPTIONS
void NnueInputSlice::WriteWeights(ofstream* os)
{
    if (previous) return previous->WriteWeights(os);
}
#endif

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
    NnueCl1 = new NnueClippedRelu(NnueHd1, 32);
    NnueHd2 = new NnueNetworkLayer(NnueCl1, 32, 32);
    NnueCl2 = new NnueClippedRelu(NnueHd2, 32);
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

bool NnueReadNet(NnueNetsource_t is)
{
    NnueReady = NnueDisabled;

    uint32_t fthash = NnueFt->GetHash();
    uint32_t nethash = NnueOut->GetHash();
    uint32_t filehash = (fthash ^ nethash);

    uint32_t version, hash, size;
    string sarchitecture;
    
    NNUEREAD(is, (char*)&version, sizeof(uint32_t));
    NNUEREAD(is, (char*)&hash, sizeof(uint32_t));
    NNUEREAD(is, (char*)&size, sizeof(uint32_t));

    sarchitecture.resize(size);
    NNUEREAD(is, (char*)&sarchitecture[0], size);

    NnueType nt;
    if (version == NNUEFILEVERSIONROTATE)
        nt = NnueRotate;
    else if (version == NNUEFILEVERSIONFLIP)
        nt = NnueFlip;
    else
        return false;


    if (hash != filehash)
        return false;


    // Read the weights of the feature transformer
    NNUEREAD(is, (char*)&hash, sizeof(uint32_t));
    if (hash != fthash) return false;
    if (!NnueFt->ReadWeights(is)) return false;

    // Read the weights of the network layers recursively
    NNUEREAD(is, (char*)&hash, sizeof(uint32_t));
    if (hash != nethash) return false;
    if (!NnueOut->ReadWeights(is)) return false;

    if (!NNUEEOF(is))
        return false;

    NnueReady = nt;

    return true;
}

#ifdef EVALOPTIONS
void NnueWriteNet(vector<string> args)
{
    string NnueNetPath = "export.nnue";
    if (args.size())
        NnueNetPath = args[0];

    ofstream os;
    os.open(NnueNetPath, ios::binary);
    if (!os && en.ExecPath != "")
        os.open(en.ExecPath + NnueNetPath, ios::binary);

    if (!os) {
        cout << "Cannot write file " << NnueNetPath << "\n";
        return;
    }

    uint32_t fthash = NnueFt->GetHash();
    uint32_t nethash = NnueOut->GetHash();
    uint32_t filehash = (fthash ^ nethash);
    uint32_t version = NNUEFILEVERSIONROTATE;
    const string sarchitecture = "Features=HalfKP(Friend)[41024->256x2],Network=AffineTransform[1<-32](ClippedReLU[32](AffineTransform[32<-32](ClippedReLU[32](AffineTransform[32<-512](InputSlice[512(0:512)])))))";
    uint32_t size = (uint32_t)sarchitecture.size();

    os.write((char*)&version, sizeof(uint32_t));
    os.write((char*)&filehash, sizeof(uint32_t));
    os.write((char*)&size, sizeof(uint32_t));
    os.write((char*)&sarchitecture[0], size);

    os.write((char*)&fthash, sizeof(uint32_t));
    NnueFt->WriteWeights(&os);

    os.write((char*)&nethash, sizeof(uint32_t));
    NnueOut->WriteWeights(&os);

    os.close();

    cout << "Network written to file " << NnueNetPath << "\n";
}

void NnueRegisterEvals()
{
    // Expose weights and bias of ouput layer as UCI options for tuning
    en.ucioptions.Register(&NnueValueScale, "NnueValueScale", ucinnuebias, to_string(NnueValueScale), INT_MIN, INT_MAX, nullptr);
    en.ucioptions.Register(&NnueOut->bias[0], "NnueOutBias", ucinnuebias, to_string(NnueOut->bias[0]), INT_MIN, INT_MAX, nullptr);
    for (int i = 0; i < 32; i++)
    {
        ostringstream osName;
        osName << "NnueOutWeight_" << setw(2) << setfill('0') << to_string(i);
        int idx = i;
#if defined(USE_AVX2)
        idx = shuffleWeightIndex(idx, 32, true);
#endif
        en.ucioptions.Register(&NnueOut->weight[idx], osName.str(), ucinnueweight, to_string(NnueOut->weight[idx]), CHAR_MIN, CHAR_MAX, nullptr);
    }
}
#endif

// Explicit template instantiation
// This avoids putting these definitions in header file
template int chessposition::NnueGetEval<NnueRotate>();
template int chessposition::NnueGetEval<NnueFlip>();


#endif
