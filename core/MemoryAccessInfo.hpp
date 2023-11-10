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

    class LoadStoreInstInfo
    {
      public:
        enum class IssuePriority : std::uint16_t
        {
            HIGHEST = 0,
            __FIRST = HIGHEST,
            CACHE_RELOAD,   // Receive mss ack, waiting for cache re-access
            CACHE_PENDING,  // Wait for another outstanding miss finish
            MMU_RELOAD,     // Receive for mss ack, waiting for mmu re-access
            MMU_PENDING,    // Wait for another outstanding miss finish
            NEW_DISP,       // Wait for new issue
            LOWEST,
            NUM_OF_PRIORITIES,
            __LAST = NUM_OF_PRIORITIES
        };

        enum class IssueState : std::uint32_t
        {
            READY = 0,          // Ready to be issued
            __FIRST = READY,
            ISSUED,         // On the flight somewhere inside Load/Store Pipe
            NOT_READY,      // Not ready to be issued
            NUM_STATES,
            __LAST = NUM_STATES
        };

        LoadStoreInstInfo() = delete;
        LoadStoreInstInfo(const MemoryAccessInfoPtr & info_ptr) :
                                                                  mem_access_info_ptr_(info_ptr),
                                                                  rank_(IssuePriority::LOWEST),
                                                                  state_(IssueState::NOT_READY){}

        // This Inst pointer will act as one of the two portals to the Inst class
        // and we will use this pointer to query values from functions of Inst class
        const InstPtr & getInstPtr() const {
            return mem_access_info_ptr_->getInstPtr();
        }

        // This MemoryAccessInfo pointer will act as one of the two portals to the MemoryAccesInfo class
        // and we will use this pointer to query values from functions of MemoryAccessInfo class
        const MemoryAccessInfoPtr & getMemoryAccessInfoPtr() const {
            return mem_access_info_ptr_;
        }

        // This is a function which will be added in the SPARTA_ADDPAIRs API.
        uint64_t getInstUniqueID() const {
            const MemoryAccessInfoPtr &mem_access_info_ptr = getMemoryAccessInfoPtr();
            return mem_access_info_ptr == nullptr ? 0 : mem_access_info_ptr->getInstUniqueID();
        }

        void setPriority(const IssuePriority & rank) {
            rank_.setValue(rank);
        }

        const IssuePriority & getPriority() const {
            return rank_.getEnumValue();
        }

        void setState(const IssueState & state) {
            state_.setValue(state);
        }

        const IssueState & getState() const {
            return state_.getEnumValue();
        }


        bool isReady() const { return (getState() == IssueState::READY); }

        bool isRetired() const { return getInstPtr()->getStatus() == Inst::Status::RETIRED; }

        bool winArb(const LoadStoreInstInfoPtr & that) const
        {
            if (that == nullptr) {
                return true;
            }

            return (static_cast<uint32_t>(this->getPriority())
                    < static_cast<uint32_t>(that->getPriority()));
        }

        const LoadStoreInstIterator getIssueQueueIterator() const
        {
            return mem_access_info_ptr_->getIssueQueueIterator();
        }

        void setIssueQueueIterator(const LoadStoreInstIterator &iter){
            mem_access_info_ptr_->setIssueQueueIterator(iter);
        }

        const LoadStoreInstIterator & getReplayQueueIterator() const
        {
            return mem_access_info_ptr_->getReplayQueueIterator();
        }

        void setReplayQueueIterator(const LoadStoreInstIterator & iter)
        {
            mem_access_info_ptr_->setReplayQueueIterator(iter);
        }

        bool isInReadyQueue() const
        {
            return in_ready_queue_;
        }

        void setInReadyQueue(bool inReadyQueue)
        {
            in_ready_queue_ = inReadyQueue;
        }

        friend bool operator < (const LoadStoreInstInfoPtr &lhs, const LoadStoreInstInfoPtr &rhs) {
            return lhs->getInstUniqueID() < rhs->getInstUniqueID();
        }
      private:
        MemoryAccessInfoPtr mem_access_info_ptr_;
        sparta::State<IssuePriority> rank_;
        sparta::State<IssueState> state_;
        bool in_ready_queue_;
    };  // class LoadStoreInstInfo
    using LoadStoreInstInfoAllocator = sparta::SpartaSharedPointerAllocator<LoadStoreInstInfo>;

    inline std::ostream& operator<<(std::ostream& os,
                                     const olympia::LoadStoreInstInfo::IssuePriority& rank){
        switch(rank){
        case LoadStoreInstInfo::IssuePriority::HIGHEST:
            os << "(highest)";
            break;
        case LoadStoreInstInfo::IssuePriority::CACHE_RELOAD:
            os << "($_reload)";
            break;
        case LoadStoreInstInfo::IssuePriority::CACHE_PENDING:
            os << "($_pending)";
            break;
        case LoadStoreInstInfo::IssuePriority::MMU_RELOAD:
            os << "(mmu_reload)";
            break;
        case LoadStoreInstInfo::IssuePriority::MMU_PENDING:
            os << "(mmu_pending)";
            break;
        case LoadStoreInstInfo::IssuePriority::NEW_DISP:
            os << "(new_disp)";
            break;
        case LoadStoreInstInfo::IssuePriority::LOWEST:
            os << "(lowest)";
            break;
        case LoadStoreInstInfo::IssuePriority::NUM_OF_PRIORITIES:
            throw sparta::SpartaException("NUM_OF_PRIORITIES cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os,
                                     const olympia::LoadStoreInstInfo::IssueState& state){
        // Print instruction issue state
        switch(state){
        case LoadStoreInstInfo::IssueState::READY:
            os << "(ready)";
            break;
        case LoadStoreInstInfo::IssueState::ISSUED:
            os << "(issued)";
            break;
        case LoadStoreInstInfo::IssueState::NOT_READY:
            os << "(not_ready)";
            break;
        case LoadStoreInstInfo::IssueState::NUM_STATES:
            throw sparta::SpartaException("NUM_STATES cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::LoadStoreInstInfo & ls_info)
    {
        os << "lsinfo: "
           << "uid: "    << ls_info.getInstUniqueID()
           << " pri:"    << ls_info.getPriority()
           << " state: " << ls_info.getState();
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::LoadStoreInstInfoPtr & ls_info)
    {
        os << *ls_info;
        return os;
    }

};
