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
// Author:  Enrico Seiler <enrico.seiler@fu-berlin.de>
// ==========================================================================

#include <chrono>
#include <numeric>
#include <random>

#include <seqan/kmer.h>

using namespace seqan;

int main()
{
    // parameters
    uint64_t const noOfRepeats{5};
    uint64_t const noOfKmers{1000000};
    uint64_t const k{12};
    uint64_t const noOfBins{64};
    uint64_t const noOfHashes{3};
    uint64_t const noOfBits{1ULL<<32};

    std::vector<int64_t> addTime;
    std::vector<int64_t> whichTime;

    for (uint64_t r = 0; r < noOfRepeats; ++r)
    {
        KmerFilter<Dna, InterleavedBloomFilter> ibf (noOfBins, noOfHashes, k, noOfBits);

        auto start = std::chrono::high_resolution_clock::now();
        for(uint64_t i = 0; i < noOfBins; ++i)
        {
            CharString file("/Users/enricoseiler/dev/eval/64/bins/");
            if (i < 10)
            {
                append(file, CharString("bin_0"));
            }
            else
            {
                append(file, CharString("bin_"));
            }
            append(file, CharString(std::to_string(i)));
            append(file, CharString(".fasta"));

            addFastaFile(ibf, toCString(file) , i);
            std::cout << "Bin " << i << " done." << '\n';
        }
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        addTime.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());

        start = std::chrono::high_resolution_clock::now();
        for(uint64_t i = 0; i < noOfKmers; ++i)
        {
            CharString file("/Users/enricoseiler/dev/eval/64/reads/");
            if (i < 10)
            {
                append(file, CharString("bin_0"));
            }
            else
            {
                append(file, CharString("bin_"));
            }
            append(file, CharString(std::to_string(i)));
            append(file, CharString(".fastq"));

            CharString id;
            String<Dna> seq;
            SeqFileIn seqFileIn;
            if (!open(seqFileIn, toCString(file)))
            {
                CharString msg = "Unable to open contigs file: ";
                append(msg, CharString(file));
                throw toCString(msg);
            }
            while(!atEnd(seqFileIn))
            {
                readRecord(id, seq, seqFileIn);
                auto x = whichBins(ibf, seq, 1);
            }
        }
        elapsed = std::chrono::high_resolution_clock::now() - start;
        whichTime.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    }

    auto addAvg = accumulate(addTime.begin(), addTime.end(), 0)/addTime.size();
    auto whichAvg = accumulate(whichTime.begin(), whichTime.end(), 0)/whichTime.size();

    std::cout << "Average addKmer: " << addAvg << " ms.\n";
    std::cout << "Average whichBins: " << whichAvg << " ms.\n";
}
