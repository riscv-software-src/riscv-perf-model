#include "ATAGE.hpp"
#include "CoreUtils.hpp"

namespace olympia
{

constexpr char ATAGE::name[];

ATAGE::ATAGE(sparta::TreeNode* node, const ATAGEParameterSet* p)
    : sparta::Unit(node),
      initialized_(false),

      // Yaml parameters
    pred_enable_               (p->pred_enable),
    nTagTables_                (p->nTagTables),
    bimodalTableCounterBits_   (p->bimodalTableCounterBits),
    tagTableCounterBits_       (p->tagTableCounterBits),
    tagTableUBits_             (p->tagTableUBits),
    maxHist_                   (p->maxHist),
    enable_global_hist_        (p->enable_global_hist),
    minHist_                   (p->minHist),
    enable_path_hist_          (p->enable_path_hist),
    pathHistBits_              (p->pathHistBits),
    tagTableTagWidths_         (p->tagTableTagWidths),
    logTagTableSizes_          (p->logTagTableSizes),
    tagTableHistLengths_       (determineHistLengths_(nTagTables_, maxHist_, minHist_, p->tagTableHistLengths)),
    tagTableAssociativity_     (p->tagTableAssociativity),
    tagTableIndexingGranularity_(p->tagTableIndexingGranularity),
    tagTableReplPolicy_        (p->tagTableReplPolicy),
    initialUResetTimerValue_   (p->initialUResetTimerValue),
    numUseAltOnNa_             (p->numUseAltOnNa),
    useAltOnNaBits_            (p->useAltOnNaBits),
    maxNumAlloc_               (p->maxNumAlloc),
    use_ght_                   (p->use_ght),
    instShiftAmt_              (p->instShiftAmt),

      // Stats
      baseAltMatchProviderCorrect_(&unit_stat_set_,
         "baseAltMatchProviderCorrect",
         "Number of times ATAGE Alt Match is the base and it is the "
         "provider and the prediction is correct",
         sparta::Counter::COUNT_NORMAL),

      baseProviderCorrect_(&unit_stat_set_,
         "baseProviderCorrect",
         "Number of times there are no hits on the ATAGE tables and the "
         "base prediction is correct",
         sparta::Counter::COUNT_NORMAL),
      
      baseProvider_(&unit_stat_set_,
         "baseProvider",
         "Number of times base was provider",
         sparta::Counter::COUNT_NORMAL),
      
      baseAltMatchProvider_(&unit_stat_set_,
         "baseAltMatchProvider",
         "Number of times base as Alt was provider",
         sparta::Counter::COUNT_NORMAL),
      
      tageLongestMatchProvider_(&unit_stat_set_,
         "tageLongestMatchProvider",
         "Number of times ATAGE longest match was provider",
         sparta::Counter::COUNT_NORMAL),

      tageLongestMatchProviderCorrect_(&unit_stat_set_,
         "tageLongestMatchProviderCorrect",
         "Number of times ATAGE has longest match provider correct",
         sparta::Counter::COUNT_NORMAL),
      
      tageLongestMatchProviderWrong_(&unit_stat_set_,
         "tageLongestMatchProviderWrong",
         "Number of times ATAGE has longest match provider wrong",
         sparta::Counter::COUNT_NORMAL),
      
      tageAltMatchProvider_(&unit_stat_set_,
         "tageAltMatchProvider",
         "Number of times ATAGE as Alt Match was provider",
         sparta::Counter::COUNT_NORMAL),
      
      tageAltMatchProviderCorrect_(&unit_stat_set_,
         "tageAltMatchProviderCorrect",
         "Number of times ATAGE as Alt Match was provider",
         sparta::Counter::COUNT_NORMAL),
      
      tageAltMatchProviderWrong_(&unit_stat_set_,
         "tageAltMatchProviderWrong",
         "Number of times ATAGE as Alt Match was provider",
         sparta::Counter::COUNT_NORMAL),

      baseAltMatchProviderWrong_(&unit_stat_set_,
         "baseAltMatchProviderWrong",
         "Number of times ATAGE Alt Match is the base and it is the "
         "provider and the prediction is wrong",
         sparta::Counter::COUNT_NORMAL),

      baseProviderWrong_(&unit_stat_set_,
         "baseProviderWrong",
         "Number of times there are no hits on the ATAGE tables and the "
         "base prediction is wrong",
         sparta::Counter::COUNT_NORMAL),
      
      altMatchProviderWouldHaveHit_(&unit_stat_set_,
            "altMatchProviderWouldHaveHit",
            "Number of times ATAGE Longest Match is the provider, the "
            "prediction is wrong and Alt Match prediction was correct",
            sparta::Counter::COUNT_NORMAL),

      allocateOnFlush_(&unit_stat_set_,
         "allocateOnFlush",
         "Number of times ATAGE Tables got allcoated",
         sparta::Counter::COUNT_NORMAL),

      uResetCounter_(&unit_stat_set_,
         "uResetCounter",
         "Number of times ATAGE Tables ubit got reset",
         sparta::Counter::COUNT_NORMAL),
      
      sizeInBits_(&unit_stat_set_,
         "sizeInBits",
         "Total bits of ATAGE predictor",
         sparta::Counter::COUNT_LATEST),

      // Histories for Tage
      tageHistory_(maxHist_, coreutils::getMaxInflightPreds(node) + coreutils::getMaxInflightBranches(node),
                         nTagTables_,
                         logTagTableSizes_,
                         tagTableHistLengths_) {
 
    if(pred_enable_) {
        
        // Initialize the ATAGE module
        init_();

        // Counters for each table
        conflicted_evictions_.resize(nTagTables_ + 1);
        flushAlloc_.resize(nTagTables_ + 1);
        longestMatchProvider_.resize(nTagTables_ + 1);
        altMatchProvider_.resize(nTagTables_ + 1);

        altMatchProviderCorrect_.resize(nTagTables_ + 1);
        altMatchProviderWrong_.resize(nTagTables_ + 1);

        longestMatchProviderCorrect_.resize(nTagTables_ + 1);
        longestMatchProviderWrong_.resize(nTagTables_ + 1);

        for (uint32_t i = 0; i <= nTagTables_; ++i) {

            auto counter = new sparta::Counter(&unit_stat_set_,
                                                "flushAlloc_" + std::to_string(i),
                                                "Number of times ATAGE Tables got allcoated",
                                                sparta::Counter::COUNT_NORMAL);
            flushAlloc_[i] = counter;

            auto counter1 = new sparta::Counter(&unit_stat_set_,
                                                "longestMatchProvider_" + std::to_string(i),
                                                "Number of times ATAGE Bank matched the longest",
                                                sparta::Counter::COUNT_NORMAL);
            longestMatchProvider_[i] = counter1;

            auto counter2 = new sparta::Counter(&unit_stat_set_,
                                                "altMatchProvider_" + std::to_string(i),
                                                "Number of times ATAGE Tables matched the atlernate",
                                                sparta::Counter::COUNT_NORMAL);
            altMatchProvider_[i] = counter2;

            auto counter3 = new sparta::Counter(&unit_stat_set_,
                                                "altMatchProviderCorrect_" + std::to_string(i),
                                                "Number of times ATAGE Alt Match is the provider and the "
                                                "prediction is correct",
                                                sparta::Counter::COUNT_NORMAL);

            altMatchProviderCorrect_[i] = counter3;

            auto counter4 = new sparta::Counter(&unit_stat_set_,
                                                "altMatchProviderWrong_" + std::to_string(i),
                                                "Number of times ATAGE Alt Match is the provider and the "
                                                "prediction is wrong",
                                                sparta::Counter::COUNT_NORMAL);

            altMatchProviderWrong_[i] = counter4;

            auto counter5 = new sparta::Counter(&unit_stat_set_,
                                                "longestMatchProviderCorrect_" + std::to_string(i),
                                                "Number of times ATAGE Longest Match is the provider and the "
                                                "prediction is correct",
                                                sparta::Counter::COUNT_NORMAL);

            longestMatchProviderCorrect_[i] = counter5;

            auto counter6 = new sparta::Counter(&unit_stat_set_,
                                                "longestMatchProviderWrong_" + std::to_string(i),
                                                "Number of times ATAGE Longest Match is the provider and the "
                                                "prediction is wrong",
                                                sparta::Counter::COUNT_NORMAL);

            longestMatchProviderWrong_[i] = counter6;

            auto counter7 = new sparta::Counter(&unit_stat_set_,
                                                "conflicted_evictions_" + std::to_string(i),
                                                "Number of times ATAGE Tables got conflict while allocating a new entry",
                                                sparta::Counter::COUNT_NORMAL);
            conflicted_evictions_[i] = counter7;
        }
    }

    sizeInBits_ = getSizeInBits_();
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------

std::vector<uint16_t> ATAGE::determineHistLengths_(uint32_t nTables, uint32_t gHistBits, uint32_t minHist,
                                                  std::vector<uint16_t> histLengths) const {
   // Check if histLenths is correctly sized
   sparta_assert((histLengths.size() == nTables+1) || (histLengths.empty()), "Invalid history lengths vector");

   // Determine history lengths for each ATAGE table if not sepcified
   if (histLengths.empty() && nTables != 0) {

       histLengths.resize(nTables+1, 0x0);
       histLengths[1] = minHist;
       histLengths[nTables] = gHistBits;
       for (uint32_t i = 2; i <= nTables; i++) {
           histLengths[i] = (int) (((double) minHist_ *
                                       pow ((double) (gHistBits) / (double) minHist,
                                       (double) (i - 1) / (double) ((nTables- 1)))) + 0.5);
       }
   }

   return histLengths;
}

void ATAGE::init_()
{
    if (initialized_) {
       return;
    }

    // Current method for periodically resetting the u counter bits only
    // works for 1 or 2 bits
    // Also make sure that it is not 0
    sparta_assert(tagTableUBits_ <= 2 && (tagTableUBits_ > 0),
                  "U counter bits must be > 0  and <= 2");

    uResetTimer_ = initialUResetTimerValue_;
    uResetTimerEnable_ = (uResetTimer_ == 0) ? false : true;

    sparta_assert(maxHist_ >= minHist_,
                  "History buffer size must be greater than 2x max history");

    useAltPredForNewlyAllocated_.resize(numUseAltOnNa_, 0);

    sparta_assert(tagTableTagWidths_.size() == (nTagTables_+1),
                  "Tag table Tag widths must be equal to nTagTables_+1");
    sparta_assert(logTagTableSizes_.size() == (nTagTables_+1),
                  "Log Tag table sizes must be equal to nTagTables_+1");

    // First entry is for the Bimodal table and it is untagged in this
    // implementation
    sparta_assert(tagTableTagWidths_[0] == 0,
                  "First entry in tagTableTagWidths_ must be 0, untagged");

    const uint64_t bimodalTableSize = 1ULL << logTagTableSizes_[0];
    btable_.assign(bimodalTableSize, TageEntry(tagTableIndexingGranularity_));
    
    // 0th table is bimodal
    g_table_.resize(nTagTables_+1);
    g_table_[0] = nullptr;
    for (uint32_t i = 1; i <= nTagTables_; ++i) {
        auto repl_policy = setReplPolicy_(tagTableReplPolicy_, i);
        g_table_[i] = std::make_unique<TageCacheFuncModel<TageEntry>>(
                        getContainer(),
                        (tagTableIndexingGranularity_ * tagTableAssociativity_[i] * (1 << logTagTableSizes_[i])) >> 10,
                        tagTableIndexingGranularity_,
                        *repl_policy,
                        "tage_table_" + std::to_string(i));
    }

    tagTableIndices_.resize(nTagTables_+1, 0x0);
    tableTags_.resize(nTagTables_+1, 0x0);

    initialized_ = true;
}

// ===================================================================
// Main interfaces
// ===================================================================
std::tuple<bool, TageBranchInfoPtr>
ATAGE::predict_dir(Addr pc, bool cond_branch, bool is_backward,
                   const bool actual_taken, const bool ght_pred) {
    if (!pred_enable_) {
        return {false, nullptr};
    }
    ILOG("Lookup branch: " << HEX16(pc) << " is_conditional: " << cond_branch);

    bool pred_taken = true;
    bool pred_update = true;

    // Members are initialized to 0, which is valid value.
    // Use it carefully
    const TageBranchInfoPtr& bp_info = sparta::allocate_sparta_shared_pointer<TageBranchInfo>(branch_info_allocator);

    bp_info->branchPC = pc;
    bp_info->actualTaken = actual_taken;
    bp_info->condBranch = cond_branch;
    bp_info->indirectBranch = false;

    // Checkpointing histories
    bp_info->pathHist = tageHistory_.pathHist;

    // Compute indices and tags for all TAGE tables using PC, GHR, and path history
    calculateIndicesAndTags_(bp_info, pc);
    
    if (cond_branch) {
        // This is a conditional branch — begin prediction process
        bp_info->predValid = true; // Mark the prediction as valid

        // Compute bimodal index for fallback prediction if bimodal is base predictor
        if (!use_ght_) {
            bp_info->bimodalIndex = bindex_(pc);
        }

        // Search for matching entries in TAGE tables
        findMatchingBanks_(bp_info);

        // Compute the actual TAGE prediction based on matching entries
        computeTagePrediction_(bp_info, ght_pred);

        pred_taken = bp_info->tagePred;
    }

    // Speculative update of the histories at the time of making prediction
    updateHistories_(pc, pred_taken);

    // Checkpointing GlobalHistory Iterator
    bp_info->gHistIter = tageHistory_.gHist.getLastIter();

    DLOG("Tage BI : " << *bp_info);
    ILOG("Lookup bb_start_pc: " << HEX16(pc) << " -> Prediction: " << pred_taken);

    return {pred_taken, bp_info};
}

std::tuple<Addr, TageBranchInfoPtr> ATAGE::predict_addr(Addr pc, bool indirect_branch, Addr btb_target_addr, bool ltage_prediction) {
    sparta_assert(false, "Indirect ATAGE target address prediction not yet implemented!");
    return {0x0, nullptr};
}

void ATAGE::update_tage(const InstPtr& branch_inst, bool flushed) {

    if (!pred_enable_) {
        return;
    }

    sparta_assert(branch_inst,"Can not update null inst");
    sparta_assert(branch_inst->getTageBranchInfo(),"Can not update null BranchInfo");

    const Addr& bb_start_pc = branch_inst->getBasicBlockStartPC();
    const bool& taken       = branch_inst->isTakenBranch();

    const bool& condBranch = branch_inst->isCondBranch();

    TageBranchInfoPtr tage_info  = branch_inst->getTageBranchInfo();

    if (flushed) {
        // This restores the global history and path history
        DLOG("[FLUSHED] Tage BI : " << *tage_info);
        restoreHistories_(tage_info);
    }
    else {
        DLOG("[UPDATE] Tage BI : " << *tage_info);
    }

    if (condBranch) {
        DLOG("Updating tables for bb_start_pc: " << HEX16(bb_start_pc) << "; taken = " << taken);

        updateTAGEStats_(taken, tage_info);
        condBranchTAGEUpdate_(bb_start_pc, taken, tage_info);
    }

    // This needs to be done after the update to the tables is done.
    // Update to the tables need the history that was seen by the instruction
    // that is causing  the flush.
    if (flushed) {
        // Updates the histories based on this outcome the global history and path history
        updateHistories_(bb_start_pc, taken);
    }
}

// -----------------------HELPER FUNCTIONS----------------------------
// -------------------------------------------------------------------
void ATAGE::GHR::rebuildAllCompressedHistories_() {

    if (HASH_IMPL == 1) {
        // Rebuild all of the compressed histories for tag tables
        for (int idx = 1; idx <= 2 * nTagTables_; ++idx) {
            tag_compressed_hist_[idx] = computeCompressedGlobalHistory_(tag_in_size_[idx], tag_out_size_[idx]);
        }
    }
}

void ATAGE::restoreHistories_(const TageBranchInfoPtr& bp_info) {

    sparta_assert(bp_info, "Why is the bp_info null??");

    DLOG("Restoring branch info: "   << HEX16(bp_info->branchPC)
                << "- PathHistory: " << HEX16(bp_info->pathHist));

    // RESTORE PATH HISTORIES
    tageHistory_.pathHist = bp_info->pathHist;

    // RESTORE GLOBAL HISTORIES
    DLOG("SPEC_GHR size = " << tageHistory_.gHist.getSpecHistSize());
    tageHistory_.gHist.flushAndRestore(bp_info->gHistIter);
    DLOG("SPEC_GHR size = " << tageHistory_.gHist.getSpecHistSize());
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void ATAGE::updateHistories_(Addr branch_pc, bool taken) {

    DLOG("Updating histories with branch: " << HEX16(branch_pc)
                                   << "; taken?: " << taken
                                << ", path Hist: " << HEX16(tageHistory_.pathHist));

    // UPDATE PATH HISTORIES
    // [TODO] : make the path history recording parametrizable
    //          with n bits per branch_pc
    uint8_t pathbit = ((branch_pc >> instShiftAmt_) & 0x1);
    tageHistory_.pathHist = (tageHistory_.pathHist << 1) | pathbit;
    //tageHistory_.pathHist.push(std::static_cast<bool>(pathbit));

    // UPDATE BRANCH HISTORIES
    tageHistory_.gHist.push(taken);
    DLOG("SPEC_GHR size = " << tageHistory_.gHist.getSpecHistSize());
}

// -------------------------------------------------------------------
// Get the Bi-Modal table index for a given branch PC
// -------------------------------------------------------------------
int ATAGE::bindex_(Addr pc_in) const {
    return ((pc_in >> instShiftAmt_) & ((1ULL << (logTagTableSizes_[0])) - 1));
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
uint64_t ATAGE::get_compressed_global_history_(uint32_t inSize, uint32_t outSize) const {
    /*
    Compress global history of last 'inSize' branches into 'outSize' by wrapping the history
    */
    uint64_t compressed_history = 0; // Stores final compressed history
    uint64_t temporary_history = 0; // Temorarily stores some bits of history
    uint64_t compressed_history_length = outSize;

    // uint32_t ghr_size = tageHistory_.gHist.size();
    // uint32_t start    = ghr_size - 1;

    uint32_t compressed_bit_num = 0;
    for (uint32_t i = 0; (i < inSize); ++i) {

        if (SPARTA_EXPECT_FALSE(compressed_bit_num == compressed_history_length))
        {
            compressed_history ^= temporary_history; // XOR current segment into the compressed history
            temporary_history = 0;
            compressed_bit_num = 0;
        }

        if (HASH == 2) {
            temporary_history |= tageHistory_.gHist.at(i) << (outSize - 1 - compressed_bit_num); // Build history bit vector
        }
        ++compressed_bit_num;
    }
    compressed_history ^= temporary_history;
    return compressed_history;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
uint64_t ATAGE::get_path_history_hash_(uint32_t bank) const {

    uint64_t A = tageHistory_.pathHist;
    uint32_t history_length = ((uint32_t)tagTableHistLengths_[bank] > pathHistBits_) ? pathHistBits_ : tagTableHistLengths_[bank];

    int A1, A2;

    A = A & ((1ULL << history_length) - 1);
    A1 = (A & ((1ULL << logTagTableSizes_[bank]) - 1));
    A2 = (A >> logTagTableSizes_[bank]);
    A2 = ((A2 << bank) & ((1ULL << logTagTableSizes_[bank]) - 1))
       + (A2 >> (logTagTableSizes_[bank] - bank));
    A = A1 ^ A2;
    A = ((A << bank) & ((1ULL << logTagTableSizes_[bank]) - 1))
      + (A >> (logTagTableSizes_[bank] - bank));
    return (A);
}

// -------------------------------------------------------------------
// gindex computes a full hash of pc, ghist and pathHist
// ------------------------------------------------------------------
uint32_t ATAGE::gindex_(Addr pc, int bank) const {

    const uint64_t shiftedPc = pc >> instShiftAmt_;
    uint64_t index;

    if (enable_global_hist_ && enable_path_hist_) {
        index = shiftedPc ^
                (shiftedPc >> ((int) abs(logTagTableSizes_[bank] - bank) + 1)) ^
                (tageHistory_.gHist.getTagCompressedGlobalHistory(bank));
    }
    else if (enable_path_hist_) {
        index = shiftedPc ^
                (shiftedPc >> ((int) abs(logTagTableSizes_[bank] - bank) + 1)) ^
                get_path_history_hash_(bank);
    }
    else if (enable_global_hist_) {
        index = shiftedPc ^
                (shiftedPc >> ((int) abs(logTagTableSizes_[bank] - bank) + 1)) ^
                (tageHistory_.gHist.getTagCompressedGlobalHistory(bank));
    }
    else {
        index = shiftedPc ^
                (shiftedPc >> ((int) abs(logTagTableSizes_[bank] - bank) + 1));
    }

    return (index & ((1ULL << (logTagTableSizes_[bank])) - 1));
}

// -------------------------------------------------------------------
// Tag computation
// -------------------------------------------------------------------
uint32_t ATAGE::gtag_(Addr pc, int bank) const {

    uint64_t tag;

    if (enable_global_hist_) {
        tag = (pc >> instShiftAmt_) ^
                (tageHistory_.gHist.getTagCompressedGlobalHistory(bank)) ^
                (tageHistory_.gHist.getTagCompressedGlobalHistory(bank + nTagTables_));
    }
    else {
        tag = (pc >> instShiftAmt_);
    }

    return (tag & ((1ULL << tagTableTagWidths_[bank]) - 1));
}

// -------------------------------------------------------------------
// Up-down saturating counter
// -------------------------------------------------------------------
// [TODO] : update the method for precomputed nbits argument?
template<typename T>
void ATAGE::ctrUpdate_(T& ctr, bool taken, uint32_t nbits) {

    sparta_assert((uint32_t)nbits <= sizeof(T) << 8,
                        "CTRUPdate: nbits exceeds number of bits in the template type, ctr");
    
    if (taken) {
        if (ctr < ((1 << (nbits - 1)) - 1)) {
            ctr++;
        }
    } else {
        if (ctr > -(1 << (nbits - 1))) {
            ctr--;
        }
    }
}

// -------------------------------------------------------------------
// Up-down unsigned saturating counter
// -------------------------------------------------------------------
void ATAGE::unsignedCtrUpdate_(uint8_t & ctr, bool up, unsigned nbits) {
    sparta_assert(nbits <= sizeof(uint8_t) << 3,
       "unsigned CTRUpdate: nbits exceeds number of bits in uint8_t");

    if (up) {
        if (ctr < ((1 << nbits) - 1)) {
            ctr++;
        }
    } else {
        if (ctr) {
            ctr--;
        }
    }
}

// -------------------------------------------------------------------
// Bimodal prediction
// -------------------------------------------------------------------
bool ATAGE::getBimodalPred_(uint32_t index) const {

    return (btable_[index].ctr >= 0) ? true : false;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void ATAGE::calculateIndicesAndTags_(const TageBranchInfoPtr& bi, Addr branch_pc) {

    // computes the table addresses and the partial tags
    for (uint32_t i = 1; i <= nTagTables_; i++) {
        tagTableIndices_[i] = gindex_(branch_pc, i);
        tableTags_[i] = gtag_(branch_pc, i);
    }

    // Store indices and tags in the branch info for later use during allocation
    bi->tableIndices.resize(nTagTables_+1, 0);
    bi->tableIndices = tagTableIndices_;

    bi->tableTags.resize(nTagTables_+1, 0);
    bi->tableTags = tableTags_;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
unsigned ATAGE::getUseAltIdx_(const TageBranchInfoPtr& bp_info) {

    // There are numHistoryTables number of counter on the ATAGE implementation
    if (numUseAltOnNa_ == nTagTables_) {
        return bp_info->hitBank;
    }
    else if (numUseAltOnNa_ == 1) {
        return 0;
    }
    else {
        sparta_assert(false, "What index do I return for the useAltOnNA??");
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void ATAGE::resetUCtr_(uint8_t & u) {
    u >>= 1;
}

void ATAGE::handleUsefulBitReset_() {
    // [STEP 4] : decrement the counter for reset
    --uResetTimer_;

    // [STEP 5] : reset the useful bits and counter if counter has reached 0
    if (uResetTimer_ == 0) {
        for (uint32_t i = 1; i <= nTagTables_; ++i) {
            
            uint32_t num_sets = (1ULL << logTagTableSizes_[i]);
            for (uint32_t j = 0; j < num_sets; ++j) {
            
                auto& set = g_table_[i]->getCacheSetAtIndex(j);
                for (auto &way : set) {
                    resetUCtr_(way.u);
                }
            }
        }
        
        ++uResetCounter_;
        uResetTimer_ = initialUResetTimerValue_;
    }
}

void ATAGE::allocateEntries_(Addr branch_pc, bool taken, const TageBranchInfoPtr& bi) {
    
    ++allocateOnFlush_;

    // [STEP 3.1] : is there some "unuseful" or less "useful" entry to allocate?
    uint32_t min = (1 << tagTableUBits_) - 1;
    uint32_t min_table = (bi->hitBank == nTagTables_) ? nTagTables_ : (bi->hitBank + 1);
    uint32_t min_table_index = 0;
    uint32_t min_way_id = 0;

    // Find the minimum u bit
    for (uint32_t i = nTagTables_; i >= bi->hitBank + 1; --i) {
    
        const uint32_t table_index = bi->tableIndices[i];
        auto set = g_table_[i]->getCacheSetAtIndex(table_index);

        for (const auto& way : set) {
            if (way.u <= min) {
                min = way.u;
                min_table = i;
                min_table_index = table_index;
                min_way_id = way.getWay();
            }
        }
    }

    // Grab the minimum ubit entry
    auto tage_entry = g_table_[min_table]->getItemAtIndexWay(min_table_index, min_way_id);
    
    // [STEP 3.2] : No entry available, forces one to be available
    if (min > 0 && min_table <= nTagTables_) {
        ++(*conflicted_evictions_[min_table]);
        tage_entry.u = 0;
        DLOG("Forced g_table_[" << min_table << "][" << min_table_index << "].u=0 for bb_start_pc: " << HEX16(branch_pc));
    }
    g_table_[min_table]->touchLRU(tage_entry);
    DLOG ("Found min_table:" << min_table << " index:" << min_table_index << " way:" << min_way_id);
    
    // [STEP 3.3] : Allocate entries
    unsigned numAllocated = 0;
    for (uint32_t i = min_table; i <= nTagTables_; ++i) {
        
        // We generate the index again for the case of more than one allocations
        const uint32_t table_index = bi->tableIndices[i];
        auto& tage_entry = g_table_[i]->getLineForReplacementWithInvalidCheck(table_index);
        
        if (tage_entry.u == 0) {
            DLOG("Allocating entry in g_table_[" << i << "][" << table_index << "] for bb_start_pc: " << HEX16(branch_pc));

            tage_entry.tage_tag = bi->tableTags[i];
            tage_entry.ctr = taken ? 0 : -1;
            
            // Allocate a new tage entry
            if (!tage_entry.isValid()) {
                g_table_[i]->allocateWithMRUUpdate(tage_entry);
            }
            // Update the existing tage entry
            else {
                g_table_[i]->touchMRU(tage_entry);
            }
        }

        ++numAllocated;
        ++(*flushAlloc_[i]);
        if (numAllocated == maxNumAlloc_) break;
    }
}

void ATAGE::updateBimodalPrediction_(Addr branch_pc, bool taken, const TageBranchInfoPtr& bi, bool& alloc) {

    // Update the bimodal table counter
    DLOG("Updating btable_[" << bi->bimodalIndex << "] for basic block PC = " << HEX16(branch_pc));
    ctrUpdate_(btable_[bi->bimodalIndex].ctr, taken, bimodalTableCounterBits_);
}

void ATAGE::updateTaggedTablePrediction_(Addr branch_pc, bool taken, const TageBranchInfoPtr& bi, bool& alloc) {
    
    if (!hasMatchingEntry_(bi->hitBank, bi->hitBankIndex, bi->hitBankTag)) {
        DLOG("No Matching longestMatchEntry");
    }
    else {
        // Retrieve the entry from the hit bank with the longest matching history
        const auto& longestMatchEntry = getMatchingEntry_(bi->hitBank, bi->hitBankIndex, bi->hitBankTag);
    
        // [STEP 1.1] : Change the alloc based on current counter value
        alloc = ((longestMatchEntry->ctr >= 0) == taken) ? false : (alloc && true);

        // [STEP 1.2] : Update the longest match tage table counter
        ctrUpdate_(longestMatchEntry->ctr, taken, tagTableCounterBits_);
        DLOG("Updating ctr at g_table_[" << bi->hitBank << "][" << bi->hitBankIndex 
            << "] for basic block PC = " << HEX16(branch_pc) << " to " << longestMatchEntry->ctr);

        // [STEP 1.3] : This means that the bi->tagePred came from the bi->altPred
        //              update use_alt_on_new_allocation counter
        if (bi->pseudoNewAlloc) {
            if (bi->longestMatchPred == taken) { alloc = false; }

            if (bi->longestMatchPred != bi->altPred) {
                ctrUpdate_(useAltPredForNewlyAllocated_[getUseAltIdx_(bi)], (bi->altPred == taken), useAltOnNaBits_);
            }
        }

        // [STEP 1.4] : if the provider entry is not certified to be useful also update the alternate prediction
        // [TODO] : Should we update alt when alt pred is used and predicted correctly?
        if (longestMatchEntry->u == 0) {
            if (bi->altBank > 0 && hasMatchingEntry_(bi->altBank, bi->altBankIndex, bi->altBankTag)) {
                
                const auto& altMatchEntry = getMatchingEntry_(bi->altBank, bi->altBankIndex, bi->altBankTag);
                
                ctrUpdate_(altMatchEntry->ctr, taken, tagTableCounterBits_);
                DLOG("Updating ALT g_table_[" << bi->altBank << "][" << bi->altBankIndex 
                    << "] for basic block PC = " << HEX16(branch_pc) << " to " << altMatchEntry->ctr);
            } 

            if (bi->altBank == 0 && !use_ght_) {
                ctrUpdate_(btable_[bi->bimodalIndex].ctr, taken, bimodalTableCounterBits_);
                DLOG("Updating ALT btable_[" << bi->bimodalIndex 
                    << "] for basic block PC = " << HEX16(branch_pc) << " to " << btable_[bi->bimodalIndex].ctr);
            }
        }

        // [STEP 1.5] : update the useful bit/counter
        if (bi->longestMatchPred != bi->altPred) {

            bool increment = (bi->longestMatchPred == taken);
            unsignedCtrUpdate_(longestMatchEntry->u, increment, tagTableUBits_);
            DLOG("Updating u_bit for g_table_[" << bi->hitBank << "][" << bi->hitBankIndex 
                << "] for basic block PC = " << HEX16(branch_pc) << " to " << static_cast<uint32_t>(longestMatchEntry->u));

            if (increment) { g_table_[bi->hitBank]->touchMRU(*longestMatchEntry); }
            else { g_table_[bi->hitBank]->touchLRU(*longestMatchEntry); }
        }
    }
}

bool ATAGE::isTaggedTableProvider_(const TageBranchInfoPtr& bi) {
    return ((bi->hitBank > 0) && (bi->hitBank <= nTagTables_));
}

bool ATAGE::shouldAllocateEntry_(const TageBranchInfoPtr& bi, bool taken) {
    
    // If the preferred pred is a mispred, return true
    return (tage_pred != taken);
}

void ATAGE::condBranchTAGEUpdate_(Addr branch_pc, bool taken, const TageBranchInfoPtr& bi) {
    // Determine whether a new entry should be allocated.
    // Allocation happens only if the prediction was incorrect 
    // and the hit bank is within range.
    bool alloc = shouldAllocateEntry_(bi, taken);
    DLOG("Should allocate new entry?" << alloc);

    // Check if the prediction came from a tagged table (not the base table).
    if (isTaggedTableProvider_(bi)) {
        // Update counters and usefulness bits for tagged table entries.
        updateTaggedTablePrediction_(branch_pc, taken, bi, alloc);
    } else {
        // Update bimodal table counter and validate allocation conditions.
        if (!use_ght_) {
            updateBimodalPrediction_(branch_pc, taken, bi, alloc);
        }
    }

    // If allocation is needed, find and allocate entries in the tagged tables.
    if (alloc) {
        allocateEntries_(branch_pc, taken, bi);
    }

    // Periodically reset the usefulness counters across all tagged tables.
    if (uResetTimerEnable_) {
        handleUsefulBitReset_();
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void ATAGE::indirectBranchUpdate_(Addr branch_pc, Addr target_addr, const TageBranchInfoPtr& bi) {
    sparta_assert(false, "Indirect ATAGE target address prediction not yet implemented!");
}

void ATAGE::updateTAGEStats_(bool actual_taken, const TageBranchInfoPtr& bp_info)
{
    // -------------------------------------------------------------------------
    // Compute correctness of the different prediction signals.
    //   - tagePred:     TAGE's own prediction.
    // -------------------------------------------------------------------------
    const bool tage_pred_correct = (bp_info->tagePred   == actual_taken);

    // -------------------------------------------------------------------------
    // Provider selection stats (who provided the prediction)
    // -------------------------------------------------------------------------
    switch (bp_info->provider) {
        case BASE_ONLY:             ++baseProvider_;              break;
        case TAGE_LONGEST_MATCH:    ++tageLongestMatchProvider_;  break;
        case BASE_ALT_MATCH:        ++baseAltMatchProvider_;           break;
        case TAGE_ALT_MATCH:        ++tageAltMatchProvider_;           break;
    }

    // -------------------------------------------------------------------------
    // Correct vs wrong prediction paths (based on TAGE prediction)
    // -------------------------------------------------------------------------
    
    if (tage_pred_correct) {
        // =====================
        // TAGE Correct prediction
        // =====================

        // Attribute correctness to the provider source.
        switch (bp_info->provider) {
            case BASE_ONLY:
                ++baseProviderCorrect_;
                break;

            case TAGE_LONGEST_MATCH:
                ++tageLongestMatchProviderCorrect_;
                ++(*longestMatchProviderCorrect_[bp_info->hitBank]);
                break;

            case BASE_ALT_MATCH:
                ++baseAltMatchProviderCorrect_;
                break;

            case TAGE_ALT_MATCH:
                ++tageAltMatchProviderCorrect_;
                ++(*altMatchProviderCorrect_[bp_info->altBank]);
                break;
        }
    } else {
        // =====================
        // TAGE Wrong prediction
        // =====================

        // Attribute the miss to the responsible provider.
        switch (bp_info->provider) {
            case BASE_ONLY:
                ++baseProviderWrong_;
                break;

            case TAGE_LONGEST_MATCH:
                ++tageLongestMatchProviderWrong_;
                ++(*longestMatchProviderWrong_[bp_info->hitBank]);

                // If the alternate prediction would have been correct, record it.
                if (bp_info->altPred == actual_taken) {
                    ++altMatchProviderWouldHaveHit_;
                }
                break;

            case BASE_ALT_MATCH:
                ++baseAltMatchProviderWrong_;

                // If longest-match prediction would have been correct, record it.
                if (bp_info->longestMatchPred == actual_taken) {
                    ++longestMatchProviderWouldHaveHit_;
                }
                break;

            case TAGE_ALT_MATCH:
                ++tageAltMatchProviderWrong_;
                ++(*altMatchProviderWrong_[bp_info->altBank]);

                // If longest-match prediction would have been correct, record it.
                if (bp_info->longestMatchPred == actual_taken) {
                    ++longestMatchProviderWouldHaveHit_;
                }
                break;
        }
    }

    // -------------------------------------------------------------------------
    // Provider distribution per bank (counts regardless of correctness)
    // -------------------------------------------------------------------------
    switch (bp_info->provider) {
        case TAGE_LONGEST_MATCH:
            ++(*longestMatchProvider_[bp_info->hitBank]);
            break;

        case TAGE_ALT_MATCH:
            ++(*altMatchProvider_[bp_info->altBank]);
            break;

        default:
            // BASE_ONLY and BASE_ALT_MATCH have no per-bank provider histogram here.
            break;
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
uint64_t ATAGE::getPathHist_() const {
    return tageHistory_.pathHist;
}

// STEP A-1
// Push the speculative history onto the list<bool>
void ATAGE::GHR::pushSpecHist_(bool taken) {

    if (speculativeHistory.size() == speculative_hist_length) {
        speculativeHistory.pop_front();
    }
    speculativeHistory.push_back(taken);
}

// STEP A
// Push at the back the CB, automatically removes the
// object from the front on overflow
void ATAGE::GHR::push(bool taken) {

    // STEP A-1
    // Push the speculative history onto the list<bool>
    pushSpecHist_(taken);

    if (HASH_IMPL == 1) {
        // Update compressed histories BEFORE we shift the ghr
        bool in_bit = taken;
        for (int idx = 1; idx <= 2 * nTagTables_; ++idx) {
            // If the in size is 0 then the compressed hist is always 0
            if (SPARTA_EXPECT_FALSE(tag_in_size_[idx] == 0)) {
                continue;
            }

            // Read the initial compressed history
            auto temp_hist = tag_compressed_hist_[idx];

            // Read the out bit that we need to remove from the compressed history
            const bool out_bit = ghr[tag_in_size_[idx] - 1];

            const int lsb_pos = 0;
            const int msb_pos = tag_out_size_[idx] - 1;

            if (HASH == 2) {
                // Rotate compressed history right
                const bool lsb_bit = temp_hist[lsb_pos];
                temp_hist >>= 1;
                temp_hist[msb_pos] = lsb_bit;
            }

            // XOR out the out bit
            temp_hist[tag_out_pos_[idx]] = temp_hist[tag_out_pos_[idx]] ^ out_bit;

            // XOR in the in bit
            temp_hist[tag_in_pos_[idx]] = temp_hist[tag_in_pos_[idx]] ^ in_bit;

            // Write the updated compressed history
            tag_compressed_hist_[idx] = temp_hist;
        }
    }

    // STEP A-2
    // shift the ghr and add the taken bit
    ghr <<= 1;
    ghr.set(0, taken);
}

// STEP B
// Snapshot the iter at which the oldest object in the list is pointing to
std::list<bool>::iterator ATAGE::GHR::getLastIter() {
    return std::prev(speculativeHistory.end());
}

// STEP C - FLUSH ROUTINE
// Removes all the younger content, pushes the result onto dequeue
void ATAGE::GHR::flushAndRestore(std::list<bool>::iterator iter) {

    // STEP C-1:
    // Calculate the number of speculations younger than this iter
    // and shift the ghr right by that amount
    ghr >>= std::distance(iter, speculativeHistory.end());

    // STEP C-2:
    // Erase the number of speculations younger than this iter
    speculativeHistory.erase(iter, speculativeHistory.end());

    // Rebuild all of the compressed histories
    rebuildAllCompressedHistories_();
}

bool ATAGE::GHR::at(uint32_t index) const {
    return ghr[index];
}

uint64_t ATAGE::GHR::computeCompressedGlobalHistory_(uint32_t inSize, uint32_t outSize) const {
    /*
    Compress global history of last 'inSize' branches into 'outSize' by wrapping the history
    */
    uint64_t compressed_history = 0; // Stores final compressed history
    uint64_t temporary_history = 0; // Temorarily stores some bits of history
    uint64_t compressed_history_length = outSize;

    uint32_t compressed_bit_num = 0;
    for (uint32_t i = 0; (i < inSize); ++i) {

        if (SPARTA_EXPECT_FALSE(compressed_bit_num == compressed_history_length))
        {
            compressed_history ^= temporary_history; // XOR current segment into the compressed history
            temporary_history = 0;
            compressed_bit_num = 0;
        }

        if (HASH == 2) {
            temporary_history |= ghr[i] << (outSize - 1 - compressed_bit_num); // Build history bit vector
        }
        ++compressed_bit_num;
    }
    compressed_history ^= temporary_history;
    return compressed_history;
}

bool ATAGE::hasMatchingEntry_(uint32_t bank, uint32_t index, uint32_t tag) const {
    const auto set = g_table_[bank]->getCacheSetAtIndex(index);
    for (const auto& way : set) {
        if (way.isValid() && way.tage_tag == tag) {
            return true;
        }
    }
    return false;
}

ATAGE::TageEntry* ATAGE::getMatchingEntry_(uint32_t bank, uint32_t index, uint32_t tag) const {
    
    TageEntry* match = nullptr;
    
    auto& set = g_table_[bank]->getCacheSetAtIndex(index);
    for (auto &way : set) {
        if (way.isValid() && way.tage_tag == tag) {
            return &set.getItemAtWay(way.getWay());
        }
    }
    sparta_assert(match, "We should have found the match!");
    return match;
}

void ATAGE::findMatchingBanks_(const TageBranchInfoPtr& bi) {

    // [STEP 1] : Look for the bank with longest matching history
    for (int i = nTagTables_; i > 0; --i) {
        if (hasMatchingEntry_(i, tagTableIndices_[i], tableTags_[i])) {
            bi->hitBank = i;
            bi->hitBankIndex = tagTableIndices_[i];
            bi->hitBankTag = tableTags_[i];
            break;
        }
    }

    // [STEP 2] : Look for the alternate bank
    for (int i = bi->hitBank - 1; i > 0; --i) {
        if (hasMatchingEntry_(i, tagTableIndices_[i], tableTags_[i])) {
            bi->altBank = i;
            bi->altBankIndex = tagTableIndices_[i];
            bi->altBankTag = tableTags_[i];
            break;
        }
    }
}

void ATAGE::computeTagePrediction_(const TageBranchInfoPtr& bi, const bool& ght_pred) {
    
    // Case 1: No hit bank — fall back to base prediction
    if (bi->hitBank <= 0) {
        // Otherwise, fall back to base prediction
        if (use_ght_) { bi->altPred = ght_pred; }
        else          { bi->altPred = getBimodalPred_(bi->bimodalIndex); }
        
        bi->tagePred = bi->altPred;                      // Use base prediction as final prediction
        bi->provider = BASE_ONLY;                        // Indicate source of prediction
        return;
    }

    // Case 2: Hit bank exists — use TAGE prediction logic
    // Retrieve the entry from the hit bank with the longest matching history
    const auto& longestMatchEntry = getMatchingEntry_(bi->hitBank, bi->hitBankIndex, bi->hitBankTag);

    // Determine alternate prediction
    if (bi->altBank > 0 && hasMatchingEntry_(bi->altBank, bi->altBankIndex, bi->altBankTag)) {
        // If an alternate bank exists, get its prediction
        const auto& altMatchEntry = getMatchingEntry_(bi->altBank, bi->altBankIndex, bi->altBankTag);
        bi->altPred = (altMatchEntry->ctr >= 0); // Positive counter means taken
        bi->altBankCtr = altMatchEntry->ctr;
    } else {
        // Otherwise, fall back to base prediction
        if (use_ght_) { bi->altPred = ght_pred; }
        else          { bi->altPred = getBimodalPred_(bi->bimodalIndex); }
    }

    // Compute prediction from the longest matching entry
    bi->longestMatchPred = (longestMatchEntry->ctr >= 0); // Positive counter means taken
    bi->hitBankCtr = longestMatchEntry->ctr;

    // Determine if the entry is newly allocated (low confidence)
    bi->pseudoNewAlloc = std::abs(2 * longestMatchEntry->ctr + 1) <= 1;

    // Decide whether to use the longest match or alternate prediction
    bool useLongestMatch = (useAltPredForNewlyAllocated_[getUseAltIdx_(bi)] < 0) || !bi->pseudoNewAlloc;

    if (useLongestMatch) {
        // Use longest match prediction
        bi->tagePred = bi->longestMatchPred;
        bi->provider = TAGE_LONGEST_MATCH;
        bi->ctr = longestMatchEntry->ctr;
    } else if (bi->altBank > 0 && hasMatchingEntry_(bi->altBank, bi->altBankIndex, bi->altBankTag)) {
        // Use alternate match prediction from alt bank
        const auto& altMatchEntry = getMatchingEntry_(bi->altBank, bi->altBankIndex, bi->altBankTag);
        bi->tagePred = bi->altPred;
        bi->provider = TAGE_ALT_MATCH;
        bi->ctr = altMatchEntry->ctr;
    } else {
        // Use alternate prediction from base table
        bi->tagePred = bi->altPred;
        bi->provider = BASE_ALT_MATCH;
    }
}

void ATAGE::logBranch_(const bool pred_taken, const TageBranchInfoPtr &bp_info)
{
    static const std::vector<std::string> PROVIDER_ABBREV =
        { "BI", "THIT", "BALT", "TALT"};

    std::stringstream msg;
    msg << "TAGELOG " << HEX8(br_log_pc_)
        << " pt=" << (pred_taken ? 'T' : '-')
        //<< " pr=" << bp_info->provider
        << " pr=" << (bp_info->provider < PROVIDER_ABBREV.size()
                      ? PROVIDER_ABBREV[bp_info->provider]
                      : "UNK")
        << " hit_tbl=" << bp_info->hitBank
        << " alt_tbl=" << bp_info->altBank;

    const TageEntry &te = g_table_[bp_info->hitBank]->getItemAtIndexWay(bp_info->hitBankIndex, bp_info->hitBankWay);
    if (te.tage_tag == tableTags_[bp_info->hitBank]) {
        msg << " tbl[" << bp_info->hitBank << "]=(" << (te.ctr >= 0 ? 'T' : '-')
            << ",ctr=" << te.ctr
            << ",idx=" << bp_info->hitBankIndex
            << ",way=" << bp_info->hitBankWay
            << ")";
    }

    const TageEntry &alt_te = g_table_[bp_info->altBank]->getItemAtIndexWay(bp_info->altBankIndex, bp_info->altBankWay);
    if (alt_te.tage_tag == tableTags_[bp_info->altBank]) {
        msg << " tbl[" << bp_info->altBank << "]=(" << (alt_te.ctr >= 0 ? 'T' : '-')
            << ",ctr=" << alt_te.ctr
            << ",idx=" << bp_info->altBankIndex
            << ",way=" << bp_info->altBankWay
            << ")";
    }

    TAGELOG(msg.str());
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
uint64_t ATAGE::getSizeInBits_() {
    uint64_t bits = 0;
    for (uint32_t i = 1; i <= nTagTables_; i++) {
        bits += (1 << logTagTableSizes_[i]) * (tagTableAssociativity_[i]) *
            (tagTableCounterBits_ + tagTableUBits_ + tagTableTagWidths_[i]);
    }

    if (!use_ght_) {
        bits += (1 << logTagTableSizes_[0])*bimodalTableCounterBits_;
    }
    return bits;
}

}
