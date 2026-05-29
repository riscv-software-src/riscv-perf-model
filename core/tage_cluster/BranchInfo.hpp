#pragma once

#include "sparta/memory/AddressTypes.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include <list>

namespace olympia {

struct TageBranchInfo {
    bool     predValid = false;
    bool     actualTaken = false;
    
    uint32_t hitBank      = 0;
    uint32_t hitBankIndex = 0;
    uint32_t hitBankWay   = 0;
    uint64_t hitBankTag   = 0x0;
    int32_t  hitBankCtr   = 0;
    
    uint32_t altBank      = 0;
    uint32_t altBankIndex = 0;
    uint32_t altBankWay   = 0;
    uint64_t altBankTag   = 0x0;
    int32_t  altBankCtr   = 0;

    std::vector<uint32_t> tableIndices;
    std::vector<uint32_t> tableTags;
    
    uint32_t bimodalIndex = 0;

    bool tagePred = false;
    sparta::memory::addr_t tagePredAddr = 0x0;

    bool altPred = false;
    sparta::memory::addr_t altPredAddr = 0x0;
    
    bool condBranch = false;
    bool indirectBranch = false;
    
    bool longestMatchPred = false;
    sparta::memory::addr_t longestMatchPredAddr = 0x0;
    
    bool pseudoNewAlloc = false;

    unsigned provider = -1;
    
    int32_t ctr = 0;
    bool    saturated = false;

    std::list<bool>::iterator gHistIter;
    uint64_t pathHist = 0x0;

    sparta::memory::addr_t branchPC = 0x0;
};

inline std::ostream & operator<<(std::ostream & os, const TageBranchInfo & tage_bi) {
    return os << "Basic Block Start PC:"   << HEX16(tage_bi.branchPC)
                << " Pred Valid:"          << tage_bi.predValid
                << " Actual Taken:"        << tage_bi.actualTaken
                << " Hit Bank:"            << tage_bi.hitBank
                << " Hit Bank Index:"      << tage_bi.hitBankIndex
                << " Hit Bank Tag:"        << HEX8(tage_bi.hitBankTag)
                << " Alt Bank:"            << tage_bi.altBank
                << " Alt Bank Index:"      << tage_bi.altBankIndex
                << " Alt Bank Tag:"        << HEX8(tage_bi.altBankTag)
                << " Longest Match Pred:"  << tage_bi.longestMatchPred
                << " Longest Match Addr:"  << HEX16(tage_bi.longestMatchPredAddr)
                << " Alt Pred:"            << tage_bi.altPred
                << " Alt Pred Addr:"       << HEX16(tage_bi.altPredAddr)
                << " Bimodal Index:"       << tage_bi.bimodalIndex
                << " Predicted Taken:"     << tage_bi.tagePred
                << " Predicted Address:"   << HEX16(tage_bi.tagePredAddr)
                << " Provider:"            << tage_bi.provider
                << " Hit Bank Ctr:"        << tage_bi.hitBankCtr
                << " Alt Bank Ctr:"        << tage_bi.altBankCtr
                << " Pred Ctr:"            << tage_bi.ctr;
}
using TageBranchInfoPtr   = sparta::SpartaSharedPointer<TageBranchInfo>;

inline sparta::SpartaSharedPointerAllocator<TageBranchInfo> branch_info_allocator (1600, 1200);
}
