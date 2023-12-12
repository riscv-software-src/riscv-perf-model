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

        enum class FlushEvent : std::uint16_t {
            TRAP = 0,
            __FIRST = TRAP,
            MISPREDICTION,
            TARGET_MISPREDICTION,
            MISFETCH,
            POST_SYNC,
            __LAST
        };

        class FlushingCriteria
        {
        public:
            FlushingCriteria(FlushEvent cause, InstPtr inst_ptr) :
                cause_(cause),
                inst_ptr_(inst_ptr) {}

            FlushingCriteria() = default;
            FlushingCriteria(const FlushingCriteria &rhs) = default;
            FlushingCriteria &operator=(const FlushingCriteria &rhs) = default;

            FlushEvent getCause() const        { return cause_; } // TODO FlushEvent -> FlushCause
            const InstPtr & getInstPtr() const { return inst_ptr_; }

            bool isInclusiveFlush() const
            {
                static const std::map<FlushEvent, bool> inclusive_flush_map = {
                    {FlushEvent::TRAP,                 true},
                    {FlushEvent::MISFETCH,             true},
                    {FlushEvent::MISPREDICTION,        false},
                    {FlushEvent::TARGET_MISPREDICTION, false},
                    {FlushEvent::POST_SYNC,            false}
                };
                if(auto match = inclusive_flush_map.find(cause_); match != inclusive_flush_map.end()) {
                    return match->second;
                }
                sparta_assert(false, "Unknown flush cause: " << static_cast<uint16_t>(cause_));
                return true;
            }

            bool flush(const InstPtr& other) const
            {
                return isInclusiveFlush() ?
                    inst_ptr_->getUniqueID() <= other->getUniqueID() :
                    inst_ptr_->getUniqueID() < other->getUniqueID();
            }

        private:
            FlushEvent cause_;
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
            out_retire_flush_(getPortSet(), "out_retire_flush", false),
            in_retire_flush_(getPortSet(), "in_retire_flush", 0),
            in_decode_flush_(getPortSet(), "in_decode_flush", 0),
            out_decode_flush_(getPortSet(), "out_decode_flush", false)
        {
            (void)params;
            in_retire_flush_.
                registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(FlushManager,
                                                                      receiveFlush_,
                                                                      FlushingCriteria));
            in_decode_flush_.
                registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(FlushManager,
                                                                        receiveFlush_,
                                                                        FlushingCriteria));

            in_retire_flush_.registerConsumerEvent(ev_flush_);
            in_decode_flush_.registerConsumerEvent(ev_flush_);
        }

    private:

        // Flushing criteria
        sparta::DataOutPort<FlushingCriteria> out_retire_flush_;
        sparta::DataInPort <FlushingCriteria> in_retire_flush_;

        // Decode Flush Port
        sparta::DataInPort<FlushingCriteria> in_decode_flush_;
        sparta::DataOutPort<FlushingCriteria> out_decode_flush_;

        sparta::UniqueEvent<> ev_flush_ {&unit_event_set_,
                                        "flush_event",
                                         CREATE_SPARTA_HANDLER(FlushManager, forwardFlush_)};

        // Arbitrates and forwards the flush request from the input flush ports, to the output ports
        void forwardFlush_()
        {
            if (in_retire_flush_.dataReceivedThisCycle())
            {
                auto flush_data = in_retire_flush_.pullData();
                out_retire_flush_.send(flush_data);
                ILOG("forwarding retirement flush request: " << flush_data);
            }
            else
            {
                sparta_assert(in_decode_flush_.dataReceivedThisCycle(), "flush event scheduled for no reason?");
                auto flush_data = in_decode_flush_.pullData();
                out_decode_flush_.send(flush_data);
                ILOG("forwarding decode flush request: " << flush_data);
            }
        }

        void receiveFlush_(const FlushingCriteria &)
        {
            ev_flush_.schedule();
        }

    };


    inline std::ostream & operator<<(std::ostream & os, const FlushManager::FlushEvent & event) {
        switch(event) {
            case FlushManager::FlushEvent::TRAP:
                os << "TRAP";
                break;
            case FlushManager::FlushEvent::MISPREDICTION:
                os << "MISPREDICTION";
                break;
            case FlushManager::FlushEvent::TARGET_MISPREDICTION:
                os << "TARGET_MISPREDICTION";
                break;
            case FlushManager::FlushEvent::MISFETCH:
                os << "MISFETCH";
                break;
            case FlushManager::FlushEvent::POST_SYNC:
                os << "POST_SYNC";
                break;
            case FlushManager::FlushEvent::__LAST:
                throw sparta::SpartaException("__LAST cannot be a valid enum event state.");
        }
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os, const FlushManager::FlushingCriteria & criteria) {
        os << criteria.getInstPtr() << " " << criteria.getCause();
        return os;
    }
}
