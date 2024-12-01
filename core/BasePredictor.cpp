//
// Created by shobhit on 12/1/24.
//

#include "BasePredictor.hpp"

namespace olympia
{
// PHT
void PatternHistoryTable::incrementCounter(uint32_t idx) {
    if(pattern_history_table_[idx] == ctr_bits_val_) {
        return;
    }
    else {
        pattern_history_table_[idx]++;
    }
}

void PatternHistoryTable::decrementCounter(uint32_t idx) {
    if(pattern_history_table_[idx] == 0) {
        return;
    }
    else {
        pattern_history_table_[idx]--;
    }
}

uint8_t PatternHistoryTable::getPrediction(uint32_t idx) {
    return pattern_history_table_[idx];
}

// BTB
uint64_t BranchTargetBuffer::getPredictedPC(uint64_t PC) {
    if(branch_target_buffer_.count(PC)) {
        return branch_target_buffer_[PC];
    }
    else {
        return 0; // recheck what to return in this condition
    }

}

bool BranchTargetBuffer::addEntry(uint64_t PC, uint64_t targetPC) {
    if(branch_target_buffer_.size() == btb_size_) {
        return false;
    }
    else {
        branch_target_buffer_.insert(std::pair<uint64_t, uint64_t>(PC, targetPC) );
        return true;
    }
}

bool BranchTargetBuffer::removeEntry(uint64_t PC) {
    if(branch_target_buffer_.count(PC)) {
        branch_target_buffer_.erase(PC);
        return true;
    }
    else {
        return false;
    }
}

// RAS
bool ReturnAddressStack::pushAddress(uint64_t address) {
    if(return_address_stack_.size() == ras_size_) {
        return false;
    }
    else {
        return_address_stack_.push(address);
        return true;
    }
}

uint64_t ReturnAddressStack::popAddress() {
    if(return_address_stack_.size() == 0) {
        return 0; // recheck what to return in this condition
    }
    else {
        uint64_t address_ = return_address_stack_.top();
        return_address_stack_.pop();
        return address_;
    }
}

// BasePredictor
void BasePredictor::handlePredictionReq() {
    return;
}

PredictionOutput BasePredictor::makePrediction(PredictionInput predInput) {

    PredictionOutput predOutput;

    // branch instruction
    if(predInput.instType == 1) {
        predOutput.predDirection = false;
        predOutput.predPC = 5;
    }
    // call instructions
    //else if(predInput.instType == 2) {
        // in this case no prediction is made, only data is stored for future reference
    //}
    // ret instruction
    else if(predInput.instType == 3) {
        predOutput.predDirection = true;
        predOutput.predPC = returnAddressStack.popAddress();
    }
    return predOutput;
}
}
