// <BIU.cpp> -*- C++ -*-

#include "sparta/utils/SpartaAssert.hpp"
#include "sparta/utils/LogUtils.hpp"

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
        i2c_addr_(p->i2c_addr),
        i2c_size_(p->i2c_size)
    {
        in_biu_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(BIU, receiveReqFromL2Cache_, olympia::MemoryAccessInfoPtr));

        in_mss_ack_sync_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(BIU, getAckFromMSS_, bool));
        in_mss_ack_sync_.setPortDelay(static_cast<sparta::Clock::Cycle>(1));

        in_i2c_ack_sync_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(BIU, getAckFromI2C_, bool));
        in_i2c_ack_sync_.setPortDelay(static_cast<sparta::Clock::Cycle>(1));

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(BIU, sendInitialCredits_));
        ILOG("BIU construct: #" << node->getGroupIdx());

        ev_handle_mss_ack_ >> ev_handle_biu_req_;
        ev_handle_i2c_ack_ >> ev_handle_biu_req_;
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

        if (addr >= i2c_addr_ && addr < i2c_addr_ + i2c_size_) {
            out_i2c_req_sync_.send(req, biu_latency_);
            ILOG("BIU request sent to I2C! Addr: 0x" << std::hex << addr);
        } else {
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

    // Handle I2C Ack
    void BIU::handleI2CAck_()
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

        ILOG("BIU response sent back (from I2C)!");
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

    // Receive I2C access acknowledge
    void BIU::getAckFromI2C_(const bool & done)
    {
        if (done) {
            ev_handle_i2c_ack_.schedule(sparta::Clock::Cycle(0));

            ILOG("I2C Ack is received!");

            return;
        }

        // Right now we expect I2C ack is always true
        sparta_assert(false, "I2C is NOT done!");
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