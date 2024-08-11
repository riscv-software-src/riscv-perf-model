#include "MMU.hpp"

namespace olympia
{
    const char MMU::name[] = "mmu";

    void MMU::setTLB(olympia::SimpleTLB & tlb) { tlb_cache_ = &tlb; }

    MMU::MMU(sparta::TreeNode* node, const MMUParameterSet* p) :
        sparta::Unit(node),
        tlb_always_hit_(p->tlb_always_hit),
        mmu_latency_(p->mmu_latency),
        busy_(false)
    {

        in_lsu_lookup_req_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(MMU, getInstsFromLSU_, LSPipelineRequest));
    }

    bool MMU::memLookup_(const MemoryAccessInfoPtr & mem_access_info_ptr)
    {
        const InstPtr & inst_ptr = mem_access_info_ptr->getInstPtr();
        uint64_t vaddr = inst_ptr->getTargetVAddr();

        bool tlb_hit = tlb_always_hit_;

        // C++ comma operator: assign tlb_hit first, then evaluate it. Just For Fun
        if (!tlb_hit)
        {
            auto tlb_entry = tlb_cache_->peekLine(vaddr);
            tlb_hit = (tlb_entry != nullptr) && tlb_entry->isValid();

            // Update MRU replacement state if TLB HIT
            if (tlb_hit)
            {
                tlb_cache_->touch(*tlb_entry);
            }
        }

        if (tlb_hit)
        {
            ILOG("TLB HIT: vaddr=0x" << std::hex << vaddr);
            tlb_hits_++;
        }
        else
        {
            ILOG("TLB MISS: vaddr=0x" << std::hex << vaddr);
            tlb_misses_++;
        }

        return tlb_hit;
    }

    // Reload TLB entry
    void MMU::reloadTLB_(uint64_t vaddr)
    {
        auto tlb_entry = &tlb_cache_->getLineForReplacementWithInvalidCheck(vaddr);
        tlb_cache_->allocateWithMRUUpdate(*tlb_entry, vaddr);

        ILOG("TLB reload complete!");
    }

    // Get Lookup Requests from LSU
    void MMU::getInstsFromLSU_(const LSPipelineRequest & ls_pipeline_req)
    {
        auto memory_access_info_ptr = ls_pipeline_req.first;

        const bool hit = memLookup_(memory_access_info_ptr);
        ILOG("MMU Lookup " << memory_access_info_ptr << " " << std::boolalpha << hit);
        if (hit)
        {
            memory_access_info_ptr->setMMUState(MemoryAccessInfo::MMUState::HIT);
            memory_access_info_ptr->setPhyAddrStatus(true);
        }
        else
        {
            memory_access_info_ptr->setMMUState(MemoryAccessInfo::MMUState::MISS);
            memory_access_info_ptr->setPhyAddrStatus(false);
            if (!busy_)
            {
                busy_ = true;
                mmu_pending_inst_ = memory_access_info_ptr;
                uev_lookup_inst_.schedule(sparta::Clock::Cycle(mmu_latency_));
            }
        }
        out_lsu_lookup_ack_.send(memory_access_info_ptr);
    }

    // TLB ready for memory access
    void MMU::lookupInst_()
    {
        busy_ = false;
        reloadTLB_(mmu_pending_inst_->getInstPtr()->getTargetVAddr());
        out_lsu_lookup_req_.send(mmu_pending_inst_);
    }

} // namespace olympia
