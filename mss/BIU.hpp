
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

#include "Inst.hpp"
#include "CoreTypes.hpp"
#include "FlushManager.hpp"

// UPDATE
#include "sparta/ports/SyncPort.hpp"
#include "sparta/resources/Pipe.hpp"

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

            PARAMETER(uint32_t, biu_req_queue_size, 4, "BIU request queue size")
            PARAMETER(uint32_t, biu_latency, 1, "Send bus request latency")
        };

        // Constructor for BIU
        // node parameter is the node that represent the BIU and p is the BIU parameter set
        BIU(sparta::TreeNode* node, const BIUParameterSet* p);

        // name of this resource.
        static const char name[];


        ////////////////////////////////////////////////////////////////////////////////
        // Type Name/Alias Declaration
        ////////////////////////////////////////////////////////////////////////////////


    private:
        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::DataInPort<olympia::InstQueue::value_type> in_biu_req_
            {&unit_port_set_, "in_biu_req", 1};

        sparta::SyncInPort<bool> in_mss_ack_sync_
            {&unit_port_set_, "in_mss_ack_sync", getClock()};


        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::DataOutPort<bool> out_biu_ack_
            {&unit_port_set_, "out_biu_ack"};

        sparta::DataOutPort<olympia::InstPtr> out_biu_resp_
            {&unit_port_set_, "out_biu_resp"};

        sparta::SyncOutPort<olympia::InstPtr> out_mss_req_sync_
            {&unit_port_set_, "out_mss_req_sync", getClock()};


        ////////////////////////////////////////////////////////////////////////////////
        // Internal States
        ////////////////////////////////////////////////////////////////////////////////

        using BusRequestQueue = std::list<olympia::InstPtr>;
        BusRequestQueue biu_req_queue_;

        const uint32_t biu_req_queue_size_;
        const uint32_t biu_latency_;

        bool biu_busy_ = false;


        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////

        // Event to handle BIU request from L2Cache
        sparta::UniqueEvent<> ev_handle_biu_req_
            {&unit_event_set_, "handle_biu_req", CREATE_SPARTA_HANDLER(BIU, handle_BIU_Req_)};

        // Event to handle MSS Ack
        sparta::UniqueEvent<> ev_handle_mss_ack_
            {&unit_event_set_, "handle_mss_ack", CREATE_SPARTA_HANDLER(BIU, handle_MSS_Ack_)};


        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        ////////////////////////////////////////////////////////////////////////////////

        // Receive new BIU request from L2Cache
        void receiveReqFromL2Cache_(const olympia::InstPtr &);

        // Handle BIU request
        void handle_BIU_Req_();

        // Handle MSS Ack
        void handle_MSS_Ack_();

        // Receive MSS access acknowledge
        // Q: Does the argument list has to be "const DataType &" ?
        void getAckFromMSS_(const bool &);


        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call
        ////////////////////////////////////////////////////////////////////////////////

        // Append BIU request queue
        void appendReqQueue_(const olympia::InstPtr &);


    };
}
