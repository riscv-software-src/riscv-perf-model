#pragma once

#include <map>
#include "BPTypes.hpp"

namespace olympia
{
    class PatternHistoryTable
    {
        public:
            PatternHistoryTable(uint32_t pht_size, uint8_t ctr_bits) : 
                pht_size_(pht_size), ctr_bits_(ctr_bits) 
                {
                    ctr_bits_val_ = pow(2, ctr_bits_);
                }

            // To increment and decrement within set bounds of ctr_bits
            void incrementCounter(uint32_t idx);
            void decrementCounter(uint32_t idx);
            uint8_t getPrediction(uint32_t idx);

        private:
            const uint32_t pht_size_;
            const uint8_t  ctr_bits_;
            const uint8_t  ctr_bits_val_;

            std::map<uint64_t, uint8_t> pattern_history_table_;
             
    };

    class BranchTargetBuffer
    {
        public:
            BranchTargetBuffer(uint32_t btb_size) : btb_size_(btb_size) {}

            bool isHit(uint64_t PC);
            uint64_t getPredictedPC(uint64_t PC);
            bool addEntry(uint64_t PC, uint64_t targetPC);
            bool removeEntry(uint64_t PC);

        private:
            const uint32_t btb_size_;
            std::map <uint64_t, uint64_t> branch_target_buffer_;
    };

    class ReturnAddressStack
    {
        public:
            ReturnAddressStack(uint32_t ras_size) : ras_size_(ras_size) {}

            // To bound the size of RAS
            bool pushAddress(uint64_t address);
            uint64_t popAddress();

        private:
            const uint32_t ras_size_;
            std::stack<uint64_t> return_address_stack_;

    };

    class BasePredictor
    {
        public:
            BasePredictor(uint32_t pht_size, uint8_t ctr_bits, 
                uint32_t btb_size, uint32_t ras_size) : 
                patternHistoryTable(pht_size, ctr_bits),
                branchTargetBuffer(bht_size),
                returnAddressStack(ras_size);
            {}

            PatternHistoryTable patternHistoryTable;
            BranchTargetBuffer  branchTargetBuffer;
            ReturnAddressStack  returnAddressStack;

            void receivedPredictionReq();
            PredictionOutput makePrediction(PredictionInput predInput);
            void handleJMP(PredictionInput predInput);
            void handleRET(PredictionInput predInput);
            void handleBRANCH(PredictionInput predInput);

    };
}
    
