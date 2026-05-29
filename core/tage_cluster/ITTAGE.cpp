#include "ITTAGE.hpp"
#include "CoreUtils.hpp"

namespace olympia
{

constexpr char ITTAGE::name[];

ITTAGE::ITTAGE(sparta::TreeNode* node, const ITTAGEParameterSet* p)
    : sparta::Unit(node),
      initialized_(false),

      // Yaml parameters
      pred_enable_               (p->pred_enable),
      nTagTables_                (p->nTagTables),
      bimodalTableCounterBits_   (p->bimodalTableCounterBits),
      tagTableCounterBits_       (p->tagTableCounterBits),
      tagTableUBits_             (p->tagTableUBits),
      gHistBits_                 (p->maxHist),
      enable_global_hist_        (p->enable_global_hist),
      minHist_                   (p->minHist),
      enable_path_hist_          (p->enable_path_hist),
      pathHistBits_              (p->pathHistBits),
      tagTableTagWidths_         (p->tagTableTagWidths),
      logTagTableSizes_          (p->logTagTableSizes),
      tagTableHistLengths_       (determineHistLengths_(nTagTables_, gHistBits_, minHist_, p->tagTableHistLengths)),
      initialUResetTimerValue_   (p->initialUResetTimerValue),
      numUseAltOnNa_             (p->numUseAltOnNa),
      useAltOnNaBits_            (p->useAltOnNaBits),
      maxNumAlloc_               (p->maxNumAlloc),
      use_btb_                   (p->use_btb),
      instShiftAmt_              (p->instShiftAmt),

      // Stats
      bimodalAltMatchProviderCorrect_(&unit_stat_set_,
         "bimodalAltMatchProviderCorrect",
         "Number of times ITTAGE Alt Match is the bimodal and it is the "
         "provider and the prediction is correct",
         sparta::Counter::COUNT_NORMAL),

      bimodalProviderCorrect_(&unit_stat_set_,
         "bimodalProviderCorrect",
         "Number of times there are no hits on the ITTAGE tables and the "
         "bimodal prediction is correct",
         sparta::Counter::COUNT_NORMAL),
      
      bimodalProvider_(&unit_stat_set_,
         "bimodalProvider",
         "Number of times Bimodal was provider",
         sparta::Counter::COUNT_NORMAL),
      
      bimodalAltProvider_(&unit_stat_set_,
         "bimodalAltProvider",
         "Number of times Bimodal as Alt was provider",
         sparta::Counter::COUNT_NORMAL),
      
      tageLongestMatchProvider_(&unit_stat_set_,
         "tageLongestMatchProvider",
         "Number of times ITTAGE longest match was provider",
         sparta::Counter::COUNT_NORMAL),
      
      tageAltProvider_(&unit_stat_set_,
         "tageAltProvider",
         "Number of times ITTAGE as Alt was provider",
         sparta::Counter::COUNT_NORMAL),
      
      tageAltProviderCorrect_(&unit_stat_set_,
         "tageAltProviderCorrect",
         "Number of times ITTAGE as Alt was provider",
         sparta::Counter::COUNT_NORMAL),
      
      tageAltProviderWrong_(&unit_stat_set_,
         "tageAltProviderWrong",
         "Number of times ITTAGE as Alt was provider",
         sparta::Counter::COUNT_NORMAL),

      bimodalAltMatchProviderWrong_(&unit_stat_set_,
         "bimodalAltMatchProviderWrong",
         "Number of times ITTAGE Alt Match is the bimodal and it is the "
         "provider and the prediction is wrong",
         sparta::Counter::COUNT_NORMAL),

      bimodalProviderWrong_(&unit_stat_set_,
         "bimodalProviderWrong",
         "Number of times there are no hits on the ITTAGE tables and the "
         "bimodal prediction is wrong",
         sparta::Counter::COUNT_NORMAL),
      
      altMatchProviderWouldHaveHit_(&unit_stat_set_,
         "altMatchProviderWouldHaveHit",
         "Number of times ITTAGE Longest Match is the provider, the "
         "prediction is wrong and Alt Match prediction was correct",
         sparta::Counter::COUNT_NORMAL),

      longestMatchProviderWouldHaveHit_(&unit_stat_set_,
         "longestMatchProviderWouldHaveHit",
         "Number of times ITTAGE Alt Match is the provider, the "
         "prediction is wrong and Longest Match prediction was correct",
         sparta::Counter::COUNT_NORMAL),
      
      tageWouldHaveHit_(&unit_stat_set_,
         "tageWouldHaveHit",
         "Number of times SC is the provider, the "
         "prediction is wrong and ITTAGE prediction was correct",
         sparta::Counter::COUNT_NORMAL),
      
      allocateOnFlush_(&unit_stat_set_,
         "allocateOnFlush",
         "Number of times ITTAGE Tables got allcoated",
         sparta::Counter::COUNT_NORMAL),
      
      uResetCounter_(&unit_stat_set_,
         "uResetCounter",
         "Number of times ITTAGE Tables ubit got reset",
         sparta::Counter::COUNT_NORMAL),
      
      sizeInBits_(&unit_stat_set_,
         "sizeInBits",
         "Tota bits of ITTAGE predictor",
         sparta::Counter::COUNT_LATEST),

      // Histories for Tage
      tageHistory_(gHistBits_, coreutils::getMaxInflightPreds(node) + coreutils::getMaxInflightBranches(node),
                   nTagTables_, logTagTableSizes_, tagTableHistLengths_) {
 
    if(pred_enable_) {
        
        // Initialize the ITTAGE module
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
                                                "Number of times ITTAGE Tables got allcoated",
                                                sparta::Counter::COUNT_NORMAL);
            flushAlloc_[i] = counter;

            auto counter1 = new sparta::Counter(&unit_stat_set_,
                                                "longestMatchProvider_" + std::to_string(i),
                                                "Number of times ITTAGE Bank matched the longest",
                                                sparta::Counter::COUNT_NORMAL);
            longestMatchProvider_[i] = counter1;

            auto counter2 = new sparta::Counter(&unit_stat_set_,
                                                "altMatchProvider_" + std::to_string(i),
                                                "Number of times ITTAGE Tables matched the atlernate",
                                                sparta::Counter::COUNT_NORMAL);
            altMatchProvider_[i] = counter2;

            auto counter3 = new sparta::Counter(&unit_stat_set_,
                                                "altMatchProviderCorrect_" + std::to_string(i),
                                                "Number of times ITTAGE Alt Match is the provider and the "
                                                "prediction is correct",
                                                sparta::Counter::COUNT_NORMAL);

            altMatchProviderCorrect_[i] = counter3;

            auto counter4 = new sparta::Counter(&unit_stat_set_,
                                                "altMatchProviderWrong_" + std::to_string(i),
                                                "Number of times ITTAGE Alt Match is the provider and the "
                                                "prediction is wrong",
                                                sparta::Counter::COUNT_NORMAL);

            altMatchProviderWrong_[i] = counter4;

            auto counter5 = new sparta::Counter(&unit_stat_set_,
                                                "longestMatchProviderCorrect_" + std::to_string(i),
                                                "Number of times ITTAGE Longest Match is the provider and the "
                                                "prediction is correct",
                                                sparta::Counter::COUNT_NORMAL);

            longestMatchProviderCorrect_[i] = counter5;

            auto counter6 = new sparta::Counter(&unit_stat_set_,
                                                "longestMatchProviderWrong_" + std::to_string(i),
                                                "Number of times ITTAGE Longest Match is the provider and the "
                                                "prediction is wrong",
                                                sparta::Counter::COUNT_NORMAL);

            longestMatchProviderWrong_[i] = counter6;

            auto counter7 = new sparta::Counter(&unit_stat_set_,
                                                "conflicted_evictions_" + std::to_string(i),
                                                "Number of times ITTAGE Tables got conflict while allocating a new entry",
                                                sparta::Counter::COUNT_NORMAL);
            conflicted_evictions_[i] = counter7;
        }
    }

    sizeInBits_ = getSizeInBits_();
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
std::vector<uint16_t> ITTAGE::determineHistLengths_(uint32_t nTables, uint32_t gHistBits, uint32_t minHist,
                                                  std::vector<uint16_t> histLengths) const {
   // Check if histLenths is correctly sized
   sparta_assert((histLengths.size() == nTables+1) || (histLengths.empty()), "Invalid history lengths vector");

   // Determine history lengths for each ITTAGE table if not sepcified
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

void ITTAGE::init_()
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

    sparta_assert(gHistBits_ >= minHist_,
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
    btable_.resize(bimodalTableSize);

    g_table_ = new TageEntry*[nTagTables_ + 1];
    for (uint32_t i = 1; i <= nTagTables_; ++i) {
        g_table_[i] = new TageEntry[ 1 << (logTagTableSizes_[i]) ];
    }

    tagTableIndices_.resize(nTagTables_+1, 0x0);
    tableTags_.resize(nTagTables_+1, 0x0);

    initialized_ = true;
}

// ===================================================================
// Main interfaces
// ===================================================================

std::tuple<Addr, TageBranchInfoPtr> ITTAGE::predict_addr(Addr pc, bool indirect_branch, Addr btb_target_addr, bool ltage_prediction) {
    
    if (!pred_enable_) {
        return {btb_target_addr, nullptr};
    }

    ILOG("Lookup branch: " << HEX16(pc) << " is_indirect: " << indirect_branch);

    Addr pred_addr = btb_target_addr;
    bool pred_taken;

    // Members are initialized to 0, which is valid value.
    const TageBranchInfoPtr& bp_info = sparta::allocate_sparta_shared_pointer<TageBranchInfo>(branch_info_allocator);

    bp_info->branchPC   = pc;

    // Checkpointing Path Histories
    bp_info->pathHist  = tageHistory_.pathHist;

    if (indirect_branch) {

        pred_taken = true;

        // This is infact an indirect branch
        bp_info->indirectBranch = true;
        bp_info->condBranch     = false;

        // Set prediction as Valid
        bp_info->predValid = true;

        // Calculates the indices in tags for this BB start PC,
        // Based on GHR and path history for all the tables
        // Populate the tagTableIndices_[] and tableTags_[] members
        calculateIndicesAndTags_(pc);

        bp_info->bimodalIndex = bindex_(pc);

        findIndirectHitAndAltBanks_(bp_info);
        computeIndirectPrediction_(bp_info, pred_addr);

        pred_addr = (bp_info->tagePredAddr);
    }
    else {
        pred_taken = ltage_prediction;
    }

    // Speculative update of the histories at the time of making prediction
    updateHistories_(pc, pred_taken);

    // Checkpointing GlobalHistory Iterator
    bp_info->gHistIter = tageHistory_.gHist.getLastIter();

    DLOG("Tage BI : " << *bp_info);
    ILOG("Lookup bb_start_pc: " << HEX16(pc) << " -> Prediction Addr: " << HEX16(pred_addr));
    return {pred_addr, bp_info};
}

void ITTAGE::update_ittage(const InstPtr& branch_inst, bool flushed) {

    if (!pred_enable_) {
        return;
    }

    sparta_assert(branch_inst,"Can not update null inst");
    sparta_assert(branch_inst->getITTageBranchInfo(),"Can not update null BranchInfo");

    const Addr& bb_start_pc = branch_inst->getBasicBlockStartPC();
    // const Addr& inst_pc     = branch_inst->getPC();
    const bool& taken       = branch_inst->isTakenBranch();
    const Addr& target_addr = branch_inst->getTargetVAddr();

    const bool& indirectBranch  = branch_inst->isIndirect() &&
                                  (branch_inst->isUncondJump() || branch_inst->isCall());

    TageBranchInfoPtr bp_info = branch_inst->getITTageBranchInfo();

    if (flushed) {
        // This restores the global history and path history
        if (indirectBranch && bp_info->hitBank > 0) {
            DLOG("[FLUSHED-INDIRECT] Tage BI : " << *bp_info);
        }
        else {
            DLOG("[FLUSHED] Tage BI : " << *bp_info);
        } 

        restoreHistories_(bp_info);
    }
    else {
        DLOG("[UPDATE] Tage BI : " << *bp_info);
    }

    // Update the tables
    if (indirectBranch) {
        DLOG("Updating tables for bb_start_pc: " << HEX16(bb_start_pc) << "; target addr = " << HEX16(target_addr));

        updateITTAGEStats_(target_addr, bp_info);
        indirectBranchUpdate_(bb_start_pc, target_addr, bp_info);
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
void ITTAGE::restoreHistories_(const TageBranchInfoPtr& bp_info) {

    sparta_assert(bp_info, "Why is the bp_info null??");

    DLOG("Restoring branch info: "   << HEX16(bp_info->branchPC)
                << "- PathHistory: " << HEX16(bp_info->pathHist));
                // << "- gHist Index: " << bp_info->gHistIter);

    // RESTORE PATH HISTORIES
    tageHistory_.pathHist = bp_info->pathHist;

    // RESTORE GLOBAL HISTORIES
    DLOG("SPEC_GHR size = " << tageHistory_.gHist.getSpecHistSize());
    tageHistory_.gHist.flushAndRestore(bp_info->gHistIter);
    DLOG("SPEC_GHR size = " << tageHistory_.gHist.getSpecHistSize());
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void ITTAGE::updateHistories_(Addr branch_pc, bool taken) {

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
int ITTAGE::bindex_(Addr pc_in) const {
    return ((pc_in >> instShiftAmt_) & ((1ULL << (logTagTableSizes_[0])) - 1));
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
uint64_t ITTAGE::get_compressed_global_history_(uint32_t inSize, uint32_t outSize) const {
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
uint64_t ITTAGE::get_path_history_hash_(uint32_t bank) const {

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
uint32_t ITTAGE::gindex_(Addr pc, int bank) const {

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
uint32_t ITTAGE::gtag_(Addr pc, int bank) const {

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
void ITTAGE::ctrUpdate_(T& ctr, bool taken, uint32_t nbits) {

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
void ITTAGE::unsignedCtrUpdate_(uint8_t & ctr, bool up, unsigned nbits) {
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
// -------------------------------------------------------------------
void ITTAGE::calculateIndicesAndTags_(Addr branch_pc) {

    // computes the table addresses and the partial tags
    for (uint32_t i = 1; i <= nTagTables_; i++) {
        tagTableIndices_[i] = gindex_(branch_pc, i);
        tableTags_[i] = gtag_(branch_pc, i);
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
unsigned ITTAGE::getUseAltIdx_(const TageBranchInfoPtr& bp_info) {

    // There are numHistoryTables number of counter on the ITTAGE implementation
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
void ITTAGE::resetUctr_(uint8_t & u) {
    u >>= 1;
}

// -------------------------------------------------------------------
// Helper: handle provider-side updates for indirect branches
void ITTAGE::indirectHandleProviderUpdate_(Addr branch_pc, Addr target_addr, const TageBranchInfoPtr& bi, bool &alloc) {
    TageEntry &hitEntry = g_table_[bi->hitBank][bi->hitBankIndex];

    // No update if the tag does not match
    if (hitEntry.tag != bi->hitBankTag) {
        return;
    }

    // Update the longest match table counter
    ctrUpdate_(hitEntry.ctr, bi->longestMatchPredAddr == target_addr, tagTableCounterBits_);
    DLOG("Updating g_table_[" << bi->hitBank << "][" << bi->hitBankIndex << "] for basic block PC = " << HEX16(branch_pc));

    if (bi->pseudoNewAlloc) {
        if (bi->longestMatchPredAddr == target_addr) {
            alloc = false;
        }
        if (bi->longestMatchPredAddr != bi->altPredAddr) {
            ctrUpdate_(useAltPredForNewlyAllocated_[getUseAltIdx_(bi)], (bi->altPredAddr == target_addr), useAltOnNaBits_);
        }
    }

    // If provider entry not useful, update alternate prediction
    if (hitEntry.u == 0) {
        if (bi->altBank > 0) {
            TageEntry &altEntry = g_table_[bi->altBank][bi->altBankIndex];
            if (altEntry.tag == bi->altBankTag) {
                ctrUpdate_(altEntry.ctr, bi->altPredAddr == target_addr, tagTableCounterBits_);
                DLOG("Updating g_table_[" << bi->altBank << "][" << bi->altBankIndex << "] for basic block PC = " << HEX16(branch_pc));
            }
        }

        if (bi->altBank == 0) {
           ctrUpdate_(btable_[bi->bimodalIndex].ctr, bi->altPredAddr == target_addr, bimodalTableCounterBits_);
           DLOG("Updating btable_[" << bi->bimodalIndex << "] for basic block PC = " << HEX16(branch_pc));
        }
    }

    // Update useful bit
    if (bi->longestMatchPredAddr != bi->altPredAddr) {
        static const bool INCREMENT = true;
        static const bool DECREMENT = !INCREMENT;

        if (bi->longestMatchPredAddr == target_addr) {
            unsignedCtrUpdate_(hitEntry.u, INCREMENT, tagTableUBits_);
        } else {
            unsignedCtrUpdate_(hitEntry.u, DECREMENT, tagTableUBits_);
        }
    }
}

// Helper: perform allocation of indirect branch entries when needed
void ITTAGE::indirectAllocateEntries_(Addr branch_pc, Addr target_addr, const TageBranchInfoPtr& bi) {
    ++allocateOnFlush_;

    uint32_t min = (1 << tagTableUBits_) - 1;
    uint32_t min_table = bi->hitBank + 1;
    uint32_t min_table_index = 0;

    for (uint32_t i = nTagTables_; i >= bi->hitBank + 1; --i) {
        uint32_t table_index = gindex_(branch_pc, i);
        if (g_table_[i][table_index].u <= min) {
            min             = g_table_[i][table_index].u;
            min_table       = i;
            min_table_index = table_index;
        }
    }

    if (min > 0 && min_table <= nTagTables_) {
        ++(*conflicted_evictions_[min_table]);
        g_table_[min_table][min_table_index].u = 0;
        DLOG("Forced g_table_[" << min_table << "][" << min_table_index << "].u=0 for bb_start_pc: " << HEX16(branch_pc));
    }

    DLOG ("Found min_table:" << min_table << " index:" << min_table_index);

    unsigned numAllocated = 0;
    for (uint32_t i = min_table; i <= nTagTables_; i++) {
        uint32_t table_index = gindex_(branch_pc, i);

        if (g_table_[i][table_index].u == 0) {
            
            g_table_[i][table_index].valid = true;
            g_table_[i][table_index].tag = gtag_(branch_pc, i);
            g_table_[i][table_index].ctr = 0;
            g_table_[i][table_index].target_addr = target_addr;

            ++numAllocated;
            ++(*flushAlloc_[i]);

            if (numAllocated == maxNumAlloc_) {
                break;
            }
        }
    }
}

// -------------------------------------------------------------------
// Helper: find hit and alternate banks for indirect branch prediction
void ITTAGE::findIndirectHitAndAltBanks_(const TageBranchInfoPtr& bp_info) {
    uint32_t hitBank = 0;
    uint32_t altBank = 0;

    // Single loop to find hit and alt banks
    for (int i = nTagTables_; i > 0; --i) {
        if (g_table_[i][tagTableIndices_[i]].valid &&
            g_table_[i][tagTableIndices_[i]].tag == tableTags_[i]) {
            if (hitBank == 0) {
                hitBank = i;
            } else {
                altBank = i;
                break;  // Found alt, no need to continue
            }
        }
    }

    // Set hit bank info
    if (hitBank > 0) {
        bp_info->hitBank = hitBank;
        bp_info->hitBankIndex = tagTableIndices_[hitBank];
        bp_info->hitBankTag = tableTags_[hitBank];
    }

    // Set alt bank info
    if (altBank > 0) {
        bp_info->altBank = altBank;
        bp_info->altBankIndex = tagTableIndices_[altBank];
        bp_info->altBankTag = tableTags_[altBank];
    }
}

// Helper: get predicted address from a bank or fallback
Addr ITTAGE::getPredictedAddr_(uint32_t bank, Addr fallback) const {
    if (bank > 0) {
        const auto& entry = g_table_[bank][tagTableIndices_[bank]];
        return entry.ctr >= 0 ? entry.target_addr : fallback;
    }
    return fallback;
}

// Helper: compute predictions for indirect branches
void ITTAGE::computeIndirectPrediction_(const TageBranchInfoPtr& bp_info, Addr pred_addr) {
    // computes the prediction and the alternate prediction
    if (bp_info->hitBank > 0) {
        bp_info->altPredAddr = getPredictedAddr_(bp_info->altBank, pred_addr);
        bp_info->longestMatchPredAddr = getPredictedAddr_(bp_info->hitBank, pred_addr);
        bp_info->pseudoNewAlloc = g_table_[bp_info->hitBank][bp_info->hitBankIndex].ctr <= 1;

        // Determine provider and final prediction
        int useAltIdx = getUseAltIdx_(bp_info);
        bool useLongest = (useAltPredForNewlyAllocated_[useAltIdx] < 0) || !bp_info->pseudoNewAlloc;

        if (useLongest) {
            bp_info->tagePredAddr = bp_info->longestMatchPredAddr;
            bp_info->provider = TAGE_LONGEST_MATCH;
        } else if (bp_info->altBank > 0) {
            bp_info->tagePredAddr = bp_info->altPredAddr;
            bp_info->provider = TAGE_ALT_MATCH;
        } else {
            bp_info->tagePredAddr = bp_info->altPredAddr;
            bp_info->provider = BIMODAL_ALT_MATCH;
        }
    } else {
        bp_info->altPredAddr = pred_addr;
        bp_info->tagePredAddr = pred_addr;
        bp_info->provider = BIMODAL_ONLY;
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void ITTAGE::indirectBranchUpdate_(Addr branch_pc, Addr target_addr, const TageBranchInfoPtr& bi) {

    // ITTAGE UPDATE
    // try to allocate a new entries only if prediction was wrong
    bool alloc = (bi->tagePredAddr != target_addr) && (bi->hitBank < nTagTables_);

    // [STEP 1] : The predictor component is not the bimodal table
    if (bi->hitBank > 0 && ((uint32_t)bi->hitBank <= nTagTables_)) {
        indirectHandleProviderUpdate_(branch_pc, target_addr, bi, alloc);
    }
    else {
        ctrUpdate_(btable_[bi->bimodalIndex].ctr, bi->tagePredAddr == target_addr, bimodalTableCounterBits_);
    }

    // [STEP 2] : Handle entry allocation
    //            This requiures PATH and GLOBAL histories to be
    //            first restored from the flushed inst
    if (alloc) {
        indirectAllocateEntries_(branch_pc, target_addr, bi);
    }

    // [STEP 3] : decrement the counter for reset
    --uResetTimer_;

    // [STEP 4] : reset the useful bits and counter if counter has reached 0
    if (uResetTimer_ == 0) {

        // reset least significant bit
        // most significant bit becomes least significant bit
        for (uint32_t i = 1; i <= nTagTables_; i++) {
            for (uint32_t j = 0; j < (1ULL << logTagTableSizes_[i]); j++) {
                resetUctr_(g_table_[i][j].u);
            }
        }

        uResetTimer_ = initialUResetTimerValue_;
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void ITTAGE::updateITTAGEStats_(Addr target_addr, const TageBranchInfoPtr& bp_info) {

    switch (bp_info->provider) {
        case BIMODAL_ONLY:
            ++bimodalProvider_;
            break;
        case TAGE_LONGEST_MATCH:
            ++tageLongestMatchProvider_;
            break;
        case BIMODAL_ALT_MATCH:
            ++bimodalAltProvider_;
            break;
        case TAGE_ALT_MATCH:
            ++tageAltProvider_;
            break;
    }

    if (target_addr == bp_info->tagePredAddr) {

        // correct prediction
        switch (bp_info->provider) {
          case BIMODAL_ONLY:
              bimodalProviderCorrect_++;
              break;
          case TAGE_LONGEST_MATCH:
              ++(*longestMatchProviderCorrect_[bp_info->hitBank]);
              break;
          case BIMODAL_ALT_MATCH:
              bimodalAltMatchProviderCorrect_++;
              break;
          case TAGE_ALT_MATCH:
              ++(*altMatchProviderCorrect_[bp_info->altBank]);
              break;
        }

    } else {

        // wrong prediction
        switch (bp_info->provider) {
          case BIMODAL_ONLY:
              bimodalProviderWrong_++;
              break;
          case TAGE_LONGEST_MATCH:
              ++(*longestMatchProviderWrong_[bp_info->hitBank]);
              if (bp_info->altPredAddr == target_addr) {
                altMatchProviderWouldHaveHit_++;
              }
              break;
          case BIMODAL_ALT_MATCH:
              bimodalAltMatchProviderWrong_++;
              break;
          case TAGE_ALT_MATCH:
              ++(*altMatchProviderWrong_[bp_info->altBank]);
              break;
        }

        switch (bp_info->provider) {
            case BIMODAL_ALT_MATCH:
            case TAGE_ALT_MATCH:
                if (bp_info->longestMatchPredAddr == target_addr) {
                    longestMatchProviderWouldHaveHit_++;
                }
                break;
        }
    }

    switch (bp_info->provider) {
        case TAGE_LONGEST_MATCH:
            ++(*longestMatchProvider_[bp_info->hitBank]);
            break;
        case TAGE_ALT_MATCH:
            ++(*altMatchProvider_[bp_info->altBank]);
            break;
    }
}

// STEP A-1
// Push the speculative history onto the list<bool>
void ITTAGE::GHR::pushSpecHist_(bool taken) {

    if (speculativeHistory.size() == speculative_hist_length) {
        speculativeHistory.pop_front();
    }
    speculativeHistory.push_back(taken);
}

// STEP A
// Push at the back the Circular-Buffer, automatically removes the
// object from the front on overflow
void ITTAGE::GHR::push(bool taken) {

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
std::list<bool>::iterator ITTAGE::GHR::getLastIter() {

    return std::prev(speculativeHistory.end());
}

// STEP C - FLUSH ROUTINE
// Removes all the younger content, pushes the result onto dequeue
void ITTAGE::GHR::flushAndRestore(std::list<bool>::iterator iter) {

    // STEP C-1:
    // Calculate the number of speculations younger than this iter
    // and shift the ghr right by that amount
    ghr >>= std::distance(iter, speculativeHistory.end());

    // STEP C-2:
    // Erase the number of speculations younger than this iter
    speculativeHistory.erase(iter, speculativeHistory.end());

    if (HASH_IMPL == 1) {
        // Rebuild all of the compressed histories
        for (int idx = 1; idx <= 2 * nTagTables_; ++idx) {
            tag_compressed_hist_[idx] = computeCompressedGlobalHistory_(tag_in_size_[idx], tag_out_size_[idx]);
        }
    }
}

bool ITTAGE::GHR::at(uint32_t index) const {
    return ghr[index];
}

uint64_t ITTAGE::GHR::computeCompressedGlobalHistory_(uint32_t inSize, uint32_t outSize) const {
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
        // uint8_t histBit = ((int)(i) >= 0) ? tageHistory_.gHist.at(i) : false;
        // uint8_t histBit = ((int)(start-i) >= 0) ? tageHistory_.gHist.at(start-i) : false;

        if (HASH == 2) {
            temporary_history |= ghr[i] << (outSize - 1 - compressed_bit_num); // Build history bit vector
        }
        ++compressed_bit_num;
    }
    compressed_history ^= temporary_history;
    return compressed_history;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
uint64_t ITTAGE::getSizeInBits_() {
    uint64_t total_bits = 0;
    uint32_t addr_width_in_bits = sizeof(Addr)*8;
    
    for (uint32_t i = 1; i <= nTagTables_; i++) {
        uint32_t bits_per_entry = (tagTableCounterBits_ + tagTableUBits_ + tagTableTagWidths_[i] + addr_width_in_bits);
        total_bits += (bits_per_entry << logTagTableSizes_[i]);
    }

    if (!use_btb_) {
        total_bits += (bimodalTableCounterBits_ << logTagTableSizes_[0]);
    }

    // total_bits += numUseAltOnNa_ * useAltOnNaBits_;
    // total_bits += tagTableHistLengths_[nTagTables_];
    // total_bits += 64; // pathHistBits_;
    // total_bits += logUResetPeriod_;
    return total_bits;
}

}
