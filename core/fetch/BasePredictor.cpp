#include "BasePredictor.hpp"

#define CALL 0
#define RET 1
#define BRANCH 2

namespace olympia
{
    namespace BranchPredictor
    {
        BasePredictor::BasePredictor(const uint32_t & pht_size, const uint8_t & pht_ctr_bits, const uint32_t & btb_size,
                                     const uint32_t & ras_size, const bool & ras_enable_overwrite) :
            pht_size_(pht_size),
            pht_ctr_bits_(pht_ctr_bits),
            pht_ctr_max_val_(1 << pht_ctr_bits_),
            btb_size_(btb_size),
            ras_size_(ras_size),
            ras_enable_overwrite_(ras_enable_overwrite)
        {
            for (uint32_t i = 0; i < pht_size_; i++)
            {
                pattern_history_table_[i] = 0;
            }
            for (uint32_t i = 0; i < btb_size; i++)
            {
                branch_target_buffer_[i] = 0;
            }
        }

        bool BasePredictor::getDirection(const uint64_t & PC, const uint8_t & instType)
        {
            std::cout << "ggggetting direction from base predictor\n";
            // branch taken or not-taken?

            // hardcoding values for each type for now
            // instType_ = 0 -> call
            // instType_ = 1 -> ret
            // instType_ = 2 -> conditional branch

            // if instruction type is call/jump or ret branch is always taken
            if (instType == 0 || instType == 1)
            {
                return true;
            }
            else
            {
                return branchTaken(PC);
            }
        }

        uint64_t BasePredictor::getTarget(const uint64_t & PC, const uint8_t & instType)
        {
            std::cout << "gggggetting pc from base predictor\n";
            // target PC

            // hardcoding values for each type for now
            // instType_ = 0 -> call
            // instType_ = 1 -> ret
            // instType_ = 2 -> conditional branch

            // if call -> instruction then save current pc to ctr_max_val_RAS
            // if ret -> pop pc from RAS
            // if conditonal branch -> use BTB
            uint64_t targetPC = 0;
            if (instType == 0)
            {
                pushAddress(PC);
                if (isHit(PC))
                {
                    targetPC = getTargetPC(PC, instType);
                }
                else
                {
                    targetPC = PC + 8;
                }
            }
            else if (instType == RET)
            {
                targetPC = popAddress();
            }
            else
            {
                targetPC = getTargetPC(PC, instType);
            }
            return targetPC;
        }

        // done
        void BasePredictor::incrementCtr(const uint32_t & idx)
        {
            if (pattern_history_table_.find(idx) != pattern_history_table_.end())
            {
                if (pattern_history_table_[idx] < pht_ctr_max_val_)
                {
                    pattern_history_table_[idx]++;
                }
            }
        }

        // done
        void BasePredictor::decrementCtr(const uint32_t & idx)
        {
            if (pattern_history_table_.find(idx) != pattern_history_table_.end())
            {
                if (pattern_history_table_[idx] > 0)
                {
                    pattern_history_table_[idx]--;
                }
            }
        }

        // done
        uint8_t BasePredictor::getCtr(const uint32_t & idx)
        {
            if (pattern_history_table_.find(idx) != pattern_history_table_.end())
            {
                return pattern_history_table_[idx];
            }
            else
            {
                return 0;
            }
        }

        // done
        bool BasePredictor::branchTaken(const uint32_t & idx)
        {
            uint8_t ctr = getCtr(idx);
            if (ctr > pht_ctr_max_val_ / 2)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        // done
        bool BasePredictor::addEntry(const uint64_t & PC, const uint64_t & targetPC)
        {
            if (branch_target_buffer_.size() < btb_size_)
            {
                branch_target_buffer_[PC] = targetPC;
                return true;
            }
            return false;
        }

        // done
        bool BasePredictor::isHit(const uint64_t & PC)
        {
            if (branch_target_buffer_.find(PC) != branch_target_buffer_.end())
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        // i think good enough for review
        uint64_t BasePredictor::getTargetPC(const uint64_t & PC, const uint8_t & instType)
        {
            uint64_t targetPC = PC + 8;
            if (isHit(PC))
            {
                if (instType == CALL)
                {
                    targetPC = branch_target_buffer_[PC];
                }
                else if (instType == BRANCH)
                {
                    if (branchTaken(PC))
                    {
                        targetPC = branch_target_buffer_[PC];
                    }
                    else
                    {
                        branch_target_buffer_.erase(PC);
                        targetPC = PC + 8;
                    }
                }
            }
            else
            {
                if (instType == CALL)
                {
                    // TODO: put something random and
                    // rely on update to correct it later?
                    addEntry(PC, PC + 8);
                    targetPC = PC + 8;
                }
                else if (instType == BRANCH)
                {
                    if (branchTaken(PC))
                    {
                        // TODO: put something random and
                        // rely on update to correct it later?
                        addEntry(PC, PC + 8);
                    }
                    else
                    {
                        targetPC = PC + 8;
                    }
                }
            }
            return targetPC;
        }

        // i think done
        void BasePredictor::pushAddress(const uint64_t & PC)
        {
            if (return_address_stack_.size() < ras_size_)
            {
                return_address_stack_.push_front(PC);
            }
            else if (ras_enable_overwrite_)
            {
                // oldest entry of RAS is overwritten to make space for new entry
                return_address_stack_.pop_back();
                return_address_stack_.push_front(PC);
            }
            else
            {
                return;
            }
        }

        // i think done for now with TODO left
        uint64_t BasePredictor::popAddress()
        {
            if (return_address_stack_.size() > 0)
            {
                uint64_t address = return_address_stack_.front();
                return_address_stack_.pop_front();
                return address;
            }
            // TODO: what to return when ras is empty but popAddress() is
            // inocrrectly called?
            return 0;
        }
    } // namespace BranchPredictor
} // namespace olympia