// HEADER PLACEHOLDER
// Contact Kunal Buch
#pragma once

#include "Inst.hpp"

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include "CacheFuncModel.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "common/LRUReplacement.hpp"
#include "common/RandomReplacement.hpp"
#include "SimpleCacheLine.hpp"

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
#include <functional>
#include <numeric>

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
#endif

#ifndef TAGELOG
    #define TAGELOG(msg) SPARTA_LOG(*branch_prediction_stat_logger_, msg)
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
class ATAGE;

//TODO: share these across bpus if there ends up being more than one
using Addr = sparta::memory::addr_t;

class ATAGE : public sparta::Unit,
              public BaseTAGE
{
    static const int MaxAddr = std::numeric_limits<int>::max();
public:
    //! \brief ...
    class ATAGEParameterSet : public sparta::ParameterSet
    {
      public:
        //! \brief ...
        ATAGEParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n)
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
                 "Minimum history size of ATAGE")

        PARAMETER(std::vector<uint16_t>,tagTableTagWidths, std::vector<uint16_t>({14, 10, 10, 11, 11}),
                 "Tag size in ATAGE tag tables")
        
        PARAMETER(std::vector<uint16_t>, logTagTableSizes, std::vector<uint16_t>({ 0, 12, 12, 13, 14}),
                 "Log2 of ATAGE table sizes")

        PARAMETER(std::vector<uint16_t>, tagTableAssociativity, std::vector<uint16_t>({ 0, 4, 4, 4, 4}),
                 "Associativity of ATAGE tables") 
                 
        PARAMETER(uint32_t, tagTableIndexingGranularity,   1,
                 "Indexing Granularity of ATAGE tables") 
                 
        PARAMETER(std::string, tagTableReplPolicy,    "PLRU",
                 "Replacement Policy of ATAGE tables")
        
        PARAMETER(std::vector<uint16_t>, tagTableHistLengths, {},
                 "History Lengths for each tagged table")
        
        PARAMETER(uint32_t, bimodalTableCounterBits,       2,
                 "Number of bimodal table counter bits")

        PARAMETER(uint32_t, tagTableCounterBits,           3,
                 "Number of tag table counter bits")
        
        PARAMETER(int32_t, tageCtrUpperThreshold,              2,
                 "If counter is greater than this, it is considered confident")
        
        PARAMETER(int32_t, tageCtrLowerThreshold,             -3,
                 "If counter is less than this, it is considered confident")
        
        PARAMETER(uint32_t, tagTableUBits,                 1,
                 "Number of tag table u bits")
        
        PARAMETER(uint32_t, maxHist,                     384,
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
                 "Max number of ATAGE entries allocted on mispredict")

        PARAMETER(bool,     use_ght,                       false,
                 "For default predictor, use GHT instead of bimodal")
    };

    //! \brief ...
    ATAGE(sparta::TreeNode* node,const ATAGEParameterSet* p);
    virtual ~ATAGE() {};

    //! \brief ...
    static constexpr char name[] = "ATAGE";

  public:
    // ------------------------------------------------------------------
    // Enums/Structs
    // ------------------------------------------------------------------
    //! \brief provider type
    enum {
        BASE_ONLY = 0,
        TAGE_LONGEST_MATCH = 1,
        BASE_ALT_MATCH = 2,
        TAGE_ALT_MATCH = 3,
        LAST_TAGE_PROVIDER_TYPE = TAGE_ALT_MATCH
    };

    // Struct for table entry for ATAGE
    struct TageEntry : public SimpleCacheLine {
        int16_t ctr;
        uint16_t tage_tag;
        uint8_t u;

        // Only used in ITTAGE
        sparta::memory::addr_t target_addr;
        TageEntry(uint64_t line_size) : SimpleCacheLine(line_size), ctr(0), tage_tag(0), u(0), target_addr(0x0) {}
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

        // Rebuilds all compressed histories for tag tables
        void rebuildAllCompressedHistories_();

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
            gHist(ghr_size, spec_hist_size, 
                  nTagTables,
                  logTagTableSizes, tagTableLengths)
        {}
    };

public:

    void disablePredictor() { pred_enable_ = false; }

    // ------------------------------------------------------------------
    //!  \brief Update ATAGE.
    // ------------------------------------------------------------------
    void update_tage(const InstPtr& branch_inst, bool flushed);

    /**
     * ATAGE prediction called from ATAGE::predict
     * @param tid The thread ID to select the global
     * histories to use.
     * @param branch_pc The unshifted branch PC.
     * @param cond_branch True if the branch is conditional.
     */
    std::tuple<bool, TageBranchInfoPtr>
    predict_dir(Addr pc, bool cond_branch, bool is_backward,
                const bool actual_taken, const bool ght_pred);

    std::tuple<Addr, TageBranchInfoPtr> predict_addr(Addr pc, bool indirect_branch, Addr btb_target_addr, bool tage_prediction);

private:

    std::vector<uint16_t> determineHistLengths_(uint32_t nTables, 
                                                uint32_t gHistBits, 
                                                uint32_t minHist,
                                                std::vector<uint16_t> histLengths) const;

    void logBranch_(const bool pred_taken, const TageBranchInfoPtr &bp_info);
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

    /**
     * Computes the index used to access the
     * bimodal table.
     * @param pc_in The unshifted branch PC.
     */
    int bindex_(Addr pc_in) const;

    /**
     * Computes the index used to access a
     * partially tagged table.
     * @param tid The thread ID used to select the
     * global histories to use.
     * @param pc The unshifted branch PC.
     * @param bank The partially tagged table to access.
     */
    uint32_t gindex_(Addr pc, int bank) const;

    /**
     * Computes the partial tag of a tagged table.
     * @param tid the thread ID used to select the
     * global histories to use.
     * @param pc The unshifted branch PC.
     * @param bank The partially tagged table to access.
     */
    uint32_t gtag_(Addr pc, int bank) const;

    /**
     * Updates a direction counter based on the actual
     * branch outcome.
     */
    template<typename T>
    void ctrUpdate_(T & ctr, bool taken, uint32_t nbits);

    // Is this counter saturated?
    bool isCtrSaturated_(int32_t ctr, uint32_t nbits) {
        if ((ctr == ((1 << (nbits - 1)) - 1))
            || (ctr == (-(1 << (nbits - 1))))) {
            return true; 
        }
    
        return false;
    }

    // Function to set the replacement policy through the parameters
    std::unique_ptr<sparta::cache::ReplacementIF> setReplPolicy_(std::string const & repl_policy, uint16_t table) {

        if (repl_policy == "PLRU") {
            return std::make_unique<sparta::cache::TreePLRUReplacement>(tagTableAssociativity_[table]);
        }
        else if (repl_policy == "LRU") {
            return std::make_unique<sparta::cache::LRUReplacement>(tagTableAssociativity_[table]);
        }
        else if (repl_policy == "RANDOM") {
            return std::make_unique<sparta::cache::RandomReplacement>(tagTableAssociativity_[table]);
        }
        else if (repl_policy == "NRU") {
            // to be implemented
        }
        else if (repl_policy == "RRIP") {
            // to be implemented
        }
        else {
            sparta_assert(false, "Invalid Replacement policy for ATAGE");
        }

        return nullptr;
    };

    /**
     * Updates an unsigned counter based on up/down parameter
     * @param ctr Reference to counter to update.
     * @param up Boolean indicating if the counter is incremented/decremented
     * If true it is incremented, if false it is decremented
     * @param nbits Counter width.
     */
    static void unsignedCtrUpdate_(uint8_t & ctr, bool up, unsigned nbits);

    /**
     * Get a branch prediction from the bimodal
     * predictor.
     * @param pc The unshifted branch PC.
     * @param bi Pointer to information on the
     * prediction.
     */
    bool getBimodalPred_(uint32_t index) const;

    /**
     * Update ATAGE for conditional branches.
     * @param branch_pc The unshifted branch PC.
     * @param taken Actual branch outcome.
     * @param bi Pointer to information on the prediction
     * recorded at prediction time.
     * @param nrand Random int number from 0 to 3
     * @param preAdjustAlloc call adjustAlloc before checking
     * pseudo newly allocated entries
     */

    void condBranchTAGEUpdate_(Addr branch_pc, bool taken, const TageBranchInfoPtr& bi);
    void indirectBranchUpdate_(Addr branch_pc, Addr target_addr, const TageBranchInfoPtr& bi);

    /**
     * Update the stats
     * @param taken Actual branch outcome
     * @param bi Pointer to information on the prediction
     * recorded at prediction time.
     */
    void updateTAGEStats_(bool taken, const TageBranchInfoPtr& bi);

    /**
     * On a prediction, calculates the ATAGE indices and tags for
     * all the different history lengths
     */
    void calculateIndicesAndTags_(const TageBranchInfoPtr& bi, Addr branch_pc);

    /**
     * Calculation of the index for useAltPredForNewlyAllocated
     * On this base ATAGE implementation it is always 0
     */
    unsigned getUseAltIdx_(const TageBranchInfoPtr& bp_info);

    /**
     * Extra calculation to tell whether ATAGE allocaitons may happen or not
     * on an update
     * For this base ATAGE implementation it does nothing
     */
    void adjustAlloc_(bool & alloc, bool taken, bool pred_taken);

    /**
     * Handles Allocation and U bits reset on an update
     */
    void handleAllocAndUReset_(bool alloc, bool taken, TageBranchInfoPtr bi, int nrand);

    /**
     * Handles the U bits reset
     */
    void handleUReset_();

    /**
     * Handles the update of the ATAGE entries
     */
    void handleTAGEUpdate_(Addr branch_pc, bool taken, TageBranchInfoPtr bi);

    /**
     * Algorithm for resetting a single U counter
     */
    void resetUCtr_(uint8_t & u);

    int8_t getCtr_(int hitBank, int hitBankIndex) const;
    unsigned getTageCtrBits_() const;
    uint64_t getPathHist_() const;
    std::vector<bool> getGlobalHist_() const;

    // Checks if a matching entry exists in the specified tagged table bank.
    bool hasMatchingEntry_(uint32_t bank, uint32_t index, uint32_t tag) const;

    // Retrieves a pointer to the matching entry in the specified tagged table bank.
    TageEntry* getMatchingEntry_(uint32_t bank, uint32_t index, uint32_t tag) const;

    // Resets the usefulness counters across all tagged tables when the reset timer expires.
    void handleUsefulBitReset_();

    // Allocates new entries in tagged tables by identifying and replacing the least useful entries.
    void allocateEntries_(Addr branch_pc, bool taken, const TageBranchInfoPtr& bi);

    // Updates the bimodal table counter and validates allocation conditions for bimodal predictions.
    void updateBimodalPrediction_(Addr branch_pc, bool taken, const TageBranchInfoPtr& bi, bool& alloc);

    // Updates counters and usefulness bits for tagged table entries based on prediction outcome.
    void updateTaggedTablePrediction_(Addr branch_pc, bool taken, const TageBranchInfoPtr& bi, bool& alloc);

    // Determines if the prediction was provided by a tagged table rather than the bimodal predictor.
    bool isTaggedTableProvider_(const TageBranchInfoPtr& bi);

    // Determines whether a new entry should be allocated based on prediction correctness and hit bank validity.
    bool shouldAllocateEntry_(const TageBranchInfoPtr& bi, bool taken);

    // Finds the longest and alternate matching banks for a given branch info
    void findMatchingBanks_(const TageBranchInfoPtr& bi);

    // Computes the TAGE prediction and alternate prediction based on matching entries
    void computeTagePrediction_(const TageBranchInfoPtr& bi, const bool& ght_pred);

private:
    std::vector<TageEntry> btable_;
    std::vector<std::unique_ptr<TageCacheFuncModel<TageEntry>>> g_table_;

    std::vector<uint32_t> tagTableIndices_;
    std::vector<uint32_t> tableTags_;

    std::vector<int8_t> useAltPredForNewlyAllocated_;
    uint64_t uResetTimer_;
    bool uResetTimerEnable_;

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
    const uint32_t maxHist_;
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
    std::vector<uint16_t> tagTableAssociativity_;
    //! \brief ...
    const uint32_t tagTableIndexingGranularity_;
    //! \brief ...
    const std::string tagTableReplPolicy_;
    //! \brief ...
    const int64_t initialUResetTimerValue_;
    //! \brief ...
    uint32_t numUseAltOnNa_;
    //! \brief ...
    uint32_t useAltOnNaBits_;
    //! \brief ...
    uint32_t maxNumAlloc_;
    //! \brief ...

    //! \brief For default predictor, use GHT instead of bimodal
    const bool use_ght_;

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
    sparta::Counter baseAltMatchProviderCorrect_;
    //! \brief see .cpp
    sparta::Counter baseProviderCorrect_;
    //! \brief see .cpp
    sparta::Counter baseProvider_;
    //! \brief see .cpp
    sparta::Counter baseAltMatchProvider_;
    //! \brief see .cpp
    sparta::Counter tageLongestMatchProvider_;
    //! \brief see .cpp
    sparta::Counter tageLongestMatchProviderCorrect_;
    //! \brief see .cpp
    sparta::Counter tageLongestMatchProviderWrong_;
    //! \brief see .cpp
    sparta::Counter tageAltMatchProvider_;
    //! \brief see .cpp
    sparta::Counter tageAltMatchProviderCorrect_;
    //! \brief see .cpp
    sparta::Counter tageAltMatchProviderWrong_;
    //! \brief see .cpp
    std::vector<sparta::Counter*> longestMatchProviderWrong_;
    //! \brief see .cpp
    std::vector<sparta::Counter*> altMatchProviderWrong_;
    //! \brief see .cpp
    sparta::Counter baseAltMatchProviderWrong_;
    //! \brief see .cpp
    sparta::Counter baseProviderWrong_;
    //! \brief see .cpp
    sparta::Counter altMatchProviderWouldHaveHit_;
    //! \brief see .cpp
    sparta::Counter longestMatchProviderWouldHaveHit_;
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

    //! \brief ... TODO
    TageHistory tageHistory_;
};
}
