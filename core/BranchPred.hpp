// <Branch.hpp> -*- C++ -*-
// <Fetch.hpp> -*- C++ -*-

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

class Prediction; // Prediction output
class Update;     // Update input
class Input;      // Prediction input


template <class PredictionT, UpdateT, InputT>
class BranchPredictor
{
public:
    PredictionT & getPrediction(const InputT &);
    void updatePredictor(UpdateT &);
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
    uint64_t correctedPC;
};

class Input
{
public:
    uint64_t FetchPC;
};
