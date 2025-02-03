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

        BranchTargetBuffer::BranchTargetBuffer(uint32_t btb_size) : btb_size_(btb_size) {}

        ReturnAddressStack::ReturnAddressStack(uint32_t ras_size) : ras_size_(ras_size) {}
    } // namespace BranchPredictor
} // namespace olympia