#include "BasePredictor.hpp"

namespace olympia
{
// Pattern History Table
void PatternHistoryTable::incrementCounter(uint32_t idx) {
    if(pattern_history_table_.at(idx) == ctr_bits_val_) {
        return;
    }
    else {
        pattern_history_table_.at(idx)++;
    }
}

void PatternHistoryTable::decrementCounter(uint32_t idx) {
    if(pattern_history_table_.at(idx) == 0) {
        return;
    }
    else {
        pattern_history_table_.at(idx)--;
    }
}

uint8_t PatternHistoryTable::getPrediction(uint32_t idx) {
    return pattern_history_table_.at(idx);
}

// Branch Target Buffer
bool BranchTargetBuffer::isHit(uint64_t PC) {
    if(branch_target_buffer_.count(PC)) {
        return true;
    }
    else {
        return false;
    }
}


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

// Return Address Stack
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
void BasePredictor::handlePredictionReq(PredictionInput predIn) {
    if(predIn.instType == branchType.JMP) {
       handleJMP(predIn);
    }
    else if(predIn.instType == branchType.RET ||
        predIn.instType == branchType.CONDITIONAL_BRANCH) {
        // make prediction 
        // event to send prediction to fetch
    }
}

void BasePredictor::handleJMP(PredictionInput predInput) {
    // push PC to RAS
    returnAddressStack.pushAddress(predInput.PC);

    // PC hit on BTB?
    if(branchTargetBuffer.isHit(predInput.PC)) {
        // event to send PC
    }
    else {
        // allocate entry in BTB
    }

}

void BasePredictor::handleRET(PredictionInput predInput);
void BasePredictor::handleBRANCH(PredictionInput predInput);

}
