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

//
// Some NNUE related constants and types
//

// PieceSquare indices
enum {
    PS_WPAWN    = 0 * 64,
    PS_BPAWN    = 1 * 64,
    PS_WKNIGHT  = 2 * 64,
    PS_BKNIGHT  = 3 * 64,
    PS_WBISHOP  = 4 * 64,
    PS_BBISHOP  = 5 * 64,
    PS_WROOK    = 6 * 64,
    PS_BROOK    = 7 * 64,
    PS_WQUEEN   = 8 * 64,
    PS_BQUEEN   = 9 * 64,
    PS_END      = 10 * 64
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

// The network architecture
NnueFeatureTransformer<NnueFtHalfdims, NnueFtInputdims> NnueFt;
NnueNetworkLayer<NnueFtOutputdims, NnueHidden1Dims> NnueHd1(&NnueFt);
NnueClippedRelu<NnueHidden1Dims> NnueCl1(&NnueHd1);
NnueNetworkLayer<NnueHidden1Dims, NnueHidden2Dims> NnueHd2(&NnueCl1);
NnueClippedRelu<NnueHidden2Dims> NnueCl2(&NnueHd2);
NnueNetworkLayer<NnueHidden2Dims, 1> NnueOut(&NnueCl2);


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

// Macros for NnueNetworkLayer Propagate for small layers
#if defined (USE_AVX2)
typedef __m256i sml_vec_t;
#define vec_setzero _mm256_setzero_si256
#define vec_set_32 _mm256_set1_epi32
#define vec_add_dpbusd_32 Simd::m256_add_dpbusd_32
#define vec_add_dpbusd_32x2 Simd::m256_add_dpbusd_32x2
#define vec_hadd Simd::m256_hadd
#define vec_haddx4 Simd::m256_haddx4

#elif defined (USE_SSSE3)
typedef __m128i sml_vec_t;
#define vec_setzero _mm_setzero_si128
#define vec_set_32 _mm_set1_epi32
#define vec_add_dpbusd_32 Simd::m128_add_dpbusd_32
#define vec_add_dpbusd_32x2 Simd::m128_add_dpbusd_32x2
#define vec_add_dpbusd_32x4 Simd::m128_add_dpbusd_epi32x4
#define vec_hadd Simd::m128_hadd
#define vec_haddx4 Simd::m128_haddx4
#endif

// Macros for Propagate for big layers and Feature ´transformation
#ifdef USE_AVX512
#define NUM_REGS 8
#define SIMD_WIDTH 512
typedef __m512i ft_vec_t, ftout_vec_t, in_vec_t, acc_vec_t, weight_vec_t, ft_vec_t;
typedef __m128i bias_vec_t;
#define vec_zero _mm512_setzero_si512()
#define vec_add_16(a,b) _mm512_add_epi16(a,b)
#define vec_sub_16(a,b) _mm512_sub_epi16(a,b)
#define vec_packs(a,b) _mm512_packs_epi16(a,b)
#define vec_clip_8(a,b) _mm512_permutexvar_epi64(_mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7) ,_mm512_max_epi8(vec_packs(a,b),_mm512_setzero_si512()))
#define vec_add_dpbusd_32x2_large Simd::m512_add_dpbusd_32x2
#define vec_haddx4_large Simd::m512_haddx4
#define vec_hadd_large Simd::m512_hadd

#elif defined(USE_AVX2)
#define NUM_REGS 16
#define SIMD_WIDTH 256
typedef __m256i ft_vec_t, ftout_vec_t, in_vec_t, acc_vec_t, weight_vec_t;
typedef __m128i bias_vec_t;
#define vec_zero _mm256_setzero_si256()
#define vec_add_16(a,b) _mm256_add_epi16(a,b)
#define vec_sub_16(a,b) _mm256_sub_epi16(a,b)
#define vec_packs(a,b) _mm256_packs_epi16(a,b)
#define vec_clip_8(a,b) _mm256_permute4x64_epi64(_mm256_max_epi8(vec_packs(a,b),_mm256_setzero_si256()), 0xd8)
#define vec_add_dpbusd_32x2_large Simd::m256_add_dpbusd_32x2
#define vec_haddx4_large Simd::m256_haddx4
#define vec_hadd_large Simd::m256_hadd

#elif defined(USE_SSE2)
#define NUM_REGS 16
#define SIMD_WIDTH 128
typedef __m128i ft_vec_t, ftout_vec_t;
#define k0x80s _mm_set1_epi8(-128)
#define vec_add_16(a,b) _mm_add_epi16(a,b)
#define vec_sub_16(a,b) _mm_sub_epi16(a,b)
#define vec_packs(a,b) _mm_packs_epi16(a,b)
#if defined(USE_SSSE3)
typedef __m128i ft_vec_t, ftout_vec_t, in_vec_t, acc_vec_t, weight_vec_t, bias_vec_t;
#define vec_zero _mm_setzero_si128()
#define vec_clip_8(a,b) vec_packs(_mm_max_epi16(a,_mm_setzero_si128()),_mm_max_epi16(b,_mm_setzero_si128()))
#define vec_add_dpbusd_32x2_large Simd::m128_add_dpbusd_32x2
#define vec_haddx4_large Simd::m128_haddx4
#define vec_hadd_large Simd::m128_hadd
#else // USE_SSSE3
#define vec_clip_8(a,b) _mm_subs_epi8(_mm_adds_epi8(_mm_packs_epi16(a, b), k0x80s), k0x80s)
#endif

#elif defined(USE_NEON)
#define NUM_REGS 16
#define SIMD_WIDTH 128
typedef int8x8_t in_vec_t, weight_vec_t;
typedef int16x8_t ft_vec_t;
typedef int8x16_t ftout_vec_t;
typedef int32x4_t acc_vec_t, bias_vec_t;
#define vec_zero {0}
#define vec_add_16(a,b) vaddq_s16(a,b)
#define vec_sub_16(a,b) vsubq_s16(a,b)
#define vec_packs(a,b) vcombine_s8(vqmovn_s16(a),vqmovn_s16(b))
#define vec_clip_8(a,b) vmaxq_s8(vec_packs(a,b),vdupq_n_s8(0))
#define vec_add_dpbusd_32x2_large Simd::neon_m128_add_dpbusd_epi32x2
#define vec_hadd_large Simd::neon_m128_hadd
#define vec_haddx4_large Simd::neon_m128_haddx4

#else
typedef int16_t ft_vec_t;
#endif


#ifdef USE_SIMD
#define TILE_HEIGHT (NUM_REGS * SIMD_WIDTH / 16)
#else
#define TILE_HEIGHT NnueFtHalfdims
#endif


template <NnueType Nt, Color c> void chessposition::UpdateAccumulator()
{
#ifdef USE_SIMD
    ft_vec_t acc[NUM_REGS];
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
            ft_vec_t* accTile = (ft_vec_t*)&accumulator[mslast].accumulation[c][i * TILE_HEIGHT];
            for (unsigned int j = 0; j < NUM_REGS; j++)
                acc[j] = accTile[j];
            for (unsigned int l = 0; pos2update[l] >= 0; l++)
            {
                // Difference calculation for the deactivated features
                for (unsigned int k = 0; k < removedIndices[l].size; k++)
                {
                    unsigned int index = removedIndices[l].values[k];
                    const unsigned int offset = NnueFtHalfdims * index + i * TILE_HEIGHT;
                    ft_vec_t* column = (ft_vec_t*)&NnueFt.weight[offset];
                    for (unsigned int j = 0; j < NUM_REGS; j++)
                        acc[j] = vec_sub_16(acc[j], column[j]);
                }

                // Difference calculation for the activated features
                for (unsigned int k = 0; k < addedIndices[l].size; k++)
                {
                    unsigned int index = addedIndices[l].values[k];
                    const unsigned int offset = NnueFtHalfdims * index + i * TILE_HEIGHT;
                    ft_vec_t* column = (ft_vec_t*)&NnueFt.weight[offset];
                    for (unsigned int j = 0; j < NUM_REGS; j++)
                        acc[j] = vec_add_16(acc[j], column[j]);
                }

                accTile = (ft_vec_t*)&accumulator[pos2update[l]].accumulation[c][i * TILE_HEIGHT];
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
                    accumulator[mslast].accumulation[c][j] -= NnueFt.weight[offset + j];
            }

            // Difference calculation for the activated features
            for (unsigned int k = 0; k < addedIndices[l].size; k++)
            {
                unsigned int index = addedIndices[l].values[k];
                const unsigned int offset = NnueFtHalfdims * index;

                for (unsigned int j = 0; j < NnueFtHalfdims; j++)
                    accumulator[mslast].accumulation[c][j] += NnueFt.weight[offset + j];
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
            ft_vec_t* ft_biases_tile = (ft_vec_t*)&NnueFt.bias[i * TILE_HEIGHT];
            for (unsigned int j = 0; j < NUM_REGS; j++)
                acc[j] = ft_biases_tile[j];

            for (unsigned int k = 0; k < activeIndices.size; k++)
            {
                unsigned int index = activeIndices.values[k];
                unsigned int offset = NnueFtHalfdims * index + i * TILE_HEIGHT;
                ft_vec_t* column = (ft_vec_t*)&NnueFt.weight[offset];
                for (unsigned int j = 0; j < NUM_REGS; j++)
                    acc[j] = vec_add_16(acc[j], column[j]);
            }

            ft_vec_t* accTile = (ft_vec_t*)&ac->accumulation[c][i * TILE_HEIGHT];
            for (unsigned int j = 0; j < NUM_REGS; j++)
                accTile[j] = acc[j];
        }
#else
        memcpy(ac->accumulation[c], NnueFt.bias, NnueFtHalfdims * sizeof(int16_t));

        for (unsigned int k = 0; k < activeIndices.size; k++)
        {
            unsigned int index = activeIndices.values[k];
            unsigned int offset = NnueFtHalfdims * index;

            for (unsigned int j = 0; j < NnueFtHalfdims; j++)
                ac->accumulation[c][j] += NnueFt.weight[offset + j];
        }
#endif
    }
}


template <NnueType Nt> void chessposition::Transform(clipped_t *output)
{
    UpdateAccumulator<Nt, WHITE>();
    UpdateAccumulator<Nt, BLACK>();

    const int perspectives[2] = { state & S2MMASK, !(state & S2MMASK) };
    for (int p = 0; p < 2; p++)
    {
        const unsigned int offset = NnueFtHalfdims * p;
        const ft_vec_t* acc = (ft_vec_t*)&accumulator[ply].accumulation[perspectives[p]][0];

#ifdef USE_SIMD
        constexpr unsigned int numChunks = (16 * NnueFtHalfdims) / SIMD_WIDTH;
        ftout_vec_t* out = (ftout_vec_t*)&output[offset];
        for (unsigned int i = 0; i < numChunks / 2; i++) {
            ft_vec_t s0 = acc[i * 2];
            ft_vec_t s1 = acc[i * 2 + 1];
            out[i] = vec_clip_8(s0, s1);
        }
#else
        for (unsigned int i = 0; i < NnueFtHalfdims; i++) {
            int16_t sum = acc[i];
            output[offset + i] = (clipped_t)max<int16_t>(0, min<int16_t>(127, sum));
        }
#endif

#ifdef NNUEDEBUG
        cout << "\ninput layer (p=" << p << "):\n";
        for (unsigned int i = 0; i < NnueFtHalfdims; i++) {
            cout << hex << setfill('0') << setw(2) << (int)output[offset + i] << " ";
            if (i % 16 == 15)
                cout << "   " << hex << setfill('0') << setw(3) << (int)(i / 16 * 16) << "\n";
        }
        cout << dec;
#endif
    }
}


eval NnueValueScale = 64;


template <NnueType Nt> int chessposition::NnueGetEval()
{
    Transform<Nt>(network.input);
    NnueHd1.Propagate(network.input, network.hidden1_values);
    NnueCl1.Propagate(network.hidden1_values, network.hidden1_clipped);
    NnueHd2.Propagate(network.hidden1_clipped, network.hidden2_values);
    NnueCl1.Propagate(network.hidden2_values, network.hidden2_clipped);
    NnueOut.Propagate(network.hidden2_clipped, &network.out_value);

    return network.out_value * NnueValueScale / 1024;
}


//
// FeatureTransformer
//

template <int ftdims, int inputdims>
bool NnueFeatureTransformer<ftdims, inputdims>::ReadFeatureWeights(NnueNetsource_t is)
{
    int i, j;
    for (i = 0; i < ftdims; ++i)
        NNUEREAD(is, (char*)&bias[i], sizeof(int16_t));

    int weightsRead = 0;
    for (i = 0; i < inputdims; i++)
    {
        if (bpz && i % (10 * 64) == 0) {
            int16_t dummyweight;
            for (j = 0; j < ftdims; ++j)
                NNUEREAD(is, (char*)&dummyweight, sizeof(int16_t));
        }
        for (j = 0; j < ftdims; ++j) {
            NNUEREAD(is, (char*)&weight[weightsRead], sizeof(int16_t));
            weightsRead++;
        }
    }

    return !NNUEREADFAIL(is);
}

#ifdef EVALOPTIONS
template <int ftdims, int inputdims>
void NnueFeatureTransformer<ftdims, inputdims>::WriteFeatureWeights(ofstream* os)
{
    int i, j;
    for (i = 0; i < ftdims; ++i)
        os->write((char*)&bias[i], sizeof(int16_t));

    int weightsRead = 0;
    for (i = 0; i < inputdims; i++)
    {
        if (bpz && i % (10 * 64) == 0) {
            int16_t dummyweight = 0;
            for (j = 0; j < ftdims; ++j)
                os->write((char*)&dummyweight, sizeof(int16_t));
        }
        for (j = 0; j < ftdims; ++j) {
            os->write((char*)&weight[weightsRead], sizeof(int16_t));
            weightsRead++;
        }
    }
}
#endif


//
// NetworkLayer
//

template <unsigned int inputdims, unsigned int outputdims>
bool NnueNetworkLayer<inputdims, outputdims>::ReadWeights(NnueNetsource_t is)
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
            idx = shuffleWeightIndex(idx);
            weight[idx] = *w++;
        }

    free(weightbuffer);

    return !NNUEREADFAIL(is);
}

#ifdef EVALOPTIONS
template <unsigned int inputdims, unsigned int outputdims>
void NnueNetworkLayer<inputdims, outputdims>::WriteWeights(ofstream* os)
{
    if (previous)
        previous->WriteWeights(os);

    for (unsigned int i = 0; i < outputdims; ++i)
        os->write((char*)&bias[i], sizeof(int32_t));

    for (unsigned int i = 0; i < outputdims * inputdims; i++)
            os->write((char*)&weight[shuffleWeightIndex(i)], sizeof(char));
}
#endif


template <unsigned int inputdims, unsigned int outputdims>
void NnueNetworkLayer<inputdims, outputdims>::Propagate(clipped_t* input, int32_t* output)
{
    if (paddedInputdims < 128)
#if defined (USE_SSSE3)
    {
        // Small Layer fast propagation
        if (outputdims % 8 == 0)
        {
            constexpr unsigned int numChunks = paddedInputdims / 4;

            const int32_t* input32 = (int32_t*)input;
            const sml_vec_t* biasvec = (sml_vec_t*)bias;
            sml_vec_t acc[NumOutputRegsSmall];
            for (unsigned int k = 0; k < NumOutputRegsSmall; ++k)
                acc[k] = biasvec[k];

            for (unsigned int i = 0; i < numChunks; i += 2)
            {
                const sml_vec_t in0 = vec_set_32(input32[i + 0]);
                const sml_vec_t in1 = vec_set_32(input32[i + 1]);
                const sml_vec_t* col0 = (sml_vec_t*)(&weight[(i + 0) * outputdims * 4]);
                const sml_vec_t* col1 = (sml_vec_t*)(&weight[(i + 1) * outputdims * 4]);
                for (unsigned int k = 0; k < NumOutputRegsSmall; ++k)
                    vec_add_dpbusd_32x2(acc[k], in0, col0[k], in1, col1[k]);
            }

            sml_vec_t* outptr = (sml_vec_t*)output;
            for (unsigned int k = 0; k < NumOutputRegsSmall; ++k)
                outptr[k] = acc[k];
}
        else {
            constexpr unsigned int numChunks = paddedInputdims / SimdWidth;
            const sml_vec_t* inputVector = (sml_vec_t*)input;

            sml_vec_t sum0 = vec_setzero();
            sml_vec_t* row0 = (sml_vec_t*)&weight[0];

            for (int j = 0; j < (int)numChunks; ++j)
            {
                const sml_vec_t in = inputVector[j];
                vec_add_dpbusd_32(sum0, in, row0[j]);
            }
            output[0] = vec_hadd(sum0, bias[0]);
        }
    }
#else
        PropagateNative(input, output);
#endif
    if (paddedInputdims >= 128)
#if  defined (USE_SSSE3) || defined(USE_NEON)
    {
        // Big Layer fast propagation
        const in_vec_t* invec = (in_vec_t*)input;

        // Perform accumulation to registers for each big block
        for (unsigned int bigBlock = 0; bigBlock < NumBigBlocks; ++bigBlock)
        {
            acc_vec_t acc[NumOutputRegsBig] = { vec_zero };

            // Each big block has NumOutputRegs small blocks in each "row", one per register.
            // We process two small blocks at a time to save on one addition without VNNI.
            for (unsigned int smallBlock = 0; smallBlock < NumSmallBlocksPerOutput; smallBlock += 2)
            {
                const weight_vec_t* weightvec = (weight_vec_t*)(weight + bigBlock * BigBlockSize + smallBlock * SmallBlockSize * NumOutputRegsBig);
                const in_vec_t in0 = invec[smallBlock + 0];
                const in_vec_t in1 = invec[smallBlock + 1];

                for (unsigned int k = 0; k < NumOutputRegsBig; ++k)
                    vec_add_dpbusd_32x2_large(acc[k], in0, weightvec[k], in1, weightvec[k + NumOutputRegsBig]);
            }

            // Horizontally add all accumulators.
            if (NumOutputRegsBig % 4 == 0)
            {
                bias_vec_t* outputvec = (bias_vec_t*)output;
                const bias_vec_t* biasvec = (bias_vec_t*)bias;

                for (unsigned int k = 0; k < NumOutputRegsBig; k += 4)
                {
                    const unsigned int idx = (bigBlock * NumOutputRegsBig + k) / 4;
                    outputvec[idx] = vec_haddx4_large(acc[k + 0], acc[k + 1], acc[k + 2], acc[k + 3], biasvec[idx]);
                }
            }
            else
            {
                for (unsigned int k = 0; k < NumOutputRegsBig; ++k)
                {
                    const unsigned int idx = (bigBlock * NumOutputRegsBig + k);
                    output[idx] = vec_hadd_large(acc[k], bias[idx]);
                }
            }
        }
    }
#else
        PropagateNative(input, output);
#endif

#ifdef NNUEDEBUG
    cout << "\nnetwork layer:\n";
    for (unsigned int i = 0; i < outputdims; i++) {
        cout << dec << setfill(' ') << setw(6) << (int)output[i] << " ";
        if (i % 16 == 15 || (i + 1 == outputdims))
            cout << "   " << hex << setfill('0') << setw(3) << (int)(i / 16 * 16) << "\n";
    }
    cout << dec;
#endif

}


template <unsigned int inputdims, unsigned int outputdims>
void NnueNetworkLayer<inputdims, outputdims>::PropagateNative(clipped_t* input, int32_t* output)
{
# if defined(USE_SSE2)
    // At least a multiple of 16, with SSE2.
    const unsigned int numChunks = paddedInputdims / 16;
    const __m128i Zeros = _mm_setzero_si128();
    const __m128i* inVec = (__m128i*)input;
# elif defined(USE_NEON)
    const unsigned int numChunks = paddedInputdims / 16;
    const int8x8_t* inVec = (int8x8_t*)input;
# endif
    for (unsigned int i = 0; i < outputdims; ++i) {
        unsigned int offset = i * inputdims;
# if defined(USE_SSE2)
        __m128i sumLo = _mm_cvtsi32_si128(bias[i]);
        __m128i sumHi = Zeros;
        const __m128i* row = (__m128i*)&weight[offset];
        for (unsigned int j = 0; j < numChunks; ++j) {
            __m128i row_j = _mm_load_si128(&row[j]);
            __m128i input_j = _mm_load_si128(&inVec[j]);
            __m128i extendedRowLo = _mm_srai_epi16(_mm_unpacklo_epi8(row_j, row_j), 8);
            __m128i extendedRowHi = _mm_srai_epi16(_mm_unpackhi_epi8(row_j, row_j), 8);
            __m128i extendedInputLo = _mm_unpacklo_epi8(input_j, Zeros);
            __m128i extendedInputHi = _mm_unpackhi_epi8(input_j, Zeros);
            __m128i productLo = _mm_madd_epi16(extendedRowLo, extendedInputLo);
            __m128i productHi = _mm_madd_epi16(extendedRowHi, extendedInputHi);
            sumLo = _mm_add_epi32(sumLo, productLo);
            sumHi = _mm_add_epi32(sumHi, productHi);
        }
        __m128i sum = _mm_add_epi32(sumLo, sumHi);
        __m128i sumHigh_64 = _mm_shuffle_epi32(sum, _MM_SHUFFLE(1, 0, 3, 2));
        sum = _mm_add_epi32(sum, sumHigh_64);
        __m128i sum_second_32 = _mm_shufflelo_epi16(sum, _MM_SHUFFLE(1, 0, 3, 2));
        sum = _mm_add_epi32(sum, sum_second_32);
        output[i] = _mm_cvtsi128_si32(sum);
# elif defined(USE_NEON)
        int32x4_t sum = { bias[i] };
        const int8x8_t* row = (int8x8_t*)&weight[offset];
        for (unsigned int j = 0; j < numChunks; ++j) {
            int16x8_t product = vmull_s8(inVec[j * 2], row[j * 2]);
            product = vmlal_s8(product, inVec[j * 2 + 1], row[j * 2 + 1]);
            sum = vpadalq_s16(sum, product);
        }
        output[i] = sum[0] + sum[1] + sum[2] + sum[3];
# else
        int32_t sum = bias[i];
        for (unsigned int j = 0; j < inputdims; ++j) {
            sum += weight[offset + j] * input[j];
        }
        output[i] = sum;
# endif
    }
}



//
// ClippedRelu
//

template <unsigned int dims>
void NnueClippedRelu<dims>::Propagate(int32_t *input, clipped_t *output)
{
#ifdef USE_AVX2
    if (dims % SimdWidth == 0) {
            const unsigned int numChunks = dims / SimdWidth;
            const __m256i Zero = _mm256_setzero_si256();
            const __m256i Offsets = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);
            const __m256i* in = (__m256i*)input;
            __m256i* out = (__m256i*)output;
            for (unsigned int i = 0; i < numChunks; ++i) {
                const __m256i words0 = _mm256_srai_epi16(_mm256_packs_epi32(
                    _mm256_load_si256(&in[i * 4 + 0]),
                    _mm256_load_si256(&in[i * 4 + 1])), NnueClippingShift);
                const __m256i words1 = _mm256_srai_epi16(_mm256_packs_epi32(
                    _mm256_load_si256(&in[i * 4 + 2]),
                    _mm256_load_si256(&in[i * 4 + 3])), NnueClippingShift);
                _mm256_store_si256(&out[i], _mm256_permutevar8x32_epi32(_mm256_max_epi8(
                    _mm256_packs_epi16(words0, words1), Zero), Offsets));
        }
    }
    else {
        const unsigned int numChunks = dims / (SimdWidth / 2);
        const __m128i Zero = _mm_setzero_si128();
        __m128i* in = (__m128i*)input;
        __m128i* out = (__m128i*)output;
        for (unsigned int i = 0; i < numChunks; i++) {
            const __m128i words0 = _mm_srai_epi16(_mm_packs_epi32(
                _mm_load_si128(&in[i * 4 + 0]),
                _mm_load_si128(&in[i * 4 + 1])), NnueClippingShift);
            const __m128i words1 = _mm_srai_epi16(_mm_packs_epi32(
                _mm_load_si128(&in[i * 4 + 2]),
                _mm_load_si128(&in[i * 4 + 3])), NnueClippingShift);
            const __m128i packedbytes = _mm_packs_epi16(words0, words1);
            _mm_store_si128(&out[i], _mm_max_epi8(packedbytes, Zero));
        }
    }
#elif defined(USE_SSE2)
    const unsigned int numChunks = dims / SimdWidth;
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
    for (unsigned int i = 0; i < dims; i++)
        output[i] = max(0, min(127, input[i] >> NnueClippingShift));
#endif

#ifdef NNUEDEBUG
    cout << "\nclipped relu:\n";
    for (unsigned int i = 0; i < dims; i++) {
        cout << hex << setfill('0') << setw(2) << (int)output[i] << " ";
        if (i % 16 == 15 || (i + 1 == dims))
            cout << "   " << hex << setfill('0') << setw(3) << (int)(i / 16 * 16) << "\n";
    }
    cout << dec;
#endif
}


//
// Global Interface
//
void NnueInit()
{
}

void NnueRemove()
{
}

bool NnueReadNet(NnueNetsource_t is)
{
    NnueReady = NnueDisabled;

    uint32_t fthash = NnueFt.GetFtHash();
    uint32_t nethash = NnueOut.GetHash();
    uint32_t filehash = (fthash ^ nethash);

    uint32_t version, hash, size;
    string sarchitecture;

    NNUEREAD(is, (char*)&version, sizeof(uint32_t));
    NNUEREAD(is, (char*)&hash, sizeof(uint32_t));
    NNUEREAD(is, (char*)&size, sizeof(uint32_t));

    sarchitecture.resize(size);
    NNUEREAD(is, (char*)&sarchitecture[0], size);

    NnueType nt;
    switch (version) {
    case NNUEFILEVERSIONROTATE:
        NnueFt.bpz = true;
        nt = NnueRotate;
        break;
    case NNUEFILEVERSIONNOBPZ:
        NnueFt.bpz = false;
        nt = NnueRotate;
        break;
    case NNUEFILEVERSIONFLIP:
        NnueFt.bpz = false;
        nt = NnueFlip;
        break;
    default:
        return false;
    }

    if (hash != filehash)
        return false;

    // Read the weights of the feature transformer
    NNUEREAD(is, (char*)&hash, sizeof(uint32_t));
    if (hash != fthash) return false;
    if (!NnueFt.ReadFeatureWeights(is)) return false;

    // Read the weights of the network layers recursively
    NNUEREAD(is, (char*)&hash, sizeof(uint32_t));
    if (hash != nethash) return false;
    if (!NnueOut.ReadWeights(is)) return false;

    if (!NNUEEOF(is))
        return false;

    NnueReady = nt;

    return true;
}

#ifdef EVALOPTIONS
void NnueWriteNet(vector<string> args)
{
    size_t ci = 0;
    size_t cs = args.size();
    string NnueNetPath = "export.nnue";
    int rescale = 0;
    NnueFt.bpz = true;
    if (ci < cs)
        NnueNetPath = args[ci++];

    while (ci < cs) {
        if (args[ci] == "rescale" && ++ci < cs)
        {
            rescale = stoi(args[ci++]);
        }
        if (args[ci] == "nobpz")
        {
            NnueFt.bpz = false;
            ci++;
        }
    }
    ofstream os;
    os.open(NnueNetPath, ios::binary);
    if (!os && en.ExecPath != "")
        os.open(en.ExecPath + NnueNetPath, ios::binary);

    if (!os) {
        cout << "Cannot write file " << NnueNetPath << "\n";
        return;
    }

    if (rescale)
    {
        NnueOut.bias[0] = (int32_t)round(NnueOut.bias[0] * rescale / NnueValueScale);
        for (int i = 0; i < NnueHidden2Dims; i++)
        {
            NnueOut.weight[i] = (weight_t)(NnueOut.weight[i] * rescale / NnueValueScale);
        }
    }

    uint32_t fthash = NnueFt.GetFtHash();
    uint32_t nethash = NnueOut.GetHash();
    uint32_t filehash = (fthash ^ nethash);
    uint32_t version;
    string sarchitecture;
    if (NnueFt.bpz) {
        sarchitecture = "Features=HalfKP(Friend)[41024->256x2],Network=AffineTransform[1<-32](ClippedReLU[32](AffineTransform[32<-32](ClippedReLU[32](AffineTransform[32<-512](InputSlice[512(0:512)])))))";
        version = NNUEFILEVERSIONROTATE;
    }
    else {
        sarchitecture = "Features=HalfKP(Friend)[40960->256x2],Network=AffineTransform[1<-32](ClippedReLU[32](AffineTransform[32<-32](ClippedReLU[32](AffineTransform[32<-512](InputSlice[512(0:512)])))))";
        version = NNUEFILEVERSIONNOBPZ;
    }
    uint32_t size = (uint32_t)sarchitecture.size();

    os.write((char*)&version, sizeof(uint32_t));
    os.write((char*)&filehash, sizeof(uint32_t));
    os.write((char*)&size, sizeof(uint32_t));
    os.write((char*)&sarchitecture[0], size);

    os.write((char*)&fthash, sizeof(uint32_t));
    NnueFt.WriteFeatureWeights(&os);

    os.write((char*)&nethash, sizeof(uint32_t));
    NnueOut.WriteWeights(&os);

    os.close();

    cout << "Network written to file " << NnueNetPath << "\n";
}

void NnueRegisterEvals()
{
    // Expose weights and bias of ouput layer as UCI options for tuning
    en.ucioptions.Register(&NnueValueScale, "NnueValueScale", ucinnuebias, to_string(NnueValueScale), INT_MIN, INT_MAX, nullptr);
    en.ucioptions.Register(&NnueOut.bias[0], "NnueOutBias", ucinnuebias, to_string(NnueOut.bias[0]), INT_MIN, INT_MAX, nullptr);
    for (int i = 0; i < NnueHidden2Dims; i++)
    {
        ostringstream osName;
        osName << "NnueOutWeight_" << setw(2) << setfill('0') << to_string(i);
        en.ucioptions.Register(&NnueOut.weight[i], osName.str(), ucinnueweight, to_string(NnueOut.weight[i]), CHAR_MIN, CHAR_MAX, nullptr);
    }
}
#endif

// Explicit template instantiation
// This avoids putting these definitions in header file
template int chessposition::NnueGetEval<NnueRotate>();
template int chessposition::NnueGetEval<NnueFlip>();
