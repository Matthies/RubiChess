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
    PS_KING     = 10 * 64,
    PS_KPEND    = 10 * 64,
    PS_KAEND    = 11 * 64
};

// table to translate PieceCode to PieceSquare index for both POVs respecting the piece order special to RubiChess
uint32_t PieceToIndex[2][16] = {
  { 0, 0, PS_WPAWN, PS_BPAWN, PS_WKNIGHT, PS_BKNIGHT, PS_WBISHOP, PS_BBISHOP, PS_WROOK, PS_BROOK, PS_WQUEEN, PS_BQUEEN, PS_KING, PS_KING, 0, 0 },
  { 0, 0, PS_BPAWN, PS_WPAWN, PS_BKNIGHT, PS_WKNIGHT, PS_BBISHOP, PS_WBISHOP, PS_BROOK, PS_WROOK, PS_BQUEEN, PS_WQUEEN, PS_KING, PS_KING, 0, 0 }
};

// table for horizontal mirroring of king buckets
static constexpr int KingBucket[64] = {
  -1, -1, -1, -1, 31, 30, 29, 28,
  -1, -1, -1, -1, 27, 26, 25, 24,
  -1, -1, -1, -1, 23, 22, 21, 20,
  -1, -1, -1, -1, 19, 18, 17, 16,
  -1, -1, -1, -1, 15, 14, 13, 12,
  -1, -1, -1, -1, 11, 10,  9,  8,
  -1, -1, -1, -1,  7,  6,  5,  4,
  -1, -1, -1, -1,  3,  2,  1,  0
};


//
// Global objects
//
NnueType NnueReady = NnueDisabled;
eval NnueValueScale = 64;
NnueArchitecture* NnueCurrentArch;


// The network architecture V1
class NnueArchitectureV1 : public NnueArchitecture {
public:
    static constexpr unsigned int NnueFtHalfdims = 256;
    static constexpr unsigned int NnueFtOutputdims = NnueFtHalfdims * 2;
    static constexpr unsigned int NnueFtInputdims = 64 * 10 * 64;   // (kingsquare x piecetype x piecesquare)
    static constexpr unsigned int NnueHidden1Dims = 32;
    static constexpr unsigned int NnueHidden2Dims = 32;
    static constexpr unsigned int NnuePsqtBuckets = 0;
    static constexpr unsigned int NnueLayerStacks = 1;
    static constexpr unsigned int NnueClippingShift = 6;
    static constexpr size_t networkfilesize =   // expected number of bytes remaining after architecture string
        sizeof(uint32_t)                                        // Ft hash
        + NnueFtHalfdims * sizeof(int16_t)                      // bias of feature layer
        + NnueFtInputdims * NnueFtHalfdims * sizeof(int16_t)    // weights of feature layer
        + sizeof(uint32_t)                                      // Network layer hash
        + NnueHidden1Dims * sizeof(int32_t)                     // bias of hidden layer 1
        + NnueFtOutputdims * NnueHidden1Dims * sizeof(int8_t)   // weights of hidden layer 1
        + NnueHidden2Dims * sizeof(int32_t)                     // bias of hidden layer 2
        + NnueHidden1Dims * NnueHidden2Dims * sizeof(int8_t)    // weights of hidden layer 2
        + 1 * sizeof(int32_t)                                   // bias of output layer
        + NnueHidden2Dims * 1 * sizeof(int8_t);                 // weights of output layer


    NnueFeatureTransformer<NnueFtHalfdims, NnueFtInputdims, NnuePsqtBuckets> NnueFt;
    class NnueLayerStack {
    public:
        NnueNetworkLayer<NnueFtOutputdims, NnueHidden1Dims> NnueHd1;
        NnueClippedRelu<NnueHidden1Dims, NnueClippingShift> NnueCl1;
        NnueNetworkLayer<NnueHidden1Dims, NnueHidden2Dims> NnueHd2;
        NnueClippedRelu<NnueHidden2Dims, NnueClippingShift> NnueCl2;
        NnueNetworkLayer<NnueHidden2Dims, 1> NnueOut;
        NnueLayerStack() : NnueHd1(nullptr), NnueCl1(&NnueHd1), NnueHd2(&NnueCl1), NnueCl2(&NnueHd2), NnueOut(&NnueCl2) {}
    } LayerStack[NnueLayerStacks];

    NnueArchitectureV1() {
        LayerStack[0].NnueHd1.previous = &NnueFt;
    }
    uint32_t GetFtHash() {
        return NnueFt.GetFtHash(NnueArchV1) ^ NnueFtOutputdims;
    }
    uint32_t GetHash() {
        return LayerStack[0].NnueOut.GetHash();
    }

    bool ReadFeatureWeights(NnueNetsource* nr, bool bpz) {
        return NnueFt.ReadFeatureWeights(nr, bpz);
    }
    bool ReadWeights(NnueNetsource* nr, uint32_t nethash) {
        uint32_t hash;
        bool okay = nr->read((unsigned char*)&hash, sizeof(uint32_t))
            && hash == nethash
            && LayerStack[0].NnueOut.ReadWeights(nr);
        return okay;
    }
    void WriteFeatureWeights(NnueNetsource* nr, bool bpz) {
        NnueFt.WriteFeatureWeights(nr, bpz);
    }
    void WriteWeights(NnueNetsource* nr, uint32_t nethash) {
        nr->write((unsigned char*)&nethash, sizeof(uint32_t));
        LayerStack[0].NnueOut.WriteWeights(nr);
    }
    void RescaleLastLayer(int ratio64) {
        LayerStack[0].NnueOut.bias[0] = (int32_t)round(LayerStack[0].NnueOut.bias[0] * ratio64 / NnueValueScale);
        for (unsigned int i = 0; i < NnueHidden2Dims; i++)
            LayerStack[0].NnueOut.weight[i] = (int32_t)round(LayerStack[0].NnueOut.weight[i] * ratio64 / NnueValueScale);
    }
    string GetArchName() {
        return "V1";
    }
    string GetArchDescription() {
        return "Features=HalfKP(Friend)[40960->256x2],Network=AffineTransform[1<-32](ClippedReLU[32](AffineTransform[32<-32](ClippedReLU[32](AffineTransform[32<-512](InputSlice[512(0:512)])))))";
    }
    int GetEval(chessposition *pos) {
        struct NnueNetwork {
            alignas(64) clipped_t input[NnueFtOutputdims];
            alignas(64) int32_t hidden1_values[NnueHidden1Dims];
            alignas(64) int32_t hidden2_values[NnueHidden2Dims];
            alignas(64) clipped_t hidden1_clipped[NnueHidden1Dims];
            alignas(64) clipped_t hidden2_clipped[NnueHidden2Dims];
            alignas(64) int32_t out_value;
        } network;

        pos->Transform<NnueArchV1, NnueFtHalfdims, NnuePsqtBuckets>(network.input);
        LayerStack[0].NnueHd1.Propagate(network.input, network.hidden1_values);
        LayerStack[0].NnueCl1.Propagate(network.hidden1_values, network.hidden1_clipped);
        LayerStack[0].NnueHd2.Propagate(network.hidden1_clipped, network.hidden2_values);
        LayerStack[0].NnueCl1.Propagate(network.hidden2_values, network.hidden2_clipped);
        LayerStack[0].NnueOut.Propagate(network.hidden2_clipped, &network.out_value);

        return network.out_value * NnueValueScale / 1024;
    }
    int16_t* GetFeatureWeight() {
        return NnueFt.weight;
    }
    int16_t* GetFeatureBias() {
        return NnueFt.bias;
    }
    int32_t* GetFeaturePsqtWeight() {
        return nullptr;
    }
    uint32_t GetFileVersion() {
        return NNUEFILEVERSIONNOBPZ;    // always write networks without BPZ
    }
    int16_t* CreateAccumulationStack() {
        return(int16_t*)allocalign64(MAXDEPTH * 2 * NnueFtOutputdims * sizeof(int16_t));
    }
    int32_t* CreatePsqtAccumulationStack() {
        return nullptr;
    }
    unsigned int GetAccumulationSize() {
        return NnueFtOutputdims;
    }
    unsigned int GetPsqtAccumulationSize() {
        return 0;
    }
    size_t GetNetworkFilesize() {
        return networkfilesize;
    }
#ifdef STATISTICS
    void SwapInputNeurons(unsigned int i1, unsigned int i2) {
        // not supported for V1
        (void)i1;
        (void)i2;
    }
    void Statistics(bool verbose, bool sort) {
        // not supported for V1
        (void)verbose;
        (void)sort;
    }
#endif
};

template <unsigned int NnueFtOutputdims>
class NnueArchitectureV5 : public NnueArchitecture {
public:
    static constexpr unsigned int NnueFtHalfdims = NnueFtOutputdims;
    static constexpr unsigned int NnueFtInputdims = 64 * 11 * 64 / 2;
    static constexpr unsigned int NnueHidden1Dims = 16;
    static constexpr unsigned int NnueHidden1Out = 15;
    static constexpr unsigned int NnueHidden2Dims = 32;
    static constexpr unsigned int NnueClippingShift = 6;
    static constexpr unsigned int NnuePsqtBuckets = 8;
    static constexpr unsigned int NnueLayerStacks = 8;
    static constexpr size_t networkfilesize =   // expected number of bytes remaining after architecture string
        sizeof(uint32_t)                                            // Ft hash
        + NnueFtOutputdims * sizeof(int16_t)                        // bias of feature layer
        + NnueFtOutputdims * NnueFtInputdims * sizeof(int16_t)      // weights of feature layer
        + NnueFtInputdims * NnuePsqtBuckets * sizeof(int32_t)       // psqt bucket weights
        + NnueLayerStacks * (
            sizeof(uint32_t)                                        // Network layer hash
            + NnueHidden1Dims * sizeof(int32_t)                     // bias of hidden layer 1
            + NnueFtOutputdims * NnueHidden1Dims * sizeof(int8_t)   // weights of hidden layer 1
            + NnueHidden2Dims * sizeof(int32_t)                     // bias of hidden layer 2
            + NnueHidden1Dims * 2 * NnueHidden2Dims * sizeof(int8_t) // weights of hidden layer 2
            + 1 * sizeof(int32_t)                                   // bias of output layer
            + NnueHidden2Dims * 1 * sizeof(int8_t)                  // weights of output layer
            );

    NnueFeatureTransformer<NnueFtHalfdims, NnueFtInputdims, NnuePsqtBuckets> NnueFt;
    class NnueLayerStack {
    public:
        NnueNetworkLayer<NnueFtOutputdims, NnueHidden1Dims> NnueHd1;
        NnueSqrClippedRelu<NnueHidden1Dims> NnueSqrCl;
        NnueClippedRelu<NnueHidden1Dims, NnueClippingShift> NnueCl1;
        NnueNetworkLayer<NnueHidden1Out * 2, NnueHidden2Dims> NnueHd2;
        NnueClippedRelu<NnueHidden2Dims, NnueClippingShift> NnueCl2;
        NnueNetworkLayer<NnueHidden2Dims, 1> NnueOut;
        NnueLayerStack() : NnueHd1(nullptr), NnueSqrCl(&NnueHd1), NnueCl1(&NnueHd1), NnueHd2(&NnueCl1), NnueCl2(&NnueHd2), NnueOut(&NnueCl2) {}
    } LayerStack[NnueLayerStacks];

    NnueArchitectureV5() {
        for (unsigned int i = 0; i < NnueLayerStacks; i++)
            LayerStack[i].NnueHd1.previous = &NnueFt;
    }
    uint32_t GetFtHash() {
        return NnueFt.GetFtHash(NnueArchV5) ^ (NnueFtOutputdims * 2);
    }
    uint32_t GetHash() {
        return LayerStack[0].NnueOut.GetHash();
    }
    bool ReadFeatureWeights(NnueNetsource* nr, bool bpz) {
        return NnueFt.ReadFeatureWeights(nr, bpz);
    }
    bool ReadWeights(NnueNetsource* nr, uint32_t nethash) {
        bool okay = true;
        for (unsigned int i = 0; okay && i < NnueLayerStacks; i++) {
            uint32_t hash;
            okay = nr->read((unsigned char*)&hash, sizeof(uint32_t))
                && hash == nethash
                && LayerStack[i].NnueOut.ReadWeights(nr);
        }
        return okay;
    }
    void WriteFeatureWeights(NnueNetsource* nr, bool bpz) {
        NnueFt.WriteFeatureWeights(nr, bpz);
    }
    void WriteWeights(NnueNetsource* nr, uint32_t nethash) {
        for (unsigned int i = 0; i < NnueLayerStacks; i++) {
            nr->write((unsigned char*)&nethash, sizeof(uint32_t));
            LayerStack[i].NnueOut.WriteWeights(nr);
        }
    }
    void RescaleLastLayer(int ratio64) {
        for (unsigned int b = 0; b < NnueLayerStacks; b++) {
            LayerStack[b].NnueOut.bias[0] = (int32_t)round(LayerStack[b].NnueOut.bias[0] * ratio64 / NnueValueScale);
            for (unsigned int i = 0; i < NnueHidden2Dims; i++)
                LayerStack[b].NnueOut.weight[i] = (int32_t)round(LayerStack[b].NnueOut.weight[i] * ratio64 / NnueValueScale);
        }
    }
    string GetArchName() {
        return "V5-" + to_string(NnueFtOutputdims);
    }
    string GetArchDescription() {
        return "HalfKAv2_hm, " + to_string(NnueFtOutputdims) + "x16+16x32x1";
    }
    int GetEval(chessposition* pos) {
        struct NnueNetwork {
            alignas(64) clipped_t input[NnueFtOutputdims];
            alignas(64)int32_t hidden1_values[NnueHidden1Dims];
            alignas(64)int32_t hidden2_values[NnueHidden2Dims];
            alignas(64)clipped_t hidden1_sqrclipped[MULTIPLEOFN(NnueHidden1Out, 32)];
            alignas(64)clipped_t hidden1_clipped[NnueHidden1Dims];
            alignas(64)clipped_t hidden2_clipped[NnueHidden2Dims];
            alignas(64)int32_t out_value;
        } network;

        int bucket = (POPCOUNT(pos->occupied00[WHITE] | pos->occupied00[BLACK]) - 1) / 4;
        int psqt = pos->Transform<NnueArchV5, NnueFtHalfdims, NnuePsqtBuckets>(network.input, bucket);
        LayerStack[bucket].NnueHd1.Propagate(network.input, network.hidden1_values);
        memset(network.hidden1_sqrclipped, 0, sizeof(network.hidden1_sqrclipped));  // FIXME: is this needed?
        LayerStack[bucket].NnueSqrCl.Propagate(network.hidden1_values, network.hidden1_sqrclipped);
        LayerStack[bucket].NnueCl1.Propagate(network.hidden1_values, network.hidden1_clipped);
        memcpy(network.hidden1_sqrclipped + NnueHidden1Out, network.hidden1_clipped, NnueHidden1Out * sizeof(clipped_t));
        LayerStack[bucket].NnueHd2.Propagate(network.hidden1_sqrclipped, network.hidden2_values);
        LayerStack[bucket].NnueCl2.Propagate(network.hidden2_values, network.hidden2_clipped);
        LayerStack[bucket].NnueOut.Propagate(network.hidden2_clipped, &network.out_value);

        int fwdout = network.hidden1_values[NnueHidden1Out] * (600 * 1024 / NnueValueScale) / (127 * (1 << NnueClippingShift));
        int positional = network.out_value + fwdout;

        return (psqt + positional) * NnueValueScale / 1024;
    }
    int16_t* GetFeatureWeight() {
        return NnueFt.weight;
    }
    int16_t* GetFeatureBias() {
        return NnueFt.bias;
    }
    int32_t* GetFeaturePsqtWeight() {
        return NnueFt.psqtWeights;
    }
    uint32_t GetFileVersion() {
        return NNUEFILEVERSIONSFNNv5_1024;
    }
    int16_t* CreateAccumulationStack() {
        return(int16_t*) allocalign64(MAXDEPTH * 2 * NnueFtOutputdims * sizeof(int16_t));
    }
    int32_t* CreatePsqtAccumulationStack() {
        return (int32_t*)allocalign64(MAXDEPTH * 2 * NnuePsqtBuckets * sizeof(int32_t));
    }
    unsigned int GetAccumulationSize() {
        return NnueFtOutputdims;
    }
    unsigned int GetPsqtAccumulationSize() {
        return NnuePsqtBuckets;
    }
    size_t GetNetworkFilesize() {
        return networkfilesize;
    }
#ifdef STATISTICS
    void SwapInputNeurons(unsigned int i1, unsigned int i2) {
        if (i1 >= NnueFtHalfdims / 2 || i2 >= NnueFtHalfdims / 2) {
            cout << "Alarm! Bad index for neuron swapping.\n";
            return;
        }
        for (int p = 0; p < 2; p++) {
            int offset = p * NnueFtHalfdims / 2;
            NnueFt.SwapWeights(offset + i1, offset + i2);
            for (int i = 0; i < NnueLayerStacks; i++)
                LayerStack[i].NnueHd1.SwapWeights(offset + i1, offset + i2);
        }
    }
    void Statistics(bool verbose, bool sort) {
        char str[512];
        sprintf(str, "");
        U64 total_n = 0;
        U64 total_count = 0;
        U64 total_nonzeroevals[NnueFtOutputdims / 2] = { 0 };
        for (int i = 0; i < NnueLayerStacks; i++) {
            total_n += LayerStack[i].NnueHd1.total_evals;
        }
        for (int i = 0; i < NnueLayerStacks; i++) {
            U64 n = LayerStack[i].NnueHd1.total_evals;
            U64 c = LayerStack[i].NnueHd1.total_count;
            total_count += c;
            double counts_per_eval = c / (double)n;
            double f1 = 100.0 * n / total_n;
            sprintf(str, "%s  L#%d %4.1f%% Avrg.:%6.2f ", str, i, f1, counts_per_eval);
        }
        sprintf(str, "%s  total Avrg.:%6.2f ", str, (double)total_count / total_n);
        guiCom << string("[STATS] NNUE: ") + str + "\n";
        for (int j = 0; j < NnueFtOutputdims / 2; j++) {
            sprintf(str, "%4d: ", j);
            for (int i = 0; i < NnueLayerStacks; i++) {
                U64 n1 = LayerStack[i].NnueHd1.nonzeroevals[j];
                U64 n2 = LayerStack[i].NnueHd1.nonzeroevals[j + NnueFtOutputdims / 2];
                total_nonzeroevals[j] += n1 + n2;
                sprintf(str, "%s   (%9lld/%9lld) ", str, n1, n2);
            }
            sprintf(str, "%s   %9lld", str, total_nonzeroevals[j]);
            if (verbose)
                guiCom << string("[STATS] ") + str + "\n";
        }
        if (sort)
        {
            for (int i1 = 0; i1 < NnueFtOutputdims / 2; i1++)
                for (int i2 = i1 + 1; i2 < NnueFtOutputdims / 2; i2++)
                    if (total_nonzeroevals[i1] < total_nonzeroevals[i2]) {
                        U64 temp_nnz = total_nonzeroevals[i1];
                        total_nonzeroevals[i1] = total_nonzeroevals[i2];
                        total_nonzeroevals[i2] = temp_nnz;
                        SwapInputNeurons(i1, i2);
                    }
        }
    }
#endif
};


//
// NNUE interface in chessposition
//
template <NnueType Nt, Color c> void chessposition::HalfkpAppendActiveIndices(NnueIndexList *active)
{
    const int ksq = kingpos[c];
    const int oksq = (Nt == NnueArchV1 ? ORIENT(c, ksq) : HMORIENT(c, ksq, ksq));
    U64 piecebb = (occupied00[0] | occupied00[1]);
    if (Nt == NnueArchV1)
        piecebb &= ~(piece00[WKING] | piece00[BKING]);
    while (piecebb)
    {
        int index = pullLsb(&piecebb);
        if (Nt == NnueArchV1)
            active->values[active->size++] = ORIENT(c, index) + PieceToIndex[c][mailbox[index]] + PS_KPEND * oksq;
        else
            active->values[active->size++] = HMORIENT(c, index, ksq) + PieceToIndex[c][mailbox[index]] + PS_KAEND * KingBucket[oksq];
    }
}


template <NnueType Nt, Color c> void chessposition::HalfkpAppendChangedIndices(DirtyPiece* dp, NnueIndexList* add, NnueIndexList* remove)
{
    const int ksq = kingpos[c];
    const int oksq = (Nt == NnueArchV1 ? ORIENT(c, ksq) : HMORIENT(c, ksq, ksq));
    for (int i = 0; i < dp->dirtyNum; i++) {
        PieceCode pc = dp->pc[i];
        if (Nt == NnueArchV1 && (pc >> 1) == KING)
            continue;
        int sq = dp->from[i];
        if (sq >= 0) {
            if (Nt == NnueArchV1)
                remove->values[remove->size++] = ORIENT(c, sq) + PieceToIndex[c][pc] + PS_KPEND * oksq;
            else
                remove->values[remove->size++] = HMORIENT(c, sq, ksq) + PieceToIndex[c][pc] + PS_KAEND * KingBucket[oksq];
        }
        sq = dp->to[i];
        if (sq >= 0) {
            if (Nt == NnueArchV1)
                add->values[add->size++] = ORIENT(c, sq) + PieceToIndex[c][pc] + PS_KPEND * oksq;
            else
                add->values[add->size++] = HMORIENT(c, sq, ksq) + PieceToIndex[c][pc] + PS_KAEND * KingBucket[oksq];
        }
    }
}

#ifdef USE_SSE2
#define vec_clip_8_128(a,b) _mm_subs_epi8(_mm_adds_epi8(_mm_packs_epi16(a, b), _mm_set1_epi8(-128)), _mm_set1_epi8(-128))
#endif

// Macros for propagation of small layers
#if defined (USE_AVX2)
typedef __m256i sml_vec_t;
#define vec_setzero _mm256_setzero_si256
#define vec_setsml_32 _mm256_set1_epi32
#define vec_addsml_dpbusd_32 Simd::m256_add_dpbusd_32
#define vec_add_dpbusd_32x2 Simd::m256_add_dpbusd_32x2
#define vec_hadd Simd::m256_hadd
#define vec_haddx4 Simd::m256_haddx4

#elif defined (USE_SSSE3)
typedef __m128i sml_vec_t;
#define vec_setzero _mm_setzero_si128
#define vec_setsml_32 _mm_set1_epi32
#define vec_addsml_dpbusd_32 Simd::m128_add_dpbusd_32
#define vec_add_dpbusd_32x2 Simd::m128_add_dpbusd_32x2
#define vec_add_dpbusd_32x4 Simd::m128_add_dpbusd_epi32x4
#define vec_hadd Simd::m128_hadd
#define vec_haddx4 Simd::m128_haddx4

#endif

// Macros for propagation of big layers and feature transformation
#ifdef USE_AVX512
#define NUM_REGS 32
#define NUM_PSQT_REGS 1
#define SIMD_WIDTH 512
#define MAXCHUNKSIZE 64
typedef __m512i ft_vec_t, ftout_vec_t, in_vec_t, acc_vec_t, weight_vec_t, ft_vec_t;
typedef __m256i psqt_vec_t;
typedef __m128i bias_vec_t;
#define vec_zero() _mm512_setzero_si512()
#define vec_set_16(a) _mm512_set1_epi16(a)
#define vec_max_16(a,b) _mm512_max_epi16(a,b)
#define vec_min_16(a,b) _mm512_min_epi16(a,b)
#define vec_mul_16(a,b) _mm512_mullo_epi16(a,b)
inline ft_vec_t vec_msb_pack_16(ft_vec_t a, ft_vec_t b) {
    ft_vec_t compacted = _mm512_packs_epi16(_mm512_srli_epi16(a, 7), _mm512_srli_epi16(b, 7));
    return _mm512_permutexvar_epi64(_mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7), compacted);
}
#define vec_add_16(a,b) _mm512_add_epi16(a,b)
#define vec_sub_16(a,b) _mm512_sub_epi16(a,b)
#define vec_packs(a,b) _mm512_packs_epi16(a,b)
#define vec_clip_8(a,b) _mm512_permutexvar_epi64(_mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7) ,_mm512_max_epi8(vec_packs(a,b),_mm512_setzero_si512()))
#define vec_add_dpbusd_32x2_large Simd::m512_add_dpbusd_32x2
#define vec_haddx4_large Simd::m512_haddx4
#define vec_hadd_large Simd::m512_hadd
#define vec_zero_psqt() _mm256_setzero_si256()
#define vec_add_psqt_32(a,b) _mm256_add_epi32(a,b)
#define vec_sub_psqt_32(a,b) _mm256_sub_epi32(a,b)
#define vec_load_psqt(a) _mm256_load_si256(a)
#define vec_store_psqt(a,b) _mm256_store_si256(a,b)
#define vec_nnz(a) _mm512_cmpgt_epi32_mask(a, _mm512_setzero_si512())
#define vec_set_32 _mm512_set1_epi32
#define vec_add_dpbusd_32 Simd::m512_add_dpbusd_32

#elif defined(USE_AVX2)
#define NUM_REGS 16
#define NUM_PSQT_REGS 1
#define SIMD_WIDTH 256
#define MAXCHUNKSIZE 32
typedef __m256i ft_vec_t, ftout_vec_t, psqt_vec_t, in_vec_t, acc_vec_t, weight_vec_t;
typedef __m128i bias_vec_t;
#define vec_zero() _mm256_setzero_si256()
#define vec_set_16(a) _mm256_set1_epi16(a)
#define vec_max_16(a,b) _mm256_max_epi16(a,b)
#define vec_min_16(a,b) _mm256_min_epi16(a,b)
#define vec_mul_16(a,b) _mm256_mullo_epi16(a,b)
inline ft_vec_t vec_msb_pack_16(ft_vec_t a, ft_vec_t b) {
    ft_vec_t compacted = _mm256_packs_epi16(_mm256_srli_epi16(a, 7), _mm256_srli_epi16(b, 7));
    return _mm256_permute4x64_epi64(compacted, 0xd8);
}
#define vec_add_16(a,b) _mm256_add_epi16(a,b)
#define vec_sub_16(a,b) _mm256_sub_epi16(a,b)
#define vec_packs(a,b) _mm256_packs_epi16(a,b)
#define vec_clip_8(a,b) _mm256_permute4x64_epi64(_mm256_max_epi8(vec_packs(a,b),_mm256_setzero_si256()), 0xd8)
#define vec_add_dpbusd_32x2_large Simd::m256_add_dpbusd_32x2
#define vec_haddx4_large Simd::m256_haddx4
#define vec_hadd_large Simd::m256_hadd
#define vec_zero_psqt() _mm256_setzero_si256()
#define vec_add_psqt_32(a,b) _mm256_add_epi32(a,b)
#define vec_sub_psqt_32(a,b) _mm256_sub_epi32(a,b)
#define vec_load_psqt(a) _mm256_load_si256(a)
#define vec_store_psqt(a,b) _mm256_store_si256(a,b)
#define vec_nnz(a) _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(a, _mm256_setzero_si256())))
#define vec_set_32 _mm256_set1_epi32
#define vec_add_dpbusd_32 Simd::m256_add_dpbusd_32

#elif defined(USE_SSE2)
#define NUM_REGS 16
#define NUM_PSQT_REGS 2
#define SIMD_WIDTH 128
#define MAXCHUNKSIZE 16
typedef __m128i ft_vec_t, ftout_vec_t, psqt_vec_t;
#define vec_zero() _mm_setzero_si128()
#define vec_set_16(a) _mm_set1_epi16(a)
#define vec_max_16(a,b) _mm_max_epi16(a,b)
#define vec_min_16(a,b) _mm_min_epi16(a,b)
#define vec_mul_16(a,b) _mm_mullo_epi16(a,b)
#define vec_add_16(a,b) _mm_add_epi16(a,b)
#define vec_sub_16(a,b) _mm_sub_epi16(a,b)
#define vec_packs(a,b) _mm_packs_epi16(a,b)
#define vec_msb_pack_16(a,b) _mm_packs_epi16(_mm_srli_epi16(a,7),_mm_srli_epi16(b,7))
#define vec_zero_psqt() _mm_setzero_si128()
#define vec_add_psqt_32(a,b) _mm_add_epi32(a,b)
#define vec_sub_psqt_32(a,b) _mm_sub_epi32(a,b)
#define vec_load_psqt(a) (*(a))
#define vec_store_psqt(a,b) *(a)=(b)

#if defined(USE_SSSE3)
typedef __m128i ft_vec_t, ftout_vec_t, in_vec_t, acc_vec_t, weight_vec_t, bias_vec_t;
#define vec_clip_8(a,b) vec_packs(_mm_max_epi16(a,_mm_setzero_si128()),_mm_max_epi16(b,_mm_setzero_si128()))
#define vec_add_dpbusd_32x2_large Simd::m128_add_dpbusd_32x2
#define vec_haddx4_large Simd::m128_haddx4
#define vec_hadd_large Simd::m128_hadd
#define vec_nnz(a) _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(a, _mm_setzero_si128())))
#define vec_set_32 _mm_set1_epi32
#define vec_add_dpbusd_32 Simd::m128_add_dpbusd_32

#else // USE_SSSE3
#define vec_clip_8(a,b) _mm_subs_epi8(_mm_adds_epi8(_mm_packs_epi16(a, b), _mm_set1_epi8(-128)), _mm_set1_epi8(-128))
#define vec_clip_16(a) _mm_min_epi16(_mm_max_epi16(a,_mm_setzero_si128()),_mm_set1_epi16(127))
#endif

#elif defined(USE_NEON)
#define NUM_REGS 16
#define NUM_PSQT_REGS 2
#define SIMD_WIDTH 128
#define MAXCHUNKSIZE 16
typedef int8x8_t in_vec_t, weight_vec_t;
typedef int16x8_t ft_vec_t;
typedef int16x8_t ftout_vec_t;
typedef int32x4_t acc_vec_t, bias_vec_t, psqt_vec_t;
#define vec_zero() {0}
#define vec_set_16(a) vdupq_n_s16(a)
#define vec_max_16(a,b) vmaxq_s16(a,b)
#define vec_min_16(a,b) vminq_s16(a,b)
#define vec_mul_16(a,b) vmulq_s16(a,b)
inline  ft_vec_t vec_msb_pack_16(ft_vec_t a, ft_vec_t b) {
    const int8x8_t shifta = vshrn_n_s16(a, 7);
    const int8x8_t shiftb = vshrn_n_s16(b, 7);
    const int8x16_t compacted = vcombine_s8(shifta, shiftb);
    return *(ft_vec_t*)&compacted;
}
#define vec_add_16(a,b) vaddq_s16(a,b)
#define vec_sub_16(a,b) vsubq_s16(a,b)
#define vec_packs(a,b) vcombine_s8(vqmovn_s16(a),vqmovn_s16(b))
#define vec_clip_8(a,b) vmaxq_s8(vec_packs(a,b),vdupq_n_s8(0))
#define vec_add_dpbusd_32x2_large Simd::neon_m128_add_dpbusd_epi32x2
#define vec_hadd_large Simd::neon_m128_hadd
#define vec_haddx4_large Simd::neon_m128_haddx4
#define vec_load_psqt(a) (*(a))
#define vec_store_psqt(a,b) *(a)=(b)
#define vec_add_psqt_32(a,b) vaddq_s32(a,b)
#define vec_sub_psqt_32(a,b) vsubq_s32(a,b)
#define vec_zero_psqt() psqt_vec_t{0}

#else
#define NUM_REGS 1
#define NUM_PSQT_REGS 1
#define SIMD_WIDTH 1
typedef int16_t ft_vec_t;
#endif

#ifdef USE_SIMD
#define PSQT_TILE_HEIGHT (NUM_PSQT_REGS * sizeof(psqt_vec_t) / 4)
#endif

#if defined(USE_SSSE3)
alignas(64) static const array<array<uint16_t, 8>, 256> lookup_indices = []() {
    array<array<uint16_t, 8>, 256> v{};
    for (int i = 0; i < 256; ++i)
    {
        int j = i;
        int k = 0;
        while (j)
        {
            unsigned int lsbIndex;
            GETLSB32(lsbIndex, j);
            j &= j - 1;
            v[i][k++] = lsbIndex;
        }
    }
    return v;
}();

alignas(64) static const array<unsigned, 256> lookup_count = []() {
    array<unsigned, 256> v;
    for (int i = 0; i < 256; ++i)
    {
        int j = i;
        int k = 0;
        while (j)
        {
            j &= j - 1;
            ++k;
        }
        v[i] = k;
    }
    return v;
}();
#endif


template <NnueType Nt, Color c, unsigned int NnueFtHalfdims, unsigned int NnuePsqtBuckets> void chessposition::UpdateAccumulator()
{
    STATISTICSINC(nnue_accupdate_all);

    int16_t* weight = NnueCurrentArch->GetFeatureWeight();
    int16_t* bias = NnueCurrentArch->GetFeatureBias();
    int32_t* psqtweight = NnueCurrentArch->GetFeaturePsqtWeight();

#ifdef USE_SIMD
    constexpr unsigned int numRegs = (NUM_REGS > NnueFtHalfdims * 16 / SIMD_WIDTH ? NnueFtHalfdims * 16 / SIMD_WIDTH : NUM_REGS);
    constexpr unsigned int tileHeight = numRegs * SIMD_WIDTH / 16;
    ft_vec_t acc[numRegs];
    psqt_vec_t psqt[NUM_PSQT_REGS];
#endif

    int mslast = ply;
    // A full update needs activation of all pieces (except kings for V1)
    int fullupdatecost = POPCOUNT(occupied00[WHITE] | occupied00[BLACK]) - (Nt == NnueArchV1 ?  2 : 0);

    while (mslast > 0 && !computationState[mslast][c])
    {
        // search for position with computed accu on stack that leads to current position by differential updates
        // break at king move or if the dirty piece updates get too expensive
        DirtyPiece* dp = &dirtypiece[mslast];
        if ((dp->pc[0] >> 1) == KING || (fullupdatecost -= dp->dirtyNum + 1) < 0)
            break;
        mslast--;
    }

    if (mslast >= 0 && computationState[mslast][c])
    {
        if (mslast == ply) {
            STATISTICSINC(nnue_accupdate_cache);
            return;
        }

        STATISTICSINC(nnue_accupdate_inc);
        NnueIndexList removedIndices[2], addedIndices[2];
        removedIndices[0].size = removedIndices[1].size = 0;
        addedIndices[0].size = addedIndices[1].size = 0;
        HalfkpAppendChangedIndices<Nt, c>(&dirtypiece[mslast + 1], &addedIndices[0], &removedIndices[0]);
        for (int ms = mslast + 2; ms <= ply; ms++)
            HalfkpAppendChangedIndices<Nt, c>(&dirtypiece[ms], &addedIndices[1], &removedIndices[1]);

        computationState[mslast + 1][c] = true;
        computationState[ply][c] = true;

        int pos2update[3] = { mslast + 1, mslast + 1 == ply ? -1 : ply, -1 };

#ifdef USE_SIMD
        for (unsigned int i = 0; i < NnueFtHalfdims / tileHeight; i++)
        {
            ft_vec_t* accTile = (ft_vec_t*)(accumulation + (mslast * 2 + c) * NnueFtHalfdims + i * tileHeight);
            for (unsigned int j = 0; j < numRegs; j++)
                acc[j] = accTile[j];
            for (unsigned int l = 0; pos2update[l] >= 0; l++)
            {
                // Difference calculation for the deactivated features
                for (unsigned int k = 0; k < removedIndices[l].size; k++)
                {
                    unsigned int index = removedIndices[l].values[k];
                    const unsigned int offset = NnueFtHalfdims * index + i * tileHeight;
                    ft_vec_t* column = (ft_vec_t*)(weight + offset);
                    for (unsigned int j = 0; j < numRegs; j++)
                        acc[j] = vec_sub_16(acc[j], column[j]);
                }

                // Difference calculation for the activated features
                for (unsigned int k = 0; k < addedIndices[l].size; k++)
                {
                    unsigned int index = addedIndices[l].values[k];
                    const unsigned int offset = NnueFtHalfdims * index + i * tileHeight;
                    ft_vec_t* column = (ft_vec_t*)(weight + offset);
                    for (unsigned int j = 0; j < numRegs; j++)
                        acc[j] = vec_add_16(acc[j], column[j]);
                }

                accTile = (ft_vec_t*)(accumulation + (pos2update[l] * 2 + c) * NnueFtHalfdims + i * tileHeight);
                for (unsigned int j = 0; j < numRegs; j++)
                    accTile[j] = acc[j];
            }
        }

        int32_t* psqtacm = psqtAccumulation + (mslast * 2 + c) * NnuePsqtBuckets;
        for (unsigned int i = 0; i < NnuePsqtBuckets / PSQT_TILE_HEIGHT; i++)
        {
            psqt_vec_t* accTilePsqt = (psqt_vec_t*)(psqtacm + i * PSQT_TILE_HEIGHT);
            for (unsigned int j = 0; j < NUM_PSQT_REGS; j++)
                psqt[j] = vec_load_psqt(&accTilePsqt[j]);
            for (unsigned int l = 0; pos2update[l] >= 0; l++)
            {
                for (unsigned int k = 0; k < removedIndices[l].size; k++)
                {
                    unsigned int index = removedIndices[l].values[k];
                    unsigned int offset = NnuePsqtBuckets * index + i * PSQT_TILE_HEIGHT;
                    psqt_vec_t* columnPsqt = (psqt_vec_t*)(psqtweight + offset);

                    for (unsigned int j = 0; j < NUM_PSQT_REGS; j++)
                        psqt[j] = vec_sub_psqt_32(psqt[j], columnPsqt[j]);
                }

                for (unsigned int k = 0; k < addedIndices[l].size; k++)
                {
                    unsigned int index = addedIndices[l].values[k];
                    unsigned int offset = NnuePsqtBuckets * index + i * PSQT_TILE_HEIGHT;
                    psqt_vec_t* columnPsqt = (psqt_vec_t*)(psqtweight + offset);

                    for (unsigned int j = 0; j < NUM_PSQT_REGS; j++)
                        psqt[j] = vec_add_psqt_32(psqt[j], columnPsqt[j]);
                }

                psqtacm = psqtAccumulation + (pos2update[l] * 2 + c) * NnuePsqtBuckets;
                accTilePsqt = (psqt_vec_t*)(psqtacm + i * PSQT_TILE_HEIGHT);
                for (unsigned int j = 0; j < NUM_PSQT_REGS; j++)
                    vec_store_psqt(&accTilePsqt[j], psqt[j]);
            }
        }

#else
        for (unsigned int l = 0; pos2update[l] >= 0; l++)
        {
            memcpy(accumulation + (pos2update[l] * 2 + c) * NnueFtHalfdims, accumulation + (mslast * 2 + c) * NnueFtHalfdims, NnueFtHalfdims * sizeof(int16_t));
            memcpy(psqtAccumulation + (pos2update[l] * 2 + c) * NnuePsqtBuckets, psqtAccumulation + (mslast * 2 + c) * NnuePsqtBuckets, NnuePsqtBuckets * sizeof(int32_t));

            mslast = pos2update[l];
            int16_t* acm = accumulation + (mslast * 2 + c) * NnueFtHalfdims;
            int32_t* psqtacm = psqtAccumulation + (mslast * 2 + c) * NnuePsqtBuckets;
            // Difference calculation for the deactivated features
            for (unsigned int k = 0; k < removedIndices[l].size; k++)
            {
                unsigned int index = removedIndices[l].values[k];
                const unsigned int offset = NnueFtHalfdims * index;

                for (unsigned int j = 0; j < NnueFtHalfdims; j++)
                    *(acm + j) -= weight[offset + j];

                for (unsigned int i = 0; i < NnuePsqtBuckets; i++)
                    *(psqtacm + i) -= psqtweight[index * NnuePsqtBuckets + i];
            }

            // Difference calculation for the activated features
            for (unsigned int k = 0; k < addedIndices[l].size; k++)
            {
                unsigned int index = addedIndices[l].values[k];
                const unsigned int offset = NnueFtHalfdims * index;

                for (unsigned int j = 0; j < NnueFtHalfdims; j++)
                    *(acm + j) += weight[offset + j];

                for (unsigned int i = 0; i < NnuePsqtBuckets; i++)
                    *(psqtacm + i) += psqtweight[index * NnuePsqtBuckets + i];
            }
        }
#endif
    }
    else {
        // Full update needed
        STATISTICSINC(nnue_accupdate_full);
        computationState[ply][c] = true;
        int16_t* acm = accumulation + (ply * 2 + c) * NnueFtHalfdims;
        int32_t* psqtacm = psqtAccumulation + (ply * 2 + c) * NnuePsqtBuckets;
        NnueIndexList activeIndices;
        activeIndices.size = 0;
        HalfkpAppendActiveIndices<Nt, c>(&activeIndices);
#ifdef USE_SIMD
        for (unsigned int i = 0; i < NnueFtHalfdims / tileHeight; i++)
        {
            ft_vec_t* ft_biases_tile = (ft_vec_t*)(bias + i * tileHeight);
            for (unsigned int j = 0; j < numRegs; j++)
                acc[j] = ft_biases_tile[j];

            for (unsigned int k = 0; k < activeIndices.size; k++)
            {
                unsigned int index = activeIndices.values[k];
                unsigned int offset = NnueFtHalfdims * index + i * tileHeight;
                ft_vec_t* column = (ft_vec_t*)(weight + offset);
                for (unsigned int j = 0; j < numRegs; j++)
                    acc[j] = vec_add_16(acc[j], column[j]);
            }

            ft_vec_t* accTile = (ft_vec_t*)(acm + i * tileHeight);
            for (unsigned int j = 0; j < numRegs; j++)
                accTile[j] = acc[j];
        }

        for (unsigned int i = 0; i < NnuePsqtBuckets / PSQT_TILE_HEIGHT; i++)
        {
            for (unsigned int j = 0; j < NUM_PSQT_REGS; j++)
                psqt[j] = vec_zero_psqt();

            for (unsigned int k = 0; k < activeIndices.size; k++)
            {
                unsigned int index = activeIndices.values[k];
                unsigned int offset = NnuePsqtBuckets * index + i * PSQT_TILE_HEIGHT;
                psqt_vec_t* columnPsqt = (psqt_vec_t*)(psqtweight + offset);

                for (unsigned int j = 0; j < NUM_PSQT_REGS; j++)
                    psqt[j] = vec_add_psqt_32(psqt[j], columnPsqt[j]);
                }

            psqt_vec_t* accTilePsqt = (psqt_vec_t*)(psqtacm + i * PSQT_TILE_HEIGHT);
            for (unsigned int j = 0; j < NUM_PSQT_REGS; j++)
                vec_store_psqt(&accTilePsqt[j], psqt[j]);
            }

#else
        memcpy(acm, bias, NnueFtHalfdims * sizeof(int16_t));
        memset(psqtacm, 0, NnuePsqtBuckets * sizeof(int32_t));

        for (unsigned int k = 0; k < activeIndices.size; k++)
        {
            unsigned int index = activeIndices.values[k];
            unsigned int offset = NnueFtHalfdims * index;

            for (unsigned int j = 0; j < NnueFtHalfdims; j++)
                *(acm + j) += weight[offset + j];

            for (unsigned int i = 0; i < NnuePsqtBuckets; i++)
                *(psqtacm + i) += psqtweight[index * NnuePsqtBuckets + i];
        }
#endif
    }

#ifdef NNUEDEBUG
    int16_t* acm = accumulation + (ply * 2 + c) * NnueFtHalfdims;
    cout << "\naccumulation (c=" << c << "):\n";
    for (unsigned int i = 0; i < NnueFtHalfdims; i++) {
        cout << hex << setfill('0') << setw(4) << (short)*(acm + i) << " ";
        if (i % 16 == 15)
            cout << "   " << hex << setfill('0') << setw(3) << (int)(i / 16 * 16) << "\n";
    }
    cout << dec;
    if (!NnuePsqtBuckets)
        return;

    int32_t* psqtacm = psqtAccumulation + (ply * 2 + c) * NnuePsqtBuckets;
    cout << "\npsqtaccumulation (c=" << c << "):\n";
    for (unsigned int i = 0; i < NnuePsqtBuckets; i++)
    {
        cout << dec << setfill(' ') << setw(6) << (int)*(psqtacm + i) << " ";
        if (i % 16 == 15 || (i + 1 == NnuePsqtBuckets))
            cout << "   " << hex << setfill('0') << setw(3) << (int)(i / 16 * 16) << "\n";
    }
    cout << dec;
#endif
}


template <NnueType Nt, unsigned int NnueFtHalfdims, unsigned int NnuePsqtBuckets>
int chessposition::Transform(clipped_t *output, int bucket)
{
    UpdateAccumulator<Nt, WHITE, NnueFtHalfdims, NnuePsqtBuckets>();
    UpdateAccumulator<Nt, BLACK, NnueFtHalfdims, NnuePsqtBuckets>();

    int16_t* acm = accumulation + ply * 2 * NnueFtHalfdims;
    int32_t* psqtacm = psqtAccumulation + ply * 2 * NnuePsqtBuckets;

    const int perspectives[2] = { state & S2MMASK, !(state & S2MMASK) };
    for (int p = 0; p < 2; p++)
    {
        const unsigned int offset = (Nt == NnueArchV1 ? NnueFtHalfdims * p : NnueFtHalfdims / 2 * p);


#ifdef USE_SIMD
        if (Nt == NnueArchV5)
        {
            const unsigned int numChunks = NnueFtHalfdims / 2 / MAXCHUNKSIZE;
            ft_vec_t Zero = vec_zero();
            ft_vec_t One = vec_set_16(127);

            const ft_vec_t* in0 = (ft_vec_t*)(acm + perspectives[p] * NnueFtHalfdims);
            const ft_vec_t* in1 = (ft_vec_t*)(acm + perspectives[p] * NnueFtHalfdims + NnueFtHalfdims / 2);
            ftout_vec_t* out = (ftout_vec_t*)&output[offset];
            for (unsigned int i = 0; i < numChunks; i++)
            {
                const ft_vec_t sum0a = vec_max_16(vec_min_16(in0[i * 2 + 0], One), Zero);
                const ft_vec_t sum0b = vec_max_16(vec_min_16(in0[i * 2 + 1], One), Zero);
                const ft_vec_t sum1a = vec_max_16(vec_min_16(in1[i * 2 + 0], One), Zero);
                const ft_vec_t sum1b = vec_max_16(vec_min_16(in1[i * 2 + 1], One), Zero);

                const ft_vec_t pa = vec_mul_16(sum0a, sum1a);
                const ft_vec_t pb = vec_mul_16(sum0b, sum1b);
#ifdef USE_FASTSSE2
                const ft_vec_t shfta =  _mm_srli_epi16(pa, 7);
                const ft_vec_t shftb = _mm_srli_epi16(pb, 7);

                out[i * 2] = shfta;
                out[i * 2 + 1] = shftb;
#else
                out[i] = vec_msb_pack_16(pa, pb);
#endif
            }
        }
        else {
            const ft_vec_t* acc = (ft_vec_t*)(acm + perspectives[p] * NnueFtHalfdims);
            constexpr unsigned int numChunks = (16 * NnueFtHalfdims) / SIMD_WIDTH;
            ftout_vec_t* out = (ftout_vec_t*)&output[offset];
#ifdef USE_FASTSSE2
            for (unsigned int i = 0; i < numChunks; i++) {
                ft_vec_t sum = (ft_vec_t)acc[i];
                out[i] = vec_clip_16(sum);
            }
#else
            for (unsigned int i = 0; i < numChunks / 2; i++) {
                ft_vec_t s0 = acc[i * 2];
                ft_vec_t s1 = acc[i * 2 + 1];
                out[i] = (ftout_vec_t)vec_clip_8(s0, s1);
            }
#endif
        }
#else
        if (Nt == NnueArchV1)
        {
            for (unsigned int i = 0; i < NnueFtHalfdims; i++) {
                int16_t sum = *(acm + perspectives[p] * NnueFtHalfdims + i);
                output[offset + i] = (clipped_t)max<int16_t>(0, min<int16_t>(127, sum));
            }
        }
        else {
            for (unsigned int i = 0; i < NnueFtHalfdims / 2; i++) {
                int16_t sum0 = *(acm + perspectives[p] * NnueFtHalfdims + i);
                int16_t sum1 = *(acm + perspectives[p] * NnueFtHalfdims + NnueFtHalfdims / 2 + i);
                sum0 = max((int16_t)0, min((int16_t)127, sum0));
                sum1 = max((int16_t)0, min((int16_t)127, sum1));
                output[offset + i] = sum0 * sum1 / 128;
            }
        }
#endif
    }

#ifdef NNUEDEBUG
    cout << "\ninput layer:\n";
    for (unsigned int i = 0; i < NnueFtHalfdims; i++) {
        cout << hex << setfill('0') << setw(2) << (int)output[i] << " ";
        if (i % 32 == 31)
            cout << "   " << hex << setfill('0') << setw(3) << (int)(i / 32 * 32) << "\n";
    }
    cout << dec;
#endif

    if (Nt == NnueArchV5)
        return (*(psqtacm + perspectives[0] * NnuePsqtBuckets + bucket) - *(psqtacm + perspectives[1] * NnuePsqtBuckets + bucket)) / 2;
    else
        return 0;
}




int chessposition::NnueGetEval()
{
    return NnueCurrentArch->GetEval(this);
}


//
// FeatureTransformer
//

template <int ftdims, int inputdims, int psqtbuckets>
bool NnueFeatureTransformer<ftdims, inputdims, psqtbuckets>::ReadFeatureWeights(NnueNetsource* nr, bool bpz)
{
    int i, j;
    bool okay = true;
    for (i = 0; i < ftdims; ++i)
        okay = okay && nr->read((unsigned char*)&bias[i], sizeof(int16_t));

    int weightsRead = 0;
    for (i = 0; i < inputdims; i++)
    {
        if (bpz && i % (10 * 64) == 0) {
            int16_t dummyweight;
            for (j = 0; j < ftdims; ++j)
                okay = okay && nr->read((unsigned char*)&dummyweight, sizeof(int16_t));
        }
        for (j = 0; j < ftdims; ++j) {
            okay = okay && nr->read((unsigned char*)&weight[weightsRead], sizeof(int16_t));
            weightsRead++;
        }
    }

    weightsRead = 0;
    for (i = 0; i < inputdims; i++)
    {
        for (j = 0; j < psqtbuckets; j++)
        {
            okay = okay && nr->read((unsigned char*)&psqtWeights[weightsRead], sizeof(int32_t));
            weightsRead++;
        }
    }

    return okay;
}

template <int ftdims, int inputdims, int psqtbuckets>
void NnueFeatureTransformer<ftdims, inputdims, psqtbuckets>::WriteFeatureWeights(NnueNetsource* nr, bool bpz)
{
    int i, j;
    for (i = 0; i < ftdims; ++i)
        nr->write((unsigned char*)&bias[i], sizeof(int16_t));

    int weightsRead = 0;
    for (i = 0; i < inputdims; i++)
    {
        if (bpz && i % (10 * 64) == 0) {
            int16_t dummyweight = 0;
            for (j = 0; j < ftdims; ++j)
                nr->write((unsigned char*)&dummyweight, sizeof(int16_t));
        }
        for (j = 0; j < ftdims; ++j) {
            nr->write((unsigned char*)&weight[weightsRead], sizeof(int16_t));
            weightsRead++;
        }
    }

    weightsRead = 0;
    for (i = 0; i < inputdims; i++)
    {
        for (j = 0; j < psqtbuckets; j++)
        {
            nr->write((unsigned char*)&psqtWeights[weightsRead], sizeof(int32_t));
            weightsRead++;
        }
    }
}


//
// NetworkLayer
//

template <unsigned int inputdims, unsigned int outputdims>
bool NnueNetworkLayer<inputdims, outputdims>::ReadWeights(NnueNetsource* nr)
{
    bool okay = true;

    if (previous)
        okay = previous->ReadWeights(nr);

    for (unsigned int i = 0; i < outputdims; ++i)
        okay = okay && nr->read((unsigned char*)&bias[i], sizeof(int32_t));

    size_t buffersize = outputdims * paddedInputdims;
    char* weightbuffer = (char*)calloc(buffersize, sizeof(char));

    if (!weightbuffer)
        return false;

    char* w = weightbuffer;
    okay = okay && nr->read((unsigned char*)weightbuffer, buffersize);

    for (unsigned int r = 0; r < outputdims; r++)
        for (unsigned int c = 0; c < paddedInputdims; c++)
        {
            unsigned int idx = r * paddedInputdims + c;
            idx = shuffleWeightIndex(idx);
            weight[idx] = *w++;
        }

    free(weightbuffer);

    if (OverflowPossible())
        guiCom << "Warning! The network evaluation in layer <" + to_string(inputdims) + "/" + to_string(outputdims) + "> can cause overflows in this build.\n";

    return okay;
}

template <unsigned int inputdims, unsigned int outputdims>
bool NnueNetworkLayer<inputdims, outputdims>::OverflowPossible()
{
    bool possible = false;

    if (paddedInputdims < 128)
    {
#if defined (USE_SSSE3)
        if (outputdims % OutputSimdWidth == 0)
        {
            constexpr unsigned int numChunks = paddedInputdims / 4;

            for (unsigned int i = 0; i < numChunks; i += 2)
            {
                const weight_t* col0 = &weight[(i + 0) * outputdims * 4];
                const weight_t* col1 = &weight[(i + 1) * outputdims * 4];

                for (unsigned int k = 0; k < NumOutputRegsSmall; ++k)
                {
                    for (unsigned int j = 0; j < SimdWidth; j += 2)
                    {
                        // Assuming input cannot be negative.
                        const int worst_case_result =
                            max(0, col0[k * SimdWidth + j + 0] * 127)
                            + max(0, col0[k * SimdWidth + j + 1] * 127)
                            + max(0, col1[k * SimdWidth + j + 0] * 127)
                            + max(0, col1[k * SimdWidth + j + 1] * 127);

                        if (worst_case_result > 32767)
                        {
                            cout << "Weights may cause saturation: "
                                << (int)col0[k * SimdWidth + j + 0] << ", "
                                << (int)col0[k * SimdWidth + j + 1] << ", "
                                << (int)col1[k * SimdWidth + j + 0] << ", "
                                << (int)col1[k * SimdWidth + j + 1] << "\n";
                            possible = true;
                        }
                    }
                }
            }
        }
#endif
    }

    if (paddedInputdims >= 128)
    {
#if defined (USE_SSSE3) || defined (USE_NEON)
        for (unsigned int bigBlock = 0; bigBlock < NumBigBlocks; ++bigBlock)
        {
            for (unsigned int smallBlock = 0; smallBlock < NumSmallBlocksPerOutput; smallBlock += 2)
            {
                const weight_t* w = (weight_t*)(weight + bigBlock * BigBlockSize + smallBlock * SmallBlockSize * NumOutputRegsBig);

                for (unsigned int k = 0; k < NumOutputRegsBig; ++k)
                {
                    for (unsigned int i = 0; i < InputSimdWidth; i += 2)
                    {
                        // Assuming input cannot be negative.
                        const int worst_case_result =
                            max(0, w[k * InputSimdWidth + i + 0] * 127)
                            + max(0, w[k * InputSimdWidth + i + 1] * 127)
                            + max(0, w[(k + NumOutputRegsBig) * InputSimdWidth + i + 0] * 127)
                            + max(0, w[(k + NumOutputRegsBig) * InputSimdWidth + i + 1] * 127);

                        if (worst_case_result > 32767)
                        {
                            cout << "Weights may cause saturation: "
                                << (int)w[k * InputSimdWidth + i + 0] << ", "
                                << (int)w[k * InputSimdWidth + i + 1] << ", "
                                << (int)w[(k + NumOutputRegsBig) * InputSimdWidth + i + 0] << ", "
                                << (int)w[(k + NumOutputRegsBig) * InputSimdWidth + i + 1] << "\n";
                            possible = true;
                        }
                    }
                }
            }
        }
#endif
    }

    return possible;
}

template <unsigned int inputdims, unsigned int outputdims>
void NnueNetworkLayer<inputdims, outputdims>::WriteWeights(NnueNetsource* nr)
{
    if (previous)
        previous->WriteWeights(nr);

    for (unsigned int i = 0; i < outputdims; ++i)
        nr->write((unsigned char*)&bias[i], sizeof(int32_t));

    for (unsigned int i = 0; i < outputdims * paddedInputdims; i++)
            nr->write((unsigned char*)&weight[shuffleWeightIndex(i)], sizeof(char));
}


template <unsigned int inputdims, unsigned int outputdims>
void NnueNetworkLayer<inputdims, outputdims>::Propagate(clipped_t* input, int32_t* output)
{
#ifdef USE_PROPAGATESPARSE
    if (useSparsePropagation)
        PropagateSparse(input, output);
    else
#endif
#ifdef USE_PROPAGATESMALL
    if (useSmallLayerPropagation)
        PropagateSmallLayer(input, output);
    else
#endif
#ifdef USE_PROPAGATEBIG
    if (useBigLayerPropagation)
        PropagateBigLayer(input, output);
    else
#endif
        PropagateNative(input, output);

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


#ifdef USE_PROPAGATESMALL
template <unsigned int inputdims, unsigned int outputdims>
inline void NnueNetworkLayer<inputdims, outputdims>::PropagateSmallLayer(clipped_t* input, int32_t* output)
{
    // Small Layer fast propagation
    if (outputdims % OutputSimdWidth == 0)
    {
        constexpr unsigned int numChunks = paddedInputdims / 4;

        const int32_t* input32 = (int32_t*)input;
        const sml_vec_t* biasvec = (sml_vec_t*)bias;
        sml_vec_t acc[NumOutputRegsSmall];
        for (unsigned int k = 0; k < NumOutputRegsSmall; ++k)
            acc[k] = biasvec[k];

        for (unsigned int i = 0; i < numChunks; i += 2)
        {
            const sml_vec_t in0 = vec_setsml_32(input32[i + 0]);
            const sml_vec_t in1 = vec_setsml_32(input32[i + 1]);
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
            vec_addsml_dpbusd_32(sum0, in, row0[j]);
        }
        output[0] = vec_hadd(sum0, bias[0]);
    }
}
#endif


#ifdef USE_PROPAGATEBIG
template <unsigned int inputdims, unsigned int outputdims>
inline void NnueNetworkLayer<inputdims, outputdims>::PropagateBigLayer(clipped_t* input, int32_t* output)
{
    // Big Layer fast propagation
    const in_vec_t* invec = (in_vec_t*)input;
    for (unsigned int bigBlock = 0; bigBlock < NumBigBlocks; ++bigBlock)
    {
        acc_vec_t acc[NumOutputRegsBig] = { vec_zero() };

        for (unsigned int smallBlock = 0; smallBlock < NumSmallBlocksPerOutput; smallBlock += 2)
        {
            const weight_vec_t* weightvec = (weight_vec_t*)(weight + bigBlock * BigBlockSize + smallBlock * SmallBlockSize * NumOutputRegsBig);
            const in_vec_t in0 = invec[smallBlock + 0];
            const in_vec_t in1 = invec[smallBlock + 1];

            for (unsigned int k = 0; k < NumOutputRegsBig; ++k)
                vec_add_dpbusd_32x2_large(acc[k], in0, weightvec[k], in1, weightvec[k + NumOutputRegsBig]);
        }

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
#endif


#ifdef USE_PROPAGATESPARSE
template <unsigned int inputdims, unsigned int outputdims>
inline void NnueNetworkLayer<inputdims, outputdims>::PropagateSparse(clipped_t* input, int32_t* output)
{
    static constexpr unsigned int ChunkSize = 4;
    constexpr unsigned int NumChunks = MULTIPLEOFN(inputdims, 8) / ChunkSize;
    constexpr unsigned int NumRegs = outputdims > OutputSimdWidth ? outputdims / OutputSimdWidth : 1;
    uint16_t nnz[NumChunks];
    unsigned int count = 0;
    const int32_t* input32 = (int32_t*)input;
    const in_vec_t* inputVector = (const in_vec_t*)input;


    constexpr unsigned int InternalInputSimdWidth = sizeof(in_vec_t) / sizeof(std::int32_t);
    constexpr unsigned int InternalChunkSize = InternalInputSimdWidth > 8 ? InternalInputSimdWidth : 8;
    constexpr unsigned int NumInternalChunks = NumChunks / InternalChunkSize;
    constexpr unsigned int InputsPerInternalChunk = InternalChunkSize / InternalInputSimdWidth;
    constexpr unsigned int OutputsPerInternalChunk = InternalChunkSize / 8;

    // Step 1: Find indices of nonzero 32bit blocks
    __m128i base = _mm_set1_epi16(0);
    __m128i increment = _mm_set1_epi16(8);
    for (unsigned int i = 0; i < NumInternalChunks; ++i)
    {
        // bitmask of nonzero values in this chunk
        unsigned int internalnnz = 0;
        for (unsigned int j = 0; j < InputsPerInternalChunk; ++j)
        {
            const in_vec_t inputChunk = inputVector[i * InputsPerInternalChunk + j];
            unsigned int newnnz = vec_nnz(inputChunk);
            internalnnz |= newnnz << (j * InternalInputSimdWidth);
#ifdef STATISTICS
            int k = (i * InputsPerInternalChunk + j) * InternalInputSimdWidth * ChunkSize;
            while (newnnz)
            {
                if (newnnz & 1)
                {
                    for (unsigned int l = 0; l < ChunkSize; l++)
                        if (input[k + l])
                            nonzeroevals[k + l]++;
                }
                k += ChunkSize;
                newnnz = newnnz >> 1;
            }
#endif
        }
        for (unsigned int j = 0; j < OutputsPerInternalChunk; ++j)
        {
            const unsigned int lookup = (internalnnz >> (j * 8)) & 0xFF;
            const __m128i offsets = _mm_loadu_si128((__m128i*)(&lookup_indices[lookup]));
            _mm_storeu_si128((__m128i*)(nnz + count), _mm_add_epi16(base, offsets));
            count += lookup_count[lookup];
            base = _mm_add_epi16(base, increment);
        }
    }

#ifdef STATISTICS
    total_evals++;
    total_count += count;
#endif

    // Step 2: Process the collected nonzero blocks
    const in_vec_t* biasvec = (const in_vec_t*)bias;
    in_vec_t acc[NumRegs];
    for (unsigned int k = 0; k < NumRegs; ++k)
        acc[k] = biasvec[k];

    for (unsigned int j = 0; j < count; ++j)
    {
        const uint16_t i = nnz[j];
        const in_vec_t in = vec_set_32(input32[i]);
        const in_vec_t* col = (const in_vec_t*)&weight[i * outputdims * ChunkSize];
        for (unsigned int k = 0; k < NumRegs; ++k)
            vec_add_dpbusd_32(acc[k], in, col[k]);
    }

    in_vec_t* outptr = (in_vec_t*)output;
    for (unsigned int k = 0; k < NumRegs; ++k)
        outptr[k] = acc[k];
}
#endif


template <unsigned int inputdims, unsigned int outputdims>
void NnueNetworkLayer<inputdims, outputdims>::PropagateNative(clipped_t* input, int32_t* output)
{
#ifdef USE_FASTSSE2
    if (outputdims % 4 == 0) {
        __m128i* outVec = (__m128i*)output;
        __m128i* biasVec = (__m128i*)bias;
        __m128i* inVec = (__m128i*)input;
        for (unsigned int i = 0; i < outputdims / 4; i++) {
            __m128i* w = (__m128i*) & weight[4 * i * paddedInputdims], p, s0, s1, s2, s3;
            s0 = s1 = s2 = s3 = _mm_setzero_si128();
            for (unsigned int j = 0; j < paddedInputdims / 8; j++) {
                p = _mm_madd_epi16(inVec[j], w[0 * paddedInputdims / 8 + j]);
                s0 = _mm_add_epi32(s0, p);
                p = _mm_madd_epi16(inVec[j], w[1 * paddedInputdims / 8 + j]);
                s1 = _mm_add_epi32(s1, p);
                p = _mm_madd_epi16(inVec[j], w[2 * paddedInputdims / 8 + j]);
                s2 = _mm_add_epi32(s2, p);
                p = _mm_madd_epi16(inVec[j], w[3 * paddedInputdims / 8 + j]);
                s3 = _mm_add_epi32(s3, p);
            }
            s0 = _mm_add_epi32(_mm_unpacklo_epi32(s0, s1), _mm_unpackhi_epi32(s0, s1));
            s2 = _mm_add_epi32(_mm_unpacklo_epi32(s2, s3), _mm_unpackhi_epi32(s2, s3));
            s0 = _mm_add_epi32(_mm_unpacklo_epi64(s0, s2), _mm_unpackhi_epi64(s0, s2));
            outVec[i] = _mm_add_epi32(s0, biasVec[i]);
        }
    }
    else {
        __m128i* iv = (__m128i*)input;
        __m128i* row = (__m128i*)weight;
        __m128i p0 = _mm_madd_epi16(iv[0], row[0]);
        __m128i p1 = _mm_madd_epi16(iv[1], row[1]);
        __m128i p2 = _mm_madd_epi16(iv[2], row[2]);
        __m128i p3 = _mm_madd_epi16(iv[3], row[3]);
        __m128i sum = _mm_add_epi32(_mm_add_epi32(p0, p1), _mm_add_epi32(p2, p3));
        sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xb));
        sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1));
        *output = _mm_cvtsi128_si32(sum) + bias[0];
    }
#else
#if defined(USE_SSE2)
    const unsigned int numChunks = paddedInputdims / 16;
    const __m128i Zeros = _mm_setzero_si128();
    const __m128i* inVec = (__m128i*)input;
# elif defined(USE_NEON)
    const unsigned int numChunks = paddedInputdims / 16;
    const int8x8_t* inVec = (int8x8_t*)input;
# endif
    for (unsigned int i = 0; i < outputdims; ++i) {
        unsigned int offset = i * paddedInputdims;
#if defined(USE_SSE2)
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
#elif defined(USE_NEON)
        int32x4_t sum = { bias[i] };
        const int8x8_t* row = (int8x8_t*)&weight[offset];
        for (unsigned int j = 0; j < numChunks; ++j) {
            int16x8_t product = vmull_s8(inVec[j * 2], row[j * 2]);
            product = vmlal_s8(product, inVec[j * 2 + 1], row[j * 2 + 1]);
            sum = vpadalq_s16(sum, product);
        }
        output[i] = sum[0] + sum[1] + sum[2] + sum[3];
#else
        int32_t sum = bias[i];
        for (unsigned int j = 0; j < inputdims; ++j) {
            sum += weight[offset + j] * input[j];
        }
        output[i] = sum;
#endif
    }
#endif
}



//
// ClippedRelu
//

template <unsigned int dims, unsigned int clippingshift>
void NnueClippedRelu<dims, clippingshift>::Propagate(int32_t *input, clipped_t *output)
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
                    _mm256_load_si256(&in[i * 4 + 1])), clippingshift);
                const __m256i words1 = _mm256_srai_epi16(_mm256_packs_epi32(
                    _mm256_load_si256(&in[i * 4 + 2]),
                    _mm256_load_si256(&in[i * 4 + 3])), clippingshift);
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
                _mm_load_si128(&in[i * 4 + 1])), clippingshift);
            const __m128i words1 = _mm_srai_epi16(_mm_packs_epi32(
                _mm_load_si128(&in[i * 4 + 2]),
                _mm_load_si128(&in[i * 4 + 3])), clippingshift);
            const __m128i packedbytes = _mm_packs_epi16(words0, words1);
            _mm_store_si128(&out[i], _mm_max_epi8(packedbytes, Zero));
        }
    }
#elif defined(USE_SSE2)
    __m128i* in = (__m128i*)input;
    __m128i* out = (__m128i*)output;
#ifdef USE_FASTSSE2
    const unsigned int numChunks = dims / 8;
    for (unsigned int i = 0; i < numChunks; i++) {
        __m128i words = _mm_srai_epi16(_mm_packs_epi32(in[i * 2], in[i * 2 + 1]),
            clippingshift);
        _mm_store_si128(&out[i], vec_clip_16(words));
    }
#else
    const unsigned int numChunks = dims / SimdWidth;
    for (unsigned int i = 0; i < numChunks; i++) {
        __m128i words0 = _mm_srai_epi16(
            _mm_packs_epi32(in[i * 4 + 0], in[i * 4 + 1]), clippingshift);
        __m128i words1 = _mm_srai_epi16(
            _mm_packs_epi32(in[i * 4 + 2], in[i * 4 + 3]), clippingshift);
        _mm_store_si128(&out[i], vec_clip_8(words0, words1));
    }
#endif
#elif defined(USE_NEON)
    const unsigned int numChunks = dims / 8;
    const int8x8_t kZero = { 0 };
    int32x4_t* in = (int32x4_t*)input;
    int8x8_t* out = (int8x8_t*)output;
    for (unsigned int i = 0; i < numChunks; i++) {
        int16x8_t shifted = vcombine_s16(
            vqshrn_n_s32(in[i * 2], clippingshift), vqshrn_n_s32(in[i * 2 + 1], clippingshift));
        out[i] = vmax_s8(vqmovn_s16(shifted), kZero);
    }
#else
    for (unsigned int i = 0; i < dims; i++)
        output[i] = max(0, min(127, input[i] >> clippingshift));
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
// SqrClippedRelu
//

template <unsigned int dims>
void NnueSqrClippedRelu<dims>::Propagate(int32_t* input, clipped_t* output)
{
#if defined(USE_SSE2)
    const unsigned int numChunks = dims / 16;
    __m128i* in = (__m128i*)input;
    __m128i* out = (__m128i*)output;

    for (unsigned int i = 0; i < numChunks; i++) {
        __m128i words0 = _mm_packs_epi32(
            _mm_load_si128(&in[i * 4 + 0]),
            _mm_load_si128(&in[i * 4 + 1]));
        __m128i words1 = _mm_packs_epi32(
            _mm_load_si128(&in[i * 4 + 2]),
            _mm_load_si128(&in[i * 4 + 3]));

        words0 = _mm_srli_epi16(_mm_mulhi_epi16(words0, words0), 3);
        words1 = _mm_srli_epi16(_mm_mulhi_epi16(words1, words1), 3);

#ifdef USE_FASTSSE2
        _mm_store_si128(&out[2 * i], _mm_min_epi16(words0, _mm_set1_epi16(127)));
        _mm_store_si128(&out[2 * i + 1], _mm_min_epi16(words1, _mm_set1_epi16(127)));
#else
        _mm_store_si128(&out[i], vec_clip_8_128(words0, words1));
#endif
    }
#else

    const unsigned int start = 0;
    for (unsigned int i = start; i < dims; ++i) {
        output[i] = (clipped_t)max(0LL, min(127LL, (((long long)input[i] * input[i]) >> (2 * 6)) / 128));
    }
#endif

#ifdef NNUEDEBUG
    cout << "\nsqrclipped relu:\n";
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
    NnueCurrentArch = nullptr;
}

void NnueRemove()
{
    if (NnueCurrentArch) {
        freealigned64(NnueCurrentArch);
        NnueCurrentArch = nullptr;
    }
}

bool NnueReadNet(NnueNetsource* nr)
{
    NnueType oldnt = NnueReady;
    unsigned int oldaccumulationsize = (NnueCurrentArch ? NnueCurrentArch->GetAccumulationSize() : 0);
    unsigned int oldpsqtaccumulationsize = (NnueCurrentArch ? NnueCurrentArch->GetPsqtAccumulationSize() : 0);

    NnueReady = NnueDisabled;

    NnueRemove();

    uint32_t version, hash, size;
    string sarchitecture;

    if (!nr->read((unsigned char*)&version, sizeof(uint32_t))
        || !nr->read((unsigned char*)&hash, sizeof(uint32_t))
        || !nr->read((unsigned char*)&size, sizeof(uint32_t)))
    return false;

    sarchitecture.resize(size);
    if (!nr->read((unsigned char*)&sarchitecture[0], size))
        return false;

    size_t remainingfilesize = nr->readbuffersize - (nr->next - nr->readbuffer);

    NnueType nt;
    bool bpz;
    char* buffer;
    switch (version) {
    case NNUEFILEVERSIONROTATE:
        bpz = true;
        nt = NnueArchV1;
        buffer = (char*)allocalign64(sizeof(NnueArchitectureV1));
        NnueCurrentArch = new(buffer) NnueArchitectureV1;
        break;
    case NNUEFILEVERSIONNOBPZ:
        bpz = false;
        nt = NnueArchV1;
        buffer = (char*)allocalign64(sizeof(NnueArchitectureV1));
        NnueCurrentArch = new(buffer) NnueArchitectureV1;
        break;
    case NNUEFILEVERSIONSFNNv5_512:
    case NNUEFILEVERSIONSFNNv5_768:
    case NNUEFILEVERSIONSFNNv5_1024:
        nt = NnueArchV5;
        bpz = false;
        switch (remainingfilesize) {
        case NnueArchitectureV5<512>::networkfilesize:
            buffer = (char*)allocalign64(sizeof(NnueArchitectureV5<512>));
            NnueCurrentArch = new(buffer) NnueArchitectureV5<512>;
            break;
        case NnueArchitectureV5<768>::networkfilesize:
            buffer = (char*)allocalign64(sizeof(NnueArchitectureV5<768>));
            NnueCurrentArch = new(buffer) NnueArchitectureV5<768>;
            break;
        case NnueArchitectureV5<1024>::networkfilesize:
            buffer = (char*)allocalign64(sizeof(NnueArchitectureV5<1024>));
            NnueCurrentArch = new(buffer) NnueArchitectureV5<1024>;
            break;
        case NnueArchitectureV5<1536>::networkfilesize:
            buffer = (char*)allocalign64(sizeof(NnueArchitectureV5<1536>));
            NnueCurrentArch = new(buffer) NnueArchitectureV5<1536>;
            break;
        default:
            break;
        }
        break;
    default:
        return false;
    }

    if (!NnueCurrentArch)
        return false;

    uint32_t fthash = NnueCurrentArch->GetFtHash();
    uint32_t nethash = NnueCurrentArch->GetHash();
    uint32_t filehash = (fthash ^ nethash);

    if (hash != filehash) {
        NnueRemove();
        return false;
    }

    // Read the weights of the feature transformer
    if (!nr->read((unsigned char*)&hash, sizeof(uint32_t)) || hash != fthash)
        return false;
    if (!NnueCurrentArch->ReadFeatureWeights(nr, bpz))
        return false;

    // Read the weights of the network layers recursively
    if (!NnueCurrentArch->ReadWeights(nr, nethash))
        return false;

    if (!nr->endOfNet())
        return false;

    NnueReady = nt;

    if (oldnt != NnueReady
        || oldaccumulationsize != NnueCurrentArch->GetAccumulationSize()
        || oldpsqtaccumulationsize != NnueCurrentArch->GetPsqtAccumulationSize())
    {
        en.allocThreads();
    }

    return true;
}


//
// Implementation of NNUE network reader including embedded networks and zipped networks
//

#ifdef USE_ZLIB

// (De)Compress input buffer using zlib
// code taken from zlib example zpipe.c
static int xFlate(bool compress, unsigned char* in, unsigned char** out, size_t insize, size_t* outsize)
{
    int ret;
    z_stream strm;

    /* allocate xflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = (compress ? deflateInit(&strm, Z_DEFAULT_COMPRESSION) : inflateInit(&strm));
    if (ret != Z_OK)
        return ret;

    strm.avail_in = (uInt)insize;
    strm.next_in = in;
    *out = (unsigned char*)malloc(insize);
    int chunks = 1;
    while (1) {
        strm.next_out = *out + (chunks - 1) * insize;
        strm.avail_out = (uInt)insize;
        ret = (compress ? deflate(&strm, Z_FINISH) : inflate(&strm, Z_NO_FLUSH));
        if (strm.avail_out > 0)
        {
            *outsize = chunks * insize - strm.avail_out;
            break;
        }
        chunks++;
        *out = (unsigned char*)realloc(*out, chunks * insize);
    }
    /* clean up and return */
    if (compress)
        deflateEnd(&strm);
    else
        inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}
#endif // USE_ZLIB


void NnueWriteNet(vector<string> args)
{
    size_t ci = 0;
    size_t cs = args.size();
    string NnueNetPath = "export.nnue";
    int rescale = 0;
    bool zExport = false;
    bool sort = false;
    if (ci < cs)
        NnueNetPath = args[ci++];

    while (ci < cs) {
        if (args[ci] == "rescale" && ++ci < cs)
        {
            rescale = stoi(args[ci++]);
        }
        else if (args[ci] == "z")
        {
            zExport = true;
            ci++;
        }
        else if (args[ci] == "sort")
        {
            sort = true;
            ci++;
        }
    }

    if (sort)
#ifdef STATISTICS
        NnueCurrentArch->Statistics(false, true);
#else
        cout << "Cannot sort input features. This needs STATISTICS collection enabled.\n";
#endif

    ofstream os;
    os.open(NnueNetPath, ios::binary);
    if (!os && en.ExecPath != "")
        os.open(en.ExecPath + NnueNetPath, ios::binary);

    if (!os) {
        cout << "Cannot write file " << NnueNetPath << "\n";
        return;
    }

    if (rescale)
        NnueCurrentArch->RescaleLastLayer(rescale);

    uint32_t fthash = NnueCurrentArch->GetFtHash();
    uint32_t nethash = NnueCurrentArch->GetHash();
    uint32_t filehash = (fthash ^ nethash);

    uint32_t version = NnueCurrentArch->GetFileVersion();
    string sarchitecture = NnueCurrentArch->GetArchDescription();
    uint32_t size = (uint32_t)sarchitecture.size();

    NnueNetsource nr;
    nr.readbuffersize = 3 * sizeof(uint32_t) + size + NnueCurrentArch->GetNetworkFilesize();
    nr.readbuffer = (unsigned char*)allocalign64(nr.readbuffersize);
    nr.next = nr.readbuffer;

    nr.write((unsigned char*)&version, sizeof(uint32_t));
    nr.write((unsigned char*)&filehash, sizeof(uint32_t));
    nr.write((unsigned char*)&size, sizeof(uint32_t));
    nr.write((unsigned char*)&sarchitecture[0], size);
    nr.write((unsigned char*)&fthash, sizeof(uint32_t));

    NnueCurrentArch->WriteFeatureWeights(&nr, false);
    NnueCurrentArch->WriteWeights(&nr, nethash);

    size_t insize = nr.next - nr.readbuffer;

#ifdef USE_ZLIB
    unsigned char* deflatebuffer = nullptr;
    size_t deflatesize = 0;
    if (zExport) {
        if (xFlate(true, nr.readbuffer, &deflatebuffer, insize, &deflatesize) == Z_OK) {
            memcpy(nr.readbuffer, deflatebuffer, deflatesize);
            insize = deflatesize;
        }
        else {
            guiCom << "Cannot alloc buffer for compression.\n";
        }
        free(deflatebuffer);
    }
#endif
    os.write((char*)nr.readbuffer, insize);
    os.close();

    cout << "Network written to file " << NnueNetPath << "\n";
}

#ifdef EVALOPTIONS
void NnueRegisterEvals()
{
    // Expose weights and bias of ouput layer as UCI options for tuning
    en.ucioptions.Register(&NnueValueScale, "NnueValueScale", ucinnuebias, to_string(NnueValueScale), INT_MIN, INT_MAX, nullptr);
}
#endif


bool NnueNetsource::open()
{
    size_t insize = 0;
    bool openOk = false;
    vector<string> filenames;
    unsigned char* inbuffer = nullptr;
    unsigned char* sourcebuffer = nullptr;
    readbuffer = nullptr;
    string NnueNetPath = en.GetNnueNetPath();

#if USE_ZLIB
    int ret;
    unsigned char* inflatebuffer = nullptr;
    size_t inflatesize = 0;
#endif

#ifdef NNUEINCLUDED
    inbuffer = (unsigned char*)&_binary_net_nnue_start;
    insize = (unsigned char*)&_binary_net_nnue_end - (unsigned char*)&_binary_net_nnue_start;
#else
    filenames.push_back(NnueNetPath);
    if (en.ExecPath != "")
        filenames.push_back(en.ExecPath + NnueNetPath);
    for (unsigned int i = 0; i < filenames.size(); i++) {
        ifstream is;
        is.open(filenames[i], ios::binary);
        if (!is)
            continue;

        struct stat stat_buf;
        if (stat(filenames[i].c_str(), &stat_buf) != 0) {
            guiCom << "info string Cannot get size of network file.\n";
            goto cleanup;
        }
        insize = stat_buf.st_size;
        inbuffer = (unsigned char*)allocalign64(insize);
        if (!inbuffer) {
            guiCom << "info string Cannot alloc buffer for network file.\n";
            goto cleanup;
        }

        is.read((char*)inbuffer, insize);
        if (insize != (size_t)is.gcount()) {
            guiCom << "info string Buffer too small for file " << filenames[i] << "\n";
            goto cleanup;
        }
        if (insize > 0)
            break;
    }
    if (!insize) {
        guiCom << "info string Cannot open file " << NnueNetPath << ". Probably doesn't exist.\n";
        goto cleanup;
    }
#endif // NNUEINCLUDED

    sourcebuffer = inbuffer;

#if USE_ZLIB
    // Now test if the input is compressed
    ret = xFlate(false, inbuffer, &inflatebuffer, insize, &inflatesize);
    if (ret == Z_OK) {
        sourcebuffer = inflatebuffer;
        insize = inflatesize;
    }
#endif // USE_ZLIB

    // Finally locate buffer for the NnueNetsource object, copy the network data and free the temporary buffers
    readbuffer = (unsigned char*)allocalign64(insize);
    if (!readbuffer) {
        guiCom << "info string Cannot alloc read buffer for network file.\n";
        goto cleanup;
    }
    memcpy(readbuffer, sourcebuffer, insize);
    readbuffersize = insize;
    next = readbuffer;

    openOk = NnueReadNet(this);

    if (!openOk)
        guiCom << "info string The network " + en.GetNnueNetPath() + " seems corrupted or format is not supported.\n";
    else
        guiCom << "info string Reading network " + en.GetNnueNetPath() + " successful. Using NNUE (" + NnueCurrentArch->GetArchName() + ").\n";

cleanup:
#ifndef NNUEINCLUDED
    freealigned64(inbuffer);
#endif
#if USE_ZLIB
    free(inflatebuffer);
#endif

    return openOk;
}

bool NnueNetsource::read(unsigned char* target, size_t readsize)
{
    if (next - readbuffer + readsize > readbuffersize)
        return false;
    memcpy(target, next, readsize);
    next += readsize;
    return true;
}

bool NnueNetsource::write(unsigned char* source, size_t writesize)
{
    if (next - readbuffer + writesize > readbuffersize)
        return false;
    memcpy(next, source, writesize);
    next += writesize;
    return true;
}

bool NnueNetsource::endOfNet()
{
    return (next == readbuffer + readbuffersize);
}
