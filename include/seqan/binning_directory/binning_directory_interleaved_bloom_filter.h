// ==========================================================================
//                 SeqAn - The Library for Sequence Analysis
// ==========================================================================
// Copyright (c) 2006-2018, Knut Reinert, FU Berlin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Knut Reinert or the FU Berlin nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL KNUT REINERT OR THE FU BERLIN BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// ==========================================================================
// Author:  Temesgen H. Dadi <temesgen.dadi@fu-berlin.de>
//          Enrico Seiler <enrico.seiler@fu-berlin.de>
// ==========================================================================

#ifndef INCLUDE_SEQAN_BINNING_DIRECTORY_BINNING_DIRECTORY_INTERLEAVED_BLOOM_FILTER_H_
#define INCLUDE_SEQAN_BINNING_DIRECTORY_BINNING_DIRECTORY_INTERLEAVED_BLOOM_FILTER_H_

// --------------------------------------------------------------------------
// Class BinningDirectory using an interleaved bloom filter
// --------------------------------------------------------------------------
namespace seqan{

/*!
 * \brief Creates and maintains a k-mer directory using an interleaved bloom filter (IBF).
 * Creates a k-mer directory to store occurrence information for k-mers stemming from different bins.
 * An IBF represents a collection of bloom filters for the individual bins.
 * A k-mer occurs in a bloom filter if all position generated by the hash functions return a 1.
 * A bloom filter may return false positives, but no false negatives.
 * Instead of concatenating the individual bloom filters, we interleave them.
 * This results in blocks, where each block represents a hash value and each position in the block corresponds to a
 * bin.
 *
 * \par example
 *
 * ```cpp
 * #include <seqan/kmer.h>
 * CharString file("sequence.fasta");
 * BinningDirectory<Dna, InterleavedBloomFilter> ibf (10, 3, 20, 16777472);
 * insertKmer(ibf, toCString(file));
 * ```
 *
 */
template<typename TConfig>
class BinningDirectory<InterleavedBloomFilter, TConfig>
{
public:
    //!\brief The type of the variables.
    typedef typename TConfig::TValue                                TValue;
    typedef typename TConfig::THash                                 THash;
    typedef typename TConfig::TBitvector                            TBitvector;
    typedef typename TConfig::TChunks                               TChunks;
    typedef String<TValue>                                          TString;
    typedef typename Value<BinningDirectory>::noOfBins              TNoOfBins;
    typedef typename Value<BinningDirectory>::noOfHashFunc          TNoOfHashFunc;
    typedef typename Value<BinningDirectory>::kmerSize              TKmerSize;
    typedef typename Value<BinningDirectory>::noOfBits              TNoOfBits;
    typedef typename Value<BinningDirectory>::noOfBlocks            TNoOfBlocks;
    typedef typename Value<BinningDirectory>::binWidth              TBinWidth;
    typedef typename Value<BinningDirectory>::blockBitSize          TBlockBitSize;
    typedef typename Value<BinningDirectory>::preCalcValues         TPreCalcValues;
    typedef typename Value<BinningDirectory>::shiftValue            TShiftValue;
    typedef typename Value<BinningDirectory>::seedValue             TSeedValue;
    typedef typename Value<BinningDirectory>::intSize               TIntSize;
    typedef typename Value<BinningDirectory>::filterMetadataSize    TFilterMetadataSize;
    typedef typename Value<BinningDirectory>::TNoOfChunks           TNoOfChunks;

    bool chunkMapSet = false;

    TNoOfChunks      chunks{TChunks::VALUE};
    std::vector<TNoOfChunks> chunkMap{0};
    TNoOfChunks      significantPositions{0};
    TNoOfChunks      significantBits{0};
    TNoOfChunks      effectiveChunks{1};
    uint64_t            chunkOffset{0};
    TNoOfChunks      currentChunk{0};
    //!\brief The number of Bins.
    TNoOfBins        noOfBins;
    //!\brief The number of hash functions.
    TNoOfHashFunc    noOfHashFunc;
    //!\brief The k-mer size.
    TKmerSize        kmerSize;
    //!\brief The size of the bit vector.
    TNoOfBits        noOfBits;
    //!\brief The number of possible hash values that can fit into a single block.
    TNoOfBlocks      noOfBlocks;
    //!\brief The number of 64 bit blocks needed to represent the number of bins.
    TBinWidth        binWidth;
    //!\brief Bits we need to represent noBins bits. Multiple of intSize.
    TBlockBitSize    blockBitSize;

    //!\brief Randomized values for hash functions.
    std::vector<TPreCalcValues>         preCalcValues;
    //!\brief Shift value used for hash function.
    static const TShiftValue            shiftValue{27};
    //!\brief Random seed.
    static const TSeedValue             seedValue{0x90b45d39fb6da1fa};
    //!\brief How many bits we can represent in the biggest unsigned int available.
    static const TIntSize               intSize{0x40};
    //!\brief The bit vector storing the bloom filters.
    Bitvector<TBitvector>         bitvector;
    //!\brief Size in bits of the meta data.
    static const TFilterMetadataSize    filterMetadataSize{256};

    /* rule of six */
    /*\name Constructor, destructor and assignment
     * \{
     */
    //!\brief Default constructor
    BinningDirectory():
        noOfBins(0),
        noOfHashFunc(0),
        kmerSize(0),
        noOfBits(0),
        bitvector() {}

    BinningDirectory(CharString fileName):
        bitvector(fileName)
    {
        getMetadata(*this);
        init();
    }

    /*!
     * \brief Constructs an IBF given parameters.
     * \param n_bins Number of bins. Preferably a multiple of 64.
     * \param n_hash_func Number of hash functions.
     * \param kmer_size The Size of the k-mer.
     * \param vec_size The size of the bit vector in bits. Preferably a power of two + 256 for metadata.
     */
    BinningDirectory(TNoOfBins n_bins, TNoOfHashFunc n_hash_func, TKmerSize kmer_size, TNoOfBits vec_size):
        noOfBins(n_bins),
        noOfHashFunc(n_hash_func),
        kmerSize(kmer_size),
        noOfBits(vec_size),
        bitvector(noOfBins, noOfBits)
    {
        init();
    }

    //!\brief Copy constructor
    BinningDirectory(BinningDirectory<InterleavedBloomFilter, TConfig> & other)
    {
        *this = other;
    }

    //!\brief Copy assignment
    BinningDirectory<InterleavedBloomFilter, TConfig> & operator=(BinningDirectory<InterleavedBloomFilter, TConfig> & other)
    {
        noOfBins = other.noOfBins;
        noOfHashFunc = other.noOfHashFunc;
        kmerSize = other.kmerSize;
        noOfBits = other.noOfBits;
        bitvector = other.bitvector;
        chunkMap = other.chunkMap;
        significantPositions = other.significantPositions;
        significantBits = other.significantBits;
        effectiveChunks = other.effectiveChunks;
        chunkOffset = other.chunkOffset;
        chunkMapSet = other.chunkMapSet;
        init();
        return *this;
    }

    //!\brief Move constrcutor
    BinningDirectory(BinningDirectory<InterleavedBloomFilter, TConfig> && other)
    {
        *this = std::move(other);
    }

    //!\brief Move assignment
    BinningDirectory<InterleavedBloomFilter, TConfig> & operator=(BinningDirectory<InterleavedBloomFilter, TConfig> && other)
    {
        noOfBins = std::move(other.noOfBins);
        noOfHashFunc = std::move(other.noOfHashFunc);
        kmerSize = std::move(other.kmerSize);
        noOfBits = std::move(other.noOfBits);
        bitvector = std::move(other.bitvector);
        chunkMap = std::move(other.chunkMap);
        significantPositions = std::move(other.significantPositions);
        significantBits = std::move(other.significantBits);
        effectiveChunks = std::move(other.effectiveChunks);
        chunkOffset = std::move(other.chunkOffset);
        chunkMapSet = std::move(other.chunkMapSet);
        init();
        return *this;
    }

    //!\brief Destructor
    ~BinningDirectory<InterleavedBloomFilter, TConfig>() = default;
    //!\}

    /*!
     * \brief Resets the bloom filter to 0 for all given bins.
     * \param bins Vector with the ID of the bins to clear.
     * \param threads Number of threads to use.
     */
    template<typename TInt>
    void clear(std::vector<TNoOfBins> const & bins, TInt&& threads)
    {
        std::vector<std::future<void>> tasks;
        // We have so many blocks that we want to distribute to so many threads
        uint64_t batchSize = noOfBlocks / threads;
        if(batchSize * threads < noOfBlocks) ++batchSize;

        for (uint8_t taskNo = 0; taskNo < threads; ++taskNo) // TODO Rather divide by chunks?
        {
            // hashBlock is the number of the block the thread will work on. Each block contains binNo bits that
            // represent the individual bins. Each thread has to work on batchSize blocks. We can get the position in
            // our bitvector by multiplying the hashBlock with noOfBins. Then we just need to add the respective
            // binNo. We have to make sure that the vecPos we generate is not out of bounds, only the case in the last
            // thread if the blocks could not be evenly distributed, and that we do not clear a bin that is assigned to
            // another thread.
            tasks.emplace_back(std::async([=] {
                for (uint64_t hashBlock=taskNo*batchSize;
                    hashBlock < noOfBlocks && hashBlock < (taskNo +1) * batchSize;
                    ++hashBlock)
                {
                    uint64_t vecPos = hashBlock * bitvector.blockBitSize;
                    for(uint32_t binNo : bins)
                    {
                        bitvector.unset_pos(vecPos + binNo);
                    }
                }
            }));
        }
        for (auto &&task : tasks)
        {
            task.get();
        }
    }

    /*!
     * \brief Counts number of occurences in each bin for a given text.
     * \param counts Vector to be filled with counts.
     * \param text Text to count occurences for.
     */
    template<typename THashCount, typename TAnyString>
    void count(std::vector<TNoOfBins> & counts, TAnyString const & text)
    {
        BDHash<TValue, THashCount, TChunks> shape;
        shape.resize(kmerSize);
        shape.setMap(chunkMap);
        shape.setPos(significantPositions);
        shape.setBits(significantBits);
        shape.setEffective(effectiveChunks);
        shape.setChunkOffset(chunkOffset);
        std::vector<uint64_t> kmerHashes = shape.getHash(text);

        for (uint64_t kmerHash : kmerHashes)
        {
            std::vector<uint64_t> vecIndices = preCalcValues;
            for(TNoOfHashFunc i = 0; i < noOfHashFunc ; ++i)
            {
                vecIndices[i] *= kmerHash;
                hashToIndex(vecIndices[i]);
            }

            TNoOfBins binNo = 0;
            for (TBinWidth batchNo = 0; batchNo < binWidth; ++batchNo)
            {
                binNo = batchNo * intSize;
                // get_int(idx, len) returns the integer value of the binary string of length len starting
                // at position idx, i.e. len+idx-1|_______|idx, Vector is right to left.
                uint64_t tmp = bitvector.get_int(vecIndices[0], intSize);

                // A k-mer is in a bin of the IBF iff all hash functions return 1 for the bin.
                for(TNoOfHashFunc i = 1; i < noOfHashFunc;  ++i)
                {
                    tmp &= bitvector.get_int(vecIndices[i], intSize);
                }

                // Behaviour for a bit shift with >= maximal size is undefined, i.e. shifting a 64 bit integer by 64
                // positions is not defined and hence we need a special case for this.
                if (tmp ^ (1ULL<<(intSize-1)))
                {
                    // As long as any bit is set
                    while (tmp > 0)
                    {
                        // sdsl::bits::lo calculates the position of the rightmost 1-bit in the 64bit integer x if it exists.
                        // For example, for 8 = 1000 it would return 3
                        uint64_t step = sdsl::bits::lo(tmp);
                        // Adjust our bins
                        binNo += step;
                        // Remove up to next 1
                        ++step;
                        tmp >>= step;
                        // Count
                        ++counts[binNo];
                        // ++binNo because step is 0-based, e.g., if we had a hit with the next bit we would otherwise count it for binNo=+ 0
                        ++binNo;
                    }
                }
                else
                {
                    ++counts[binNo + intSize - 1];
                }
                // We will now start with the next batch if possible, so we need to shift all the indices.
                for (TNoOfHashFunc i = 0; i < noOfHashFunc; ++i)
                {
                    vecIndices[i] += intSize;
                }
            }
        }
    }

    /*!
     * \brief Tests for occurence in each bin given a text and count threshold.
     * \param selected Vector to be filled with booleans signalling occurence.
     * \param text Text to count occurences for.
     * \param threshold Minimal count (>=) of containing k-mers to report bin as containing text.
     */
    template<typename TAnyString, typename TInt>
    inline void select(std::vector<bool> & selected, TAnyString const & text, TInt && threshold)
    {
        std::vector<TNoOfBins> counts(noOfBins, 0);
        count(counts, text);
        for(TNoOfBins binNo=0; binNo < noOfBins; ++binNo)
        {
            if(counts[binNo] >= threshold)
                selected[binNo] = true;
        }
    }

    /*!
     * \brief Calculates the first index of a block that corresponds to a hash value.
     * \param hash hash value
     */
    inline void hashToIndex(uint64_t & hash) const
    {
        // We do something
        hash ^= hash >> shiftValue;
        // Bring it back into our vector range (noOfBlocks = possible hash values)
        hash %= noOfBlocks;
        // hash &= (noOfBlocks - 1);
        // Since each block needs blockBitSize bits, we multiply to get the correct location
        hash *= blockBitSize;
    }

    /*!
     * \brief Adds all k-mers from a text to the IBF.
     * \param text Text to process.
     * \param binNo bin ID to insertKmer k-mers in.
     */
    template<typename TChunk>
    inline void insertKmer(TString const & text, TNoOfBins binNo, TChunk && chunkNo)
    {
        currentChunk = chunkNo;
        insertKmer(text, binNo);
    }

    /*!
     * \brief Adds all k-mers from a text to the IBF.
     * \param text Text to process.
     * \param binNo bin ID to insertKmer k-mers in.
     */
    template<typename THashInsert>
    inline void insertKmer(TString const & text, TNoOfBins binNo)
    {
        BDHash<TValue, THashInsert, TChunks> shape;
        shape.resize(kmerSize);
        shape.setMap(chunkMap);
        shape.setPos(significantPositions);
        shape.setBits(significantBits);
        shape.setEffective(effectiveChunks);
        shape.setChunkOffset(chunkOffset);
        std::vector<uint64_t> kmerHashes = shape.getHash(text);

        for (auto kmerHash : kmerHashes)
        {
            for(TNoOfHashFunc i = 0; i < noOfHashFunc ; ++i)
            {
                uint64_t vecIndex = preCalcValues[i] * kmerHash;
                hashToIndex(vecIndex);
                vecIndex += binNo;
                bitvector.set_pos(vecIndex);
            }
        }
    }

    //! \brief Initialises internal variables.
    inline void init()
    {
        chunkMap = std::vector<uint8_t>{0};
        // effectiveChunks = 1;
        // significantBits = 0;
        // significantPositions = 0;
        // chunkOffset = 0;
        // How many blocks of 64 bit do we need to represent our noOfBins
        binWidth = std::ceil((double)noOfBins / intSize);
        // How big is then a block (multiple of 64 bit)
        blockBitSize = binWidth * intSize;
        // How many hash values can we represent
        noOfBlocks = noOfBits / blockBitSize;

        preCalcValues.resize(noOfHashFunc);
        for(TNoOfHashFunc i = 0; i < noOfHashFunc ; i++)
            preCalcValues[i] = i ^  (kmerSize * seedValue);
        chunkOffset = noOfBits / (chunks * blockBitSize);
    }

    /*!
     * \brief Increases the number of bins in the binning directory.
     * \param bins The new number of bins.
     * This function only works for uncompressed bitvectors.
     * The resulting bitvector has an increased size proportional to the increase in the binWidth, e.g.
     * resizing a BD with 40 bins to 73 bins also increases the binWidth from 64 to 128 and hence the new bitvector
     * will be twice the size.
     * This increase in size is necessary to avoid invalidating all computed hash functions.
     * If you want to add more bins while keeping the size constant, you need to rebuild the BD.
     * The function will store the underlying bitvector to disk, read it from disk (buffered) and store the old values  * in the new bitvector, i.e. you only need enough memory to hold the new bitvector.
     */
    void resizeBins(TNoOfBins bins)
    {
        static_assert(std::is_same<TBitvector, Uncompressed>::value,
            "Resize is only available for Uncompressed Bitvectors.");
        TBinWidth newBinWidth = std::ceil((double)bins / intSize);
        TBlockBitSize newBlockBitSize = newBinWidth * intSize;
        TNoOfBits newNoOfBits = noOfBlocks * newBlockBitSize;
        bitvector.resize(bins, newNoOfBits, newBlockBitSize, newBinWidth);
        noOfBins = bins;
        binWidth = newBinWidth;
        blockBitSize = newBlockBitSize;
        noOfBits = newNoOfBits;
    }
};
}   // namespace seqan

#endif  // INCLUDE_SEQAN_BINNING_DIRECTORY_BINNING_DIRECTORY_INTERLEAVED_BLOOM_FILTER_H_
