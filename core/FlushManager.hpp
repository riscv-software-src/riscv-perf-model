// <Flush.h> -*- C++ -*-


/**
 * \file   FlushManager.h
 *
 * \brief  File that defines support for event flushing in SPARTA
 */

#pragma once

#include <cinttypes>
#include <string>

#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/utils/ValidValue.hpp"

#include "Inst.hpp"

namespace olympia
{
    /**
     * \class FlushManager
     *
     * \brief Class used by performance models for signaling a
     *        flushing event across blocks.
     *
     * The usage is pretty simple.  Create a FlushManager within
     * the topology and have individual units bind their
     * DataInPorts to the appropriate flush ports (based on type
     * [reflected in the name]).
     *
     * When a Flush is instigated on the Tick phase, on the phase
     * sparta::SchedulingPhase::Flush the signal will be delivered
     * to the unit (+1 cycle or more later).  The unit will be
     * given a criteria for flushing that it can use to determine
     * what components it needs to remove from its internal data
     * structures.
     */
    class FlushManager : public sparta::Unit
    {
    public:

        enum class FlushCause : std::uint16_t {
            TRAP = 0,
            __FIRST = TRAP,
            MISPREDICTION,
            TARGET_MISPREDICTION,
            MISFETCH,
            POST_SYNC,
            UNKNOWN,
            __LAST
        };

        static bool determineInclusive(FlushCause cause)
        {
            static const std::map<FlushCause, bool> inclusive_flush_map = {
                {FlushCause::TRAP,                 true},
                {FlushCause::MISFETCH,             true},
                {FlushCause::MISPREDICTION,        false},
                {FlushCause::TARGET_MISPREDICTION, false},
                {FlushCause::POST_SYNC,            false}
            };

            if(auto match = inclusive_flush_map.find(cause); match != inclusive_flush_map.end()) {
                return match->second;
            }
            sparta_assert(false, "Unknown flush cause: " << static_cast<uint16_t>(cause));
            return false;
        }

        class FlushingCriteria
        {
        public:
            FlushingCriteria(FlushCause cause, const InstPtr & inst_ptr) :
                cause_(cause),
                is_inclusive_(determineInclusive(cause_)),
                inst_ptr_(inst_ptr)
            {}

            FlushingCriteria() = default;
            FlushingCriteria(const FlushingCriteria &rhs) = default;
            FlushingCriteria &operator=(const FlushingCriteria &rhs) = default;

            FlushCause getCause() const        { return cause_; }
            const InstPtr & getInstPtr() const { return inst_ptr_; }
            bool isInclusiveFlush() const { return is_inclusive_; }
            bool isLowerPipeFlush() const { return cause_ == FlushCause::MISFETCH; }

            bool includedInFlush(const InstPtr& other) const
            {
                return isInclusiveFlush() ?
                    inst_ptr_->getUniqueID() <= other->getUniqueID() :
                    inst_ptr_->getUniqueID() < other->getUniqueID();
            }

        private:
            // Cannot be const since these are copied into the PLE
            FlushCause cause_ = FlushCause::UNKNOWN;
            bool is_inclusive_ = false;
            InstPtr inst_ptr_;
        };

        static constexpr char name[] = "flushmanager";

        class FlushManagerParameters : public sparta::ParameterSet
        {
        public:
            FlushManagerParameters(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            { }

        };

        /*!
         * \brief Create a FlushManager in the tree
         * \param rc     The parent resource tree node
         * \param params Pointer to the flush manager parameters
         */
        FlushManager(sparta::TreeNode *rc, const FlushManagerParameters * params) :
            Unit(rc, name),
            in_flush_request_(getPortSet(), "in_flush_request", 0),
            out_flush_lower_(getPortSet(), "out_flush_lower", false),
            out_flush_upper_(getPortSet(), "out_flush_upper", false)
        {
            (void)params;

            in_flush_request_.
                registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(FlushManager,
                                                                      receiveFlush_,
                                                                      FlushingCriteria));

            in_flush_request_.registerConsumerEvent(ev_flush_);
        }

    private:

        // Flushing criteria
        sparta::DataInPort <FlushingCriteria> in_flush_request_;
        sparta::DataOutPort<FlushingCriteria> out_flush_lower_;
        sparta::DataOutPort<FlushingCriteria> out_flush_upper_;

        sparta::UniqueEvent<> ev_flush_ {&unit_event_set_,
                                        "flush_event",
                                         CREATE_SPARTA_HANDLER(FlushManager, forwardFlush_)};

        // Hold oldest incoming flush request for forwarding
        sparta::utils::ValidValue<FlushingCriteria> pending_flush_;

        // Arbitrates and forwards the flush request from the input flush ports, to the output ports
        void forwardFlush_()
        {
            sparta_assert(pending_flush_.isValid(), "no flush to forward onwards?");
            auto flush_data = pending_flush_.getValue();
            if (flush_data.isLowerPipeFlush())
            {
                ILOG("instigating lower pipeline flush for: " << flush_data);
                out_flush_lower_.send(flush_data);
            }
            else
            {
                ILOG("instigating upper pipeline flush for: " << flush_data);
                out_flush_upper_.send(flush_data);
            }
            pending_flush_.clearValid();
        }

        void receiveFlush_(const FlushingCriteria & flush_data)
        {
            ev_flush_.schedule();

            // Capture the oldest flush request only
            if (pending_flush_.isValid())
            {
                auto pending = pending_flush_.getValue();
                if (pending.includedInFlush(flush_data.getInstPtr()))
                {
                    return;
                }
            }
            pending_flush_ = flush_data;
        }

    };


    inline std::ostream & operator<<(std::ostream & os, const FlushManager::FlushCause & cause) {
        switch(cause) {
            case FlushManager::FlushCause::TRAP:
                os << "TRAP";
                break;
            case FlushManager::FlushCause::MISPREDICTION:
                os << "MISPREDICTION";
                break;
            case FlushManager::FlushCause::TARGET_MISPREDICTION:
                os << "TARGET_MISPREDICTION";
                break;
            case FlushManager::FlushCause::MISFETCH:
                os << "MISFETCH";
                break;
            case FlushManager::FlushCause::POST_SYNC:
                os << "POST_SYNC";
                break;
            case FlushManager::FlushCause::UNKNOWN:
                os << "UNKNOWN";
                break;
            case FlushManager::FlushCause::__LAST:
                throw sparta::SpartaException("__LAST cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os, const FlushManager::FlushingCriteria & criteria) {
        os << criteria.getInstPtr() << " " << criteria.getCause();
        return os;
    }
}
