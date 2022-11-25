// <Dispatcher.h> -*- C++ -*-

#pragma once

#include <cinttypes>
#include <string>

#include "sparta/ports/DataPort.hpp"
#include "sparta/log/MessageSource.hpp"

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
            return unit_credits_ != 0;
        }

        // Have this dispatcher accecpt the new instruction
        void acceptInst(const InstPtr & inst) {
            sparta_assert(unit_credits_ != 0, "Dispatcher " << name_
                          << " cannot accept the given instruction: " << inst);
            if(SPARTA_EXPECT_FALSE(info_logger_)) {
                info_logger_ << name_ << ": dispatching " << inst;
            }
            out_inst_->send(inst);
            --unit_credits_;
        }

    private:
        uint32_t unit_credits_ = 0;

        const std::string            name_;
        Dispatch                   * dispatch_ = nullptr;
        sparta::log::MessageSource & info_logger_;

        sparta::DataOutPort<InstQueue::value_type> * out_inst_;

        // Receive credits from the execution block
        void receiveCredits_(const uint32_t & credits);
    };
}
