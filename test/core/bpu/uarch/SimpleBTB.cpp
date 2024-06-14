
#include "uarch/SimpleBTB.hpp"

namespace olympia
{

constexpr char SimpleBTB::name[];

SimpleBTB::SimpleBTB(sparta::TreeNode* node,
                     const SimpleBTBParameterSet* p)
    : sparta::Unit(node),

      pred_enable_(p->pred_enable),
      num_entries_(p->num_entries),
      tag_bits_(p->tag_bits),
      inst_shift_amount_(p->inst_shift_amount),
      num_threads_(p->num_threads),

      simple_btb_lookups_(&unit_stat_set_,
        "simple_btb_lookups_", "BTB access counter",
        sparta::Counter::COUNT_NORMAL),

      simple_btb_updates_(&unit_stat_set_,
        "simple_btb_updates_", "BTB update counter",
        sparta::Counter::COUNT_NORMAL),

      simple_btb_misses_(&unit_stat_set_,
        "simple_btb_misses_", "BTB miss access counter",
        sparta::Counter::COUNT_NORMAL)
{
  if(pred_enable_)
  {
      sparta_assert(num_threads_ == 1,
                    "num_threads must be 1 in this version");

      sparta_assert(num_entries_ != 0,
                    "num_entries can not be zero");

      sparta_assert(isPowerOfTwo(num_entries_),
                    "num_entries is not a power of 2");

      btb_.resize(num_entries_);

      index_mask_       = num_entries_ - 1;
      tag_mask_         = (1 << tag_bits_) - 1;
      tag_shift_amount_ = inst_shift_amount_ + floorLog2(num_entries_);
  }

  i_bpu_invalidate_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(SimpleBTB, handleBpuInvalidate,
                                            uint32_t));
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
void SimpleBTB::handleBpuInvalidate(const uint32_t & count)
{
//  if(!pred_enable_ || !inval) return;
//  memInvalidate();
}
// -------------------------------------------------------------------
// TODO BranchType is not used, if we need it the lookup/update/miss
// stats can be broken out by branch type.
// -------------------------------------------------------------------
void SimpleBTB::updateStats(const BranchType brType,
                            const BTBStat stat, const int32_t val)
{   
  if(brType == BranchType::NoBranch) return;

  sparta_assert(stat < BTBStat::NUM_BTBSTAT,
                "Unknown BTBStat type: "<<(uint32_t) stat);

  if(stat == BTBStat::LookUp)      simple_btb_lookups_ += val;
  else if(stat == BTBStat::Miss)   simple_btb_misses_  += val;
  else if(stat == BTBStat::Update) simple_btb_updates_ += val;
}  

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void SimpleBTB::memInvalidate()
{
    for (uint32_t i = 0; i < num_entries_; ++i)
    {
        btb_[i].valid = false;
    }
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
inline uint32_t SimpleBTB::getIndex(const Addr instPC,const ThreadID tid)
{
    // Need to shift PC over by the word offset.
    return ((instPC >> inst_shift_amount_)
      ^ (tid << (tag_shift_amount_ - inst_shift_amount_ - log2_num_threads_)))
      & index_mask_;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
inline SimpleBTB::Addr SimpleBTB::getTag(const Addr instPC)
{
    return (instPC >> tag_shift_amount_) & tag_mask_;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
SimpleBTB::BTBEntry *SimpleBTB::findEntry(const Addr instPC,const ThreadID tid)
{
    uint32_t btb_idx = getIndex(instPC, tid);

    sparta_assert(btb_idx < num_entries_,
                  "BTB index exceeds bounds of BTB entries");

    Addr inst_tag = getTag(instPC);


    if (btb_[btb_idx].valid
        && inst_tag == btb_[btb_idx].tag
        && btb_[btb_idx].tid == tid) {
        return &btb_[btb_idx];
    }

    return nullptr;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool SimpleBTB::valid(const ThreadID tid,const Addr instPC)
{
    BTBEntry *entry = findEntry(instPC, tid);
    return entry != nullptr;
}

// -------------------------------------------------------------------
// TODO Create some sort of return struct that has both whether or not the
// address is valid, and also the address.  For now will just use addr = 0 to
// represent invalid entry.
// -------------------------------------------------------------------
const SimpleBTB::PCStateBase *SimpleBTB::lookup(const ThreadID tid,
                                                const Addr instPC,
                                                const BranchType brType)
{
    updateStats(brType,BTBStat::LookUp,1);

    BTBEntry *entry = findEntry(instPC, tid);

    if (entry)
    {
        return entry->target.get();
    }

    updateStats(brType,BTBStat::Miss,1);

    return nullptr;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
const SimpleBTB::StaticInstPtr SimpleBTB::getInst(const ThreadID tid,
                                                  const Addr instPC)
{
    BTBEntry *entry = findEntry(instPC, tid);

    if (entry)
    {
        return entry->inst;
    }
    return nullptr;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void SimpleBTB::update(const ThreadID tid, const Addr instPC,
                       const PCStateBase &target,
                       const BranchType brType, const StaticInstPtr inst)
{
    uint32_t btb_idx = getIndex(instPC, tid);

    sparta_assert(btb_idx < num_entries_,
                  "BTB index exceeds bounds of BTB entries");

    updateStats(brType,BTBStat::Update,1);

    btb_[btb_idx].tid = tid;
    btb_[btb_idx].valid = true;
//    set(btb_[btb_idx].target, target);
    btb_[btb_idx].tag  = getTag(instPC);
    btb_[btb_idx].inst = inst;
}

}
