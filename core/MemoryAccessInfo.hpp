// <MemoryAccessInfo.hpp> -*- C++ -*-

#pragma once

#include "sparta/resources/Scoreboard.hpp"
#include "sparta/resources/Buffer.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"
#include "Inst.hpp"

namespace olympia {
    class LoadStoreInstInfo;
    using LoadStoreInstInfoPtr = sparta::SpartaSharedPointer<LoadStoreInstInfo>;
    using LoadStoreInstIterator = sparta::Buffer<LoadStoreInstInfoPtr>::const_iterator;
    class MemoryAccessInfo {
    public:

        enum class MMUState : std::uint32_t {
            NO_ACCESS = 0,
            __FIRST = NO_ACCESS,
            MISS,
            HIT,
            NUM_STATES,
            __LAST = NUM_STATES
        };

        enum class CacheState : std::uint64_t {
            NO_ACCESS = 0,
            __FIRST = NO_ACCESS,
            MISS,
            HIT,
            NUM_STATES,
            __LAST = NUM_STATES
        };

        MemoryAccessInfo() = delete;

        MemoryAccessInfo(
            const InstPtr &inst_ptr) :
            ldst_inst_ptr_(inst_ptr),
            phy_addr_ready_(false),
            mmu_access_state_(MMUState::NO_ACCESS),

            // Construct the State object here
            cache_access_state_(CacheState::NO_ACCESS),
            cache_data_ready_(false){}

        virtual ~MemoryAccessInfo() {}

        // This Inst pointer will act as our portal to the Inst class
        // and we will use this pointer to query values from functions of Inst class
        const InstPtr &getInstPtr() const { return ldst_inst_ptr_; }

        // This is a function which will be added in the SPARTA_ADDPAIRs API.
        uint64_t getInstUniqueID() const {
            const InstPtr &inst_ptr = getInstPtr();

            return inst_ptr == nullptr ? 0 : inst_ptr->getUniqueID();
        }

        void setPhyAddrStatus(bool is_ready) { phy_addr_ready_ = is_ready; }

        bool getPhyAddrStatus() const { return phy_addr_ready_; }

        MMUState getMMUState() const {
            return mmu_access_state_;
        }

        void setMMUState(const MMUState &state) {
            mmu_access_state_ = state;
        }

        CacheState getCacheState() const {
            return cache_access_state_;
        }

        void setCacheState(const CacheState &state) {
            cache_access_state_ = state;
        }

        bool isCacheHit() const {
            return cache_access_state_ == MemoryAccessInfo::CacheState::HIT;
        }

        bool isDataReady() const {
            return cache_data_ready_;
        }

        void setDataReady(bool is_ready) {
            cache_data_ready_ = is_ready;
        }
        const LoadStoreInstIterator getIssueQueueIterator() const
        {
            return issue_queue_iterator_;
        }

        void setIssueQueueIterator(const LoadStoreInstIterator &iter){
            issue_queue_iterator_ = iter;
        }

        const LoadStoreInstIterator & getReplayQueueIterator() const
        {
            return replay_queue_iterator_;
        }

        void setReplayQueueIterator(const LoadStoreInstIterator & iter)
        {
            replay_queue_iterator_ = iter;
        }
    private:
        // load/store instruction pointer
        InstPtr ldst_inst_ptr_;

        // Indicate MMU address translation status
        bool phy_addr_ready_;

        // MMU access status
        MMUState mmu_access_state_;

        // DCache access status
        CacheState cache_access_state_;

        bool cache_data_ready_;

        LoadStoreInstIterator issue_queue_iterator_;
        LoadStoreInstIterator replay_queue_iterator_;

    };

    using MemoryAccessInfoPtr       = sparta::SpartaSharedPointer<MemoryAccessInfo>;
    using MemoryAccessInfoAllocator = sparta::SpartaSharedPointerAllocator<MemoryAccessInfo>;

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::MemoryAccessInfo::CacheState & cache_access_state){
        switch(cache_access_state){
        case olympia::MemoryAccessInfo::CacheState::NO_ACCESS:
            os << "no_access";
            break;
        case olympia::MemoryAccessInfo::CacheState::MISS:
            os << "miss";
            break;
        case olympia::MemoryAccessInfo::CacheState::HIT:
            os << "hit";
            break;
        case olympia::MemoryAccessInfo::CacheState::NUM_STATES:
            throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::MemoryAccessInfo::MMUState & mmu_access_state){
        switch(mmu_access_state){
        case olympia::MemoryAccessInfo::MMUState::NO_ACCESS:
            os << "no_access";
            break;
        case olympia::MemoryAccessInfo::MMUState::MISS:
            os << "miss";
            break;
        case olympia::MemoryAccessInfo::MMUState::HIT:
            os << "hit";
            break;
        case olympia::MemoryAccessInfo::MMUState::NUM_STATES:
            throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::MemoryAccessInfo & mem)
    {
        os << "memptr: " << mem.getInstPtr();
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::MemoryAccessInfoPtr & mem_ptr)
    {
        os << *mem_ptr;
        return os;
    }


};
