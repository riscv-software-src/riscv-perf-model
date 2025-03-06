#pragma once

#include <cstdint>
#include <cmath>
#include <map>
#include <stack>

namespace olympia
{
    namespace BranchPredictor
    {
        class PatternHistoryTable
        {
          public:
            PatternHistoryTable(uint32_t pht_size, uint8_t ctr_bits);

            void incrementCounter(uint32_t idx);
            void decrementCounter(uint32_t idx);
            uint8_t getPrediction(uint32_t idx);

          private:
            const uint32_t pht_size_;
            const uint8_t ctr_bits_;
            const uint8_t ctr_bits_val_;
            std::map<uint64_t, uint8_t> pht_;
        };

        class BranchTargetBuffer
        {
          public:
            BranchTargetBuffer(uint32_t btb_size);

            bool addEntry(uint64_t PC, uint64_t targetPC);
            bool removeEntry(uint64_t PC);
            uint64_t getPredictedPC(uint64_t PC);
            bool isHit(uint64_t PC);

          private:
            const uint32_t btb_size_;
            std::map<uint64_t, uint64_t> btb_;
        };

        class ReturnAddressStack
        {
          public:
            ReturnAddressStack(uint32_t ras_size);

            void pushAddress();
            uint64_t popAddress();

          private:
            const uint32_t ras_size_;
            std::stack<uint64_t> ras_;
        };

        class BasePredictor
        {
          public:
            BasePredictor(uint32_t pht_size, uint8_t ctr_bits, uint32_t btb_size,
                          uint32_t ras_size);

          private:
            PatternHistoryTable pattern_history_table;
            BranchTargetBuffer branch_target_buffer;
            ReturnAddressStack return_address_stack;
        };
    } // namespace BranchPredictor
} // namespace olympia