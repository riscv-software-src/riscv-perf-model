#include "MMU.hpp"

namespace olympia {
    const char MMU::name[] = "mmu";

    void MMU::setTLB(olympia::SimpleTLB &tlb) {
        tlb_cache_ = &tlb;
    }

    MMU::MMU(sparta::TreeNode *node, const MMUParameterSet *p) :
            sparta::Unit(node),
            tlb_always_hit_(p->tlb_always_hit),
            mmu_latency_(p->mmu_latency) {
    }

    bool MMU::memLookup(const MemoryAccessInfoPtr &mem_access_info_ptr) {
        const InstPtr &inst_ptr = mem_access_info_ptr->getInstPtr();
        uint64_t vaddr = inst_ptr->getTargetVAddr();

        bool tlb_hit = tlb_always_hit_;

        // C++ comma operator: assign tlb_hit first, then evaluate it. Just For Fun
        if (!tlb_hit) {
            auto tlb_entry = tlb_cache_->peekLine(vaddr);
            tlb_hit = (tlb_entry != nullptr) && tlb_entry->isValid();

            // Update MRU replacement state if TLB HIT
            if (tlb_hit) {
                tlb_cache_->touch(*tlb_entry);
            }
        }


        if (tlb_always_hit_) {
            ILOG("TLB HIT all the time: vaddr=0x" << std::hex << vaddr);
            tlb_hits_++;
        } else if (tlb_hit) {
            ILOG("TLB HIT: vaddr=0x" << std::hex << vaddr);
            tlb_hits_++;
        } else {
            ILOG("TLB MISS: vaddr=0x" << std::hex << vaddr);
            tlb_misses_++;
        }

        return tlb_hit;
    }

    // Reload TLB entry
    void MMU::reloadTLB_(uint64_t vaddr) {
        auto tlb_entry = &tlb_cache_->getLineForReplacementWithInvalidCheck(vaddr);
        tlb_cache_->allocateWithMRUUpdate(*tlb_entry, vaddr);

        ILOG("TLB reload complete!");
    }

}
