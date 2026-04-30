#include "L2tlb.hpp"
#include <algorithm>

namespace olympia {

// Constructor
L2tlb::L2tlb(sparta::TreeNode* node, const L2tlbParameterSet* p) :
    sparta::Unit(node),
    mmu_type_(p->mmu_type),
    tlb_hit_latency_(p->tlb_hit_latency),
    ptw_latency_(p->ptw_latency),
    req_queue_size_(p->req_queue_size)
{
    in_l1mmu_req_.registerConsumerHandler(
        CREATE_SPARTA_HANDLER_WITH_DATA(L2tlb, inL1MmuReq_, MemoryAccessInfoPtr));

    // To achieve the requested round trip tlb hit latency we need to adjust the
    // lookup delay to account for miscellaneous additional internal delays in
    // the model
    const int misc_internal_delays = 3;
    lookup_delay_ = tlb_hit_latency_ - misc_internal_delays;

    sparta::StartupEvent(node,CREATE_SPARTA_HANDLER(L2tlb, initialSetup_));
}

void L2tlb::initialSetup_() {
    if (mmu_type_ == "Data") {
        tlb_cache_ = getContainer()->getParent()->getChild("dl2mmu")->getResourceAs<olympia::MMU>();
        sparta_assert (tlb_cache_ != nullptr, "error creating dmmu unit");
        tlb_page_size_ = tlb_cache_->getTlbPageSize();
        sparta_assert(sparta::utils::is_power_of_2(tlb_page_size_), "Data L2 TLB page size is not a power of 2");
        tlb_page_mask_ = ~(tlb_page_size_ - 1);

        ILOG("Data tlb_page_size : " << tlb_page_size_);
    }
    else if (mmu_type_ == "Inst") {
        tlb_cache_ = getContainer()->getParent()->getChild("il2mmu")->getResourceAs<olympia::MMU>();
        sparta_assert (tlb_cache_ != nullptr, "error creating immu unit");
        tlb_page_size_ = tlb_cache_->getTlbPageSize();
        sparta_assert(sparta::utils::is_power_of_2(tlb_page_size_), "Inst L2 TLB page size is not a power of 2");
        tlb_page_mask_ = ~(tlb_page_size_ - 1);

        ILOG("Inst tlb_page_size : " << tlb_page_size_);
    }
    else {
        sparta_assert(false, "Incorrect MMU Type!");
    }

    out_l1mmu_credits_.send(req_queue_size_);
}

void L2tlb::inL1MmuReq_(const MemoryAccessInfoPtr& trans) {
    // We can receive multiple requests per cycle
    ILOG("Received trans:[" << trans << "]");
    req_queue_.push_back(trans);
    uev_start_lookup_.schedule(sparta::Clock::Cycle(1));
}

void L2tlb::uevStartLookup_() {
    uev_start_lookup_once_per_cycle_checker_.check();

    // The purpose of this kind of awkward arrangement is to implement
    // a serializing delay pipe
    sparta_assert(!req_queue_.empty(), "Expected req_queue to not be empty");
    MemoryAccessInfoPtr trans = req_queue_.front();
    ILOG("Starting lookup trans:[" << trans << "]");
    ev_lookup_.preparePayload(trans)->schedule(lookup_delay_);
    req_queue_.pop_front();

    if (!req_queue_.empty()) {
        uev_start_lookup_.schedule(sparta::Clock::Cycle(1));
    }
}

void L2tlb::evLookup_(const MemoryAccessInfoPtr& trans) {
    ev_lookup_once_per_cycle_checker_.check();

    const auto vaddr = trans->getVAddr();
    const bool hit = tlb_cache_->lookup(vaddr);
    if (hit) {
        l2tlb_hits_++;
        ILOG("Hit in L2TLB trans:[" << trans << "]");
        resp_queue_.push_back(trans);
        uev_send_resp_.schedule(sparta::Clock::Cycle(1));
    } else {
        l2tlb_misses_++;
        ILOG("Miss in L2TLB trans:[" << trans << "]");
        ILOG("Starting PTW trans:[" << trans << "]")
        ev_ptw_.preparePayload(trans)->schedule(ptw_latency_);
    }
}

void L2tlb::evPtw_(const MemoryAccessInfoPtr& trans) {
    ev_ptw_once_per_cycle_checker_.check();

    const auto vaddr = trans->getVAddr();
    tlb_cache_->reload(vaddr);

    resp_queue_.push_back(trans);
    uev_send_resp_.schedule(sparta::Clock::Cycle(1));
}

void L2tlb::uevSendResp_() {
    uev_send_resp_once_per_cycle_checker_.check();

    sparta_assert(!resp_queue_.empty(), "Expected resp_queue to not be empty");
    MemoryAccessInfoPtr trans = resp_queue_.front();
    resp_queue_.pop_front();
    ILOG("Sending trans:[" << trans << "]");

    // Send the response
    out_l1mmu_resp_.send(trans);
    out_l1mmu_credits_.send(1);

    if (!resp_queue_.empty()) {
        uev_send_resp_.schedule(sparta::Clock::Cycle(1));
    }
}

} // namespace olympia
