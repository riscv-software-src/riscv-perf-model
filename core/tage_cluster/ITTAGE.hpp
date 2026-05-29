// HEADER PLACEHOLDER
// Contact Kunal Buch

#pragma once

#include "Inst.hpp"

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include "sparta/log/MessageSource.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/statistics/Histogram.hpp"
#include "sparta/statistics/BasicHistogram.hpp"

#include <limits>
#include <random>
#include <vector>
#include <list>
#include <bitset>
#include <algorithm>
#include <cmath>

#include "BaseTAGE.hpp"
#include "BranchInfo.hpp"

#ifndef Tag
#define Tag uint16_t
#endif

#ifndef Index
#define Index uint16_t
#endif

#ifndef Path
#define Path uint64_t
#endif

#ifndef History
#define History uint64_t
#endif

#ifndef HASH_GUARD
#define HASH_GUARD

constexpr bool HASH = 2;
constexpr bool HASH_IMPL = 1;
constexpr bool HASH_CHECK = false;
#endif

// Compressed histories
//
// HASH=2: bit reversed hash that works in all implementations
//
// HASH_IMPL=1: incremental hash implementation that maintains compressed histories
//
// This section will describe the different types of hashes
//
// Assume that the in size is 8 and the out size is 3
//
// The initial ghr
// 8 7 6 5 4 3 2 1
//
// Compressed Hash 2
// 1 2 3
// 4 5 6
// 7 8
//
// The rows are XORed together to get the final compressed hash
//
// The ghr with a new bit shifted in
// 8 7 6 5 4 3 2 1 0
//
// Compressed Hash 2
// 0 1 2
// 3 4 5
// 6 7 8
//
// Compressed Hash 2
// This hash rotates to the right. Bits are inserted at the msb. Bits are
// removed at a consistent position depending on the in size and out size of
// the hash. Notice that the bits XORed in each column do not change as the
// algorithm progresses. This hash can be accelerate by rotating to the right
// and XORing in the in bit and XORing out the out bit.

namespace olympia
{
class ITTAGE : public sparta::Unit,
              public BaseTAGE
{
    static const int MaxAddr = std::numeric_limits<int>::max();
public:
    //! \brief ...
    class ITTAGEParameterSet : public sparta::ParameterSet
    {
      public:
        //! \brief ...
        ITTAGEParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n)
        {
        }

        PARAMETER(bool,     pred_enable,                   false,
                  "Enable prediction")
        
        PARAMETER(uint32_t, instShiftAmt,                  5,
                 "Number of bits to shift instructions by")

        PARAMETER(uint32_t, nTagTables,                    16,
                 "Number of tagged tables")
        
        PARAMETER(bool,     enable_global_hist,            true,
                 "Enable Global History while creating tags and index")

        PARAMETER(uint32_t, minHist,                       4,
                 "Minimum history size of ITTAGE")

        PARAMETER(std::vector<uint16_t>,tagTableTagWidths, std::vector<uint16_t>({14, 10, 10, 11, 11, 11, 12, 12, 13, 13, 12, 12, 11, 11, 10, 10,  9}),
                 "Tag size in ITTAGE tag tables")
        
        PARAMETER(std::vector<uint16_t>, logTagTableSizes, std::vector<uint16_t>({ 0,  7,  7,  8,  8,  9, 10, 11, 12, 12, 13, 14, 15, 16, 16, 17, 17}),
                 "Log2 of ITTAGE table sizes")
        
        PARAMETER(std::vector<uint16_t>, tagTableHistLengths, {},
                 "History Lengths for each tagged table")
        
        PARAMETER(uint32_t, bimodalTableCounterBits,       2,
                 "Number of bimodal table counter bits")

        PARAMETER(uint32_t, tagTableCounterBits,           3,
                 "Number of tag table counter bits")
        
        PARAMETER(uint32_t, tagTableUBits,                 1,
                 "Number of tag table u bits")
        
        PARAMETER(uint32_t, maxHist,                    1024,
                 "A large number to track all branch histories")

        PARAMETER(bool,     enable_path_hist,              true,
                 "Enable Path History while creating index")

        PARAMETER(uint32_t, pathHistBits,                  64,
                 "Path history size")

        PARAMETER(uint32_t, numUseAltOnNa,                 1,
                 "Number of USE_ALT_ON_NA counters")

        PARAMETER(uint64_t,  initialUResetTimerValue,      0x100000,
                 "Initial value of uResetTimer")

        PARAMETER(uint32_t, useAltOnNaBits,                4,
                 "Size of the USE_ALT_ON_NA counter(s)")

        PARAMETER(uint32_t, maxNumAlloc,                   1,
                 "Max number of ITTAGE entries allocted on mispredict")

        PARAMETER(bool,     use_btb,                       true,
                 "For default predictor, use BTB instead of bimodal")
    };

    //! \brief ...
    ITTAGE(sparta::TreeNode* node,const ITTAGEParameterSet* p);
    virtual ~ITTAGE() {};

    //! \brief ...
    static constexpr char name[] = "ITTAGE";

  public:
    // ------------------------------------------------------------------
    // Enums/Structs
    // ------------------------------------------------------------------
    //! \brief provider type
    enum {
        BIMODAL_ONLY = 0,
        TAGE_LONGEST_MATCH = 1,
        BIMODAL_ALT_MATCH = 2,
        TAGE_ALT_MATCH = 3,
        LAST_TAGE_PROVIDER_TYPE = TAGE_ALT_MATCH
    };

    // Struct for table entry for ITTAGE
    struct TageEntry {
        bool valid;

        int16_t ctr;
        uint16_t tag;
        uint8_t u;

        // Only used in ITTAGE
        sparta::memory::addr_t target_addr;
        TageEntry() : valid(false), ctr(0), tag(0), u(0), target_addr(0x0) {}
    };

    class GHR {

        public:
        GHR(uint32_t histLength,
            uint32_t specHistLength,
            uint32_t nTagTables,
            const std::vector<uint16_t>& logTagTableSizes,
            const std::vector<uint16_t>& tagTableLengths) :
            total_hist_length(histLength + specHistLength),
            speculative_hist_length(specHistLength),
            nTagTables_(nTagTables),
            logTagTableSizes_(logTagTableSizes),
            tagTableLengths_(tagTableLengths),
            tag_in_size_(2 * nTagTables_ + 1),
            tag_out_size_(2 * nTagTables_ + 1),
            tag_in_pos_(2 * nTagTables_ + 1),
            tag_out_pos_(2 * nTagTables_ + 1),
            tag_compressed_hist_(2 * nTagTables_ + 1),
            ghr(0) {
            
            // Setup to compute compressed histories for TAG tables
            // idx 0:                                  Base table. This unused.
            // idx 1 ... nTagTables_:                  in_size = histLengths[bank], out_size = logTagTableSizes[bank]
            // idx nTagTables_+1 ... 2*nTagTables_:    in_size = histLengths[bank], out_size = logTagTableSizes[bank]-1
            for (int idx = 1; idx <= 2 * nTagTables_; ++idx) {
                const int bank = idx <= nTagTables_ ? idx : idx - nTagTables_;
                const int in_size = tagTableLengths_[bank];
                const int out_size = logTagTableSizes_[bank] - (idx <= nTagTables_ ? 0 : 1);
                tag_in_size_[idx] = in_size;
                tag_out_size_[idx] = out_size;

                if (HASH == 2) {
                    tag_in_pos_[idx] = out_size - 1;

                    int num_bits_remaining = (in_size + 1) % out_size;
                    if (num_bits_remaining == 0) num_bits_remaining = out_size;
                    tag_out_pos_[idx] = (out_size - 1) - (num_bits_remaining - 1);
                }
            }

            (void) total_hist_length;
        }

        // Push at LSB of teh bitset, automatically masks the
        // bits from the front beyond the size
        void push(bool taken);

        // Removes all the younger content, pushes the result onto list
        void flushAndRestore(std::list<bool>::iterator iter);

        // Get the Iterator from specHistory list
        std::list<bool>::iterator getLastIter();

        bool at(uint32_t index) const;

        uint32_t getSpecHistSize() const {
            return speculativeHistory.size();
        }

        uint32_t size() const {
            return MAX_GHR_SIZE;
        }

        uint64_t getTagCompressedGlobalHistory(int bank) const { return tag_compressed_hist_[bank].to_ulong(); }

        uint64_t getGhr64() const {
            std::bitset<64> out;
            for (int i = 0; i < 64; ++i) {
                out[i] = ghr[i];
            }
            return out.to_ulong();
        }

        private:

        // append to the speculative part of history
        void pushSpecHist_(bool taken);

        // Compute the compressed global history on demand
        uint64_t computeCompressedGlobalHistory_(uint32_t inSize, uint32_t outSize) const;

        static const size_t MAX_GHR_SIZE = 5120; // Define a maximum size

        // History lengths
        const uint32_t total_hist_length = 0;
        const uint32_t speculative_hist_length = 0;

        uint64_t nTagTables_ = 0;
        
        std::vector<uint16_t> logTagTableSizes_;
        
        std::vector<uint16_t> tagTableLengths_;

        std::vector<uint16_t> tag_in_size_;   // The number of bits from the ghr
        std::vector<uint16_t> tag_out_size_;  // The number of bits in the compressed history
        std::vector<uint16_t> tag_in_pos_;    // The bit position to shift into the compressed history
        std::vector<uint16_t> tag_out_pos_;   // The bit position to shift out of the compressed history
        std::vector<std::bitset<64>> tag_compressed_hist_;
        
        // History bitset and speculative list
        std::bitset<MAX_GHR_SIZE> ghr;
        std::list<bool> speculativeHistory;
    };

    struct TageHistory {

        // Speculative path history
        uint64_t pathHist;

        // Speculative branch direction history (circular buffer)
        GHR gHist;

        TageHistory(uint32_t ghr_size,
                    uint32_t spec_hist_size,
                    uint32_t nTagTables,
                    const std::vector<uint16_t>& logTagTableSizes,
                    const std::vector<uint16_t>& tagTableLengths) :
            pathHist(0x0),
            gHist(ghr_size, spec_hist_size, nTagTables, logTagTableSizes, tagTableLengths)
        {}
    };

public:

    void disablePredictor() { pred_enable_ = false; }

    // ------------------------------------------------------------------
    //!  \brief Update ITTAGE.
    // ------------------------------------------------------------------
    void update_ittage(const InstPtr& branch_inst, bool flushed);

    // ITTAGE prediction called from ITTAGE::predict_addr
    std::tuple<Addr, TageBranchInfoPtr> predict_addr(Addr pc, bool indirect_branch, Addr btb_target_addr, bool tage_prediction);

private:

    std::vector<uint16_t> determineHistLengths_(uint32_t nTables, 
                                                uint32_t gHistBits, 
                                                uint32_t minHist,
                                                std::vector<uint16_t> histLengths) const;

    uint64_t getSizeInBits_();

    //! \brief ...
    void  init_();

    // ------------------------------------------------------------------
    //!  \brief Restores speculatively updated path and direction histories.
    //!  This version of restoreHistories_() is called once on a branch update.
    // ------------------------------------------------------------------
    void restoreHistories_(const TageBranchInfoPtr& bi);

    //! \brief (Speculatively) updates global histories (path and direction).
    // ------------------------------------------------------------------
    void updateHistories_(Addr branch_pc, bool taken);

    // helper hash function to compress the path history
    uint64_t get_path_history_hash_(uint32_t component) const;

    // Compress global history of last 'inSize' branches into 'outSize' by wrapping the history
    uint64_t get_compressed_global_history_(uint32_t inSize, uint32_t outSize) const;

    int bindex_(Addr pc_in) const;
    uint32_t gindex_(Addr pc, int bank) const;
    uint32_t gtag_(Addr pc, int bank) const;

    template<typename T>
    void ctrUpdate_(T & ctr, bool taken, uint32_t nbits);

    static void unsignedCtrUpdate_(uint8_t & ctr, bool up, unsigned nbits);

    void indirectBranchUpdate_(Addr branch_pc, Addr target_addr, const TageBranchInfoPtr& bi);

    void updateITTAGEStats_(Addr target_addr, const TageBranchInfoPtr& bi);

    void calculateIndicesAndTags_(Addr branch_pc);

    unsigned getUseAltIdx_(const TageBranchInfoPtr& bp_info);

    void resetUctr_(uint8_t & u);

    /* Helper functions for indirect branch updates (refactor targets) */
    void indirectHandleProviderUpdate_(Addr branch_pc, Addr target_addr, const TageBranchInfoPtr& bi, bool &alloc);
    void indirectAllocateEntries_(Addr branch_pc, Addr target_addr, const TageBranchInfoPtr& bi);

    /* Helper functions for indirect branch prediction (refactor targets) */
    Addr getPredictedAddr_(uint32_t bank, Addr fallback) const;
    void findIndirectHitAndAltBanks_(const TageBranchInfoPtr& bp_info);
    void computeIndirectPrediction_(const TageBranchInfoPtr& bp_info, Addr pred_addr);

    int8_t getCtr_(int hitBank, int hitBankIndex) const;

private:
    std::vector<TageEntry> btable_;
    TageEntry **g_table_;

    std::vector<uint32_t> tagTableIndices_;
    std::vector<uint32_t> tableTags_;

    std::vector<int8_t> useAltPredForNewlyAllocated_;
    uint64_t uResetTimer_;

    bool initialized_{false};

    // ------------------------------------------------------------------
    // Parameters
    // ------------------------------------------------------------------

    //! \brief Parameter: master enable
    bool pred_enable_;

    //! \brief ...
    const uint32_t nTagTables_;
    //! \brief ...
    const uint32_t bimodalTableCounterBits_;
    //! \brief ...
    const uint32_t tagTableCounterBits_;
    //! \brief ...
    const uint32_t tagTableUBits_;
    //! \brief ...
    const uint32_t gHistBits_;
    //! \brief ...
    const uint32_t enable_global_hist_;
    //! \brief ...
    const uint32_t minHist_;
    //! \brief ...
    const uint32_t enable_path_hist_;
    //! \brief ...
    const uint32_t pathHistBits_;
    //! \brief ...
    std::vector<uint16_t> tagTableTagWidths_;
    //! \brief ...
    std::vector<uint16_t> logTagTableSizes_;
    //! \brief ...
    std::vector<uint16_t> tagTableHistLengths_;
    //! \brief ...
    const int64_t initialUResetTimerValue_;
    //! \brief ...
    uint32_t numUseAltOnNa_;
    //! \brief ...
    uint32_t useAltOnNaBits_;
    //! \brief ...
    uint32_t maxNumAlloc_;

    //! \brief For default predictor, use GHT instead of bimodal
    bool use_btb_;

    //! \brief ...
    const uint32_t instShiftAmt_;

    // ------------------------------------------------------------------
    // Counters/Stats
    // ------------------------------------------------------------------
    //! \brief see .cpp
    std::vector<sparta::Counter*> longestMatchProviderCorrect_;
    //! \brief see .cpp
    std::vector<sparta::Counter*> altMatchProviderCorrect_;
    //! \brief see .cpp
    sparta::Counter bimodalAltMatchProviderCorrect_;
    //! \brief see .cpp
    sparta::Counter bimodalProviderCorrect_;
    //! \brief see .cpp
    sparta::Counter bimodalProvider_;
    //! \brief see .cpp
    sparta::Counter bimodalAltProvider_;
    //! \brief see .cpp
    sparta::Counter tageLongestMatchProvider_;
    //! \brief see .cpp
    sparta::Counter tageAltProvider_;
    //! \brief see .cpp
    sparta::Counter tageAltProviderCorrect_;
    //! \brief see .cpp
    sparta::Counter tageAltProviderWrong_;
    //! \brief see .cpp
    std::vector<sparta::Counter*> longestMatchProviderWrong_;
    //! \brief see .cpp
    std::vector<sparta::Counter*> altMatchProviderWrong_;
    //! \brief see .cpp
    sparta::Counter bimodalAltMatchProviderWrong_;
    //! \brief see .cpp
    sparta::Counter bimodalProviderWrong_;
    //! \brief see .cpp
    sparta::Counter altMatchProviderWouldHaveHit_;
    //! \brief see .cpp
    sparta::Counter longestMatchProviderWouldHaveHit_;
    //! \brief see .cpp
    sparta::Counter tageWouldHaveHit_;
    //! \brief see .cpp
    sparta::Counter allocateOnFlush_;
    //! \brief see .cpp
    sparta::Counter uResetCounter_;
    //! \brief see .cpp
    sparta::Counter sizeInBits_;
    //! \brief see .cpp
    std::vector<sparta::Counter*> flushAlloc_;
    //! \brief see .cpp
    std::vector<sparta::Counter*> longestMatchProvider_;
    //! \brief see .cpp
    std::vector<sparta::Counter*> altMatchProvider_;
    //! \brief see .cpp
    std::vector<sparta::Counter*> conflicted_evictions_;

    sparta::Counter stat_bp_hit_table_correct_ { getStatisticSet(), "bp_hit_table_correct",
        "Number of times the hit table would have been correct",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_hit_table_incorrect_ { getStatisticSet(), "bp_hit_table_incorrect",
        "Number of times the hit table would have been incorrect",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_alt_table_correct_ { getStatisticSet(), "bp_alt_table_correct",
        "Number of times the alt table would have been correct",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_alt_table_incorrect_ { getStatisticSet(), "bp_alt_table_incorrect",
        "Number of times the alt table would have been incorrect",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_hit_table_used_correct_ { getStatisticSet(), "bp_hit_table_used_correct",
        "Number of times the hit table was select and was correct",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_hit_table_used_incorrect_ { getStatisticSet(), "bp_hit_table_used_incorrect",
        "Number of times the hit table was select and was inccorrect",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_alt_table_used_correct_ { getStatisticSet(), "bp_alt_table_used_correct",
        "Number of times the alt table was select and was correct",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_alt_table_used_incorrect_ { getStatisticSet(), "bp_alt_table_used_incorrect",
        "Number of times the alt table was select and was incorrect",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_oracle_converted_to_hit_ { getStatisticSet(), "bp_oracle_converted_to_hit",
        "Number of times alt oracle changed prediction to use the hit table",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_oracle_converted_to_alt_ { getStatisticSet(), "bp_oracle_converted_to_alt",
        "Number of times alt oracle changed prediction to use the alt table",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_oracle_corrected_to_hit_ { getStatisticSet(), "bp_oracle_corrected_to_hit",
        "Number of times alt oracle changed prediction to use the hit table, and the other prediction would have been wrong",
        sparta::Counter::COUNT_NORMAL
    };

    sparta::Counter stat_bp_oracle_corrected_to_alt_ { getStatisticSet(), "bp_oracle_corrected_to_alt",
        "Number of times alt oracle changed prediction to use the alt table, and the other prediction would have been wrong",
        sparta::Counter::COUNT_NORMAL
    };

    //! \brief ... TODO
    TageHistory tageHistory_;
};
}
