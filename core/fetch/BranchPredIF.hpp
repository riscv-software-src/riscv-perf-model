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
 *
 * Support for multiple predictions/updates is added parallel to the single 
 * prediction/update interface
 * 
 * A optional signaling interface is proposed for support of prediction override 
 * in the multi-prediction case
 * 
 * */
#pragma once

namespace olympia
{
namespace BranchPredictor
{

    template <class PredictionT, class UpdateT, class InputT, class SignalT>
    class BranchPredictorIF
    {
      using NInputT = std::vector<InputT>;
      using NPredictionT = std::vector<PredictionT>;
      using NUpdateT = std::vector<UpdateT>;
      using NSignalT = std::map<std::string,SignalT>;
    public:
        // TODO: create constexpr for bytes per compressed and uncompressed inst
        static constexpr uint8_t bytes_per_inst = 4;
        virtual ~BranchPredictorIF() = default;

        virtual PredictionT getPrediction(const InputT &) = 0;
        virtual void updatePredictor(const UpdateT &) = 0;

        //N-prediction requests
        virtual NPredictionT getPrediction(const NInputT &inputs) {
          std::vector<PredictionT> result;
          result.reserve(inputs.size());
          for (const auto &input : inputs)
              result.push_back(getPrediction(input));
          return result;
        }

        //N-update requests
        virtual void updatePredictor(const NUpdateT &updates) {
            for (const auto &update : updates)
                updatePredictor(update);
        }

        //Optional signal interface for staging predictions
        virtual std::optional<NSignalT> getSignals() const {
           return std::nullopt;
        }

        // Name the predictor
        virtual std::string getName() const = 0;
    };

} // namespace BranchPredictor
} // namespace olympia
