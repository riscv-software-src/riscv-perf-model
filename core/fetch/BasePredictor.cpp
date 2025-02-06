#include "BasePredictor.hpp"

namespace olympia
{
    namespace BranchPredictor
    {
        BasePredictor::BasePredictor(uint32_t pht_size, uint8_t ctr_bits, uint32_t btb_size,
                                     uint32_t ras_size) :
            pattern_history_table(pht_size, ctr_bits),
            branch_target_buffer(btb_size),
            return_address_stack(ras_size)
        {
        }

        PatternHistoryTable::PatternHistoryTable(uint32_t pht_size, uint8_t ctr_bits) :
            pht_size_(pht_size),
            ctr_bits_(ctr_bits),
            ctr_bits_val_(pow(2, ctr_bits))
        {
        }

        void PatternHistoryTable::incrementCounter(uint32_t idx)
        {
            if(pht_.find(idx) != pht_.end()) {
                if(pht_[idx] < ctr_bits_val_) {
                    pht_[idx]++;
                }
            }
        }

        void PatternHistoryTable::decrementCounter(uint32_t idx)
        {
            if(pht_.find(idx) != pht_.end()) {
                if(pht_[idx] > 0) {
                    pht_[idx]--;
                }
            }
        }

        uint8_t PatternHistoryTable::getPrediction(uint32_t idx)
        {
            if(pht_.find(idx) != pht_.end()) {
                return pht_[idx];
            }
            else {
                return 0;
            }
        }

        BranchTargetBuffer::BranchTargetBuffer(uint32_t btb_size) : btb_size_(btb_size) {}

        bool BranchTargetBuffer::addEntry(uint64_t PC, uint64_t targetPC)
        {
            if(btb_.size() < btb_size_) {
                btb_[PC] = targetPC;
                return true;
            }
            return false;
        }

        bool BranchTargetBuffer::removeEntry(uint64_t PC)
        {
            return btb_.erase(PC);
        }

        uint64_t BranchTargetBuffer::getPredictedPC(uint64_t PC)
        {
            if(isHit(PC)) {
                return btb_[PC];
            }
            else {
                return 0; // change it later
            }
        }

        bool BranchTargetBuffer::isHit(uint64_t PC)
        {
            if(btb_.find(PC) != btb_.end()) {
                return true;
            }
            else {
                return false;
            }
        }

        ReturnAddressStack::ReturnAddressStack(uint32_t ras_size) : ras_size_(ras_size) {}

        void ReturnAddressStack::pushAddress()
        {

        }

        uint64_t ReturnAddressStack::popAddress()
        {
            return 0;
        }
    } // namespace BranchPredictor
} // namespace olympia