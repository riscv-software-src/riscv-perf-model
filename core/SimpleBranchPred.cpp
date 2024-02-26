#include "SimpleBranchPred.hpp"

/*
 * The algorithm used for prediction / update is as follows:
 * Prediction:
 *    - look up BHT to determine if the branch is predicted taken or not
 *      using 2-bit saturated counter
 *       - value 3: strongly taken
 *       - value 2: weakly taken
 *       - value 1: weakly not taken
 *       - value 0: strongly not taken
 *    - look up BTB to see if an entry exists for the input fetch pc
 *       - if present in BTB and predicted taken, BTB entry is used to determine
 *         prediction branch idx and predicted_PC
 *       - if present in BTB but predicted not taken, BTB entry is used to determine
 *         prediction branch idx, while predicted_PC is the fall through addr
 *       - if not present in BTB entry, prediction branch idx is the last instr of
 *         the FetchPacket, while predicted PC is the fall through addr. Also, create
 *         a new BTB entry
 * Update:
 *    - a valid BTB entry must be present for fetch PC
 *    - TBD
 *
 */
namespace olympia
{

    void SimpleBranchPredictor::updatePredictor(const DefaultUpdate & update) {

        sparta_assert(branch_target_buffer_.find(update.fetch_PC) != branch_target_buffer_.end());
        branch_target_buffer_[update.fetch_PC].branch_idx = update.branch_idx;
        if (update.actually_taken) {
            branch_history_table_[update.fetch_PC] =
                (branch_history_table_[update.fetch_PC] == 3) ? 3 :
                 branch_history_table_[update.fetch_PC] + 1;
            branch_target_buffer_[update.fetch_PC].predicted_PC = update.corrected_PC;
        } else {
            branch_history_table_[update.fetch_PC] =
                (branch_history_table_[update.fetch_PC] == 0) ? 0 :
                 branch_history_table_[update.fetch_PC] - 1;
        }
    }

    DefaultPrediction SimpleBranchPredictor::getPrediction(const DefaultInput & input) {
        bool predictTaken = false;
        if (branch_history_table_.find(input.fetch_PC) != branch_history_table_.end()) {
            predictTaken = (branch_history_table_[input.fetch_PC] > 1);
        } else {
            // add a new entry to BHT, biased towards not taken
            branch_history_table_.insert(std::pair<uint64_t, uint8_t>(input.fetch_PC, 1));
        }

        DefaultPrediction prediction;
        if (branch_target_buffer_.find(input.fetch_PC) != branch_target_buffer_.end()) {
            // BTB hit
            const BTBEntry & btb_entry = branch_target_buffer_[input.fetch_PC];
            prediction.branch_idx = btb_entry.branch_idx;
            if (predictTaken) {
                prediction.predicted_PC = btb_entry.predicted_PC;
            } else {
                // fall through address
                prediction.predicted_PC = input.fetch_PC + prediction.branch_idx + BranchPredictorIF::bytes_per_inst;
            }
        } else {
            // BTB miss
            prediction.branch_idx = max_fetch_insts_;
            prediction.predicted_PC = input.fetch_PC + max_fetch_insts_ * bytes_per_inst;
            // add new entry to BTB
            branch_target_buffer_.insert(std::pair<uint64_t,BTBEntry>(
                input.fetch_PC, BTBEntry(prediction.branch_idx, prediction.predicted_PC)));
        }

        return prediction;
    }

} // namespace olympia
