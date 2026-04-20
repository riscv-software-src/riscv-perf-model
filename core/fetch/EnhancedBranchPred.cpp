#include "EnhancedBranchPred.hpp"

#include "ReplacementFactory.hpp"
#include "sparta/utils/SpartaAssert.hpp"

/*
 * High-level behavior:
 *   - Prediction: BTB supplies branch slot/target, BHT supplies direction.
 *   - Fallback: if BTB misses, predict straight-line fetch.
 *   - Training: once the branch resolves, score the pending prediction,
 *     update BHT direction state, then refresh BTB metadata.
 */

namespace
{
    bool isPowerOfTwo(const uint32_t value)
    {
        return value && ((value & (value - 1)) == 0);
    }
}

namespace olympia
{
namespace BranchPredictor
{

    EnhancedBranchPredictor::EnhancedBranchPredictor(uint32_t max_fetch_insts,
                                                     uint32_t btb_entries,
                                                     uint32_t btb_ways,
                                                     uint32_t bht_entries) :
        max_fetch_insts_(max_fetch_insts),
        btb_entries_(btb_entries),
        btb_ways_(btb_ways),
        btb_sets_(btb_entries_ / btb_ways_),
        bht_entries_(bht_entries),
        branch_history_table_(bht_entries_, 1)
    {
        sparta_assert(btb_entries_ > 0, "BTB entries must be > 0");
        sparta_assert(btb_ways_ > 0, "BTB ways must be > 0");
        sparta_assert(bht_entries_ > 0, "BHT entries must be > 0");
        sparta_assert((btb_entries_ % btb_ways_) == 0,
                      "BTB entries must be divisible by BTB ways");
        sparta_assert(isPowerOfTwo(btb_sets_), "BTB sets must be a power of two");
        sparta_assert(isPowerOfTwo(bht_entries_), "BHT entries must be a power of two");

        // Use 1KB pseudo-lines so each BTB entry maps to one cache line.
        btb_replacement_policy_ = olympia::ReplacementFactory::selectReplacementPolicy("LRU", btb_ways_);
        btb_cache_ = std::make_unique<sparta::cache::SimpleCache2<BTBCacheLine>>(
            btb_entries_,
            kBTBCacheLineBytes,
            kBTBCacheLineBytes,
            BTBCacheLine(),
            *btb_replacement_policy_);
    }

    uint64_t EnhancedBranchPredictor::btbCacheAddress_(uint64_t fetch_pc) const {
        // Convert instruction-addressed fetch PC into the cache-line address
        // expected by SimpleCache2 while preserving intended BTB indexing.
        return fetch_pc << (kBTBCacheLineShift - kCompressedIndexShift);
    }

    uint32_t EnhancedBranchPredictor::bhtIndex_(uint64_t fetch_pc) const {
        return static_cast<uint32_t>((fetch_pc >> 1) & (bht_entries_ - 1));
    }

    bool EnhancedBranchPredictor::btbLookup_(uint64_t fetch_pc, BTBEntry * entry) {
        auto cache_line = btb_cache_->peekLine(btbCacheAddress_(fetch_pc));
        if ((cache_line != nullptr) && cache_line->isValid()) {
            if (entry != nullptr) {
                *entry = cache_line->btb_entry;
            }
            // Keep replacement state hot for recently used BTB lines.
            btb_cache_->touchMRU(*cache_line);
            return true;
        }
        return false;
    }

    void EnhancedBranchPredictor::btbUpdate_(uint64_t fetch_pc, const BTBEntry & entry) {
        const auto btb_addr = btbCacheAddress_(fetch_pc);
        // One path for both invalid-line fill and victim replacement.
        auto & replacement_line = btb_cache_->getLineForReplacementWithInvalidCheck(btb_addr);
        btb_cache_->allocateWithMRUUpdate(replacement_line, btb_addr);
        replacement_line.btb_entry = entry;
    }

    bool EnhancedBranchPredictor::bhtPredict_(uint64_t fetch_pc) const {
        return branch_history_table_[bhtIndex_(fetch_pc)] > 1;
    }

    void EnhancedBranchPredictor::bhtUpdate_(uint64_t fetch_pc, bool actually_taken) {
        auto & counter = branch_history_table_[bhtIndex_(fetch_pc)];
        if (actually_taken) {
            counter = (counter == 3) ? 3 : counter + 1;
        } else {
            counter = (counter == 0) ? 0 : counter - 1;
        }
    }

    DefaultPrediction EnhancedBranchPredictor::getPrediction(const DefaultInput & input) {
        DefaultPrediction prediction;
        prediction.branch_idx = max_fetch_insts_;
        prediction.predicted_PC = input.fetch_PC + max_fetch_insts_ * bytes_per_inst;

        BTBEntry btb_entry;
        if (btbLookup_(input.fetch_PC, &btb_entry)) {
            prediction.branch_idx = btb_entry.branch_idx;
            if (bhtPredict_(input.fetch_PC)) {
                prediction.predicted_PC = btb_entry.predicted_PC;
            } else {
                prediction.predicted_PC = input.fetch_PC + prediction.branch_idx + bytes_per_inst;
            }
        }

        // Hold this until execute resolves the branch, then score accuracy.
        pending_predictions_[input.fetch_PC] = prediction;

        return prediction;
    }

    void EnhancedBranchPredictor::updatePredictor(const DefaultUpdate & update) {
        sparta_assert(update.branch_idx < max_fetch_insts_);

        const uint64_t actual_next_pc = update.actually_taken ?
            update.corrected_PC :
            (update.fetch_PC + update.branch_idx + bytes_per_inst);

        const auto pending_it = pending_predictions_.find(update.fetch_PC);
        if (pending_it != pending_predictions_.end()) {
            ++total_predictions_;
            const bool correct_prediction =
                (pending_it->second.branch_idx == update.branch_idx) &&
                (pending_it->second.predicted_PC == actual_next_pc);
            if (correct_prediction) {
                ++correct_predictions_;
            } else {
                ++mispredictions_;
            }
            pending_predictions_.erase(pending_it);
        }

        bhtUpdate_(update.fetch_PC, update.actually_taken);

        BTBEntry entry;
        const bool btb_hit = btbLookup_(update.fetch_PC, &entry);

        if (btb_hit) {
            entry.branch_idx = update.branch_idx;
            if (update.actually_taken) {
                entry.predicted_PC = update.corrected_PC;
            }
        } else {
            const uint64_t default_target = update.actually_taken ?
                update.corrected_PC :
                (update.fetch_PC + update.branch_idx + bytes_per_inst);
            entry = BTBEntry(update.branch_idx, default_target);
        }

        btbUpdate_(update.fetch_PC, entry);
    }

} // namespace BranchPredictor
} // namespace olympia