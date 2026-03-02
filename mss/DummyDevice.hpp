
#pragma once

#include "DeviceBase.hpp"
#include "sparta/utils/LogUtils.hpp"

namespace olympia_mss
{
    class DummyDevice : public DeviceBase
    {
    public:
        class DummyDeviceParameterSet : public sparta::ParameterSet
        {
        public:
            DummyDeviceParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
        };

        DummyDevice(sparta::TreeNode *node, const DummyDeviceParameterSet *p);

        static const char name[];

    private:
        void handleReq_(const olympia::MemoryAccessInfoPtr &req);
    };
}
