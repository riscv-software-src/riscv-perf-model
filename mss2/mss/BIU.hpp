
#pragma once

#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/collection/Collectable.hpp"
#include "sparta/events/StartupEvent.hpp"

#include "MemoryAccessInfo.hpp"
#include "CoreTypes.hpp"
#include "FlushManager.hpp"

#include <random>
#include <algorithm>

namespace olympia_mss
{
    class BIU : public sparta::Unit
    {
    public:
        //! Parameters for BIU model
        class BIUParameterSet : public sparta::ParameterSet
        {
        public:
            // Constructor for BIUParameterSet
            BIUParameterSet(sparta::TreeNode* n):
                sparta::ParameterSet(n)
            { }

            PARAMETER(uint32_t, biu_req_queue_size,    64, "BIU request queue size")
            PARAMETER(uint32_t, l3_resp_queue_size,    64, "L2 resp queue size")
            PARAMETER(uint32_t, biu_delay_seed,       0x9, "Random delay seed")
            PARAMETER(uint32_t, biu_delay_min,        154, "Random delay min latency")
            PARAMETER(uint32_t, biu_delay_max,        154, "Random delay max latency")
        };

        // Constructor for BIU
        // node parameter is the node that represent the BIU and p is the BIU parameter set
        BIU(sparta::TreeNode* node, const BIUParameterSet* p);

        // name of this resource.
        static const char name[];

    private:
        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_l3_req_
            {&unit_port_set_, "in_l3_req", 1};

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::DataOutPort<uint32_t> out_l3_credits_
            {&unit_port_set_, "out_l3_credits"};

        sparta::DataOutPort<olympia::MemoryAccessInfoPtr> out_biu_resp_
            {&unit_port_set_, "out_l3_resp"};

        ////////////////////////////////////////////////////////////////////////////////
        // Internal States
        ////////////////////////////////////////////////////////////////////////////////

        using BusRequestQueue = std::list<olympia::MemoryAccessInfoPtr>;
        BusRequestQueue biu_req_queue_;
        BusRequestQueue biu_req_holding_queue_;
        BusRequestQueue l3_resp_queue_;

        const uint32_t biu_req_queue_size_;
        const uint32_t l3_resp_queue_size_;

        // Seeding the generator
        std::mt19937 gen_;

        // Distribution declaration
        std::uniform_int_distribution<> delay_dist_;
        std::uniform_real_distribution<> random_dist_;

        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////

        // Event to handle BIU request from L3
        sparta::UniqueEvent<> ev_handle_biu_req_
            {&unit_event_set_, "handle_biu_req",
                                CREATE_SPARTA_HANDLER(BIU, handle_BIU_Req_)};

        // Event to handle BIU ack for L3
        sparta::UniqueEvent<> ev_handle_biu_l3_credits_
            {&unit_event_set_, "ev_handle_biu_l3_credits",
                                CREATE_SPARTA_HANDLER(BIU, handle_BIU_L3_Credits_)};

        // Event to handle BIU Resp for L3
        sparta::UniqueEvent<> ev_handle_biu_l3_resp_
            {&unit_event_set_, "ev_handle_biu_l3_resp",
                                CREATE_SPARTA_HANDLER(BIU, handle_BIU_L3_Resp_)};

        // Payload event for the packet to be appended to biu_l3_resp_
        sparta::PayloadEvent<olympia::MemoryAccessInfoPtr> pev_handle_biu_l3_resp_append_
            {&unit_event_set_, "handle_biu_l3_resp_append",
                                CREATE_SPARTA_HANDLER_WITH_DATA(BIU, handle_BIU_L3_Resp_Append_, olympia::MemoryAccessInfoPtr)};

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        ////////////////////////////////////////////////////////////////////////////////

        // Receive new BIU request from L3
        void receiveReqFromL3_(const olympia::MemoryAccessInfoPtr &);

        // Handle BIU request
        void handle_BIU_Req_();

        // Handle ack back to L3
        void handle_BIU_L3_Credits_();

        // Handle Resp back to L3
        void handle_BIU_L3_Resp_();

        // Append L3 Resp Queue after a given delay
        void handle_BIU_L3_Resp_Append_(const olympia::MemoryAccessInfoPtr &);

        // Sending initial credits to L3
        void sendInitialCredits_();

        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call
        ////////////////////////////////////////////////////////////////////////////////

        // Append BIU request queue
        void appendReqQueue_(const olympia::MemoryAccessInfoPtr &);
    };
}
