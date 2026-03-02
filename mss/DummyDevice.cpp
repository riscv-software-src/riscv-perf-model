
#include "DummyDevice.hpp"

namespace olympia_mss
{
    const char DummyDevice::name[] = "dummy_device";

    DummyDevice::DummyDevice(sparta::TreeNode *node, const DummyDeviceParameterSet *) :
        DeviceBase(node, node->getName())
    {
        in_req_sync_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(DummyDevice, handleReq_, olympia::MemoryAccessInfoPtr));
    }

    void DummyDevice::handleReq_(const olympia::MemoryAccessInfoPtr &req)
    {
        ILOG(getName() << " received request for addr 0x" << std::hex << req->getPhyAddr());
        out_ack_sync_.send(true, 1);
    }
}
