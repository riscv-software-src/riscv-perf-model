// <BIU.cpp> -*- C++ -*-

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/utils/SpartaException.hpp"

#include "BIU.hpp"

namespace olympia_mss
{
    const char BIU::name[] = "biu";

    ////////////////////////////////////////////////////////////////////////////////
    // Constructor
    ////////////////////////////////////////////////////////////////////////////////

    BIU::BIU(sparta::TreeNode *node, const BIUParameterSet *p) :
        sparta::Unit(node),
        biu_req_queue_size_(p->biu_req_queue_size),
        biu_latency_(p->biu_latency),
        mapped_devices_(p->mapped_devices)
    {
        in_biu_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(BIU, receiveReqFromL2Cache_, olympia::MemoryAccessInfoPtr));

        in_mss_ack_sync_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(BIU, getAckFromMSS_, bool));
        in_mss_ack_sync_.setPortDelay(static_cast<sparta::Clock::Cycle>(1));

        // Validate mapped devices and create ports
        for (size_t i = 0; i < mapped_devices_.size(); ++i) {
            const auto& device = mapped_devices_[i];
            
            // Check for overlaps with previous devices
            for (size_t j = 0; j < i; ++j) {
                const auto& other = mapped_devices_[j];
                bool overlap = std::max(device.addr, other.addr) < std::min(device.addr + device.size, other.addr + other.size);
                if (overlap) {
                    throw sparta::SpartaException("BIU: Overlapping address ranges detected between devices: ")
                        << device.device_name << " [0x" << std::hex << device.addr << ", 0x" << (device.addr + device.size) << ") and "
                        << other.device_name << " [0x" << other.addr << ", 0x" << (other.addr + other.size) << ")";
                }
            }

            // Create output port for request
            std::string out_port_name = "out_" + device.device_name + "_req_sync";
            out_device_req_sync_.emplace_back(
                new sparta::SyncOutPort<olympia::MemoryAccessInfoPtr>(&unit_port_set_, out_port_name, getClock()));

            // Create input port for ack
            std::string in_port_name = "in_" + device.device_name + "_ack_sync";
            in_device_ack_sync_.emplace_back(
                new sparta::SyncInPort<bool>(&unit_port_set_, in_port_name, getClock()));

            // Register handler for ack
            in_device_ack_sync_.back()->registerConsumerHandler(
                CREATE_SPARTA_HANDLER_WITH_DATA(BIU, getAckFromDevice_, bool));
            in_device_ack_sync_.back()->setPortDelay(static_cast<sparta::Clock::Cycle>(1));
        }

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(BIU, sendInitialCredits_));
        ILOG("BIU construct: #" << node->getGroupIdx());

        ev_handle_mss_ack_ >> ev_handle_biu_req_;
        ev_handle_device_ack_ >> ev_handle_biu_req_;
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Sending Initial credits to L2Cache
    void BIU::sendInitialCredits_() {
        out_biu_credits_.send(biu_req_queue_size_);
        ILOG("Sending initial credits to L2Cache : " << biu_req_queue_size_);
    }

    // Receive new BIU request from L2Cache
    void BIU::receiveReqFromL2Cache_(const olympia::MemoryAccessInfoPtr & memory_access_info_ptr)
    {
        appendReqQueue_(memory_access_info_ptr);

        // Schedule BIU request handling event only when:
        // (1)BIU is not busy, and (2)Request queue is not empty
        if (!biu_busy_) {
            // NOTE:
            // We could set this flag immediately here, but a better/cleaner way to do this is:
            // (1)Schedule the handling event immediately;
            // (2)Update flag in that event handler.

            ev_handle_biu_req_.schedule(sparta::Clock::Cycle(0));
            // NOTE:
            // The handling event must be scheduled immediately (0 delay). Otherwise,
            // BIU could potentially send another request to MSS before the busy flag is set
        }
        else {
            ILOG("This request cannot be serviced right now, MSS is already busy!");
        }
    }

    // Handle BIU request
    void BIU::handleBIUReq_()
    {
        biu_busy_ = true;

        const auto & req = biu_req_queue_.front();
        const uint64_t addr = req->getPhyAddr();

        bool routed = false;
        for (size_t i = 0; i < mapped_devices_.size(); ++i) {
            const auto& device = mapped_devices_[i];
            if (addr >= device.addr && addr < device.addr + device.size) {
                out_device_req_sync_[i]->send(req, biu_latency_);
                ILOG("BIU request sent to " << device.device_name << "! Addr: 0x" << std::hex << addr);
                routed = true;
                break;
            }
        }

        if (!routed) {
            out_mss_req_sync_.send(req, biu_latency_);
            ILOG("BIU request sent to MSS! Addr: 0x" << std::hex << addr);
        }
    }

    // Handle MSS Ack
    void BIU::handleMSSAck_()
    {
        out_biu_resp_.send(biu_req_queue_.front(), biu_latency_);

        biu_req_queue_.pop_front();

        // Send out a credit to L2Cache, as we just created space in biu_req_queue_
        out_biu_credits_.send(1);

        biu_busy_ = false;

        // Schedule BIU request handling event only when:
        // (1)BIU is not busy, and (2)Request queue is not empty
        if (biu_req_queue_.size() > 0) {
            ev_handle_biu_req_.schedule(sparta::Clock::Cycle(0));
        }

        ILOG("BIU response sent back (from MSS)!");
    }

    // Handle Generic Device Ack
    void BIU::handleDeviceAck_()
    {
        out_biu_resp_.send(biu_req_queue_.front(), biu_latency_);

        biu_req_queue_.pop_front();

        // Send out a credit to L2Cache, as we just created space in biu_req_queue_
        out_biu_credits_.send(1);

        biu_busy_ = false;

        // Schedule BIU request handling event only when:
        // (1)BIU is not busy, and (2)Request queue is not empty
        if (biu_req_queue_.size() > 0) {
            ev_handle_biu_req_.schedule(sparta::Clock::Cycle(0));
        }

        ILOG("BIU response sent back (from Device)!");
    }

    // Receive MSS access acknowledge
    void BIU::getAckFromMSS_(const bool & done)
    {
        if (done) {
            ev_handle_mss_ack_.schedule(sparta::Clock::Cycle(0));

            ILOG("MSS Ack is received!");

            return;
        }

        // Right now we expect MSS ack is always true
        sparta_assert(false, "MSS is NOT done!");
    }

    // Receive Generic Device access acknowledge
    void BIU::getAckFromDevice_(const bool & done)
    {
        if (done) {
            ev_handle_device_ack_.schedule(sparta::Clock::Cycle(0));

            ILOG("Device Ack is received!");

            return;
        }

        // Right now we expect Device ack is always true
        sparta_assert(false, "Device is NOT done!");
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////

    // Append BIU request queue
    void BIU::appendReqQueue_(const olympia::MemoryAccessInfoPtr& memory_access_info_ptr)
    {
        sparta_assert(biu_req_queue_.size() <= biu_req_queue_size_ ,"BIU request queue overflows!");

        // Push new requests from back
        biu_req_queue_.emplace_back(memory_access_info_ptr);

        ILOG("Append BIU request queue!");
    }

}
