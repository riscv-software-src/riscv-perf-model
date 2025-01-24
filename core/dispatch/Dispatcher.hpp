// <Dispatcher.h> -*- C++ -*-

#pragma once

#include <cinttypes>
#include <string>

#include "sparta/ports/DataPort.hpp"
#include "sparta/log/MessageSource.hpp"
#include "sparta/utils/LogUtils.hpp"

#include "Inst.hpp"
#include "CoreTypes.hpp"

namespace olympia
{
    class Dispatch;

    /*!
     * \class Dispatcher
     * \brief Class that "connects" Dispatch to an execution unit.
     *
     * See https://github.com/riscv-software-src/riscv-perf-model/discussions/7
     *
     * This class connects to an execution unit and handles credits
     * and instruction transfers.
     */
    class Dispatcher
    {
    public:
        // Create a dispatcher
        Dispatcher(const std::string            & name,
                   Dispatch                     * dispatch,
                   sparta::log::MessageSource   & info_logger,
                   sparta::DataInPort<uint32_t> * in_credits,
                   sparta::DataOutPort<InstQueue::value_type> * out_inst) :
            name_(name),
            dispatch_(dispatch),
            info_logger_(info_logger),
            out_inst_(out_inst)
        {
            in_credits->
                registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Dispatcher, receiveCredits_, uint32_t));
        }

        // Get the name of this Dispatcher
        const std::string & getName() const {
            return name_;
        }

        // Can this dispatcher accept a new instruction?
        bool canAccept() const {
            // The dispatcher must be have enough credits in the
            // execution pipe AND still has bandwidth
            return (unit_credits_ != 0) && (num_can_dispatch_ != 0);
        }

        // Have this dispatcher accecpt the new instruction
        void acceptInst(const InstPtr & inst) {
            sparta_assert(unit_credits_ != 0, "Dispatcher " << name_
                          << " cannot accept the given instruction (not enough credits): " << inst);
            sparta_assert(num_can_dispatch_ != 0, "Dispatcher " << name_
                          << " cannot accept the given instruction (already accepted an instruction)");
            ILOG(name_ << ": dispatching " << inst);
            out_inst_->send(inst);
            --unit_credits_;
            --num_can_dispatch_;
        }

        // Reset the bandwidth
        void reset() { num_can_dispatch_ = 1; }

        uint32_t getCredits() const {return unit_credits_;}

    private:
        uint32_t unit_credits_ = 0;
        uint32_t num_can_dispatch_ = 1;

        const std::string            name_;
        Dispatch                   * dispatch_ = nullptr;
        sparta::log::MessageSource & info_logger_;

        sparta::DataOutPort<InstQueue::value_type> * out_inst_;

        // Receive credits from the execution block
        void receiveCredits_(const uint32_t & credits);
    };
}
