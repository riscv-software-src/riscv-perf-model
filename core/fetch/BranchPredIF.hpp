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
#pragma once

namespace olympia
{
namespace BranchPredictor
{

    template <class PredictionT, class UpdateT, class InputT>
    class BranchPredictorIF
    {
    public:
        // TODO: create constexpr for bytes per compressed and uncompressed inst
        static constexpr uint8_t bytes_per_inst = 4;
        virtual ~BranchPredictorIF() { };
        virtual PredictionT getPrediction(const InputT &) = 0;
        virtual void updatePredictor(const UpdateT &) = 0;
    };

} // namespace BranchPredictor
} // namespace olympia
