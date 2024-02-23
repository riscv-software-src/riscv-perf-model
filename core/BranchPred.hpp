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

#include <cstdint>
#include <map>
#include <limits>
#include "sparta/utils/SpartaAssert.hpp"

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

// following class definitions are example inputs & outputs for a very simple branch
// predictor
class DefaultPrediction
{
public:
    // index of branch instruction in the fetch packet
    // branch_idx can vary from 0 to (FETCH_WIDTH - 1)
    // initialized to default max to catch errors
    uint32_t branch_idx = std::numeric_limits<uint32_t>::max();
    // predicted target PC
    uint64_t predicted_PC = std::numeric_limits<uint64_t>::max();
};

class DefaultUpdate
{
public:
    uint64_t fetch_PC = std::numeric_limits<uint64_t>::max();
    uint32_t branch_idx = std::numeric_limits<uint32_t>::max();
    uint64_t corrected_PC = std::numeric_limits<uint64_t>::max();
    bool actually_taken = false;
};

class DefaultInput
{
public:
    // PC of first instruction of fetch packet
    uint64_t fetch_PC = std::numeric_limits<uint64_t>::max();
};

class BTBEntry
{
public:
    // use of BTBEntry in std:map operator [] requires default constructor
    BTBEntry() = default;
    BTBEntry(uint32_t bidx, uint64_t predPC) :
        branch_idx(bidx),
        predicted_PC(predPC)
    {}
    uint32_t branch_idx {std::numeric_limits<uint32_t>::max()};
    uint64_t predicted_PC {std::numeric_limits<uint64_t>::max()};
};

// Currently SimpleBranchPredictor works only with uncompressed instructions
// TODO: generalize SimpleBranchPredictor for both compressed and uncompressed instructions
class SimpleBranchPredictor : public BranchPredictorIF<DefaultPrediction, DefaultUpdate, DefaultInput>
{
public:
    SimpleBranchPredictor(uint32_t max_fetch_insts) :
        max_fetch_insts_(max_fetch_insts)
    {}
    DefaultPrediction getPrediction(const DefaultInput &);
    void updatePredictor(const DefaultUpdate &);
private:
    // maximum number of instructions in a FetchPacket
    const uint32_t max_fetch_insts_;
    // BHT and BTB of SimpleBranchPredictor is unlimited in size
    // a map of branch PC to 2 bit staurating counter tracking branch history
    std::map <uint64_t, uint8_t> branch_history_table_; // BHT
    // a map of branch PC to target of the branch
    std::map <uint64_t, BTBEntry> branch_target_buffer_; // BTB
};
