#include "BranchPred.hpp"
#include "sparta/utils/SpartaTester.hpp"

TEST_INIT

void runTest(int argc, char **argv)
{
   SimpleBranchPredictor predictor(4); //specify max num insts to fetch

   DefaultInput input;
   input.FetchPC = 0x0;

   // BTB miss
   DefaultPrediction prediction = predictor.getPrediction(input);

   EXPECT_EQUAL(prediction.branch_idx, 4);
   EXPECT_EQUAL(prediction.predictedPC, 16);

   // there was a taken branch at the 3rd instruction from fetchPC, with target 0x100
   DefaultUpdate update;
   update.FetchPC = 0x0;
   update.branch_idx = 2;
   update.correctedPC = 0x100;
   update.actuallyTaken = true;
   predictor.updatePredictor(update);

   // try the same input with fetchPC 0x0 again
   prediction = predictor.getPrediction(input);
   
   EXPECT_EQUAL(prediction.branch_idx, 2);
   EXPECT_EQUAL(prediction.predictedPC, 0x100);

   // TODO: add more tests
            
}

int main(int argc, char **argv)
{
    runTest(argc, argv);

    REPORT_ERROR;
    return (int)ERROR_CODE;
}
