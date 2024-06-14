// HEADER PLACEHOLDER
// contact Jeff Nye
#pragma once

#include "Inst.hpp"

#include "sparta/ports/DataPort.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include "sparta/ports/DataPort.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/utils/LogUtils.hpp"

namespace olympia
{

class SimpleBTB : public sparta::Unit
//: public BranchTargetBuffer
{
    //TODO: share these across bpus if there ends up being more than one
    using Addr          = sparta::memory::addr_t;
    using ThreadID      = int16_t;
    using StaticInstPtr = InstPtr;
    using PCStateBase   = uint32_t; //TODO

    //! \brief ...
    //TODO: find the CAM/Mavis version of these
    enum BranchType
    {
        NoBranch       = 0,
        Return         = 1,
        CallDirect     = 2,
        CallIndirect   = 3,
        DirectCond     = 4,
        DirectUncond   = 5,
        IndirectCond   = 6,
        IndirectUncond = 7,
        NUM_BRANCHTYPE = 8
    };

    //! \brief ...
    enum BTBStat
    {
        Miss   = 0,
        LookUp = 1,
        Update = 2,
        NUM_BTBSTAT = 3
    };

public:
    //! \brief ...
    class SimpleBTBParameterSet : public sparta::ParameterSet
    {
      public:
        //! \brief ...
        SimpleBTBParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n)
        {
        }

        PARAMETER(bool, pred_enable, false, "Enable prediction")
        PARAMETER(uint32_t, num_entries, 128, "Number of BTB entries")
        PARAMETER(uint32_t, tag_bits,18,"Number of address tag bits used")
        PARAMETER(uint32_t, inst_shift_amount,2,"Address left shift")
        PARAMETER(uint32_t, num_threads,1,"Only =1 supported")
    };

    //! \brief ...
    SimpleBTB(sparta::TreeNode* node,const SimpleBTBParameterSet* p);

    //! \brief ...
    static constexpr char name[] = "simplebtb";

    //! \brief ...
    void  memInvalidate();

    //! \brief ...
    bool  valid(const ThreadID tid, const Addr instPC);

    //! \brief ...
    const PCStateBase *lookup(const ThreadID tid, const Addr instPC,
                              const BranchType type = BranchType::NoBranch);

    //! \brief ...
    void update(const ThreadID tid,
                const Addr instPC,
                const PCStateBase &target_pc,
                const BranchType type = BranchType::NoBranch,
                const StaticInstPtr inst = nullptr);

    //! \brief ...
    const InstPtr getInst(ThreadID tid, Addr instPC);

    //! \brief ...
    inline bool isPowerOfTwo(const uint32_t x) const
    {
        return x && !(x & (x - 1));
    }

    //! \brief ...
    uint32_t floorLog2(const uint32_t value) const
    {
        sparta_assert(value > 0,"Log2(0) is undefined");
        uint32_t tmp = value;
        uint32_t log = 0;
        while (tmp >>= 1) { log++; }
        return log;
    }

    //! \brief ...
    void updateStats(const BranchType,const BTBStat,const int32_t);

  private: //Ports
    // ------------------------------------------------------------------
    sparta::DataInPort<uint32_t> i_bpu_invalidate_{
        &unit_port_set_, "i_bpu_invalidate", 1};

  private: //Port handlers
    void handleBpuInvalidate(const uint32_t & count);

  private: //Structs
    // ------------------------------------------------------------------
    //! \brief ...
    struct BTBEntry
    {
        //! \brief The entry's tag.
        Addr tag = 0;

        //! \brief The entry's target.
        std::unique_ptr<PCStateBase> target;

        //! \brief The entry's thread id.
        ThreadID tid;

        //! \brief Whether or not the entry is valid.
        bool valid = false;

        //! \brief Pointer to the static branch instruction at this address
        StaticInstPtr inst = nullptr;
    };

  private: //Methods
    // ------------------------------------------------------------------
    //! \brief Returns the index into the BTB, based on the branch's PC.
    //!
    //! @param inst_PC The branch to look up.
    //! @return Returns the index into the BTB.
    inline unsigned getIndex(const Addr instPC, const ThreadID tid);

    //! \brief Returns the tag bits of a given address.
    //!
    //! @param inst_PC The branch's address.
    //! @return Returns the tag bits.
    inline Addr getTag(const Addr instPC);

    //! \brief Internal call to find an address in the BTB
    //!
    //!    @param instPC The branch's address.
    //!    @return Returns a pointer to the BTB entry if found, 
    //!            nullptr otherwise.
    BTBEntry *findEntry(const Addr instPC, const ThreadID tid);

    // ------------------------------------------------------------------
    // Parameters
    // ------------------------------------------------------------------

    //! \brief Parameter: master enable
    bool pred_enable_;

    //! \brief Parameter: the number of entries in the BTB.
    unsigned num_entries_;

    //! \brief Parameter: the number of tag bits per entry.
    unsigned tag_bits_;

    //! \brief Parameter: number of bits to shift PC when calculating index.
    unsigned inst_shift_amount_;

    //! \brief Parameter: number of program threads
    //!
    //! This is parameter for future generality. 
    //! TODO: at the moment this must be 1
    unsigned num_threads_{1};

    // ------------------------------------------------------------------
    // Counters/Stats
    // ------------------------------------------------------------------
    //! \brief track access to brb array
    sparta::Counter simple_btb_lookups_;

    //! \brief track updates to brb array
    sparta::Counter simple_btb_updates_;

    //! \brief track access misses to brb array
    sparta::Counter simple_btb_misses_;

    // ------------------------------------------------------------------
    //! \brief The actual BTB.
    std::vector<BTBEntry> btb_;

    //! \brief The index mask.
    unsigned index_mask_;

    //! \brief The tag mask.
    unsigned tag_mask_;

    //! \brief Number of bits to shift PC when calculating tag.
    unsigned tag_shift_amount_;

    //! \brief Log2 NumThreads used for hashing threadid
    //!
    //! This is a calculated value for future generality. 
    //! TODO: at the moment this must be 0
    unsigned log2_num_threads_{0};
};

}
