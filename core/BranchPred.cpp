#include "BranchPred.hpp"

/*
 * The algorithm used for prediction / update is as follows:
 * Update:
 *   - if a branch is actually taken, increment its counter in branch history table
 *   - if a branch is actually not taken, decrement its counter in branch history table
 *   - max and min counter values are 3 and 0 respectively
 *   - if taken, update BTB with target (?)
 * Prediction:
 *   - Lookup BHT using the input PC.
 *      - If an entry exist, determine if it's taken or not-taken historically
 *        - If not taken, predicted PC = input PC + X bytes, where X is the fixed
 *          fetch bandwidth (for example, 4*4 = 16 bytes)
 *        - If taken, look up BTB using input PC and output that as predicted PC
 *      - If no entry exist, this branch is not seen before. Assume not taken and
 *        predicted PC = input PC + X bytes
 *
 *  Notes:
 *   - requires an update for each prediction
 *      - possible optimization ?
 *   - need to look up fetch PC from predicted PC: how?
 */

// TODO : must be function of fetch width param
#define FETCH_WIDTH_BYTES 4 * 4

template <class PredictionT, UpdateT, InputT>
void BranchPredictor<PredictionT, UpdateT, InputT>::updatePredictor(UpdateT & update) {
    // look up fetch pc for the predicted PC: how?
    // uint64_t fetchPC = ...
    bool isTaken = ((update.correctedPC - fetchPC) != FETCH_WIDTH_BYTES);
    if (isTaken) {
        branch_history_table_[fetchPC] =
            (branch_history_table_[fetchPC] == 3) ? 3 :
             branch_history_table_[fetchPC] + 1;
    } else {
        branch_history_table_[fetchPC] =
            (branch_history_table_[fetchPC] == 0) ? 0 :
             branch_history_table_[fetchPC] - 1;
    }

    if (isTaken) {
        branch_target_buffer_[fetchPC] = update.correctedPC;
    }
}

template <class PredictionT, UpdateT, InputT>
PredictionT BranchPredictor<PredictionT, UpdateT, InputT>::getPrediction(const InputT & input) {
    bool predictTaken;
    if (branch_history_table_.count(input.fetchPC) > 0) {
        predictTaken = (branch_history_table_[input.fetchPC] > 1);
    } else {
        predictTaken = false;
    }

    PredictionT prediction;
    if (predictTaken) {
        prediction.predictedPC = branch_target_buffer[input.fetchPC];
    } else {
        prediction.predictedPC = input.fetchPC + FETCH_WIDTH_BYTES;
    }

    return prediction;
}
