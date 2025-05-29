// <BranchPredIF.hpp> -*- C++ -*-

//!
//! \file BranchPredIF.hpp
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
 * An optional signaling interface is proposed for support of prediction override 
 * in the multi-prediction case
 * 
 */

#pragma once

#include <optional>
#include <map>
#include <string>
#include <vector>
#include <type_traits>

namespace olympia {
namespace BranchPredictor {

template <class PredictionT, class UpdateT, class InputT, class SignalT = void>
class BranchPredictorIF
{
public:
    // Make this public so it can be used in subclasses
    static constexpr uint8_t bytes_per_inst = 4;

    using NInputT = std::vector<InputT>;
    using NPredictionT = std::vector<PredictionT>;
    using NUpdateT = std::vector<UpdateT>;

    virtual ~BranchPredictorIF() = default;

    // Scalar interface
    virtual PredictionT getPrediction(const InputT &) = 0;
    virtual void updatePredictor(const UpdateT &) = 0;

    // N-input interface
    virtual NPredictionT getPrediction(const NInputT &inputs)
    {
        NPredictionT result;
        result.reserve(inputs.size());
        for (const auto &input : inputs)
            result.push_back(getPrediction(input));
        return result;
    }

    virtual void updatePredictor(const NUpdateT &updates)
    {
        for (const auto &update : updates)
            updatePredictor(update);
    }

    // Optional signal interface enabled only when SignalT != void
    template <typename T = SignalT>
    std::enable_if_t<!std::is_same_v<T, void>, std::optional<std::map<std::string, T>>>
    getSignals() const {
        return std::nullopt;
    }

    virtual std::string getName() const = 0;
};

} // namespace BranchPredictor
} // namespace olympia

