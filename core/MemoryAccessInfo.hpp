// <MemoryAccessInfo.hpp> -*- C++ -*-

#pragma once

#include "sparta/resources/Scoreboard.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"

#include "Inst.hpp"

namespace olympia {

    class MemoryAccessInfoPairDef;

    class MemoryAccessInfo {
    public:

        // The modeler needs to alias a type called
        // "SpartaPairDefinitionType" to the Pair Definition class of
        // itself
        using SpartaPairDefinitionType = MemoryAccessInfoPairDef;

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

        // Scoreboards
        using ScoreboardViews = std::array<std::unique_ptr<sparta::ScoreboardView>, core_types::N_REGFILES>;
        ScoreboardViews scoreboard_views_;

    };

    /*!
     * \class MemoryAccessInfoPairDef
     * \brief Pair Definition class of the Memory Access Information that flows through the example/CoreModel
     */

    // This is the definition of the PairDefinition class of MemoryAccessInfo.
    // This PairDefinition class could be named anything but it needs to inherit
    // publicly from sparta::PairDefinition templatized on the actual class MemoryAcccessInfo.
    class MemoryAccessInfoPairDef : public sparta::PairDefinition<MemoryAccessInfo> {
    public:

        // The SPARTA_ADDPAIRs APIs must be called during the construction of the PairDefinition class
        MemoryAccessInfoPairDef() : PairDefinition<MemoryAccessInfo>() {
            SPARTA_INVOKE_PAIRS(MemoryAccessInfo);
        }

        SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("DID",   &MemoryAccessInfo::getInstUniqueID),
                              SPARTA_ADDPAIR("valid", &MemoryAccessInfo::getPhyAddrStatus),
                              SPARTA_ADDPAIR("mmu",   &MemoryAccessInfo::getMMUState),
                              SPARTA_ADDPAIR("cache", &MemoryAccessInfo::getCacheState),
                              SPARTA_FLATTEN(&MemoryAccessInfo::getInstPtr))
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
