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
        l3_resp_queue_size_(p->l3_resp_queue_size),
        gen_(p->biu_delay_seed),
        delay_dist_(p->biu_delay_min, p->biu_delay_max),
        random_dist_(0.0, 1.0) {
        
        in_l3_req_.registerConsumerHandler
            (CREATE_SPARTA_HANDLER_WITH_DATA(BIU, receiveReqFromL3_, olympia::MemoryAccessInfoPtr));
        
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(BIU, sendInitialCredits_));
        ILOG("BIU construct: #" << node->getGroupIdx());
    }


    ////////////////////////////////////////////////////////////////////////////////
    // Callbacks
    ////////////////////////////////////////////////////////////////////////////////

    // Sending Initial credits to L3
    void BIU::sendInitialCredits_() {
        out_l3_credits_.send(biu_req_queue_size_);
        ILOG("Sending initial credits to L3 : " << biu_req_queue_size_);
    }

    // Receive new BIU request from L3
    void BIU::receiveReqFromL3_(const olympia::MemoryAccessInfoPtr & ma_ptr) {
        appendReqQueue_(ma_ptr);

        // Schedule BIU request handling event only when:
        ev_handle_biu_req_.schedule(sparta::Clock::Cycle(0));
    }

    // Handle BIU request
    void BIU::handle_BIU_Req_() {
        const olympia::MemoryAccessInfoPtr& ma_ptr = biu_req_queue_.front();
        
        uint32_t delay = delay_dist_(gen_);

        if (ma_ptr->getDestUnit() != olympia::MemoryAccessInfo::UnitName::BIU) {
            pev_handle_biu_l3_resp_append_.preparePayload(biu_req_queue_.front())->schedule(delay);
            biu_req_holding_queue_.emplace_back(biu_req_queue_.front());
            ILOG("BIU delay = " << delay << " MA : " << biu_req_queue_.front());
        }
        else if (ma_ptr->getDestUnit() == olympia::MemoryAccessInfo::UnitName::BIU &&
                 ma_ptr->getReqType()  == olympia::MemoryAccessInfo::RequestType::WRITE) {

            ILOG("BIU write request is completed!");
        }
        else {
            sparta_assert(false, "What case is this?");
        }
        
        biu_req_queue_.pop_front();
        if (!biu_req_queue_.empty()) {
            ev_handle_biu_req_.schedule(sparta::Clock::Cycle(1));
        }

        // Send out the ack to L3 through , we just created space in biu_req_queue_
        ev_handle_biu_l3_credits_.schedule(sparta::Clock::Cycle(1));
    }

    // Handle ack backto L3
    void BIU::handle_BIU_L3_Credits_() {
        uint32_t available_slots = biu_req_queue_size_ - biu_req_queue_.size();
        out_l3_credits_.send(available_slots);

        ILOG("BIU->L3 :  Credits are sent : " << available_slots);
    }

    // Append L3 Resp Queue after a given delay
    void BIU::handle_BIU_L3_Resp_Append_(const olympia::MemoryAccessInfoPtr & ma_ptr) {
        // Push this MA to l3 resp queue
        l3_resp_queue_.emplace_back(ma_ptr);
        
        // find the req from biu_req_queue and erase it
        auto req = std::find(biu_req_holding_queue_.begin(), biu_req_holding_queue_.end(), ma_ptr);
        biu_req_holding_queue_.erase(req);

        ev_handle_biu_l3_resp_.schedule(sparta::Clock::Cycle(1));

        ILOG("l3_resp_queue_ is appended with MA : " << ma_ptr);
    }

    // Handle Resp back to L3
    void BIU::handle_BIU_L3_Resp_() {
        auto req = l3_resp_queue_.front();
        out_biu_resp_.send(req);

        ILOG("BIU->L3 :  Resp is sent with MA: " << req);
        l3_resp_queue_.pop_front();

        if (!l3_resp_queue_.empty()) {
            ev_handle_biu_l3_resp_.schedule(sparta::Clock::Cycle(1));
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Regular Function/Subroutine Call
    ////////////////////////////////////////////////////////////////////////////////

    // Append BIU request queue
    void BIU::appendReqQueue_(const olympia::MemoryAccessInfoPtr& inst_ptr) {
        sparta_assert(biu_req_queue_.size() <= biu_req_queue_size_ ,"BIU request queue overflows!");

        // Push new requests from back
        biu_req_queue_.emplace_back(inst_ptr);

        ILOG("Append BIU request queue!");
    }
}
