#pragma once

#include "MemoryAccessInfo.hpp"

#include "sparta/simulation/State.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"

#include <cinttypes>
#include <string>

namespace olympia
{
    // Forward declaration of the Pair Definition class is must as we
    // are friending it.
    class LoadStoreInstInfoPair;

    class LoadStoreInstInfo
    {
      public:
        enum class IssuePriority : std::uint16_t
        {
            HIGHEST = 0,
            __FIRST = HIGHEST,
            CACHE_RELOAD,  // Receive mss ack, waiting for cache re-access
            CACHE_PENDING, // Wait for another outstanding miss finish
            MMU_RELOAD,    // Receive for mss ack, waiting for mmu re-access
            MMU_PENDING,   // Wait for another outstanding miss finish
            NEW_DISP,      // Wait for new issue
            LOWEST,
            NUM_OF_PRIORITIES,
            __LAST = NUM_OF_PRIORITIES
        };

        enum class IssueState : std::uint32_t
        {
            READY = 0, // Ready to be issued
            __FIRST = READY,
            ISSUED,    // On the flight somewhere inside Load/Store Pipe
            NOT_READY, // Not ready to be issued
            NUM_STATES,
            __LAST = NUM_STATES
        };

        // The modeler needs to alias a type called
        // "SpartaPairDefinitionType" to the Pair Definition class of
        // itself
        using SpartaPairDefinitionType = LoadStoreInstInfoPair;

        LoadStoreInstInfo() = delete;

        LoadStoreInstInfo(const MemoryAccessInfoPtr & info_ptr) :
            mem_access_info_ptr_(info_ptr),
            rank_(IssuePriority::LOWEST),
            state_(IssueState::NOT_READY)
        {
        }

        // This Inst pointer will act as one of the two portals to the Inst class
        // and we will use this pointer to query values from functions of Inst class
        const InstPtr & getInstPtr() const { return mem_access_info_ptr_->getInstPtr(); }

        // This MemoryAccessInfo pointer will act as one of the two portals to the MemoryAccesInfo
        // class and we will use this pointer to query values from functions of MemoryAccessInfo
        // class
        const MemoryAccessInfoPtr & getMemoryAccessInfoPtr() const { return mem_access_info_ptr_; }

        // This is a function which will be added in the SPARTA_ADDPAIRs API.
        uint64_t getInstUniqueID() const
        {
            const MemoryAccessInfoPtr & mem_access_info_ptr = getMemoryAccessInfoPtr();
            return mem_access_info_ptr == nullptr ? 0 : mem_access_info_ptr->getInstUniqueID();
        }

        // Get the mnemonic of the instruction this load/store is
        // associated.  Will return <unassoc> if not associated
        std::string getMnemonic() const {
            return (mem_access_info_ptr_ != nullptr ?
                    mem_access_info_ptr_->getMnemonic() : "<unassoc>");
        }

        void setPriority(const IssuePriority & rank) { rank_.setValue(rank); }

        const IssuePriority & getPriority() const { return rank_.getEnumValue(); }

        void setState(const IssueState & state) { state_.setValue(state); }

        const IssueState & getState() const { return state_.getEnumValue(); }

        bool isReady() const { return (getState() == IssueState::READY); }

        bool isRetired() const { return getInstPtr()->getStatus() == Inst::Status::RETIRED; }

        bool winArb(const LoadStoreInstInfoPtr & that) const
        {
            if (that == nullptr)
            {
                return true;
            }

            return (static_cast<uint32_t>(this->getPriority())
                    < static_cast<uint32_t>(that->getPriority()));
        }

        const LoadStoreInstIterator getIssueQueueIterator() const
        {
            return mem_access_info_ptr_->getIssueQueueIterator();
        }

        void setIssueQueueIterator(const LoadStoreInstIterator & iter)
        {
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

        bool isInReadyQueue() const { return in_ready_queue_; }

        void setInReadyQueue(bool inReadyQueue) { in_ready_queue_ = inReadyQueue; }

        friend bool operator<(const LoadStoreInstInfoPtr & lhs, const LoadStoreInstInfoPtr & rhs)
        {
            return lhs->getInstUniqueID() < rhs->getInstUniqueID();
        }

      private:
        MemoryAccessInfoPtr mem_access_info_ptr_;
        sparta::State<IssuePriority> rank_;
        sparta::State<IssueState> state_;
        bool in_ready_queue_;
    }; // class LoadStoreInstInfo

    using LoadStoreInstInfoAllocator = sparta::SpartaSharedPointerAllocator<LoadStoreInstInfo>;

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::LoadStoreInstInfo::IssuePriority & rank)
    {
        switch (rank)
        {
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

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::LoadStoreInstInfo::IssueState & state)
    {
        // Print instruction issue state
        switch (state)
        {
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

    inline std::ostream & operator<<(std::ostream & os, const olympia::LoadStoreInstInfo & ls_info)
    {
        os << "lsinfo: "
           << "uid: " << ls_info.getInstUniqueID() << " pri:" << ls_info.getPriority()
           << " state: " << ls_info.getState();
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os,
                                     const olympia::LoadStoreInstInfoPtr & ls_info)
    {
        os << *ls_info;
        return os;
    }

    /*!
     * \class LoadStoreInstInfoPair
     * \brief Pair Definition class of the LoadStoreInstInfo for pipeout collection
     *
     * This is the definition of the PairDefinition class of LoadStoreInstInfo.
     * It's mostly used for pipeline collection (-z option).  This
     * PairDefinition class could be named anything but it needs to
     * inherit publicly from sparta::PairDefinition templatized on the
     * actual class LoadStoreInstInfo.
     */
    class LoadStoreInstInfoPair : public sparta::PairDefinition<LoadStoreInstInfo>
    {
    public:

        // The SPARTA_ADDPAIRs APIs must be called during the construction of the PairDefinition class
        LoadStoreInstInfoPair() : sparta::PairDefinition<LoadStoreInstInfo>() {
            SPARTA_INVOKE_PAIRS(LoadStoreInstInfo);
        }
        SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("DID",       &LoadStoreInstInfo::getInstUniqueID),  // Used by Argos to color code
                              SPARTA_ADDPAIR("uid",       &LoadStoreInstInfo::getInstUniqueID),
                              SPARTA_ADDPAIR("mnemonic",  &LoadStoreInstInfo::getMnemonic),
                              SPARTA_ADDPAIR("pri:",      &LoadStoreInstInfo::getPriority),
                              SPARTA_ADDPAIR("state",     &LoadStoreInstInfo::getState))
    };


} // namespace olympia
