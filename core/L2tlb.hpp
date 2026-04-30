#pragma once

#include <string>
#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/events/SingleCycleUniqueEvent.hpp"
#include "Inst.hpp"
#include "MMU.hpp"
#include "MemoryAccessInfo.hpp"
#include "OncePerCycleChecker.hpp"

////////////////////////////////////////////////////////////////////
// Unit model of L2TLB
////////////////////////////////////////////////////////////////////
namespace olympia {

class L2tlb : public sparta::Unit {
  public:
    class L2tlbParameterSet : public sparta::ParameterSet
    {
      public:
        L2tlbParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

        // Parameters for the l2TLB cache
        PARAMETER(std::string, mmu_type,        "Data", "Type of MMU. Valid values are 'Data' and 'Inst'.")
        PARAMETER(uint32_t,    tlb_hit_latency,      8, "Round trip TLB hit latency from requester send req to requester receive resp")
        PARAMETER(uint32_t,    ptw_latency,        100, "Additional latency for a TLB miss and page table walk")
        PARAMETER(uint32_t,    req_queue_size,       8, "Size of the request queue")
    };

    static constexpr char name[] = "l2tlb";

    L2tlb(sparta::TreeNode* node, const L2tlbParameterSet* p);

  private:
    using MemoryAccessInfoPtr = sparta::SpartaSharedPointer<MemoryAccessInfo>;

    // Parameters
    const std::string mmu_type_;
    const uint32_t tlb_hit_latency_;
    const uint32_t ptw_latency_;
    const uint32_t req_queue_size_;

    MMU* tlb_cache_{nullptr};
    uint64_t tlb_page_size_{0};
    uint64_t tlb_page_mask_{0};

    uint32_t lookup_delay_{0};

    std::deque<MemoryAccessInfoPtr> req_queue_;
    std::deque<MemoryAccessInfoPtr> resp_queue_;

    // Callbacks
    void inL1MmuReq_(const MemoryAccessInfoPtr& trans);
    void uevStartLookup_();
    void evLookup_(const MemoryAccessInfoPtr& trans);
    void evPtw_(const MemoryAccessInfoPtr& trans);
    void uevSendResp_();

    // Functions
    void initialSetup_() ;

    // Input Ports
    sparta::DataInPort<MemoryAccessInfoPtr> in_l1mmu_req_{&unit_port_set_, "in_l1mmu_req", 0};

    // Output Ports
    sparta::DataOutPort<uint32_t> out_l1mmu_credits_{&unit_port_set_, "out_l1mmu_credits"};
    sparta::DataOutPort<MemoryAccessInfoPtr> out_l1mmu_resp_{&unit_port_set_, "out_l1mmu_resp"};

    // Events
    sparta::SingleCycleUniqueEvent<> uev_start_lookup_{&unit_event_set_, "uev_start_lookup", CREATE_SPARTA_HANDLER(L2tlb, uevStartLookup_)};
    sparta::PayloadEvent<MemoryAccessInfoPtr> ev_lookup_ {&unit_event_set_, "ev_lookup", CREATE_SPARTA_HANDLER_WITH_DATA(L2tlb, evLookup_, MemoryAccessInfoPtr)};
    sparta::PayloadEvent<MemoryAccessInfoPtr> ev_ptw_ {&unit_event_set_, "ev_ptw", CREATE_SPARTA_HANDLER_WITH_DATA(L2tlb, evPtw_, MemoryAccessInfoPtr)};
    sparta::SingleCycleUniqueEvent<> uev_send_resp_{&unit_event_set_, "uev_send_resp", CREATE_SPARTA_HANDLER(L2tlb, uevSendResp_)};

     // Checkers
    OncePerCycleChecker uev_start_lookup_once_per_cycle_checker_{getClock()};
    OncePerCycleChecker ev_lookup_once_per_cycle_checker_{getClock()};
    OncePerCycleChecker ev_ptw_once_per_cycle_checker_{getClock()};
    OncePerCycleChecker uev_send_resp_once_per_cycle_checker_{getClock()};

    // Counters
    sparta::Counter l2tlb_hits_{getStatisticSet(), "l2tlb_hits",
                              "Number of L2TLB hits",
                              sparta::Counter::COUNT_NORMAL};
    sparta::Counter l2tlb_misses_{getStatisticSet(), "l2tlb_misses",
                                "Number of L2TLB misses",
                                sparta::Counter::COUNT_NORMAL};
};  // l2tlb class
} // namespace olympia
