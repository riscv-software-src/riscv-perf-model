
#pragma once

#include <iostream>
#include <cinttypes>
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/ValidValue.hpp"

#include "Inst.hpp"
#include "MemoryAccessInfo.hpp"

namespace olympia_mss {

class L23InstInfo;
using L23InstInfoPtr = sparta::SpartaSharedPointer<L23InstInfo>;

// Keep record of instruction issue information
class L23InstInfo {

    using L23MemAccess = olympia::MemoryAccessInfo;
    using L23MemAccessPtr = olympia::MemoryAccessInfoPtr;

public:

    enum class L23PipeState {
        NA = 0,
        __FIRST = NA,
        REQ_ISSUED,
        RELOAD_ISSUED,
        RELOAD_STARTED,
        MISS,
        HIT,
        STAT_PF_HIT,
        STAT_PF_MISS,
        COMPLETED,
        NUM_STATES,
        __LAST = NUM_STATES
    };

    enum class L23MrqState {
        NA = 0,
        __FIRST = NA,
        READY,
        ISSUED,
        MISS,
        HIT,
        NUM_STATES,
        __LAST = NUM_STATES
    };

    L23InstInfo() = delete;

    L23InstInfo(const L23MemAccessPtr& mem_access_ptr, const uint32_t& bank_id, const uint32_t& row_id)
                                            : bank_id_(bank_id),
                                              row_id_(row_id)
    {
        
        sparta_assert(mem_access_ptr, "Why is mem_access_ptr = null for this transaction?");
        setL23MemAccessPtr(mem_access_ptr);

        paddr_ = mem_access_ptr->getPAddr();
        
        if (mem_access_ptr_->getInstPtr() != nullptr) {
            inst_ptr_ = mem_access_ptr_->getInstPtr();
        }
    }

    const olympia::InstPtr & getInstPtr() const { return inst_ptr_; }

    const L23MemAccessPtr& getMemAccessPtr() const { return mem_access_ptr_; }
    void setL23MemAccessPtr(const L23MemAccessPtr& mem_access_ptr) { mem_access_ptr_ = mem_access_ptr; }

    uint64_t getInstUniqueID() const { return inst_ptr_ ? inst_ptr_->getUniqueID() : 0; }

    sparta::memory::addr_t getPAddr() const { return paddr_; }
    sparta::memory::addr_t getCacheLinePAddr(uint64_t line_mask) const { return line_mask & paddr_; }

    void setL23PipeState(const L23PipeState& l23_state) { l23_state_ = l23_state; }
    const L23PipeState& getL23PipeState() const { return l23_state_; }

    void setL23MrqState(const L23MrqState& l23_mrq_state) { l23_miss_pending_buffer_state_ = l23_mrq_state; }
    const L23MrqState& getL23MrqState() const { return l23_miss_pending_buffer_state_; }

    void setL23InstBankID(const uint32_t& bank_id) { bank_id_ = bank_id; }
    const uint32_t& getL23InstBankID() const { return bank_id_; }

    void setL23InstRowID(const uint32_t& row_id) { row_id_ = row_id; }
    const uint32_t& getL23InstRowID() const { return row_id_; }

    void setPrevMrqReq(const L23InstInfoPtr& prev_mrq_req) { prev_mrq_req_ = prev_mrq_req; }
    const L23InstInfoPtr& getPrevMrqReq() const { return prev_mrq_req_; }

    void setNextMrqReq(const L23InstInfoPtr& next_mrq_req) { next_mrq_req_ = next_mrq_req; }
    const L23InstInfoPtr& getNextMrqReq() const { return next_mrq_req_; }

private:
    olympia::InstPtr inst_ptr_;
    sparta::memory::addr_t paddr_;
    L23MemAccessPtr mem_access_ptr_;
    L23PipeState l23_state_{L23PipeState::NA};
    L23MrqState l23_miss_pending_buffer_state_{L23MrqState::NA};
    uint32_t bank_id_;
    uint32_t row_id_;

    // Tracking MRQ linked list
    L23InstInfoPtr prev_mrq_req_ = nullptr;
    L23InstInfoPtr next_mrq_req_ = nullptr;
};  // class L23InstInfo

inline std::ostream & operator<<(std::ostream & os,
                                 const L23InstInfo::L23PipeState & l23_cache_state)
{
    switch (l23_cache_state) {
    case L23InstInfo::L23PipeState::NA:                  os << "na";   break;
    case L23InstInfo::L23PipeState::REQ_ISSUED:          os << "req_issued";   break;
    case L23InstInfo::L23PipeState::MISS:                os << "miss";   break;
    case L23InstInfo::L23PipeState::HIT:                 os << "hit";   break;
    case L23InstInfo::L23PipeState::STAT_PF_HIT:         os << "stat_pf_hit";   break;
    case L23InstInfo::L23PipeState::STAT_PF_MISS:        os << "stat_pf_miss";   break;
    case L23InstInfo::L23PipeState::RELOAD_ISSUED:       os << "reload_issued";   break;
    case L23InstInfo::L23PipeState::RELOAD_STARTED:      os << "reload_started";   break;
    case L23InstInfo::L23PipeState::COMPLETED:           os << "completed";   break;
    case L23InstInfo::L23PipeState::NUM_STATES:
            throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
    }
    return os;
}

inline std::ostream & operator<<(std::ostream & os,
                                 const L23InstInfo::L23MrqState & l23_mrq_state)
{
    switch (l23_mrq_state) {
    case L23InstInfo::L23MrqState::NA:           os << "na";   break;
    case L23InstInfo::L23MrqState::READY:         os << "ready";   break;
    case L23InstInfo::L23MrqState::ISSUED:         os << "issued";   break;
    case L23InstInfo::L23MrqState::MISS:         os << "miss";   break;
    case L23InstInfo::L23MrqState::HIT:          os << "hit";   break;
    case L23InstInfo::L23MrqState::NUM_STATES:
            throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
    }
    return os;
}


inline std::ostream & operator<<(std::ostream& os,
                                 const L23InstInfo & ls_info)
{
    os << " mem_access:"               << ls_info.getMemAccessPtr()
       << " l23_cache_state:"           << ls_info.getL23PipeState()
       << " l23_mrq_state:"             << ls_info.getL23MrqState()
       << " l23_bank_id:"               << ls_info.getL23InstBankID()
       << " l23_row_id:"                << ls_info.getL23InstRowID();
    return os;
}

inline std::ostream & operator<<(std::ostream& os,
                                 const L23InstInfoPtr & ls_info_ptr)
{
    if (ls_info_ptr) {
        os << *ls_info_ptr;
    } else {
        os << "nullptr";
    }
    return os;
}

// L23InstInfo allocators
using L23InstInfoAllocator = sparta::SpartaSharedPointerAllocator<L23InstInfo>;
extern L23InstInfoAllocator l23_inst_info_allocator;

} // namespace olympia_mss
