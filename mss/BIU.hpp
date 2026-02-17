
#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <regex>

#include "sparta/utils/SpartaException.hpp"
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

// UPDATE
#include "sparta/ports/SyncPort.hpp"
#include "sparta/resources/Pipe.hpp"

namespace olympia_mss
{
    struct MappedDevice
    {
        uint64_t addr = 0;
        uint32_t size = 0;
        std::string device_name;

        // to compare two devices
        bool operator==(const MappedDevice& other) const {
            return addr == other.addr && size == other.size && device_name == other.device_name;
        }
    };
}

namespace olympia_mss {
    inline std::istream& operator>>(std::istream& is, MappedDevice& md) {
        std::string s;
        char c;
        if (!(is >> std::ws) || is.peek() != '[') {
            is.setstate(std::ios::failbit);
            return is;
        }

        int balance = 0;
        while (is.get(c)) {
            s += c;
            if (c == '[') balance++;
            else if (c == ']') balance--;
            if (balance == 0) break;
        }

        // the regex matches [addr, size, "name"] or [addr, size, name]
        // addr and size can be hex (0x...) or dec
        static const std::regex device_re("\\[\\s*(0x[0-9a-fA-F]+|[0-9]+)\\s*,\\s*(0x[0-9a-fA-F]+|[0-9]+)\\s*,\\s*(?:\"([^\"]*)\"|([^,\\]\\s]+))\\s*\\]");
        std::match_results<std::string::const_iterator> match;
        if (std::regex_match(s, match, device_re)) {
            try {
                md.addr = std::stoull(match[1].str(), nullptr, 0);
                md.size = std::stoul(match[2].str(), nullptr, 0);
                md.device_name = match[3].matched ? match[3].str() : match[4].str();
                return is;
            } catch (const std::exception & e) {
                throw sparta::SpartaException("Malformed parameter for mapped device: ") << s
                    << ". Expected: [addr, size, \"name\"]. Internal error: " << e.what();
            }
        }
        throw sparta::SpartaException("Malformed parameter for mapped device: ") << s
            << ". Expected: [addr, size, \"name\"]";
    }
    inline std::ostream& operator<<(std::ostream& os, const MappedDevice& md) {
        os << "[" << std::hex << md.addr << ", " << md.size << ", \"" << md.device_name << "\"]" << std::dec;
        return os;
    }
}

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
            PARAMETER(std::vector<MappedDevice>, mapped_devices, {}, R"(Vector of Mapped Devices in simulation.

Example:
    top.*.biu.mapped_devices "[[0x40000000, 0x1000, \"i2c\"]]"

)")
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

        sparta::DataInPort<olympia::MemoryAccessInfoPtr> in_biu_req_
            {&unit_port_set_, "in_biu_req", 1};

        sparta::SyncInPort<bool> in_mss_ack_sync_
            {&unit_port_set_, "in_mss_ack_sync", getClock()};

        std::vector<std::unique_ptr<sparta::SyncInPort<bool>>> in_device_ack_sync_;

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::DataOutPort<uint32_t> out_biu_credits_
            {&unit_port_set_, "out_biu_credits"};

        sparta::DataOutPort<olympia::MemoryAccessInfoPtr> out_biu_resp_
            {&unit_port_set_, "out_biu_resp"};

        sparta::SyncOutPort<olympia::MemoryAccessInfoPtr> out_mss_req_sync_
            {&unit_port_set_, "out_mss_req_sync", getClock()};

        std::vector<std::unique_ptr<sparta::SyncOutPort<olympia::MemoryAccessInfoPtr>>> out_device_req_sync_;


        ////////////////////////////////////////////////////////////////////////////////
        // Internal States
        ////////////////////////////////////////////////////////////////////////////////

        using BusRequestQueue = std::list<olympia::MemoryAccessInfoPtr>;
        BusRequestQueue biu_req_queue_;

        const uint32_t biu_req_queue_size_;
        const uint32_t biu_latency_;
        std::vector<MappedDevice> mapped_devices_;

        bool biu_busy_ = false;


        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////

        // Event to handle BIU request from L2Cache
        sparta::UniqueEvent<> ev_handle_biu_req_
            {&unit_event_set_, "handle_biu_req", CREATE_SPARTA_HANDLER(BIU, handleBIUReq_)};

        // Event to handle MSS Ack
        sparta::UniqueEvent<> ev_handle_mss_ack_
            {&unit_event_set_, "handle_mss_ack", CREATE_SPARTA_HANDLER(BIU, handleMSSAck_)};

        // Generic event to handle Device Ack
        sparta::UniqueEvent<> ev_handle_device_ack_
            {&unit_event_set_, "handle_device_ack", CREATE_SPARTA_HANDLER(BIU, handleDeviceAck_)};

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        ////////////////////////////////////////////////////////////////////////////////

        // Receive new BIU request from L2Cache
        void receiveReqFromL2Cache_(const olympia::MemoryAccessInfoPtr &);

        // Handle BIU request
        void handleBIUReq_();

        // Handle MSS Ack
        void handleMSSAck_();

        // Handle Device Ack
        void handleDeviceAck_();

        // Receive MSS access acknowledge
        // Q: Does the argument list has to be "const DataType &" ?
        void getAckFromMSS_(const bool &);

        // Receive generic device access acknowledge
        void getAckFromDevice_(const bool &);

        // Sending initial credits to L2Cache
        void sendInitialCredits_();

        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call
        ////////////////////////////////////////////////////////////////////////////////

        // Append BIU request queue
        void appendReqQueue_(const olympia::MemoryAccessInfoPtr &);
    };
}
