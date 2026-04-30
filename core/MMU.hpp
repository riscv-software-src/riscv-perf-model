#pragma once

#include "sparta/memory/AddressTypes.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"

#include "SimpleTLB.hpp"

namespace olympia {

class MMU : public sparta::Unit {

public:

    class MMUParameterSet : public sparta::ParameterSet {
    public:
        MMUParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

        PARAMETER(uint64_t, tlb_page_size_kb,      4, "Page size in KB (power of 2)")
        PARAMETER(uint64_t, tlb_num_entries,      32, "L1 TLB num entries (power of 2)")
        PARAMETER(uint64_t, tlb_associativity,    32, "L1 TLB associativity (power of 2)")
        PARAMETER(bool,     tlb_always_hit,    false, "L1 TLB will always hit")
    };

    MMU(sparta::TreeNode* node, const MMUParameterSet* p) :
        sparta::Unit(node),
        tlb_page_size_(p->tlb_page_size_kb * 1024),
        tlb_num_entries_(p->tlb_num_entries),
        tlb_always_hit_(p->tlb_always_hit),
        tlb_func_model_{p->tlb_page_size_kb * 1024, p->tlb_num_entries, p->tlb_associativity}
    {
        sparta_assert(p->tlb_num_entries >= p->tlb_associativity, "For the " << node->getName()
            << " the number of tlb entries cannot be less than the tlb associativity");
    };

    static constexpr char name[] = "mmu";

    uint64_t getTlbPageSize() const { return tlb_page_size_; }

    uint64_t getTlbNumEntries() const { return tlb_num_entries_; }

    bool lookup(sparta::memory::addr_t vaddr) {
        bool hit = false;
        if (tlb_always_hit_) {
            hit = true;
        } else {
            const auto* tlb_entry = tlb_func_model_.peekLine(vaddr);
            hit = (tlb_entry != nullptr) && tlb_entry->isValid();

            // Update MRU replacement state if hit
            if (hit) {
                tlb_func_model_.touchMRU(*tlb_entry);
            }
        }
        return hit;
    }

    void reload(sparta::memory::addr_t vaddr) {
        auto tlb_entry = &tlb_func_model_.getLineForReplacementWithInvalidCheck(vaddr);
        tlb_func_model_.allocateWithMRUUpdate(*tlb_entry, vaddr);
    }

private:
    const uint64_t tlb_page_size_;
    const uint64_t tlb_num_entries_;
    const bool tlb_always_hit_;
    SimpleTLB tlb_func_model_;
};

} // namespace olympia
