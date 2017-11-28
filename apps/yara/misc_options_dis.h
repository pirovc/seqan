// ==========================================================================
//                      Yara - Yet Another Read Aligner
// ==========================================================================
// Copyright (c) 2011-2014, Enrico Siragusa, FU Berlin
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
//     * Neither the name of Enrico Siragusa or the FU Berlin nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ENRICO SIRAGUSA OR THE FU BERLIN BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// ==========================================================================
// Author: Temesgen H. Dadi <temesgen.dadi@fu-berlin.de>
// ==========================================================================

#ifndef APP_YARA_MISC_OPTIONS_DIS_H_
#define APP_YARA_MISC_OPTIONS_DIS_H_

using namespace seqan;

std::mutex mtx;

using namespace seqan;
class Semaphore
{
    std::mutex m;
    std::condition_variable cv;
    int count;

public:
    Semaphore(int n) : count{n} {}
    void notify()
    {
        std::unique_lock<std::mutex> l(m);
        ++count;
        cv.notify_one();
    }
    void wait()
    {
        std::unique_lock<std::mutex> l(m);
        cv.wait(l, [this]{ return count!=0; });
        --count;
    }
};

class Critical_section
{
    Semaphore &s;
public:
    Critical_section(Semaphore &ss) : s{ss} { s.wait(); }
    ~Critical_section() { s.notify(); }
};


namespace seqan
{
    static const uint8_t bitsPerChar = 0x08;
    static const uint8_t bitMask[0x08] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

    template<uint8_t BINS_SIZE, uint8_t KMER_SIZE, uint8_t N_HASH, uint64_t SIZE, typename TString=Dna5String>
    class SeqAnBloomFilter
    {
    public:

        typedef Shape<Dna, UngappedShape<KMER_SIZE> > TShape;

        bool save(const char *fileName) const
        {
            std::ofstream myFile(fileName, std::ios::out | std::ios::binary);

            std::cerr << "Storing filter. Filter is " << m_sizeInBytes << " bytes." << std::endl;
            assert(myFile);

            //write out each block
            myFile.write(reinterpret_cast<char*>(_filterFile), m_sizeInBytes);

            myFile.close();
            assert(myFile);
            return true;
        }

        SeqAnBloomFilter()
        {
            _initPreCalcValues();
            initSize();
            memset(_filterFile, 0, m_sizeInBytes);
        }

        SeqAnBloomFilter(const char *fileName)
        {
            _initPreCalcValues();
            initSize();
            FILE *file = fopen(fileName, "rb");
            if (file == NULL)
            {
                std::cerr << "file \"" << fileName << "\" could not be read." << std::endl;
                exit(1);
            }

            long int lCurPos = ftell(file);
            fseek(file, 0, 2);
            size_t fileSize = ftell(file);
            fseek(file, lCurPos, 0);
            if (fileSize != m_sizeInBytes)
            {
                std::cerr << "Error: " << fileName
                << " does not match size given by its header. Size: "
                << fileSize << " vs " << m_sizeInBytes << " bytes." << std::endl;
                exit(1);
            }

            size_t countRead = std::fread(_filterFile, fileSize, 1, file);
            if (countRead != 1 && fclose(file) != 0)
            {
                std::cerr << "file \"" << fileName << "\" could not be read." << std::endl;
                exit(1);
            }
        }

        void addKmers(TString const & text, uint8_t const & binNo)
        {
            _addKmers(text, binNo);
        }

        void whichBins(std::vector<bool> & selected, TString const & text, uint8_t const & threshold) const
        {
            TShape kmerShape;
            hashInit(kmerShape, begin(text));

            std::vector<uint8_t> counts(BINS_SIZE, 0);

            uint8_t possible = length(text) - length(kmerShape) + 1;

            std::vector<uint64_t> kmerHashes(possible, 0);
            auto it = begin(text);
            for (uint8_t i = 0; i < possible; ++i)
            {
                kmerHashes[i] = hashNext(kmerShape, it);
                ++it;
            }


//            for (uint64_t kmerHash : kmerHashes)
//            {
//                for (uint8_t binNo = 0; binNo < BINS_SIZE; ++binNo)
//                {
//                    if (threshold - counts[binNo] > possible || selected[binNo])
//                        continue;
//
//                    if (containsKmer(kmerHash, binNo))
//                    {
//                        ++counts[binNo];
//                        if(counts[binNo] >= threshold)
//                            selected[binNo] = true;
//                    }
//                }
//                --possible;
//            }
            
            for (uint64_t kmerHash : kmerHashes)
            {
                for (uint8_t batchNo = 0; batchNo < m_binSizeInChars; ++batchNo)
                {
                    uint8_t batchRes = containsKmerBatch(kmerHash, batchNo);
                    if(batchRes == 0) continue;
                    for(uint8_t offset=0; offset<bitsPerChar; ++offset)
                    {
                        uint8_t binNo = batchNo * bitsPerChar + offset;
                        if (threshold - counts[binNo] > possible || selected[binNo]) continue;

                        if (IsBitSet(batchRes, offset))
                        {
                            ++counts[binNo];
                            if(counts[binNo] >= threshold)
                                selected[binNo] = true;
                        }
                    }
                }
                --possible;
            }

        }

        std::vector<bool> whichBins(TString const & text, uint8_t const & threshold) const
        {
            std::vector<bool> selected(BINS_SIZE, false);
            whichBins(selected, text, threshold);
            return selected;
        }

        std::vector<bool> whichBins(TString const & text_fwd, TString const & text_rev, uint8_t const & threshold) const
        {
            std::vector<bool> selected(BINS_SIZE, false);
            whichBins(selected, text_fwd, threshold);
            whichBins(selected, text_rev, threshold);
            return selected;
        }

    private:

        void initSize()
        {
            if (SIZE % 8 != 0)
            {
                std::cerr << "ERROR: Filter Size \"" << SIZE << "\" is not a multiple of 8." << std::endl;
                exit(1);
            }
            m_sizeInBytes = SIZE / bitsPerChar;
            m_sizeInHashes = SIZE / BINS_SIZE;
            m_binSizeInChars = BINS_SIZE/bitsPerChar;
            _filterFile = new uint8_t[m_sizeInBytes];
        }

        void insertKmer(uint64_t & kmerHash, uint8_t const & binNo)
        {
            uint64_t tmp = 0;
            for(uint8_t i = 0; i < N_HASH ; i++)
            {
                tmp = kmerHash * (_preCalcValues[i]);
                tmp ^= tmp >> _shiftValue;
                uint64_t normalizedValue = (tmp % m_sizeInHashes) * BINS_SIZE + binNo;
                __sync_or_and_fetch(&_filterFile[normalizedValue / bitsPerChar],
                                    bitMask[normalizedValue % bitsPerChar]);
            }
        }
        bool IsBitSet(uint8_t num, uint8_t bit) const
        {
            return 1 == ( (num >> bit) & 1);
        }

        uint8_t containsKmerBatch(uint64_t & kmerHash, uint8_t const & batch) const
        {
            uint8_t res = 255;
            uint64_t tmp = 0;
            uint64_t normalizedValue;
            for(uint8_t i = 0; i < N_HASH ; i++)
            {
                tmp = kmerHash * (_preCalcValues[i]);
                tmp ^= tmp >> _shiftValue;
                normalizedValue = (tmp % m_sizeInHashes) * BINS_SIZE;
                res = res & _filterFile[normalizedValue / bitsPerChar + batch];
            }
            return res;
        }

        bool containsKmer(uint64_t & kmerHash, uint8_t const & binNo) const
        {
            uint64_t tmp = 0;
            for(uint8_t i = 0; i < N_HASH ; i++)
            {
                tmp = kmerHash * (_preCalcValues[i]);
                tmp ^= tmp >> _shiftValue;
                uint64_t normalizedValue = (tmp % m_sizeInHashes) * BINS_SIZE + binNo;
                if (!IsBitSet(_filterFile[normalizedValue / bitsPerChar], (binNo % bitsPerChar)))
                    return false;
            }
            return true;
        }

        void _addKmers(TString const & text, uint8_t const & binNo)
        {
            TShape kmerShape;
            hashInit(kmerShape, begin(text));

            for (uint32_t i = 0; i < length(text) - length(kmerShape) + 1; ++i)
            {
                uint64_t kmerHash = hashNext(kmerShape, begin(text) + i);
                insertKmer(kmerHash, binNo);
            }
        }


        inline void _initPreCalcValues()
        {
            for(uint8_t i = 0; i < N_HASH ; i++)
            {
                _preCalcValues.push_back(i ^ KMER_SIZE * _seedValue);
            }
        }


        size_t                  m_sizeInBytes;
        size_t                  m_sizeInHashes;
        size_t                  m_binSizeInChars;
        uint8_t*                _filterFile;
        std::vector<uint64_t>   _preCalcValues = {};
        uint64_t const          _shiftValue = 27;
        uint64_t const          _seedValue = 0x90b45d39fb6da1fa;
    };
}

// ============================================================================
// Functions
// ============================================================================
// ----------------------------------------------------------------------------
// Function appendFileName()
// ----------------------------------------------------------------------------
inline void appendFileName(CharString & target, CharString const & source, uint32_t const i)
{
    target = source;
    append(target, std::to_string(i));
}

inline void appendFileName(CharString & target, uint32_t const i)
{
    CharString source = target;
    appendFileName(target, source, i);
}

// ----------------------------------------------------------------------------
// Function getExtensionWithLeadingDot()
// ----------------------------------------------------------------------------

template <typename TString>
inline typename Suffix<TString const>::Type
getExtensionWithLeadingDot(TString const & string)
{
    return suffix(string, firstOf(string, IsDot()));
}

// ----------------------------------------------------------------------------
// Function getFilesInDir()
// ----------------------------------------------------------------------------
inline void getFilesInDir(StringSet<CharString> & fileNames, CharString const directoryPath)
{
    DIR *dir;
    struct dirent *ent;
    struct stat st;

    dir = opendir(toCString(directoryPath));
    while ((ent = readdir(dir)) != NULL)
    {
        CharString fileName = ent->d_name;
        CharString fullFileName = directoryPath;
        append(fullFileName, "/");
        append(fullFileName, fileName);


        bool invalidFile = (fileName[0] == '.')
        || (stat(toCString(fullFileName), &st) == -1)
        || ((st.st_mode & S_IFDIR) != 0);

        if (!invalidFile)
            appendValue(fileNames, fileName);
    }
}

// ----------------------------------------------------------------------------
// Function getValidFilesInDir()
// ----------------------------------------------------------------------------
inline void getValidFilesInDir(StringSet<CharString> & fileNames,
                               CharString const directoryPath,
                               std::vector<std::string> const & validExtensions)
{
    StringSet<CharString>  allFileNames;
    getFilesInDir(allFileNames, directoryPath);
    for (uint32_t i = 0; i < length(allFileNames); ++i)
    {
        CharString ext = getExtensionWithLeadingDot(allFileNames[i]);

        auto it = std::find(validExtensions.begin(), validExtensions.end(), std::string(toCString(ext)));
        if (it != validExtensions.end())
            appendValue(fileNames, allFileNames[i]);
    }

}

// ----------------------------------------------------------------------------
// Function verifyIndicesDir()
// ----------------------------------------------------------------------------
inline bool verifyIndicesDir(CharString const directoryPath, uint32_t const numberOfBins)
{
    CharString bloomIndexFile = directoryPath;
    append(bloomIndexFile, "bloom.bf");

    FILE *file = fopen(toCString(bloomIndexFile), "rb");
    if (file == NULL)
    {
        std::cerr << "No bloom filter found in the given directory!" << std::endl;
        return false;
    }
    for (uint32_t i=0; i < numberOfBins; ++i)
    {
        CharString contigsLimitFile;
        appendFileName(contigsLimitFile, directoryPath, i);
        append(contigsLimitFile, ".txt.size");

        String<uint64_t> limits;

        if (!open(limits, toCString(contigsLimitFile), OPEN_RDONLY|OPEN_QUIET))
        {
            std::cerr << "No index for bin " << i << '\n';
            return false;
        }
    }
    return true;
}

template<typename T>
std::ostream& operator<<(std::ostream& s, const std::vector<T>& v) {
    char comma[3] = {'\0', ' ', '\0'};
    for (const auto& e : v) {
        s << comma << e;
        comma[0] = ',';
    }
    return s;
}

#endif  // #ifndef APP_YARA_MISC_OPTIONS_DIS_H_
