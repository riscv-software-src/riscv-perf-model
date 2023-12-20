#include "BranchPred.hpp"

/*
 * The algorithm used for prediction / update is as follows:
 * Prediction:
 *    - look up BHT to determine if the branch is predicted taken or not
 *    - look up BTB to see if an entry exists for the input fetch pc
 *       - if present in BTB and predicted taken, BTB entry is used to determine
 *         prediction branch idx and predictedPC
 *       - if present in BTB but predicted not taken, BTB entry is used to determine
 *         prediction branch idx, while predictedPC is the fall through addr
 *       - if not present in BTB entry, prediction branch idx is the last instr of
 *         the FetchPacket, while predicted PC is the fall through addr. Also, create
 *         a new BTB entry 
 * Update:
 *    - a valid BTB entry must be present for fetch PC
 *    - TBD 
 *
 */

#define BYTES_PER_INST 4

void SimpleBranchPredictor::updatePredictor(DefaultUpdate & update) {

    sparta_assert(branch_target_buffer_.find(update.FetchPC) != branch_target_buffer_.end());
    branch_target_buffer_[input.FetchPC].branch_idx = update.branch_idx;
    if (update.actuallyTaken) {
        branch_history_table_[update.FetchPC] =
            (branch_history_table_[update.FetchPC] == 3) ? 3 :
             branch_history_table_[update.FetchPC] + 1;
        branch_target_buffer_[input.FetchPC].predictedPC = update.predictedPC;
    } else {
        branch_history_table_[update.FetchPC] =
            (branch_history_table_[update.FetchPC] == 0) ? 0 :
             branch_history_table_[update.FetchPC] - 1;
    }
}

DefaultPrediction SimpleBranchPredictor::getPrediction(const DefaultInput & input) {
    bool predictTaken;
    if (branch_history_table_.find(input.FetchPC) != branch_history_table_.end()) {
        predictTaken = (branch_history_table_[input.fetchPC] > 1);
    } else {
        predictTaken = false;
    }

    DefaultPrediction prediction;
    if (branch_target_buffer_.find(input.FetchPC) != branch_target_buffer_.end()) {
        // BTB hit
        BTBEntry btb_entry = branch_target_buffer_[input.FetchPC];
        prediction.branch_idx = btb_entry.branch_idx;
        if (predictTaken) {
            prediction.predictedPC = btb_entry.predictedPC;
        } else {
            // fall through address
            prediction.predictedPC = input.FetchPC + prediction.branch_idx + BYTES_PER_INST;
        }
    } else {
        // BTB miss
        prediction.branch_idx = max_fetch_insts_;
        prediction.predictedPC = input.FetchPC + max_fetch_insts_ * BYTES_PER_INST;
        // add new entry to BTB
        branch_target_buffer_.insert(std::pair<uint64_t,BTBEntry>(
            input.FetchPC, BTBEntry(prediction.branch_idx, prediction.predictedPC));
    }

    return prediction;
}
