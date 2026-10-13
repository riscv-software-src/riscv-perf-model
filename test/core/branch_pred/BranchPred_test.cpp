#include "fetch/SimpleBranchPred.hpp"
#include "fetch/EnhancedBranchPred.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT

template <typename PredictorT>
void runBasicPredictorBehavior(const char * predictor_name)
{
    PredictorT predictor(4); // specify max num insts to fetch

   olympia::BranchPredictor::DefaultInput input;
   input.fetch_PC = 0x0;

   // BTB miss
   olympia::BranchPredictor::DefaultPrediction prediction = predictor.getPrediction(input);

    (void) predictor_name;
   EXPECT_EQUAL(prediction.branch_idx, 4);
   EXPECT_EQUAL(prediction.predicted_PC, 16);

   // there was a taken branch at the 3rd instruction from fetchPC, with target 0x100
   olympia::BranchPredictor::DefaultUpdate update;
   update.fetch_PC = 0x0;
   update.branch_idx = 2;
   update.corrected_PC = 0x100;
   update.actually_taken = true;
   predictor.updatePredictor(update);

   // try the same input with fetchPC 0x0 again
   prediction = predictor.getPrediction(input);

   EXPECT_EQUAL(prediction.branch_idx, 2);
   EXPECT_EQUAL(prediction.predicted_PC, 0x100);
}

void runEnhancedReplacementBehavior()
{
    olympia::BranchPredictor::EnhancedBranchPredictor predictor(4);

    auto train_taken = [&predictor](uint64_t fetch_pc, uint32_t branch_idx, uint64_t target_pc) {
        olympia::BranchPredictor::DefaultUpdate update;
        update.fetch_PC = fetch_pc;
        update.branch_idx = branch_idx;
        update.corrected_PC = target_pc;
        update.actually_taken = true;
        predictor.updatePredictor(update);
    };

    // All three PCs map to same BTB set with different tags (set index from PC[8:2]).
    train_taken(0x000, 1, 0x100);
    train_taken(0x200, 1, 0x300);

    // Touch 0x000 again so it becomes MRU; 0x200 becomes LRU.
    olympia::BranchPredictor::DefaultInput input;
    input.fetch_PC = 0x000;
    auto prediction = predictor.getPrediction(input);
    EXPECT_EQUAL(prediction.branch_idx, 1);
    EXPECT_EQUAL(prediction.predicted_PC, 0x100);

    // Insert third entry in same set to force 2-way LRU eviction.
    train_taken(0x400, 1, 0x500);

    // 0x200 should now miss because it was the LRU way.
    input.fetch_PC = 0x200;
    prediction = predictor.getPrediction(input);
    EXPECT_EQUAL(prediction.branch_idx, 4);
    EXPECT_EQUAL(prediction.predicted_PC, 0x210);

    // 0x000 and 0x400 should still hit.
    input.fetch_PC = 0x000;
    prediction = predictor.getPrediction(input);
    EXPECT_EQUAL(prediction.branch_idx, 1);
    EXPECT_EQUAL(prediction.predicted_PC, 0x100);

    input.fetch_PC = 0x400;
    prediction = predictor.getPrediction(input);
    EXPECT_EQUAL(prediction.branch_idx, 1);
    EXPECT_EQUAL(prediction.predicted_PC, 0x500);
}

void runEnhancedConfigurableGeometryBehavior()
{
    // 4-entry direct-mapped BTB and 8-entry BHT to validate runtime configurability.
    olympia::BranchPredictor::EnhancedBranchPredictor predictor(4, 4, 1, 8);

    auto train_taken = [&predictor](uint64_t fetch_pc, uint64_t target_pc) {
        olympia::BranchPredictor::DefaultUpdate update;
        update.fetch_PC = fetch_pc;
        update.branch_idx = 1;
        update.corrected_PC = target_pc;
        update.actually_taken = true;
        predictor.updatePredictor(update);
    };

    // For 4 sets, 0x0 and 0x10 map to same set. Second insert should evict first.
    train_taken(0x0, 0x40);
    train_taken(0x10, 0x80);

    olympia::BranchPredictor::DefaultInput input;
    input.fetch_PC = 0x0;
    auto prediction = predictor.getPrediction(input);
    EXPECT_EQUAL(prediction.branch_idx, 4);
    EXPECT_EQUAL(prediction.predicted_PC, 0x10);

    input.fetch_PC = 0x10;
    prediction = predictor.getPrediction(input);
    EXPECT_EQUAL(prediction.branch_idx, 1);
    EXPECT_EQUAL(prediction.predicted_PC, 0x80);
}

void runEnhancedStatsBehavior()
{
    olympia::BranchPredictor::EnhancedBranchPredictor predictor(4);

    olympia::BranchPredictor::DefaultInput input;
    input.fetch_PC = 0x0;

    // First observation is a BTB miss, so this update should score as a mispredict.
    auto prediction = predictor.getPrediction(input);
    EXPECT_EQUAL(prediction.branch_idx, 4);
    EXPECT_EQUAL(prediction.predicted_PC, 0x10);

    olympia::BranchPredictor::DefaultUpdate update;
    update.fetch_PC = 0x0;
    update.branch_idx = 2;
    update.corrected_PC = 0x100;
    update.actually_taken = true;
    predictor.updatePredictor(update);

    // After training, the same branch should be predicted correctly.
    prediction = predictor.getPrediction(input);
    EXPECT_EQUAL(prediction.branch_idx, 2);
    EXPECT_EQUAL(prediction.predicted_PC, 0x100);
    predictor.updatePredictor(update);

    EXPECT_EQUAL(predictor.getTotalPredictions(), 2);
    EXPECT_EQUAL(predictor.getCorrectPredictions(), 1);
    EXPECT_EQUAL(predictor.getMispredictions(), 1);
}

void runTest(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    runBasicPredictorBehavior<olympia::BranchPredictor::SimpleBranchPredictor>("simple");
    runBasicPredictorBehavior<olympia::BranchPredictor::EnhancedBranchPredictor>("enhanced");
    runEnhancedReplacementBehavior();
    runEnhancedConfigurableGeometryBehavior();
    runEnhancedStatsBehavior();

   // TODO: add more tests

}

int main(int argc, char **argv)
{
    runTest(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
