// <Branch.hpp> -*- C++ -*-

//!
//! \file BranchPred.hpp
//! \brief Definition of Branch Prediction API
//!

/*
 * This file defines the Branch Prediction API.
 * The goal is to define an API that is generic and yet flexible enough to support various
 * branch prediction microarchitecture.
 * To the end, we envision a generic branch predictor as a black box with following inputs
 * and outputs:
 *   * A generic Prediction output
 *   * A generic Prediction input
 *   * A generic Update input
 *
 * The generic branch predictor may have two operations:
 *   * getPrediction: produces Prediction output based on the Prediction input.
 *   * updatePredictor: updates Predictor with Update input.
 *
 * It is intended that an implementation of branch predictor must also specify
 * implementations of Prediction output, Prediction input and Update input, along with
 * implementations of getPrediction and updatePredictor operations.
 * */
#include <map>

class Prediction; // Prediction output
class Update;     // Update input
class Input;      // Prediction input


template <class PredictionT, class UpdateT, class InputT>
class BranchPredictor
{
public:
    BrandPredictor() = default;
    PredictionT & getPrediction(const InputT &);
    void updatePredictor(UpdateT &);
private:
    // a map of branch PC to 2 bit staurating counter tracking branch history
    std::map <uint64_t, uint8_t> branch_history_table_; // BHT
    // a map of branch PC to target of the branch
    std::map <uint64_t, uint8_t> branch_target_buffer_; // BTB
};

// following class definitions are example inputs & outputs for a very simple branch
// predictor
class Prediction
{
public:
    uint64_t predictedPC;
};

class Update
{
public:
    uint64_t predictedPC;
    uint64_t correctedPC;
};

class Input
{
public:
    uint64_t FetchPC;
};
