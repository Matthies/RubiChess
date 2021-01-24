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
#define NEWSIMD
#ifdef NNUE

#if defined(USE_AVX2)
#include <immintrin.h>

#elif defined(USE_SSSE3)
#include <tmmintrin.h>

#elif defined(USE_SSE2)
#include <emmintrin.h>

#elif defined(USE_NEON)
#include <arm_neon.h>

#endif


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
template <NnueType Nt> void chessposition::HalfkpAppendActiveIndices(int c, NnueIndexList *active)
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

template <NnueType Nt> void chessposition::AppendActiveIndices(NnueIndexList active[2])
{
    for (int c = 0; c < 2; c++)
        HalfkpAppendActiveIndices<Nt>(c, &active[c]);
}

template <NnueType Nt> void chessposition::HalfkpAppendChangedIndices(int c, DirtyPiece* dp, NnueIndexList* add, NnueIndexList* remove)
{
    const int r = (Nt == NnueRotate) ? 0x3f : 0x3c;
    int k = ORIENT(c, kingpos[c], r);
    for (int i = 0; i < dp->dirtyNum; i++) {
        PieceCode pc = dp->pc[i];
        if ((pc >> 1) == KING) continue;
        if (dp->from[i] >= 0) {
            remove->values[remove->size++] = ORIENT(c, dp->from[i], r) + PieceToIndex[c][pc] + PS_END * k;
        }
        if (dp->to[i] >= 0) {
            add->values[add->size++] = ORIENT(c, dp->to[i], r) + PieceToIndex[c][pc] + PS_END * k;
        }
    }
}

template <NnueType Nt> void chessposition::AppendChangedIndices(NnueIndexList add[2], NnueIndexList remove[2], bool reset[2])
{
    DirtyPiece* dp = &dirtypiece[mstop];

    if (accumulator[mstop - 1].computationState)
    {
        for (int c = 0; c < 2; c++) {
            reset[c] = (dp->pc[0] == (PieceCode)(WKING | c));
            if (reset[c])
                HalfkpAppendActiveIndices<Nt>(c, &add[c]);
            else
                HalfkpAppendChangedIndices<Nt>(c, dp, &add[c], &remove[c]);
        }
    }
    else {
        DirtyPiece* lastdp = &dirtypiece[mstop - 1];
        for (unsigned c = 0; c < 2; c++) {
            reset[c] = dp->pc[0] == (PieceCode)(WKING | c) || lastdp->pc[0] == (PieceCode)(WKING | c);
            if (reset[c]) {
                HalfkpAppendActiveIndices<Nt>(c, &add[c]);
            }
            else {
                HalfkpAppendChangedIndices<Nt>(c, dp, &add[c], &remove[c]);
                HalfkpAppendChangedIndices<Nt>(c, lastdp, &add[c], &remove[c]);
            }
        }
    }
}

#ifdef USE_AVX2
#define SIMD_WIDTH 256
typedef __m256i vec_t;
#define vec_add_16(a,b) _mm256_add_epi16(a,b)
#define vec_sub_16(a,b) _mm256_sub_epi16(a,b)
typedef __m256i vec8_t, vec16_t;
typedef uint32_t mask_t;
#define vec_clip_8(a,b) _mm256_max_epi8(vec_packs(a,b),_mm256_setzero_si256())
#define NUM_REGS 16

#elif defined(USE_SSE2)
#define SIMD_WIDTH 128
typedef __m128i vec_t;
#define vec_add_16(a,b) _mm_add_epi16(a,b)
#define vec_sub_16(a,b) _mm_sub_epi16(a,b)

#endif

#if defined(USE_SSE2)
#define NUM_REGS 16
#endif


#if defined(USE_SSE2) || defined(USE_MMX)
#define TILE_HEIGHT (NUM_REGS * SIMD_WIDTH / 16)
#else
#define TILE_HEIGHT NnueFtHalfdims
#endif


template <NnueType Nt> void chessposition::RefreshAccumulator()
{
    NnueAccumulator *ac = &accumulator[mstop];
    NnueIndexList activeIndices[2];
    activeIndices[0].size = activeIndices[1].size = 0;
    AppendActiveIndices<Nt>(activeIndices);

    for (int c = 0; c < 2; c++) {

        for (int i = 0; i < NnueFtHalfdims / TILE_HEIGHT; i++) {

#if defined(USE_SSE2) || defined(USE_MMX)
            vec_t* ft_biases_tile = (vec_t*)&NnueFt->bias[i * TILE_HEIGHT];
            vec_t* accTile = (vec_t*)&ac->accumulation[c][i * TILE_HEIGHT];
            vec_t acc[NUM_REGS];
            for (unsigned j = 0; j < NUM_REGS; j++)
                acc[j] = ft_biases_tile[j];

#else

            memcpy(&(ac->accumulation[c][i * TILE_HEIGHT]), &NnueFt->bias[i * TILE_HEIGHT], TILE_HEIGHT * sizeof(int16_t));
#endif

            for (size_t k = 0; k < activeIndices[c].size; k++) {
                unsigned index = activeIndices[c].values[k];
                unsigned offset = NnueFtHalfdims * index + i * TILE_HEIGHT;

#if defined(USE_SSE2) || defined(USE_MMX)
                vec_t* column = (vec_t*)&NnueFt->weight[offset];
                for (unsigned j = 0; j < NUM_REGS; j++)
                    acc[j] = vec_add_16(acc[j], column[j]);

#elif defined(USE_NEON)
                int16x8_t* accumulation = (int16x8_t*)&ac->accumulation[c][i * TILE_HEIGHT];
                int16x8_t* column = (int16x8_t*)&NnueFt->weight[offset];
                const unsigned numChunks = NnueFtHalfdims / 8;
                for (unsigned j = 0; j < numChunks; j++)
                    accumulation[j] = vaddq_s16(accumulation[j], column[j]);

#else

                for (unsigned j = 0; j < NnueFtHalfdims; j++)
                    ac->accumulation[c][i * TILE_HEIGHT + j] += NnueFt->weight[offset + j];
#endif
            }

#if defined(USE_SSE2) || defined(USE_MMX)
            for (unsigned j = 0; j < NUM_REGS; j++)
                accTile[j] = acc[j];

#endif
        }
    }

    ac->computationState = true;
}

// Test if we can update the accumulator from the previous position
template <NnueType Nt> bool chessposition::UpdateAccumulator()
{
    NnueAccumulator* ac = &accumulator[mstop];
    if (ac->computationState)
        return true;

    if (mstop == 0)
        return false;

    NnueAccumulator* prevac = &accumulator[mstop - 1];
    if (!prevac->computationState)
    {
        if (mstop == 1)
            return false;
        prevac = &accumulator[mstop - 2];
        if (!prevac->computationState)
            return false;
    }

    NnueIndexList removedIndices[2], addedIndices[2];
    removedIndices[0].size = removedIndices[1].size = 0;
    addedIndices[0].size = addedIndices[1].size = 0;

    bool reset[2];
    AppendChangedIndices<Nt>(addedIndices, removedIndices, reset);

    for (int i = 0; i < NnueFtHalfdims / TILE_HEIGHT; i++) {
        for (int c = 0; c < 2; c++) {
#if defined(USE_SSE2) || defined(USE_MMX)
            vec_t* accTile = (vec_t*)&ac->accumulation[c][i * TILE_HEIGHT];
            vec_t acc[NUM_REGS];

#elif defined(USE_NEON)
            const unsigned numChunks = NnueFtHalfdims / 8;
            int16x8_t* accTile = (int16x8_t*)&ac->accumulation[c][i * TILE_HEIGHT];

#endif

            if (reset[c]) {
#if defined(USE_SSE2) || defined(USE_MMX)
                vec_t* ft_b_tile = (vec_t*)&NnueFt->bias[i * TILE_HEIGHT];
                for (unsigned j = 0; j < NUM_REGS; j++)
                    acc[j] = ft_b_tile[j];
#else

                memcpy(&ac->accumulation[c][i * TILE_HEIGHT], &NnueFt->bias[i * TILE_HEIGHT], TILE_HEIGHT * sizeof(int16_t));
#endif
            }
            else {
#if defined(USE_SSE2) || defined(USE_MMX)
                vec_t* prevAccTile = (vec_t*)&prevac->accumulation[c][i * TILE_HEIGHT];
                for (unsigned j = 0; j < NUM_REGS; j++)
                    acc[j] = prevAccTile[j];
#else
                memcpy(&ac->accumulation[c][i * TILE_HEIGHT], &prevac->accumulation[c][i * TILE_HEIGHT], TILE_HEIGHT * sizeof(int16_t));
#endif
                // Difference calculation for the deactivated features
                for (size_t k = 0; k < removedIndices[c].size; k++) {
                    int index = removedIndices[c].values[k];
                    const int offset = NnueFtHalfdims * index + i * TILE_HEIGHT;
#if defined(USE_SSE2) || defined(USE_MMX)
                    vec_t* column = (vec_t*)&NnueFt->weight[offset];
                    for (unsigned j = 0; j < NUM_REGS; j++)
                        acc[j] = vec_sub_16(acc[j], column[j]);

#elif defined(USE_NEON)
                    int16x8_t* column = (int16x8_t*)&NnueFt->weight[offset];
                    for (unsigned j = 0; j < numChunks; j++)
                        accTile[j] = vsubq_s16(accTile[j], column[j]);

#else
                    for (int j = 0; j < NnueFtHalfdims; j++)
                        ac->accumulation[c][i * TILE_HEIGHT + j] -= NnueFt->weight[offset + j];
#endif
                }
            }
            // Difference calculation for the activated features
            for (size_t k = 0; k < addedIndices[c].size; k++) {
                int index = addedIndices[c].values[k];
                const int offset = NnueFtHalfdims * index + i * TILE_HEIGHT;
#if defined(USE_SSE2) || defined(USE_MMX)
                vec_t* column = (vec_t*)&NnueFt->weight[offset];
                for (unsigned j = 0; j < NUM_REGS; j++)
                    acc[j] = vec_add_16(acc[j], column[j]);

#elif defined(USE_NEON)
                int16x8_t* column = (int16x8_t*)&NnueFt->weight[offset];
                for (unsigned j = 0; j < numChunks; j++)
                    accTile[j] = vaddq_s16(accTile[j], column[j]);

#else
                for (int j = 0; j < TILE_HEIGHT; j++)
                    ac->accumulation[c][i * TILE_HEIGHT + j] += NnueFt->weight[offset + j];
#endif
            }

#if defined(USE_SSE2) || defined(USE_MMX)
            for (unsigned j = 0; j < NUM_REGS; j++)
                accTile[j] = acc[j];

#endif
        }
    }

    ac->computationState = true;
    return true;
}

template <NnueType Nt> void chessposition::Transform(clipped_t *output)
{
    if (!UpdateAccumulator<Nt>())
        RefreshAccumulator<Nt>();

    int16_t(*acc)[2][256] = &accumulator[mstop].accumulation;

#if defined(USE_AVX2)
    const unsigned numChunks = NnueFtHalfdims / 32;
    const __m256i kZero = _mm256_setzero_si256();

#elif defined(USE_SSSE3)
    const unsigned numChunks = NnueFtHalfdims / 16;
    const __m128i k0x80s = _mm_set1_epi8(-128);

#elif defined(USE_NEON)
    const unsigned numChunks = NnueFtHalfdims / 8;
    const int8x8_t kZero = { 0 };

#endif

    const int perspectives[2] = { state & S2MMASK, !(state & S2MMASK) };
    for (int p = 0; p < 2; p++)
    {
        const unsigned int offset = NnueFtHalfdims * p;

#if defined(USE_AVX2)
        __m256i* out = (__m256i*) & output[offset];
        for (unsigned i = 0; i < numChunks; i++) {
            __m256i sum0 = ((__m256i*)(*acc)[perspectives[p]])[i * 2 + 0];
            __m256i sum1 = ((__m256i*)(*acc)[perspectives[p]])[i * 2 + 1];
#if 0
            out[i] = _mm256_permute4x64_epi64(_mm256_max_epi8(
                _mm256_packs_epi16(sum0, sum1), kZero), 0xd8);
#else
            out[i] = _mm256_max_epi8(_mm256_packs_epi16(sum0, sum1), _mm256_setzero_si256());
#endif
        }

#elif defined(USE_SSSE3)

        __m128i* out = (__m128i*) & output[offset];
        for (unsigned i = 0; i < numChunks; i++) {
            __m128i sum0 = ((__m128i*)(*acc)[perspectives[p]])[i * 2 + 0];
            __m128i sum1 = ((__m128i*)(*acc)[perspectives[p]])[i * 2 + 1];
            __m128i packedbytes = _mm_packs_epi16(sum0, sum1);
            out[i] = _mm_subs_epi8(_mm_adds_epi8(packedbytes, k0x80s), k0x80s);
        }

#elif defined(USE_NEON)
        int8x8_t* out = (int8x8_t*)&output[offset];
        for (unsigned i = 0; i < numChunks; i++) {
            int16x8_t sum = ((int16x8_t*)(*acc)[perspectives[p]])[i];
            out[i] = vmax_s8(vqmovn_s16(sum), kZero);
        }

#else
        for (int i = 0; i < NnueFtHalfdims; i++)
        {
            int16_t sum = (*acc)[perspectives[p]][i];
            output[offset + i] = max<int16_t>(0, min<int16_t>(127, sum));
        }
#endif
    }
}


struct NnueNetwork {
    alignas(64) clipped_t input[NnueFtOutputdims];
    int32_t hidden1_values[32];
    int32_t hidden2_values[32];
    clipped_t hidden1_clipped[32];
    clipped_t hidden2_clipped[32];
    int32_t out_value;
};

template <NnueType Nt> int chessposition::NnueGetEval()
{
    NnueNetwork network;

    Transform<Nt>(network.input);
    NnueHd1->Propagate(network.input, network.hidden1_values);
    NnueCl1->Propagate(network.hidden1_values, network.hidden1_clipped);
    NnueHd2->Propagate(network.hidden1_clipped, network.hidden2_values);
    NnueCl1->Propagate(network.hidden2_values, network.hidden2_clipped);
    NnueOut->OutLayer(network.hidden2_clipped, &network.out_value);

    return network.out_value / NnueValueScale;
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

bool NnueFeatureTransformer::ReadWeights(ifstream *is)
{
    int i;
    for (i = 0; i < NnueFtHalfdims; ++i)
        is->read((char*)&bias[i], sizeof(int16_t));
    for (i = 0; i < NnueFtHalfdims * NnueFtInputdims; ++i)
        is->read((char*)&weight[i], sizeof(int16_t));

    return !is->fail();

    return true;
}

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
    allocsize = (size_t)id * (size_t)od * sizeof(int8_t);
    weight = (int8_t*)allocalign64(allocsize);
}

NnueNetworkLayer::~NnueNetworkLayer()
{
    freealigned64(bias);
    freealigned64(weight);
}


#ifndef USE_NEON
unsigned int bit_shuffle(unsigned v, int left, int right, unsigned mask)
{
    unsigned w = v & mask;
    w = (w << left) | (w >> right);
    return (v & ~mask) | (w & mask);
}
#endif

unsigned int wt_idx(unsigned r, unsigned c, unsigned dims, bool outlayer)
{
    unsigned k = r * dims + c;
#ifdef NEWSIMD
#if defined(USE_AVX2)
    if (dims > 32)
        k = bit_shuffle(k, 1, 1, 0x18);
    else if (dims == 32) {
        k = bit_shuffle(k, 2, 1, 0x1c);
        if (!outlayer)
            k = bit_shuffle(k, 1, 3, 0xf0);
    }
#endif
#endif
    return k;
}


bool NnueNetworkLayer::ReadWeights(ifstream* is)
{
    if (previous)
        previous->ReadWeights(is);

    for (unsigned int i = 0; i < outputdims; ++i)
        is->read((char*)&bias[i], sizeof(int32_t));

    char* c = (char*)calloc(outputdims * inputdims, sizeof(char));
    char* d = c;
    is->read(c, outputdims * inputdims * sizeof(char));

    for (unsigned int r = 0; r < outputdims; r++)
        for (unsigned int c = 0; c < inputdims; c++)
            weight[wt_idx(r, c, inputdims, outputdims == 1)] = *d++;

    free(c);

    return !is->fail();

    return true;
}

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

#elif defined(USE_MMX)
    __m64* iv = (__m64*)input;
    __m64* row = (__m64*)weights;
    __m64 s0 = _mm_setzero_si64(), s1 = s0;
    for (unsigned j = 0; j < 4; j++) {
        s0 = _mm_add_pi32(s0, _mm_madd_pi16(row[2 * j], iv[2 * j]));
        s1 = _mm_add_pi32(s1, _mm_madd_pi16(row[2 * j + 1], iv[2 * j + 1]));
    }
    __m64 sum = _mm_add_pi32(s0, s1);
    sum = _mm_add_pi32(sum, _mm_unpackhi_pi32(sum, sum));
    return _mm_cvtsi64_si32(sum) + biases[0];

#elif defined(USE_NEON)
    int8x8_t* iv = (int8x8_t*)input;
    int8x8_t* row = (int8x8_t*)weights;
    int32x4_t sum = { biases[0] };
    for (unsigned j = 0; j < 2; j++) {
        int16x8_t prod = vmull_s8(iv[2 * j], row[2 * j]);
        prod = vmlal_s8(prod, iv[2 * j + 1], row[2 * j + 1]);
        sum = vpadalq_s16(sum, prod);
    }
    return sum[0] + sum[1] + sum[2] + sum[3];

#else
    *output = bias[0];
    for (unsigned j = 0; j < 32; j++)
        *output += weight[j] * input[j];
#endif

}


void NnueNetworkLayer::Propagate(clipped_t* input, int32_t* output)
{
#ifdef NEWSIMD
    myassert(inputdims == 32 || inputdims % 128 == 0, nullptr, 1, inputdims);
    myassert(outputdims % 8 == 0, nullptr, 1, outputdims);

#if defined(USE_AVX2)
    if (inputdims > 32) {
        __m128i* outVec = (__m128i*)output;
        __m128i* biasVec = (__m128i*)bias;
        __m256i* inVec = (__m256i*)input;
        for (unsigned i = 0; i < outputdims / 4; i++) {
            __m256i* w = (__m256i*) & weight[4 * i * inputdims];
            __m256i s0, s1, s2, s3;
            s0 = s1 = s2 = s3 = _mm256_setzero_si256();
            const __m256i kOnes = _mm256_set1_epi16(1);
            __m256i p1, p2;
            for (unsigned j = 0; j < inputdims / 64; j++) {
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
        for (unsigned i = 0; i < outputdims / 8; i++) {
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
    for (unsigned i = 0; i < outputdims / 4; i++) {
        __m128i* w = (__m128i*) & weight[4 * i * inputdims], p1, p2, s0, s1, s2, s3;
        s0 = s1 = s2 = s3 = _mm_setzero_si128();
        for (unsigned j = 0; j < inputdims / 32; j++) {
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
    __m128i* biasVec = (__m128i*)biases;
    __m128i* inVec = (__m128i*)input;
    for (unsigned i = 0; i < outDims / 4; i++) {
        __m128i* w = (__m128i*) & weights[4 * i * inDims], p, s0, s1, s2, s3;
        s0 = s1 = s2 = s3 = _mm_setzero_si128();
        for (unsigned j = 0; j < inDims / 8; j++) {
            p = _mm_madd_epi16(inVec[j], w[0 * inDims / 8 + j]);
            s0 = _mm_add_epi32(s0, p);
            p = _mm_madd_epi16(inVec[j], w[1 * inDims / 8 + j]);
            s1 = _mm_add_epi32(s1, p);
            p = _mm_madd_epi16(inVec[j], w[2 * inDims / 8 + j]);
            s2 = _mm_add_epi32(s2, p);
            p = _mm_madd_epi16(inVec[j], w[3 * inDims / 8 + j]);
            s3 = _mm_add_epi32(s3, p);
        }
        s0 = _mm_add_epi32(_mm_unpacklo_epi32(s0, s1), _mm_unpackhi_epi32(s0, s1));
        s2 = _mm_add_epi32(_mm_unpacklo_epi32(s2, s3), _mm_unpackhi_epi32(s2, s3));
        s0 = _mm_add_epi32(_mm_unpacklo_epi64(s0, s2), _mm_unpackhi_epi64(s0, s2));
        outVec[i] = _mm_add_epi32(s0, biasVec[i]);
    }

#elif defined(USE_MMX)
    __m64* outVec = (__m64*)output;
    __m64* biasVec = (__m64*)biases;
    __m64* inVec = (__m64*)input;
    for (unsigned i = 0; i < outDims / 2; i++) {
        __m64* w = (__m64*) & weights[2 * i * inDims], p, s0, s1, s2, s3;
        s0 = s1 = s2 = s3 = _mm_setzero_si64();
        for (unsigned j = 0; j < inDims / 8; j++) {
            p = _mm_madd_pi16(inVec[2 * j + 0], w[0 * inDims / 4 + 2 * j + 0]);
            s0 = _mm_add_pi32(s0, p);
            p = _mm_madd_pi16(inVec[2 * j + 0], w[1 * inDims / 4 + 2 * j + 0]);
            s1 = _mm_add_pi32(s1, p);
            p = _mm_madd_pi16(inVec[2 * j + 1], w[0 * inDims / 4 + 2 * j + 1]);
            s2 = _mm_add_pi32(s2, p);
            p = _mm_madd_pi16(inVec[2 * j + 1], w[1 * inDims / 4 + 2 * j + 1]);
            s3 = _mm_add_pi32(s3, p);
        }
        s0 = _mm_add_pi32(s0, s2);
        s1 = _mm_add_pi32(s1, s3);
        s0 = _mm_add_pi32(_mm_unpacklo_pi32(s0, s1), _mm_unpackhi_pi32(s0, s1));
        outVec[i] = _mm_add_pi32(s0, biasVec[i]);
    }

#elif defined(USE_NEON)
    int32x4_t* outVec = (int32x4_t*)output;
    int32x4_t* biasVec = (int32x4_t*)biases;
    int8x8_t* inVec = (int8x8_t*)input;
    int16x8_t p;
    for (unsigned i = 0; i < outDims / 4; i++) {
        int8x8_t* w = (int8x8_t*)&weights[4 * i * inDims];
        int32x4_t s0 = { 0 }, s1 = { 0 }, s2 = { 0 }, s3 = { 0 };
        for (unsigned j = 0; j < inDims / 16; j++) {
            p = vmull_s8(inVec[2 * j], w[0 * inDims / 8 + 2 * j]);
            p = vmlal_s8(p, inVec[2 * j + 1], w[0 * inDims / 8 + 2 * j + 1]);
            s0 = vpadalq_s16(s0, p);
            p = vmull_s8(inVec[2 * j], w[1 * inDims / 8 + 2 * j]);
            p = vmlal_s8(p, inVec[2 * j + 1], w[1 * inDims / 8 + 2 * j + 1]);
            s1 = vpadalq_s16(s1, p);
            p = vmull_s8(inVec[2 * j], w[2 * inDims / 8 + 2 * j]);
            p = vmlal_s8(p, inVec[2 * j + 1], w[2 * inDims / 8 + 2 * j + 1]);
            s2 = vpadalq_s16(s2, p);
            p = vmull_s8(inVec[2 * j], w[3 * inDims / 8 + 2 * j]);
            p = vmlal_s8(p, inVec[2 * j + 1], w[3 * inDims / 8 + 2 * j + 1]);
            s3 = vpadalq_s16(s3, p);
        }
        s0 = vpaddq_s32(s0, s1);
        s2 = vpaddq_s32(s2, s3);
        s0 = vpaddq_s32(s0, s2);
        outVec[i] = vaddq_s32(s0, biasVec[i]);
    }

#else
    for (unsigned i = 0; i < outputdims; i++) {
        unsigned int offset = i * inputdims;
        int32_t sum = bias[i];
        for (unsigned j = 0; j < inputdims; j++)
            sum += weight[offset + j] * input[j];
        output[i] = sum;
    }

#endif


#else
#if defined(USE_AVX2)
const unsigned numChunks = inputdims / 32;
__m256i* inVec = (__m256i*)input;
const __m256i kOnes = _mm256_set1_epi16(1);

#elif defined(USE_SSSE3)
const unsigned numChunks = inputdims / 16;
const __m128i kOnes = _mm_set1_epi16(1);
__m128i* inVec = (__m128i*)input;

#elif defined(USE_NEON)
    const unsigned numChunks = inputdims / 16;
    int8x8_t* inVec = (int8x8_t*)input;

#endif

    for (int i = 0; i < outputdims; ++i) {
        unsigned int offset = i * inputdims;

#if defined(USE_AVX2)

        __m256i sum = _mm256_setzero_si256();
        __m256i* row = (__m256i*) &weight[offset];

        for (unsigned j = 0; j < numChunks; j++) {
            __m256i product = _mm256_maddubs_epi16(inVec[j], row[j]);
            product = _mm256_madd_epi16(product, kOnes);
            sum = _mm256_add_epi32(sum, product);
        }

        __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, 0x4E)); //_MM_PERM_BADC
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, 0xB1)); //_MM_PERM_CDAB
        output[i] = _mm_cvtsi128_si32(sum128) + bias[i];

#elif defined(USE_SSSE3)
        __m128i sum = _mm_setzero_si128();
        __m128i* row = (__m128i*) &weight[offset];
        for (unsigned j = 0; j < numChunks - 1; j += 2) {
            __m128i product0 = _mm_maddubs_epi16(inVec[j], row[j]);
            product0 = _mm_madd_epi16(product0, kOnes);
            sum = _mm_add_epi32(sum, product0);
            __m128i product1 = _mm_maddubs_epi16(inVec[j + 1], row[j + 1]);
            product1 = _mm_madd_epi16(product1, kOnes);
            sum = _mm_add_epi32(sum, product1);
        }
        sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4E)); //_MM_PERM_BADC
        sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xB1)); //_MM_PERM_CDAB
        output[i] = _mm_cvtsi128_si32(sum) + bias[i];

#elif defined(USE_NEON)
        int32x4_t sum = { bias[i] };
        int8x8_t* row = (int8x8_t*)&weight[offset];
        for (unsigned j = 0; j < numChunks; j++) {
            int16x8_t product = vmull_s8(inVec[j * 2], row[j * 2]);
            product = vmlal_s8(product, inVec[j * 2 + 1], row[j * 2 + 1]);
            sum = vpadalq_s16(sum, product);
    }
        output[i] = sum[0] + sum[1] + sum[2] + sum[3];

#else
        int32_t sum = bias[i];
        for (int j = 0; j < inputdims; j++)
            sum += weight[offset + j] * input[j];
        output[i] = sum;

#endif
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

bool NnueClippedRelu::ReadWeights(ifstream* is)
{
    if (previous) return previous->ReadWeights(is);
    return true;
}

uint32_t NnueClippedRelu::GetHash()
{
    return NNUECLIPPEDRELUHASH + previous->GetHash();
}

void NnueClippedRelu::Propagate(int32_t *input, clipped_t *output)
{
#if defined(USE_AVX2)
#ifndef NEWSIMD
    const unsigned numChunks = dims / 32;
    const __m256i kZero = _mm256_setzero_si256();
    const __m256i kOffsets = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);
    __m256i* in = (__m256i*)input;
    __m256i* out = (__m256i*)output;
    for (unsigned i = 0; i < numChunks; i++) {
        __m256i words0 = _mm256_srai_epi16(_mm256_packs_epi32(
            in[i * 4 + 0], in[i * 4 + 1]), NnueClippingShift);
        __m256i words1 = _mm256_srai_epi16(_mm256_packs_epi32(
            in[i * 4 + 2], in[i * 4 + 3]), NnueClippingShift);
        out[i] = _mm256_permutevar8x32_epi32(_mm256_max_epi8(
            _mm256_packs_epi16(words0, words1), kZero), kOffsets);
    }
#else
    const unsigned numChunks = dims / 32;
    const __m256i kZero = _mm256_setzero_si256();
    __m256i* in = (__m256i*)input;
    __m256i* out = (__m256i*)output;
    for (unsigned i = 0; i < numChunks; i++) {
        __m256i words0 = _mm256_srai_epi16(_mm256_packs_epi32(
            in[i * 4 + 0], in[i * 4 + 1]), NnueClippingShift);
        __m256i words1 = _mm256_srai_epi16(_mm256_packs_epi32(
            in[i * 4 + 2], in[i * 4 + 3]), NnueClippingShift);
        out[i] = _mm256_max_epi8(_mm256_packs_epi16(words0, words1), kZero);
    }
#endif
#elif defined(USE_SSSE3)
    const unsigned numChunks = dims / 16;
    const __m128i k0x80s = _mm_set1_epi8(-128);
    __m128i* in = (__m128i*)input;
    __m128i* out = (__m128i*)output;
    for (unsigned i = 0; i < numChunks; i++) {
        __m128i words0 = _mm_srai_epi16(
            _mm_packs_epi32(in[i * 4 + 0], in[i * 4 + 1]), NnueClippingShift);
        __m128i words1 = _mm_srai_epi16(
            _mm_packs_epi32(in[i * 4 + 2], in[i * 4 + 3]), NnueClippingShift);
        __m128i packedbytes = _mm_packs_epi16(words0, words1);
        out[i] = _mm_subs_epi8(_mm_adds_epi8(packedbytes, k0x80s), k0x80s);
    }

#elif defined(USE_NEON)
    const unsigned numChunks = dims / 8;
    const int8x8_t kZero = { 0 };
    int32x4_t* in = (int32x4_t*)input;
    int8x8_t* out = (int8x8_t*)output;
    for (unsigned i = 0; i < numChunks; i++) {
        int16x8_t shifted;
        int16x4_t* pack = (int16x4_t*)&shifted;
        pack[0] = vqshrn_n_s32(in[i * 2 + 0], NnueClippingShift);
        pack[1] = vqshrn_n_s32(in[i * 2 + 1], NnueClippingShift);
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

bool NnueInputSlice::ReadWeights(ifstream* is)
{
    if (previous) return previous->ReadWeights(is);
    return true;
}

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

void NnueReadNet(ifstream* is)
{
    NnueReady = NnueDisabled;

    uint32_t fthash = NnueFt->GetHash();
    uint32_t nethash = NnueOut->GetHash();
    uint32_t filehash = (fthash ^ nethash);

    uint32_t version, hash, size;
    string sarchitecture;
    
    is->read((char*)&version, sizeof(uint32_t));
    is->read((char*)&hash, sizeof(uint32_t));
    is->read((char*)&size, sizeof(uint32_t));
    if (size)
    {
        sarchitecture.resize(size);
        is->read((char*)&sarchitecture[0], size);
    }

    NnueType nt;
    if (version == NNUEFILEVERSIONROTATE)
        nt = NnueRotate;
    else if (version == NNUEFILEVERSIONFLIP)
        nt = NnueFlip;
    else
        return;

    if (hash != filehash) return;

    is->read((char*)&hash, sizeof(uint32_t));
    if (hash != fthash) return;
    // Read the weights of the feature transformer
    if (!NnueFt->ReadWeights(is)) return;
    is->read((char*)&hash, sizeof(uint32_t));
    if (hash != nethash) return;
    // Read the weights of the network layers recursively
    if (!NnueOut->ReadWeights(is)) return;
    if (is->peek() != ios::traits_type::eof())
        return;

    NnueReady = nt;
}

// Explicit template instantiation
// This avoids putting these definitions in header file
template int chessposition::NnueGetEval<NnueRotate>();
template int chessposition::NnueGetEval<NnueFlip>();


#endif