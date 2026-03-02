
#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/SyncPort.hpp"
#include "MemoryAccessInfo.hpp"

namespace olympia_mss
{
    /**
     * @class DeviceBase
     * @brief Base class for all devices/peripherals connected to the BIU.
     * 
     * This class provides the standard port interface that the BIU expects
     * for any mapped device.
     */
    class DeviceBase : public sparta::Unit
    {
    public:
        DeviceBase(sparta::TreeNode *node, const std::string& name) :
            sparta::Unit(node),
            in_req_sync_(&unit_port_set_, "in_" + name + "_req_sync", getClock()),
            out_ack_sync_(&unit_port_set_, "out_" + name + "_ack_sync", getClock())
        {
        }

    protected:
        //! Input port for memory access requests from the BIU
        sparta::SyncInPort<olympia::MemoryAccessInfoPtr> in_req_sync_;

        //! Output port for acknowledgments back to the BIU
        sparta::SyncOutPort<bool> out_ack_sync_;
    };
}
