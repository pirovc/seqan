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
// Author: Enrico Seiler <enrico.seiler@fu-berlin.de>
// ==========================================================================
// kmer lib
// ==========================================================================

// --------------------------------------------------------------------------
// Class KmerFilter using direct addressing
// --------------------------------------------------------------------------
namespace seqan{

template<typename TValue>
class KmerFilter<TValue, DirectAddressing>
{
public:
    typedef typename Value<KmerFilter>::Type    THValue;
    THValue    noOfBins;
    THValue    kmerSize;
    THValue    noOfBits;

    THValue    binIntWidth;
    THValue    blockBitSize;
    THValue    noOfHashPos;

    sdsl::bit_vector                    filterVector;

    static const uint32_t               filterMetadataSize{256};
    static const uint8_t                INT_WIDTH{0x40};

    typedef Shape<TValue, SimpleShape>  TShape;

    KmerFilter():
        noOfBins(0),
        kmerSize(0),
        noOfBits(0),
        filterVector(sdsl::bit_vector(0, 0)) {}

    KmerFilter(THValue n_bins, THValue kmer_size, THValue vec_size):
        noOfBins(n_bins),
        kmerSize(kmer_size),
        noOfBits(vec_size),
        filterVector(sdsl::bit_vector(vec_size, 0))
    {
            init(*this);
            std::cout << noOfBins << '\n';
            std::cout << kmerSize << '\n';
            std::cout << noOfBits << '\n';
            std::cout << binIntWidth << '\n';
            std::cout << blockBitSize << '\n';
            std::cout << noOfHashPos << '\n';
    }

    KmerFilter(KmerFilter<TValue, DirectAddressing> const & other)
    {
        *this = other;
    }

    KmerFilter<TValue, DirectAddressing> & operator=(KmerFilter<TValue, DirectAddressing> const & other)
    {
        noOfBins = other.noOfBins;
        kmerSize = other.kmerSize;
        noOfBits = other.noOfBits;
        filterVector = other.filterVector;
        init(*this);
        return *this;
    }

    template<typename TInt>
    void clearBins(std::vector<THValue> & bins, TInt&& threads)
    {
        std::vector<std::future<void>> tasks;

        uint64_t batchSize = noOfHashPos/threads;
        if(batchSize * threads < noOfHashPos) ++batchSize;

        for (uint32_t taskNo = 0; taskNo < threads; ++taskNo)
        {
            tasks.emplace_back(std::async([=] {
                for (uint64_t hashBlock=taskNo*batchSize; hashBlock < noOfHashPos && hashBlock < (taskNo +1) * batchSize; ++hashBlock)
                {
                    uint64_t vecPos = hashBlock * blockBitSize;
                    if (vecPos >= noOfHashPos)
                        break;
                    for(uint32_t binNo : bins)
                    {
                        filterVector[vecPos + binNo] = false;
                    }
                }
            }));
        }
        for (auto &&task : tasks)
        {
            task.get();
        }
    }

    template<typename TString, typename TInt>
    inline void whichBins(std::vector<bool> & selected, TString const & text, TInt && threshold) const
    {
        uint8_t possible = length(text) - kmerSize + 1;

        std::vector<uint8_t> counts(noOfBins, 0);
        std::vector<uint64_t> kmerHashes(possible, 0);

        TShape kmerShape;
        resize(kmerShape, kmerSize);
        hashInit(kmerShape, begin(text));
        auto it = begin(text);
        for (uint32_t i = 0; i < possible; ++i)
        {
            kmerHashes[i] = hashNext(kmerShape, it);
            ++it;
        }

        for (uint64_t kmerHash : kmerHashes)
        {
            if (kmerHash >= noOfHashPos)
                continue;

            kmerHash *= blockBitSize;
            uint32_t binNo = 0;
            for (uint8_t batchNo = 0; batchNo < binIntWidth; ++batchNo)
            {
                binNo = batchNo * INT_WIDTH;
                uint64_t tmp = filterVector.get_int(kmerHash, INT_WIDTH);
                if (tmp ^ (1ULL<<(INT_WIDTH-1)))
                {
                    while (tmp > 0)
                    {
                        uint64_t step = sdsl::bits::lo(tmp);
                        binNo += step;
                        ++step;
                        tmp >>= step;
                        ++counts[binNo];
                        ++binNo;
                    }
                }
                else
                {
                    ++counts[binNo + INT_WIDTH - 1];
                }
                kmerHash += INT_WIDTH;
            }
        }

        for(uint32_t binNo=0; binNo < noOfBins; ++binNo)
        {
            if(counts[binNo] >= threshold)
                selected[binNo] = true;
        }
    }

    template<typename TString, typename TInt>
    inline void addKmer(TString const & text, TInt && binNo)
    {

        TShape kmerShape;
        resize(kmerShape, kmerSize);
        hashInit(kmerShape, begin(text));

        for (uint64_t i = 0; i < length(text) - length(kmerShape) + 1; ++i)
        {

            uint64_t kmerHash = hashNext(kmerShape, begin(text) + i);
            if (blockBitSize * kmerHash + binNo >= noOfHashPos)
            {
                //String<TValue> debug;
                //debug = infix(text, begin(text) + i, begin(text) + i + 3);
                //std::cout << debug << '\n';
                //std::cout << kmerHash << '\n';
                std::cout << "Filter to small" << '\n';
                continue;
            }
            filterVector[blockBitSize * kmerHash + binNo] = 1;
        }
    }
};
}
