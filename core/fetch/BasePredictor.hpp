#pragma once

#include <cstdint>
#include <cmath>
#include <map>
#include <stack>
#include <vector>
#include <deque>

#include <iostream>

namespace olympia
{
    namespace BranchPredictor
    {
        class BasePredictor
        {
          public:
            BasePredictor(const uint32_t & pht_size, const uint8_t & pht_ctr_bits, const uint32_t & btb_size,
                          const uint32_t & ras_size, const bool & ras_enable_overwrite);

            bool getDirection(const uint64_t &, const uint8_t &);
            uint64_t getTarget(const uint64_t &, const uint8_t &);

            // PHT
            void incrementCtr(const uint32_t & idx);
            void decrementCtr(const uint32_t & idx);
            uint8_t getCtr(const uint32_t & idx);
            bool branchTaken(const uint32_t & idx);

            // BTB
            bool addEntry(const uint64_t & PC, const uint64_t & targetPC);
            bool isHit(const uint64_t & PC);
            uint64_t getTargetPC(const uint64_t & PC, const uint8_t & instType);

            // RAS
            void pushAddress(const uint64_t & PC);
            uint64_t popAddress();

          private:
            const uint32_t pht_size_;
            const uint8_t pht_ctr_bits_;
            const uint8_t pht_ctr_max_val_;
            const uint32_t btb_size_;
            const uint32_t ras_size_;
            // Boolean flag to set whether newer entries to RAS on maximum
            // capacity should overwrite or not.
            const bool ras_enable_overwrite_;

            std::map<uint64_t, uint8_t> pattern_history_table_;
            std::map<uint64_t, uint64_t> branch_target_buffer_;
            std::deque<uint64_t> return_address_stack_;
        };
    } // namespace BranchPredictor
} // namespace olympia