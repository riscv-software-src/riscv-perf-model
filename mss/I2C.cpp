#include "I2C.hpp"
#include "sparta/utils/LogUtils.hpp"

namespace olympia_mss
{
    const char I2C::name[] = "i2c";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    I2C::I2C(sparta::TreeNode* node, const I2CParameterSet* p) :
        sparta::Unit(node),
        i2c_latency_(p->i2c_latency)
    {
        in_i2c_req_sync_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(I2C, getReqFromBIU_, olympia::MemoryAccessInfoPtr));

        ILOG("I2C construct: #" << node->getGroupIdx());
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Receive new I2C request from BIU
    void I2C::getReqFromBIU_(const olympia::MemoryAccessInfoPtr & memory_access_info_ptr)
    {
        if (i2c_busy_) {
            ILOG("I2C is busy! Overlapping requests not fully supported in this simple model.");
        }
        
        i2c_busy_ = true;
        
        // Schedule I2C request handling event
        ev_handle_i2c_req_.schedule(sparta::Clock::Cycle(i2c_latency_));

        ILOG("I2C request received for address: 0x" << std::hex << memory_access_info_ptr->getPhyAddr());
    }

    // Handle I2C request
    void I2C::handleI2CReq_()
    {
        i2c_busy_ = false;
        out_i2c_ack_sync_.send(true);
        ILOG("I2C request completed, sending ACK");
    }
}
