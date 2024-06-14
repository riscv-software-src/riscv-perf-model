
#include "uarch/Gem5Tage.hpp"

namespace olympia
{

constexpr char Gem5Tage::name[];

Gem5Tage::Gem5Tage(sparta::TreeNode* node,
                     const Gem5TageParameterSet* p)
    : sparta::Unit(node),
      initialized(false),
      gen(std::random_device{}()),
      dis(std::numeric_limits<int>::min(),
          std::numeric_limits<int>::max()),
      //Yaml parameters
      pred_enable_(p->pred_enable),
      logRatioBiModalHystEntries(p->logRatioBiModalHystEntries),
      nHistoryTables(p->nHistoryTables),
      tagTableCounterBits(p->tagTableCounterBits),
      tagTableUBits(p->tagTableUBits),
      histBufferSize(p->histBufferSize),
      minHist(p->minHist),
      maxHist(p->maxHist),
      pathHistBits(p->pathHistBits),
      tagTableTagWidths(p->tagTableTagWidths),
      logTagTableSizes(p->logTagTableSizes),
      logUResetPeriod(p->logUResetPeriod),
      initialTCounterValue(p->initialTCounterValue),
      numUseAltOnNa(p->numUseAltOnNa),
      useAltOnNaBits(p->useAltOnNaBits),
      maxNumAlloc(p->maxNumAlloc),
      noSkip(p->noSkip),
      speculativeHistUpdate(p->speculativeHistUpdate),
      instShiftAmt(p->instShiftAmt),
      numThreads(p->numThreads),
      //Stats
      longestMatchProviderCorrect(&unit_stat_set_,
         "longestMatchProviderCorrect",
         "Number of times TAGE Longest Match is the provider and the "
         "prediction is correct",
         sparta::Counter::COUNT_NORMAL),

      altMatchProviderCorrect(&unit_stat_set_,
         "altMatchProviderCorrect",
         "Number of times TAGE Alt Match is the provider and the "
         "prediction is correct",
         sparta::Counter::COUNT_NORMAL),

      bimodalAltMatchProviderCorrect(&unit_stat_set_,
         "bimodalAltMatchProviderCorrect",
         "Number of times TAGE Alt Match is the bimodal and it is the "
         "provider and the prediction is correct",
         sparta::Counter::COUNT_NORMAL),

      bimodalProviderCorrect(&unit_stat_set_,
         "bimodalProviderCorrect",
         "Number of times there are no hits on the TAGE tables and the "
         "bimodal prediction is correct",
         sparta::Counter::COUNT_NORMAL),

      longestMatchProviderWrong(&unit_stat_set_,
         "longestMatchProviderWrong",
         "Number of times TAGE Longest Match is the provider and the "
         "prediction is wrong",
         sparta::Counter::COUNT_NORMAL),

      altMatchProviderWrong(&unit_stat_set_,
         "altMatchProviderWrong",
         "Number of times TAGE Alt Match is the provider and the "
         "prediction is wrong",
         sparta::Counter::COUNT_NORMAL),

      bimodalAltMatchProviderWrong(&unit_stat_set_,
         "bimodalAltMatchProviderWrong",
         "Number of times TAGE Alt Match is the bimodal and it is the "
         "provider and the prediction is wrong",
         sparta::Counter::COUNT_NORMAL),

      bimodalProviderWrong(&unit_stat_set_,
         "bimodalProviderWrong",
         "Number of times there are no hits on the TAGE tables and the "
         "bimodal prediction is wrong",
         sparta::Counter::COUNT_NORMAL),

      altMatchProviderWouldHaveHit(&unit_stat_set_,
         "altMatchProviderWouldHaveHit",
         "Number of times TAGE Longest Match is the provider, the "
         "prediction is wrong and Alt Match prediction was correct",
         sparta::Counter::COUNT_NORMAL),

      longestMatchProviderWouldHaveHit(&unit_stat_set_,
         "longestMatchProviderWouldHaveHit",
         "Number of times TAGE Alt Match is the provider, the "
         "prediction is wrong and Longest Match prediction was correct",
         sparta::Counter::COUNT_NORMAL)

{
    if(pred_enable_) {

      init();

      threadHistory.resize(p->numThreads);

      if (noSkip.empty())
      {
          // Set all the tables to enabled by default
          noSkip.resize(nHistoryTables + 1, true);
      }

    }

    i_bpu_request_.registerConsumerHandler(
    CREATE_SPARTA_HANDLER_WITH_DATA(Gem5Tage,handleBpuRequest,BpuRequestInfo));
}
// ===================================================================
// Main interface
// ===================================================================

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::squash(ThreadID tid, bool taken, BranchInfo *bi, Addr target)
{
    if (!speculativeHistUpdate) {
        /* If there are no speculative updates, no actions are needed */
        return;
    }

    ThreadHistory& tHist = threadHistory[tid];
    DLOG("Tage: Restoring branch info: "<<bi->branchPC
                <<"; taken? "<<taken
                <<"; PathHistory:"<<bi->pathHist
                <<", pointer:"<<bi->ptGhist);

    tHist.pathHist = bi->pathHist;
    tHist.ptGhist = bi->ptGhist;
    tHist.gHist = &(tHist.globalHistory[tHist.ptGhist]);
    tHist.gHist[0] = (taken ? 1 : 0);
    for (uint32_t i = 1; i <= nHistoryTables; i++) {
        tHist.computeIndices[i].comp = bi->ci[i];
        tHist.computeTags[0][i].comp = bi->ct0[i];
        tHist.computeTags[1][i].comp = bi->ct1[i];
        tHist.computeIndices[i].update(tHist.gHist);
        tHist.computeTags[0][i].update(tHist.gHist);
        tHist.computeTags[1][i].update(tHist.gHist);
    }
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool Gem5Tage::lookup(ThreadID tid, Addr pc, void * &bp_history)
{
    bool retval = predict(tid, pc, true, bp_history);

    DLOG("Tage: Lookup branch: "<<pc<<"; predict:"<<retval);

    return retval;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool Gem5Tage::predict(ThreadID tid, Addr pc, bool cond_branch, void* &b)
{
    TageBranchInfo *bi = new TageBranchInfo(this);//nHistoryTables+1);
    b = (void*)(bi);
    return tagePredict(tid, pc, cond_branch, bi->tageBranchInfo);
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
//void Gem5Tage::update(ThreadID tid, Addr branch_pc,bool taken,BranchInfo* bi)
void Gem5Tage::update(ThreadID tid, Addr pc, bool taken, void * &bp_history,
                      bool squashed, const StaticInstPtr & inst, Addr target)

{
    sparta_assert(bp_history,"Can not update null bp_history");

    TageBranchInfo *bi = static_cast<TageBranchInfo*>(bp_history);
    BranchInfo *tage_bi = bi->tageBranchInfo;

    if (squashed) {
        // This restores the global history, then update it
        // and recomputes the folded histories.
        squash(tid, taken, tage_bi, target);
        return;
    }

    int nrand = dis(gen) & 3;

    if (bi->tageBranchInfo->condBranch) {
        DLOG("Tage: Updating tables for branch:"<<pc<<"; taken?:"<<taken);
        updateStats(taken, bi->tageBranchInfo);
        condBranchUpdate(tid, pc, taken, tage_bi, nrand,
                         target, bi->tageBranchInfo->tagePred);

    }

    // optional non speculative update of the histories
    updateHistories(tid, pc, taken, tage_bi, false, inst, target);
    delete bi;
    bp_history = nullptr;

}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::updateHistories(ThreadID tid, Addr branch_pc, bool taken,
                          BranchInfo* bi, bool speculative,
                          const StaticInstPtr &inst, Addr target)
{
    if (speculative != speculativeHistUpdate) {
        return;
    }
    ThreadHistory& tHist = threadHistory[tid];
    //  UPDATE HISTORIES
    bool pathbit = ((branch_pc >> instShiftAmt) & 1);
    //on a squash, return pointers to this and recompute indices.
    //update user history
    updateGHist(tHist.gHist, taken, tHist.globalHistory, tHist.ptGhist);
    tHist.pathHist = (tHist.pathHist << 1) + pathbit;
    tHist.pathHist = (tHist.pathHist & ((1ULL << pathHistBits) - 1));

    if (speculative) {
        bi->ptGhist = tHist.ptGhist;
        bi->pathHist = tHist.pathHist;
    }

    //prepare next index and tag computations for user branchs
    for (uint32_t i = 1; i <= nHistoryTables; i++)
    {
        if (speculative) {
            bi->ci[i]  = tHist.computeIndices[i].comp;
            bi->ct0[i] = tHist.computeTags[0][i].comp;
            bi->ct1[i] = tHist.computeTags[1][i].comp;
        }
        tHist.computeIndices[i].update(tHist.gHist);
        tHist.computeTags[0][i].update(tHist.gHist);
        tHist.computeTags[1][i].update(tHist.gHist);
    }

    DLOG("Tage: Updating global histories with branch:"<<branch_pc
                <<"; taken?:"<<taken
                <<", path Hist: "<<tHist.pathHist
                <<"; pointer:"<<tHist.ptGhist);

    sparta_assert(threadHistory[tid].gHist ==
            &threadHistory[tid].globalHistory[threadHistory[tid].ptGhist],
            "Mismatched gHist and ptGhist for tid");

}
// ===================================================================
// support methods
// ===================================================================
Gem5Tage::BranchInfo* Gem5Tage::makeBranchInfo()
{
    return new BranchInfo(*this);
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::init()
{
    if (initialized) {
       return;
    }

    // Current method for periodically resetting the u counter bits only
    // works for 1 or 2 bits
    // Also make sure that it is not 0
    sparta_assert(tagTableUBits <= 2 && (tagTableUBits > 0),
                  "U counter bits must be > 0  and <= 2");

    // we use int type for the path history, so it cannot be more than
    // its size
    sparta_assert(pathHistBits <= (sizeof(int)*8),
                  "Path history bits must fit into native int type");

    // initialize the counter to half of the period
    sparta_assert(logUResetPeriod != 0,
                  "log U reset period can not be zero");

    tCounter = initialTCounterValue;

    sparta_assert(histBufferSize > maxHist * 2,
                  "History buffer size must be greater than 2x max history");

    useAltPredForNewlyAllocated.resize(numUseAltOnNa, 0);

    for (auto& history : threadHistory) {
        history.pathHist = 0;
        history.globalHistory = new uint8_t[histBufferSize];
        history.gHist = history.globalHistory;
        memset(history.gHist, 0, histBufferSize);
        history.ptGhist = 0;
    }

    histLengths = new int [nHistoryTables+1];

    calculateParameters();

    sparta_assert(tagTableTagWidths.size() == (nHistoryTables+1),
                  "Tag table Tag widths must be equal to nHistoryTables+1");
    sparta_assert(logTagTableSizes.size() == (nHistoryTables+1),
                  "Log Tag table sizes must be equal to nHistoryTables+1");

    // First entry is for the Bimodal table and it is untagged in this
    // implementation
    sparta_assert(tagTableTagWidths[0] == 0,
                  "First entry in tagTableTagWidths must be 0, untagged");

    for (auto& history : threadHistory) {
        history.computeIndices = new FoldedHistory[nHistoryTables+1];
        history.computeTags[0] = new FoldedHistory[nHistoryTables+1];
        history.computeTags[1] = new FoldedHistory[nHistoryTables+1];

        initFoldedHistories(history);
    }

    const uint64_t bimodalTableSize = 1ULL << logTagTableSizes[0];
    btablePrediction.resize(bimodalTableSize, false);
    btableHysteresis.resize(bimodalTableSize >> logRatioBiModalHystEntries,
                            true);

    gtable = new TageEntry*[nHistoryTables + 1];
    buildTageTables();

    tableIndices = new int [nHistoryTables+1];
    tableTags = new int [nHistoryTables+1];

    longestMatchProvider.resize(nHistoryTables+1,1);
    altMatchProvider.resize(nHistoryTables+1,1);

    initialized = true;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::initFoldedHistories(ThreadHistory & history)
{
    for (uint32_t i = 1; i <= nHistoryTables; i++) {
        history.computeIndices[i].init(
            histLengths[i], (logTagTableSizes[i]));
        history.computeTags[0][i].init(
            history.computeIndices[i].origLength, tagTableTagWidths[i]);
        history.computeTags[1][i].init(
            history.computeIndices[i].origLength, tagTableTagWidths[i]-1);

        DLOG("Tage "<<"HistLength:"<<histLengths[i]
                      <<", TTSize:"<<logTagTableSizes[i]
                      <<", TTTWidth:"<<tagTableTagWidths[i]);
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::buildTageTables()
{
    for (uint32_t i = 1; i <= nHistoryTables; i++) {
        gtable[i] = new TageEntry[1<<(logTagTableSizes[i])];
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::calculateParameters()
{
    histLengths[1] = minHist;
    histLengths[nHistoryTables] = maxHist;

    for (uint32_t i = 2; i <= nHistoryTables; i++) {
        histLengths[i] = (int) (((double) minHist *
                       pow ((double) (maxHist) / (double) minHist,
                           (double) (i - 1) / (double) ((nHistoryTables- 1))))
                       + 0.5);
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::btbUpdate(ThreadID tid, Addr branch_pc, BranchInfo* &bi)
{
    if (speculativeHistUpdate) {
        ThreadHistory& tHist = threadHistory[tid];
        DLOG("Tage; BTB miss resets prediction: "<<branch_pc);
        sparta_assert(tHist.gHist == &tHist.globalHistory[tHist.ptGhist],
          "tHist.gHist != to tHist.globalHistory[tHist.ptGhist]");
        tHist.gHist[0] = 0;
        for (uint32_t i = 1; i <= nHistoryTables; i++) {
            tHist.computeIndices[i].comp = bi->ci[i];
            tHist.computeTags[0][i].comp = bi->ct0[i];
            tHist.computeTags[1][i].comp = bi->ct1[i];
            tHist.computeIndices[i].update(tHist.gHist);
            tHist.computeTags[0][i].update(tHist.gHist);
            tHist.computeTags[1][i].update(tHist.gHist);
        }
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
int Gem5Tage::bindex(Addr pc_in) const
{
    return ((pc_in >> instShiftAmt) & ((1ULL << (logTagTableSizes[0])) - 1));
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
int Gem5Tage::F(int A, int size, int bank) const
{
    int A1, A2;

    A = A & ((1ULL << size) - 1);
    A1 = (A & ((1ULL << logTagTableSizes[bank]) - 1));
    A2 = (A >> logTagTableSizes[bank]);
    A2 = ((A2 << bank) & ((1ULL << logTagTableSizes[bank]) - 1))
       + (A2 >> (logTagTableSizes[bank] - bank));
    A = A1 ^ A2;
    A = ((A << bank) & ((1ULL << logTagTableSizes[bank]) - 1))
      + (A >> (logTagTableSizes[bank] - bank));
    return (A);
}

// -------------------------------------------------------------------
// gindex computes a full hash of pc, ghist and pathHist
// -------------------------------------------------------------------
int Gem5Tage::gindex(ThreadID tid, Addr pc, int bank) const
{
    int index;
    uint32_t hlen = ((uint32_t)histLengths[bank] > pathHistBits) ? pathHistBits :
                                                         histLengths[bank];
    const unsigned int shiftedPc = pc >> instShiftAmt;
    index =
        shiftedPc ^
        (shiftedPc >> ((int) abs(logTagTableSizes[bank] - bank) + 1)) ^
        threadHistory[tid].computeIndices[bank].comp ^
        F(threadHistory[tid].pathHist, hlen, bank);

    return (index & ((1ULL << (logTagTableSizes[bank])) - 1));
}


// -------------------------------------------------------------------
// Tag computation
// -------------------------------------------------------------------
uint16_t Gem5Tage::gtag(ThreadID tid, Addr pc, int bank) const
{
    int tag = (pc >> instShiftAmt) ^
              threadHistory[tid].computeTags[0][bank].comp ^
              (threadHistory[tid].computeTags[1][bank].comp << 1);

    return (tag & ((1ULL << tagTableTagWidths[bank]) - 1));
}


// -------------------------------------------------------------------
// Up-down saturating counter
// -------------------------------------------------------------------
template<typename T>
void Gem5Tage::ctrUpdate(T & ctr, bool taken, int nbits)
{
    sparta_assert((uint32_t)nbits <= sizeof(T) << 3,
       "CTRUPdate: nbits exceeds number of bits in the template type, ctr");
    if (taken) {
        if (ctr < ((1 << (nbits - 1)) - 1))
            ctr++;
    } else {
        if (ctr > -(1 << (nbits - 1)))
            ctr--;
    }
}

// -------------------------------------------------------------------
// int8_t and int versions of this function may be needed
// -------------------------------------------------------------------
template void Gem5Tage::ctrUpdate(int8_t & ctr, bool taken, int nbits);
template void Gem5Tage::ctrUpdate(int & ctr, bool taken, int nbits);

// -------------------------------------------------------------------
// Up-down unsigned saturating counter
// -------------------------------------------------------------------
void Gem5Tage::unsignedCtrUpdate(uint8_t & ctr, bool up, unsigned nbits)
{
    sparta_assert(nbits <= sizeof(uint8_t) << 3,
       "unsigned CTRUpdate: nbits exceeds number of bits in uint8_t");

    if (up) {
        if (ctr < ((1 << nbits) - 1))
            ctr++;
    } else {
        if (ctr)
            ctr--;
    }
}

// -------------------------------------------------------------------
// Bimodal prediction
// -------------------------------------------------------------------
bool Gem5Tage::getBimodePred(Addr pc, BranchInfo* bi) const
{
    return btablePrediction[bi->bimodalIndex];
}

// -------------------------------------------------------------------
// Update the bimodal predictor: a hysteresis bit is shared among N prediction
// bits (N = 2 ^ logRatioBiModalHystEntries)
// -------------------------------------------------------------------
void Gem5Tage::baseUpdate(Addr pc, bool taken, BranchInfo* bi)
{
    int inter = (btablePrediction[bi->bimodalIndex] << 1)
        + btableHysteresis[bi->bimodalIndex >> logRatioBiModalHystEntries];
    if (taken) {
        if (inter < 3)
            inter++;
    } else if (inter > 0) {
        inter--;
    }
    const bool pred = inter >> 1;
    const bool hyst = inter & 1;
    btablePrediction[bi->bimodalIndex] = pred;
    btableHysteresis[bi->bimodalIndex >> logRatioBiModalHystEntries] = hyst;
    DLOG("Tage: Updating branch "<<pc<<", pred:"<<pred<<", hyst:"<<hyst);
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
// shifting the global history:  we manage the history in a big table in order
// to reduce simulation time
void Gem5Tage::updateGHist(uint8_t * &h, bool dir, uint8_t * tab, int &pt)
{
    if (pt == 0) {
        DLOG("Tage: Rolling over the histories");
         // Copy beginning of globalHistoryBuffer to end, such that
         // the last maxHist outcomes are still reachable
         // through pt[0 .. maxHist - 1].
         for (uint32_t i = 0; i < maxHist; i++)
             tab[histBufferSize - maxHist + i] = tab[i];
         pt =  histBufferSize - maxHist;
         h = &tab[pt];
    }
    pt--;
    h--;
    h[0] = (dir) ? 1 : 0;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::calculateIndicesAndTags(ThreadID tid, Addr branch_pc,
                                  BranchInfo* bi)
{
    // computes the table addresses and the partial tags
    for (uint32_t i = 1; i <= nHistoryTables; i++) {
        tableIndices[i] = gindex(tid, branch_pc, i);
        bi->tableIndices[i] = tableIndices[i];
        tableTags[i] = gtag(tid, branch_pc, i);
        bi->tableTags[i] = tableTags[i];
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
unsigned Gem5Tage::getUseAltIdx(BranchInfo* bi, Addr branch_pc)
{
    // There is only 1 counter on the base TAGE implementation
    return 0;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool Gem5Tage::tagePredict(ThreadID tid, Addr branch_pc,
              bool cond_branch, BranchInfo* bi)
{
    Addr pc = branch_pc;
    bool pred_taken = true;

    if (cond_branch) {
        // TAGE prediction

        calculateIndicesAndTags(tid, pc, bi);

        bi->bimodalIndex = bindex(pc);

        bi->hitBank = 0;
        bi->altBank = 0;
        //Look for the bank with longest matching history
        for (int i = nHistoryTables; i > 0; i--) {
            if (noSkip[i] &&
                gtable[i][tableIndices[i]].tag == tableTags[i]) {
                bi->hitBank = i;
                bi->hitBankIndex = tableIndices[bi->hitBank];
                break;
            }
        }
        //Look for the alternate bank
        for (int i = bi->hitBank - 1; i > 0; i--) {
            if (noSkip[i] &&
                gtable[i][tableIndices[i]].tag == tableTags[i]) {
                bi->altBank = i;
                bi->altBankIndex = tableIndices[bi->altBank];
                break;
            }
        }
        //computes the prediction and the alternate prediction
        if (bi->hitBank > 0) {
            if (bi->altBank > 0) {
                bi->altTaken =
                    gtable[bi->altBank][tableIndices[bi->altBank]].ctr >= 0;
                extraAltCalc(bi);
            }else {
                bi->altTaken = getBimodePred(pc, bi);
            }

            bi->longestMatchPred =
                gtable[bi->hitBank][tableIndices[bi->hitBank]].ctr >= 0;
            bi->pseudoNewAlloc =
                abs(2 * gtable[bi->hitBank][bi->hitBankIndex].ctr + 1) <= 1;

            //if the entry is recognized as a newly allocated entry and
            //useAltPredForNewlyAllocated is positive use the alternate
            //prediction
            if ((useAltPredForNewlyAllocated[getUseAltIdx(bi, branch_pc)] < 0)
                || ! bi->pseudoNewAlloc) {
                bi->tagePred = bi->longestMatchPred;
                bi->provider = TAGE_LONGEST_MATCH;
            } else {
                bi->tagePred = bi->altTaken;
                bi->provider = bi->altBank ? TAGE_ALT_MATCH
                                           : BIMODAL_ALT_MATCH;
            }
        } else {
            bi->altTaken = getBimodePred(pc, bi);
            bi->tagePred = bi->altTaken;
            bi->longestMatchPred = bi->altTaken;
            bi->provider = BIMODAL_ONLY;
        }
        //end TAGE prediction

        pred_taken = (bi->tagePred);
        DLOG("Tage: Predict for "<<branch_pc
                    <<": taken?:"<<pred_taken
                    <<", tagePred:"<<bi->tagePred
                    <<", altPred:"<<bi->altTaken);
    
    }
    bi->branchPC = branch_pc;
    bi->condBranch = cond_branch;
    return pred_taken;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::adjustAlloc(bool & alloc, bool taken, bool pred_taken)
{
    // Nothing for this base class implementation
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::handleAllocAndUReset(bool alloc, bool taken, BranchInfo* bi,
                           int nrand)
{
    if (alloc) {
        // is there some "unuseful" entry to allocate
        uint8_t min = 1;
        for (int i = nHistoryTables; i > bi->hitBank; i--) {
            if (gtable[i][bi->tableIndices[i]].u < min) {
                min = gtable[i][bi->tableIndices[i]].u;
            }
        }

        // we allocate an entry with a longer history
        // to  avoid ping-pong, we do not choose systematically the next
        // entry, but among the 3 next entries
        int Y = nrand &
            ((1ULL << (nHistoryTables - bi->hitBank - 1)) - 1);
        int X = bi->hitBank + 1;
        if (Y & 1) {
            X++;
            if (Y & 2)
                X++;
        }
        // No entry available, forces one to be available
        if (min > 0) {
            gtable[X][bi->tableIndices[X]].u = 0;
        }


        //Allocate entries
        unsigned numAllocated = 0;
        for (uint32_t i = X; i <= nHistoryTables; i++) {
            if (gtable[i][bi->tableIndices[i]].u == 0) {
                gtable[i][bi->tableIndices[i]].tag = bi->tableTags[i];
                gtable[i][bi->tableIndices[i]].ctr = (taken) ? 0 : -1;
                ++numAllocated;
                if (numAllocated == maxNumAlloc) {
                    break;
                }
            }
        }
    }

    tCounter++;

    handleUReset();
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::handleUReset()
{
    //periodic reset of u: reset is not complete but bit by bit
    if ((tCounter & ((1ULL << logUResetPeriod) - 1)) == 0) {
        // reset least significant bit
        // most significant bit becomes least significant bit
        for (uint32_t i = 1; i <= nHistoryTables; i++) {
            for (uint32_t j = 0; j < (1ULL << logTagTableSizes[i]); j++) {
                resetUctr(gtable[i][j].u);
            }
        }
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::resetUctr(uint8_t & u)
{
    u >>= 1;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::condBranchUpdate(ThreadID tid, Addr branch_pc, bool taken,
     BranchInfo* bi, int nrand, Addr corrTarget, bool pred, bool preAdjustAlloc)
{
    // TAGE UPDATE
    // try to allocate a  new entries only if prediction was wrong
    bool alloc = (bi->tagePred != taken) && ((uint32_t)bi->hitBank < nHistoryTables);

    if (preAdjustAlloc) {
        adjustAlloc(alloc, taken, pred);
    }

    if (bi->hitBank > 0) {
        // Manage the selection between longest matching and alternate
        // matching for "pseudo"-newly allocated longest matching entry
        bool PseudoNewAlloc = bi->pseudoNewAlloc;
        // an entry is considered as newly allocated if its prediction
        // counter is weak
        if (PseudoNewAlloc) {
            if (bi->longestMatchPred == taken) {
                alloc = false;
            }
            // if it was delivering the correct prediction, no need to
            // allocate new entry even if the overall prediction was false
            if (bi->longestMatchPred != bi->altTaken) {
                ctrUpdate(
                    useAltPredForNewlyAllocated[getUseAltIdx(bi, branch_pc)],
                    bi->altTaken == taken, useAltOnNaBits);
            }
        }
    }

    if (!preAdjustAlloc) {
        adjustAlloc(alloc, taken, pred);
    }

    handleAllocAndUReset(alloc, taken, bi, nrand);

    handleTAGEUpdate(branch_pc, taken, bi);
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::handleTAGEUpdate(Addr branch_pc, bool taken, BranchInfo* bi)
{
    if (bi->hitBank > 0) {
          DLOG("Tage: Updating tag table entry "
                 <<"("<<bi->hitBank<<","<<bi->hitBankIndex<<") "
                 <<"for branch "<<branch_pc)

        ctrUpdate(gtable[bi->hitBank][bi->hitBankIndex].ctr, taken,
                  tagTableCounterBits);
        // if the provider entry is not certified to be useful also update
        // the alternate prediction
        if (gtable[bi->hitBank][bi->hitBankIndex].u == 0) {
            if (bi->altBank > 0) {
                ctrUpdate(gtable[bi->altBank][bi->altBankIndex].ctr, taken,
                          tagTableCounterBits);
                DLOG("Tage: Updating tag table entry "
                       <<"("<<bi->hitBank<<","<<bi->hitBankIndex<<") "
                       <<"for branch "<<branch_pc)

            }
            if (bi->altBank == 0) {
                baseUpdate(branch_pc, taken, bi);
            }
        }

        // update the u counter
        if (bi->tagePred != bi->altTaken) {
            unsignedCtrUpdate(gtable[bi->hitBank][bi->hitBankIndex].u,
                              bi->tagePred == taken, tagTableUBits);
        }
    } else {
        baseUpdate(branch_pc, taken, bi);
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::extraAltCalc(BranchInfo* bi)
{
    // do nothing. This is only used in some derived classes
    return;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void Gem5Tage::updateStats(bool taken, BranchInfo* bi)
{
    if (taken == bi->tagePred) {
        // correct prediction
        switch (bi->provider) {
          case BIMODAL_ONLY:
              bimodalProviderCorrect++;
              break;
          case TAGE_LONGEST_MATCH:
              longestMatchProviderCorrect++;
              break;
          case BIMODAL_ALT_MATCH:
              bimodalAltMatchProviderCorrect++;
              break;
          case TAGE_ALT_MATCH:
              altMatchProviderCorrect++;
              break;
        }
    } else {
        // wrong prediction
        switch (bi->provider) {
          case BIMODAL_ONLY:
              bimodalProviderWrong++;
              break;
          case TAGE_LONGEST_MATCH:
              longestMatchProviderWrong++;
              if (bi->altTaken == taken) {
                altMatchProviderWouldHaveHit++;
              }
              break;
          case BIMODAL_ALT_MATCH:
              bimodalAltMatchProviderWrong++;
              break;
          case TAGE_ALT_MATCH:
              altMatchProviderWrong++;
              break;
        }

        switch (bi->provider) {
            case BIMODAL_ALT_MATCH:
            case TAGE_ALT_MATCH:
                if (bi->longestMatchPred == taken) {
                    longestMatchProviderWouldHaveHit++;
                }
                break;
        }
    }

    switch (bi->provider) {
        case TAGE_LONGEST_MATCH:
        case TAGE_ALT_MATCH:
          longestMatchProvider[bi->hitBank]++;
          altMatchProvider[bi->altBank]++;
          break;
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
unsigned Gem5Tage::getGHR(ThreadID tid, BranchInfo *bi) const
{
    unsigned val = 0;
    for (unsigned i = 0; i < 32; i++) {
        // Make sure we don't go out of bounds
        int gh_offset = bi->ptGhist + i;
        sparta_assert(&(threadHistory[tid].globalHistory[gh_offset]) <
               threadHistory[tid].globalHistory + histBufferSize,
               "GHR mismatch in getGHR");

        val |= ((threadHistory[tid].globalHistory[gh_offset] & 0x1) << i);
    }

    return val;
}


// -------------------------------------------------------------------
// -------------------------------------------------------------------
int8_t Gem5Tage::getCtr(int hitBank, int hitBankIndex) const
{
    return gtable[hitBank][hitBankIndex].ctr;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
unsigned Gem5Tage::getTageCtrBits() const
{
    return tagTableCounterBits;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
int Gem5Tage::getPathHist(ThreadID tid) const
{
    return threadHistory[tid].pathHist;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool Gem5Tage::isSpeculativeUpdateEnabled() const
{
    return speculativeHistUpdate;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
size_t Gem5Tage::getSizeInBits() const {
    size_t bits = 0;
    for (uint32_t i = 1; i <= nHistoryTables; i++) {
        bits += (1 << logTagTableSizes[i]) *
            (tagTableCounterBits + tagTableUBits + tagTableTagWidths[i]);
    }
    uint64_t bimodalTableSize = 1ULL << logTagTableSizes[0];
    bits += numUseAltOnNa * useAltOnNaBits;
    bits += bimodalTableSize;
    bits += (bimodalTableSize >> logRatioBiModalHystEntries);
    bits += histLengths[nHistoryTables];
    bits += pathHistBits;
    bits += logUResetPeriod;
    return bits;
}

}
