// <EnhancedBranchPred.hpp> -*- C++ -*-

/*
 * EnhancedBranchPredictor models a bounded predictor with practical storage
 * limits:
 *   - BTB state lives in a set-associative SimpleCache2
 *   - Direction state lives in fixed-size 2-bit saturating BHT counters
 */
#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "cache/BasicCacheItem.hpp"
#include "cache/ReplacementIF.hpp"
#include "cache/SimpleCache2.hpp"
#include "sparta/utils/SpartaAssert.hpp"
#include "SimpleBranchPred.hpp"

namespace olympia
{
namespace BranchPredictor
{

    class EnhancedBranchPredictor : public BranchPredictorIF<DefaultPrediction, DefaultUpdate, DefaultInput>
    {
    public:
        static constexpr uint32_t kDefaultBTBEntries = 256;
        static constexpr uint32_t kDefaultBTBWays = 2;
        static constexpr uint32_t kDefaultBHTEntries = 512;

        explicit EnhancedBranchPredictor(uint32_t max_fetch_insts,
                                         uint32_t btb_entries = kDefaultBTBEntries,
                                         uint32_t btb_ways = kDefaultBTBWays,
                                         uint32_t bht_entries = kDefaultBHTEntries);

        DefaultPrediction getPrediction(const DefaultInput &) override;
        void updatePredictor(const DefaultUpdate &) override;

        uint64_t getTotalPredictions() const { return total_predictions_; }
        uint64_t getCorrectPredictions() const { return correct_predictions_; }
        uint64_t getMispredictions() const { return mispredictions_; }

    private:
        // One BTB cache line holds branch metadata for a single fetch PC tag.
        struct BTBCacheLine : public sparta::cache::BasicCacheItem
        {
            BTBEntry btb_entry;

            // Required by SimpleCache2
            void reset(uint64_t addr)
            {
                setValid(true);
                BasicCacheItem::setAddr(addr);
            }

            // Required by SimpleCache2
            void setValid(bool valid) { valid_ = valid; }

            // Required by BasicCacheSet
            bool isValid() const { return valid_; }

            // Required by SimpleCache2
            void setModified(bool) {}

            // Required by SimpleCache2
            bool read(uint64_t offset, uint32_t size, uint32_t * buf) const
            {
                (void) offset;
                (void) size;
                (void) buf;
                sparta_assert(false);
                return true;
            }

            // Required by SimpleCache2
            bool write(uint64_t offset, uint32_t size, uint32_t * buf) const
            {
                (void) offset;
                (void) size;
                (void) buf;
                sparta_assert(false);
                return true;
            }

        private:
            bool valid_ = false;
        };

        // SimpleCache2 uses 1KB pseudo-lines here; shift fetch PC so BTB
        // indexing keeps PC[9:1] behavior (important for compressed support).
        uint64_t btbCacheAddress_(uint64_t fetch_pc) const;
        uint32_t bhtIndex_(uint64_t fetch_pc) const;

        bool btbLookup_(uint64_t fetch_pc, BTBEntry * entry);
        void btbUpdate_(uint64_t fetch_pc, const BTBEntry & entry);
        bool bhtPredict_(uint64_t fetch_pc) const;
        void bhtUpdate_(uint64_t fetch_pc, bool actually_taken);

        static constexpr uint32_t kCompressedIndexShift = 1;
        static constexpr uint32_t kBTBCacheLineShift = 10;
        static constexpr uint32_t kBTBCacheLineBytes = 1u << kBTBCacheLineShift;
        static_assert(kBTBCacheLineShift >= kCompressedIndexShift,
                      "BTB cache line shift must be >= compressed index shift");

        const uint32_t max_fetch_insts_;
        const uint32_t btb_entries_;
        const uint32_t btb_ways_;
        const uint32_t btb_sets_;
        const uint32_t bht_entries_;
        std::unique_ptr<sparta::cache::ReplacementIF> btb_replacement_policy_;
        std::unique_ptr<sparta::cache::SimpleCache2<BTBCacheLine>> btb_cache_;
        std::vector<uint8_t> branch_history_table_;
        std::unordered_map<uint64_t, DefaultPrediction> pending_predictions_;
        uint64_t total_predictions_ = 0;
        uint64_t correct_predictions_ = 0;
        uint64_t mispredictions_ = 0;
    };

} // namespace BranchPredictor
} // namespace olympia