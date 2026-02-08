#pragma once

#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/SyncPort.hpp"
#include "sparta/events/EventSet.hpp"
#include "sparta/events/UniqueEvent.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"

#include "MemoryAccessInfo.hpp"

namespace olympia_mss
{
    class I2C : public sparta::Unit
    {
    public:
        //! Parameters for I2C model
        class I2CParameterSet : public sparta::ParameterSet
        {
        public:
            // Constructor for I2CParameterSet
            I2CParameterSet(sparta::TreeNode* n):
                sparta::ParameterSet(n)
            { }

            PARAMETER(uint32_t, i2c_latency, 10, "I2C access latency")
        };

        // Constructor for I2C
        I2C(sparta::TreeNode* node, const I2CParameterSet* p);

        // name of this resource.
        static const char name[];

    private:
        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::SyncInPort<olympia::MemoryAccessInfoPtr> in_i2c_req_sync_
            {&unit_port_set_, "in_i2c_req_sync", getClock()};

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::SyncOutPort<bool> out_i2c_ack_sync_
            {&unit_port_set_, "out_i2c_ack_sync", getClock()};

        ////////////////////////////////////////////////////////////////////////////////
        // Internal States
        ////////////////////////////////////////////////////////////////////////////////
        const uint32_t i2c_latency_;
        bool i2c_busy_ = false;

        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////

        // Event to handle I2C request from BIU
        sparta::UniqueEvent<> ev_handle_i2c_req_
            {&unit_event_set_, "handle_i2c_req", CREATE_SPARTA_HANDLER(I2C, handleI2CReq_)};

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        ////////////////////////////////////////////////////////////////////////////////

        // Receive new I2C request from BIU
        void getReqFromBIU_(const olympia::MemoryAccessInfoPtr &);

        // Handle I2C request
        void handleI2CReq_();
    };
}
