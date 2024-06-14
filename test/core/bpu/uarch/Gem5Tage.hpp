// HEADER PLACEHOLDER
// contact Jeff Nye
#pragma once

#include "Inst.hpp"
#include "commonTypes.h"

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

// these are empty in this translated version
// but kept to inidicate the ret values of the
// original methods
#define CONST
#define VIRTUAL

#define nullStaticInstPtr  nullptr

namespace olympia
{

class Gem5Tage : public sparta::Unit
{
    //TODO: share these across bpus if there ends up being more than one
    using Addr          = sparta::memory::addr_t;
    using ThreadID      = int16_t;
    using StaticInstPtr = InstPtr;
    using PCStateBase   = uint32_t; 

    static const int MaxAddr = std::numeric_limits<int>::max();
public:
    //! \brief ...
    class Gem5TageParameterSet : public sparta::ParameterSet
    {
      public:
        //! \brief ...
        Gem5TageParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n)
        {
        }

        PARAMETER(bool,     pred_enable,                   false,
                  "Enable prediction")

        PARAMETER(uint32_t, numThreads,                    1, 
                 "number of execution threads")

        PARAMETER(uint32_t, instShiftAmt,                  2, 
                 "Number of bits to shift instructions by")

        PARAMETER(uint32_t, nHistoryTables,                7,
                 "Number of history tables")

        PARAMETER(uint32_t, minHist,                       5,  
                 "Minimum history size of TAGE")

        PARAMETER(uint32_t, maxHist,                       130,
                 "Maximum history size of TAGE")

        PARAMETER(std::vector<uint32_t>,tagTableTagWidths, {},
                 "Tag size in TAGE tag tables")

        PARAMETER(std::vector<int32_t>, logTagTableSizes,  {},
                 "Log2 of TAGE table sizes")

        PARAMETER(uint32_t, logRatioBiModalHystEntries,    2, 
                 "Log num of prediction entries for a shared "
                 "hysteresis bit for the Bimodal")

        PARAMETER(uint32_t, tagTableCounterBits,           3, 
                 "Number of tag table counter bits")

        PARAMETER(uint32_t, tagTableUBits,                 2, 
                 "Number of tag table u bits")

        PARAMETER(uint32_t, histBufferSize,                2097152, 
                 "A large number to track all branch histories "
                 "(2MEntries default)")

        PARAMETER(uint32_t, pathHistBits,                  16, 
                 "Path history size")

        PARAMETER(uint64_t, logUResetPeriod,               18,      
                 "Log period in number of branches to reset TAGE  "
                 "useful counters")

        PARAMETER(uint32_t, numUseAltOnNa,                 1,      
                 "Number of USE_ALT_ON_NA counters")

        PARAMETER(int64_t,  initialTCounterValue,          (1<<17),      
                 "Initial value of tCounter")

        PARAMETER(uint32_t, useAltOnNaBits,                4,      
                 "Size of the USE_ALT_ON_NA counter(s)")

        PARAMETER(uint32_t, maxNumAlloc,                   1,      
                 "Max number of TAGE entries allocted on mispredict")

        PARAMETER(std::vector<bool>,    noSkip,            {},
                 "Vector of enabled TAGE tables")

        PARAMETER(bool,     speculativeHistUpdate,         true,      
                 "Use speculative update for histories")

    };

    //! \brief ...
    Gem5Tage(sparta::TreeNode* node,const Gem5TageParameterSet* p);

    //! \brief ...
    static constexpr char name[] = "gem5tage";

    //! \brief ...
    void  init();
    //! \brief ...
    void  initThreadHistory(const uint32_t);

  private:
    // ------------------------------------------------------------------
    // Ports
    // ------------------------------------------------------------------
    sparta::DataInPort<BpuRequestInfo> i_bpu_request_{
        &unit_port_set_, "i_bpu_request", 1
    };

    sparta::DataOutPort<BpuResponseInfo> o_bpu_response_{
        &unit_port_set_, "o_bpu_response"};

  private:
    // ------------------------------------------------------------------
    // Port handlers
    // ------------------------------------------------------------------
    //! \brief ...
    void handleBpuRequest(const BpuRequestInfo & req)
    {
    }

    //! \brief ...
    void handleBpuResponse(const uint32_t & count)
    {
    }

  public:
    // ------------------------------------------------------------------
    // Enums/Structs
    // ------------------------------------------------------------------
    //! \brief provider type
    enum
    {
        BIMODAL_ONLY = 0,
        TAGE_LONGEST_MATCH,
        BIMODAL_ALT_MATCH,
        TAGE_ALT_MATCH,
        LAST_TAGE_PROVIDER_TYPE = TAGE_ALT_MATCH
    };

    //! \brief ...
    struct TageEntry
    {
        int8_t ctr;
        uint16_t tag;
        uint8_t u;
        TageEntry() : ctr(0), tag(0), u(0) { }
    };

    //! Folded History Table - compressed history
    //!
    //! to mix with instruction PC to index partially tagged tables.
    struct FoldedHistory
    {
        unsigned comp;
        int compLength;
        int origLength;
        int outpoint;
        int bufferSize;

        FoldedHistory()
        {
            comp = 0;
        }

        void init(int original_length, int compressed_length)
        {
            origLength = original_length;
            compLength = compressed_length;
            outpoint = original_length % compressed_length;
        }

        void update(uint8_t * h)
        {
            comp = (comp << 1) | h[0];
            comp ^= h[origLength] << outpoint;
            comp ^= (comp >> compLength);
            comp &= (1ULL << compLength) - 1;
        }
    };

    //! \brief Primary branch history entry
    struct BranchInfo
    {
        int pathHist;
        int ptGhist;
        int hitBank;
        int hitBankIndex;
        int altBank;
        int altBankIndex;
        int bimodalIndex;

        bool tagePred;
        bool altTaken;
        bool condBranch;
        bool longestMatchPred;
        bool pseudoNewAlloc;
        Addr branchPC;

        // Pointer to dynamically allocated storage
        // to save table indices and folded histories.
        // To do one call to new instead of five.
        int *storage;

        // Pointers to actual saved array within the dynamically
        // allocated storage.
        int *tableIndices;
        int *tableTags;
        int *ci;
        int *ct0;
        int *ct1;

        // for stats purposes
        unsigned provider;

        BranchInfo(const Gem5Tage &tage)
            : pathHist(0), ptGhist(0),
              hitBank(0), hitBankIndex(0),
              altBank(0), altBankIndex(0),
              bimodalIndex(0),
              tagePred(false), altTaken(false),
              condBranch(false), longestMatchPred(false),
              pseudoNewAlloc(false), branchPC(0),
              provider(-1)
        {
            int sz = tage.nHistoryTables + 1;
            storage = new int [sz * 5];
            tableIndices = storage;
            tableTags = storage + sz;
            ci = tableTags + sz;
            ct0 = ci + sz;
            ct1 = ct0 + sz;
        }

        virtual ~BranchInfo()
        {
            delete[] storage;
        }
    };

    // This is a carry over due to the translation from GEM5
    // and intent to make minimal changes to original source
    // This can be smashed out for performance and clarity once 
    // there are test cases in place
    struct TageBranchInfo
    {
        BranchInfo *tageBranchInfo;
        TageBranchInfo(Gem5Tage *tage)
          : tageBranchInfo(tage->makeBranchInfo())
        {
        }
        virtual ~TageBranchInfo() 
        {
           delete tageBranchInfo;
        }

    };

//protected:
public:
    // Keep per-thread histories to
    // support SMT.
    struct ThreadHistory
    {
        // Speculative path history
        // (LSB of branch address)
        int pathHist;

        // Speculative branch direction
        // history (circular buffer)
        // @TODO Convert to std::vector<bool>
        uint8_t *globalHistory;

        // Pointer to most recent branch outcome
        uint8_t* gHist;

        // Index to most recent branch outcome
        int ptGhist;

        // Speculative folded histories.
        FoldedHistory *computeIndices;
        FoldedHistory *computeTags[2];
    };


private:
    // ------------------------------------------------------------------
    // Main Interface Methods
    //
    // squash - called once on branch misprediction
    // lookup - 
    // predict - 
    // update - 
    // updateHistories - 
    // ------------------------------------------------------------------
    //!  \brief Restores speculatively updated path and direction histories.
    //!
    //!  Also recomputes compressed (folded) histories based on the
    //!  correct branch outcome.
    //!
    //!  This version of squash() is called once on a branch misprediction.
    //!
    //!  @param tid The Thread ID to select the histories to rollback.
    //!  @param taken The correct branch outcome.
    //!  @param bp_history Wrapping pointer to BranchInfo (to allow
    //!         storing derived class prediction information in the
    //!         base class).
    //!  @param target The correct branch target
    //!  @post bp_history points to valid memory.
    // ------------------------------------------------------------------
    void squash(ThreadID tid, bool taken, BranchInfo *bi, Addr target);

    // ------------------------------------------------------------------
    //! \brief Predict a known conditional branch
    //!
    //! Calls predict(...,bool cond_branch=true,...)
    //! Part of the previous GEM5 inheritance based BPU interface
    //! Indirection kept for now.
    // ------------------------------------------------------------------
    bool lookup(ThreadID tid, Addr pc, void * &bp_history);

    // ------------------------------------------------------------------
    //! \brief Return a prediction for PC 
    //!
    //! Calls tagePredict()
    //! Part of the previous GEM5 inheritance based BPU interface
    //! Indirection kept for now.
    // ------------------------------------------------------------------
    bool predict(ThreadID tid, Addr pc, bool cond_branch, void* &b);

    // ------------------------------------------------------------------
    //!  \brief Update TAGE. 
    //! 
    //!  Called at execute to repair histories on a misprediction
    //!  and at commit to update the tables.
    //! 
    //!  @param tid The thread ID to select the global
    //!         histories to use.
    //!  @param branch_pc The unshifted branch PC.
    //!  @param taken Actual branch outcome.
    //!  @param bi Pointer to information on the prediction
    //!         recorded at prediction time.
    // ------------------------------------------------------------------
    //void update(ThreadID tid, Addr branch_pc, bool taken, BranchInfo* bi);

    void update(ThreadID tid, Addr pc, bool taken, void * &bp_history,
                bool squashed, const StaticInstPtr & inst, Addr target);

    // ------------------------------------------------------------------
    //! \brief (Speculatively) updates global histories (path and direction).
    //! 
    //! Also recomputes compressed (folded) histories based on the
    //! branch direction.
    //! 
    //! @param tid The thread ID to select the histories
    //! to update.
    //! @param branch_pc The unshifted branch PC.
    //! @param taken (Predicted) branch direction.
    //! @param b Wrapping pointer to BranchInfo (to allow
    //! storing derived class prediction information in the
    //! base class).
    // ------------------------------------------------------------------
    VIRTUAL void updateHistories(
        ThreadID tid, Addr branch_pc, bool taken, BranchInfo* b,
        bool speculative,
        const StaticInstPtr & inst = nullStaticInstPtr,
        Addr target = MaxAddr);

    // ------------------------------------------------------------------
    // Other Methods
    // ------------------------------------------------------------------
    VIRTUAL BranchInfo *makeBranchInfo();

    /**
     * Computes the index used to access the
     * bimodal table.
     * @param pc_in The unshifted branch PC.
     */
    VIRTUAL int bindex(Addr pc_in) const;

    /**
     * Computes the index used to access a
     * partially tagged table.
     * @param tid The thread ID used to select the
     * global histories to use.
     * @param pc The unshifted branch PC.
     * @param bank The partially tagged table to access.
     */
    VIRTUAL int gindex(ThreadID tid, Addr pc, int bank) const;

    /**
     * Utility function to shuffle the path history
     * depending on which tagged table we are accessing.
     * @param phist The path history.
     * @param size Number of path history bits to use.
     * @param bank The partially tagged table to access.
     */
    VIRTUAL int F(int phist, int size, int bank) const;

    /**
     * Computes the partial tag of a tagged table.
     * @param tid the thread ID used to select the
     * global histories to use.
     * @param pc The unshifted branch PC.
     * @param bank The partially tagged table to access.
     */
    VIRTUAL uint16_t gtag(ThreadID tid, Addr pc, int bank) const;

    /**
     * Updates a direction counter based on the actual
     * branch outcome.
     * @param ctr Reference to counter to update.
     * @param taken Actual branch outcome.
     * @param nbits Counter width.
     */
    template<typename T>
    static void ctrUpdate(T & ctr, bool taken, int nbits);

    /**
     * Updates an unsigned counter based on up/down parameter
     * @param ctr Reference to counter to update.
     * @param up Boolean indicating if the counter is incremented/decremented
     * If true it is incremented, if false it is decremented
     * @param nbits Counter width.
     */
    static void unsignedCtrUpdate(uint8_t & ctr, bool up, unsigned nbits);

    /**
     * Get a branch prediction from the bimodal
     * predictor.
     * @param pc The unshifted branch PC.
     * @param bi Pointer to information on the
     * prediction.
     */
    VIRTUAL bool getBimodePred(Addr pc, BranchInfo* bi) const;

    /**
     * Updates the bimodal predictor.
     * @param pc The unshifted branch PC.
     * @param taken The actual branch outcome.
     * @param bi Pointer to information on the prediction
     * recorded at prediction time.
     */
    void baseUpdate(Addr pc, bool taken, BranchInfo* bi);

   /**
    * (Speculatively) updates the global branch history.
    * @param h Reference to pointer to global branch history.
    * @param dir (Predicted) outcome to update the histories
    * with.
    * @param tab
    * @param PT Reference to path history.
    */
    void updateGHist(uint8_t * &h, bool dir, uint8_t * tab, int &PT);

    /**
     * Update TAGE for conditional branches.
     * @param branch_pc The unshifted branch PC.
     * @param taken Actual branch outcome.
     * @param bi Pointer to information on the prediction
     * recorded at prediction time.
     * @nrand Random int number from 0 to 3
     * @param corrTarget The correct branch target
     * @param pred Final prediction for this branch
     * @param preAdjustAlloc call adjustAlloc before checking
     * pseudo newly allocated entries
     */
    VIRTUAL void condBranchUpdate(
        ThreadID tid, Addr branch_pc, bool taken, BranchInfo* bi,
        int nrand, Addr corrTarget, bool pred, bool preAdjustAlloc = false);

    /**
     * TAGE prediction called from TAGE::predict
     * @param tid The thread ID to select the global
     * histories to use.
     * @param branch_pc The unshifted branch PC.
     * @param cond_branch True if the branch is conditional.
     * @param bi Pointer to the BranchInfo
     */
    bool tagePredict(
        ThreadID tid, Addr branch_pc, bool cond_branch, BranchInfo* bi);

    /**
     * Update the stats
     * @param taken Actual branch outcome
     * @param bi Pointer to information on the prediction
     * recorded at prediction time.
     */
    VIRTUAL void updateStats(bool taken, BranchInfo* bi);

    /**
     * Instantiates the TAGE table entries
     */
    VIRTUAL void buildTageTables();

    /**
     * Calculates the history lengths
     * and some other paramters in derived classes
     */
    VIRTUAL void calculateParameters();

    /**
     * On a prediction, calculates the TAGE indices and tags for
     * all the different history lengths
     */
    VIRTUAL void calculateIndicesAndTags(
        ThreadID tid, Addr branch_pc, BranchInfo* bi);

    /**
     * Calculation of the index for useAltPredForNewlyAllocated
     * On this base TAGE implementation it is always 0
     */
    VIRTUAL unsigned getUseAltIdx(BranchInfo* bi, Addr branch_pc);

    /**
     * Extra calculation to tell whether TAGE allocaitons may happen or not
     * on an update
     * For this base TAGE implementation it does nothing
     */
    VIRTUAL void adjustAlloc(bool & alloc, bool taken, bool pred_taken);

    /**
     * Handles Allocation and U bits reset on an update
     */
    VIRTUAL void handleAllocAndUReset(
        bool alloc, bool taken, BranchInfo* bi, int nrand);

    /**
     * Handles the U bits reset
     */
    VIRTUAL void handleUReset();

    /**
     * Handles the update of the TAGE entries
     */
    VIRTUAL void handleTAGEUpdate(
        Addr branch_pc, bool taken, BranchInfo* bi);

    /**
     * Algorithm for resetting a single U counter
     */
    VIRTUAL void resetUctr(uint8_t & u);

    /**
     * Extra steps for calculating altTaken
     * For this base TAGE class it does nothing
     */
    VIRTUAL void extraAltCalc(BranchInfo* bi);

    VIRTUAL bool isHighConfidence(BranchInfo* bi) const
    {
        return false;
    }

    void btbUpdate(ThreadID tid, Addr branch_addr, BranchInfo* &bi);
    unsigned getGHR(ThreadID tid, BranchInfo *bi) const;
    int8_t getCtr(int hitBank, int hitBankIndex) const;
    unsigned getTageCtrBits() const;
    int getPathHist(ThreadID tid) const;
    bool isSpeculativeUpdateEnabled() const;
    size_t getSizeInBits() const;

  protected:
    std::vector<bool> btablePrediction;
    std::vector<bool> btableHysteresis;
    TageEntry **gtable;

    /**
     * Initialization of the folded histories
     */
    VIRTUAL void initFoldedHistories(ThreadHistory & history);

    int *histLengths;
    int *tableIndices;
    int *tableTags;

    std::vector<int8_t> useAltPredForNewlyAllocated;
    int64_t tCounter;

    // Tells which tables are active
    // (for the base TAGE implementation all are active)
    // Some other classes use this for handling associativity


    bool initialized{false};

    std::mt19937 gen;
    std::uniform_int_distribution<int> dis;

    //! \brief ... TODO
    std::vector<ThreadHistory> threadHistory;

    // ------------------------------------------------------------------
    // Parameters
    // ------------------------------------------------------------------

    //! \brief Parameter: master enable
    bool pred_enable_;

    //! \brief see cpp
    CONST uint32_t logRatioBiModalHystEntries;
    //! \brief ...
    CONST uint32_t nHistoryTables;
    //! \brief ...
    CONST uint32_t tagTableCounterBits;
    //! \brief ...
    CONST uint32_t tagTableUBits;
    //! \brief ...
    CONST uint32_t histBufferSize;
    //! \brief ...
    CONST uint32_t minHist;
    //! \brief ...
    CONST uint32_t maxHist;
    //! \brief ...
    CONST uint32_t pathHistBits;
    //! \brief ...
    std::vector<unsigned> tagTableTagWidths;
    //! \brief ...
    std::vector<int> logTagTableSizes;
    //! \brief ...
    uint64_t logUResetPeriod;
    //! \brief ...
    CONST int64_t initialTCounterValue;
    //! \brief ...
    uint32_t numUseAltOnNa;
    //! \brief ...
    uint32_t useAltOnNaBits;
    //! \brief ...
    uint32_t maxNumAlloc;
    //! \brief ...
    std::vector<bool> noSkip;
    //! \brief ...
    CONST bool speculativeHistUpdate;
    //! \brief ...
    CONST uint32_t instShiftAmt;
    //! \brief ...
    CONST uint32_t numThreads;
    // ------------------------------------------------------------------
    // Counters/Stats
    // ------------------------------------------------------------------
    //! \brief see .ccp
    sparta::Counter longestMatchProviderCorrect;
    //! \brief see .ccp
    sparta::Counter altMatchProviderCorrect;
    //! \brief see .ccp
    sparta::Counter bimodalAltMatchProviderCorrect;
    //! \brief see .ccp
    sparta::Counter bimodalProviderCorrect;
    //! \brief see .ccp
    sparta::Counter longestMatchProviderWrong;
    //! \brief see .ccp
    sparta::Counter altMatchProviderWrong;
    //! \brief see .ccp
    sparta::Counter bimodalAltMatchProviderWrong;
    //! \brief see .ccp
    sparta::Counter bimodalProviderWrong;
    //! \brief see .ccp
    sparta::Counter altMatchProviderWouldHaveHit;
    //! \brief see .ccp
    sparta::Counter longestMatchProviderWouldHaveHit;
    //! \brief see .ccp 
    std::vector<uint32_t> longestMatchProvider;
    //sparta::BasicHistogram<uint32_t> longestMatchProvider;
    //! \brief see .ccp 
    std::vector<uint32_t> altMatchProvider;
    //sparta::BasicHistogram<uint32_t> altMatchProvider;

    // ------------------------------------------------------------------
};
#undef CONST
#undef nullStaticInstPtr
#undef VIRTUAL
}
