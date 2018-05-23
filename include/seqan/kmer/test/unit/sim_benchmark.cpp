#include <seqan/seq_io.h>
#include <random>
using namespace seqan;

CharString baseDir{"/group/ag_abi/seiler/benchmark/"};
uint8_t maxErrors{2};
uint16_t readLength{100};
uint32_t noOfReads{1UL<<20};
uint8_t noOfHaplotypes{16};
std::random_device rd;
std::mt19937 rng(rd());
std::uniform_real_distribution<double> dist(0, 1);

int main()
{
    for(uint16_t noOfBins : {64, 256, 1024, 8192})
    {
        // Since numerator and denominator are powers of two, each bin should get an equal number of reads
        uint32_t readsPerBin = noOfReads / noOfBins;
        // Since numerator and denominator are powers of two, each haplotype should get an equal number of reads
        uint32_t readsPerHaplotype = readsPerBin / noOfHaplotypes;

        for(int32_t bin = 0; bin < noOfBins; ++bin)
        {
            CharString fileIn(baseDir);
            append(fileIn, CharString(std::to_string(noOfBins)));
            append(fileIn, CharString{"/bins/bin_"});
            append(fileIn, CharString(std::string(numDigits(bins)-numDigits(bin), '0') + (std::to_string(bin))));
            append(fileIn, CharString(".fasta"));

            CharString fileOut(baseDir);
            append(fileOut, CharString(std::to_string(noOfBins)));
            append(fileOut, CharString{"/reads/bin_"});
            append(fileOut, CharString(std::string(numDigits(bins)-numDigits(bin), '0') + (std::to_string(bin))));
            append(fileOut, CharString(".fastq"));

            SeqFileIn seqFileIn;
            if (!open(seqFileIn, toCString(fileIn)))
            {
                CharString msg = "Unable to open contigs file: ";
                append(msg, CharString(fileIn));
                std::cerr << msg << '\n';
                throw toCString(msg);
            }

            SeqFileOut seqFileOut;
            if (!open(seqFileOut, toCString(fileOut)))
            {
                CharString msg = "Unable to open contigs file: ";
                append(msg, CharString(fileOut));
                std::cerr << msg << '\n';
                throw toCString(msg);
            }

            CharString id;
            String<Dna> seq;

            while(!atEnd(seqFileIn))
            {
                readRecord(id, seq, seqFileIn);
                uint32_t refLength = length(seq);
                std::uniform_real_distribution<uint32_t> readPos(0, refLength - readLength + 1);
                for(uint_32_t r = 0; r < readsPerHaplotype; ++r)
                {
                    uint32_t pos = readPos(rng);
                    DnaString segment = infixWithLength(seq, pos, readLength);
                    std::uniform_real_distribution<uint16_t> errorPos(0, readLength);
                    for(uint8_t e = 0; e < maxErrors; ++e)
                    {
                        uint32_t pos = errorPos(rng);
                        Dna currentBase = segment[pos];
                        Dna newBase = currentBase;
                        while (newBase == currentBase)
                            newBase = Dna(static_cast<int>(dist(rng) / 0.25));
                        segment[pos] = newBase;
                        if (dist(rng) < 0.5)
                            break;
                    }
                    writeRecord(seqFileOut, id, segment);
                }
            }
        }
    }
}
