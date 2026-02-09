#include "fetch/SimpleBranchPred.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT

void runTest(int argc, char **argv)
{
   olympia::BranchPredictor::SimpleBranchPredictor predictor(4); //specify max num insts to fetch

   olympia::BranchPredictor::DefaultInput input;
   input.fetch_PC = 0x0;

   // BTB miss
   olympia::BranchPredictor::DefaultPrediction prediction = predictor.getPrediction(input);

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

   // TODO: add more tests

}

int main(int argc, char **argv)
{
    runTest(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
