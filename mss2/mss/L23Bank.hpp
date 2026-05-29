#pragma once

#include <memory>
#include "sparta/memory/AddressTypes.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "L23InstInfo.hpp"
#include "L23Cache.hpp"

namespace olympia_mss {

class L23;

////////////////////////////////////////////////////////////////////////////////
// L23 Bank
////////////////////////////////////////////////////////////////////////////////
class L23Bank{

public:
    
    std::string name = "l23bank";

    public:
            
        ////////////////////////////////////////////////////////////////////////////////
        // Type Name/Alias Declaration
        ////////////////////////////////////////////////////////////////////////////////
        using L23Pipeline = sparta::Pipeline<L23InstInfoPtr>;
        using L23InstQueue = sparta::Queue<L23InstInfoPtr>;
        using L23InstBuffer = sparta::Buffer<L23InstInfoPtr>;
        using MemAccess = olympia::MemoryAccessInfo;
        using MemAccessPtr = olympia::MemoryAccessInfoPtr;
        using L23UnitName = olympia::MemoryAccessInfo::UnitName;
        using PipeState = L23InstInfo::L23PipeState;
        using MrqState = L23InstInfo::L23MrqState;
        
        class PipelineStages {
            public:
                PipelineStages(uint32_t latency) : NUM_STAGES(latency),
                                                   CACHE_READ(NUM_STAGES - 1),
                                                   MRQ_LOOKUP(2),
                                                   CACHE_LOOKUP(1) {
                    sparta_assert(NUM_STAGES >= 4, "Latency for L23 cannot be less than 4 cycles!");
                }

            const uint32_t NUM_STAGES;
            const uint32_t CACHE_READ;
            const uint32_t MRQ_LOOKUP;
            const uint32_t CACHE_LOOKUP;
            const uint32_t NO_ACCESS = 0;
        };

        // Constructor
        L23Bank (const uint32_t&                 bank_id,
                L23                                 *l23,
                sparta::TreeNode*                   node,
                sparta::log::MessageSource&  info_logger,
                sparta::log::MessageSource& debug_logger);

        // Check if there are enough credits for the request to be issued to the l23_pipeline_
        bool hasCreditsForPipelineIssue_(const uint32_t& row_id);

        // Pipeline request issue callback
        void issue_Req_();

        // Decrement the scheduling delay counter
        void update_Bank_Scheduling_Delay_();

        // Event to issue request to pipeline
        sparta::UniqueEvent<>* uev_issue_req_; 

        // Event to decrement the scheduling delay counter
        sparta::UniqueEvent<>* uev_update_bank_scheduling_delay_;

        // Pipeline callbacks
        // Stage 1
        void handleCacheLookup_();

        // Stage 2
        void handleMRQLookup_();

        // Stage 3
        void handleCacheRead_();

        const uint32_t bank_id_;
        const uint32_t num_rows_;
        L23* l23_ {nullptr};
        sparta::log::MessageSource& info_logger_;
        sparta::log::MessageSource& debug_logger_;
        sparta::StatisticSet*       unit_stat_set_;
        sparta::EventSet*           unit_event_set_;

        PipelineStages stages_;
        std::vector<std::unique_ptr<L23Pipeline>> l23_pipeline_;
        
        std::vector<std::unique_ptr<L23InstQueue>> l1_pipe_read_req_queue_;
        std::vector<std::unique_ptr<L23InstQueue>> l1_pipe_write_req_queue_;
        std::vector<std::unique_ptr<L23InstQueue>> biu_pipe_req_queue_;
        
        const uint32_t max_bank_scheduling_delay_ = 0;
        uint32_t bank_scheduling_delay_counter_ = 0;

        std::vector<std::unique_ptr<L23InstBuffer>> miss_pending_buffer_;

        // Stats
        sparta::Counter num_reqs_issued_;             // Counter for total number of reqs issued for this bank
        sparta::Counter num_read_reqs_issued_;        // Counter for number of read reqs issued for this bank
        sparta::Counter num_write_reqs_issued_;       // Counter for number of write reqs issued for this bank
        sparta::Counter num_biu_reloads_issued_;      // Counter for number of biu reqs issued for this bank

        sparta::Counter num_bank_stalls_;              // Counter for total number stalls for this bank
        sparta::Counter num_miss_pending_buffer_hits_; // Counter for total number of hits in miss_pending_buffer_
        sparta::Counter num_read_mpb_hits_;            // Counter for total number of read hits in miss_pending_buffer_
        sparta::Counter num_write_mpb_hits_;           // Counter for total number of write hits in miss_pending_buffer_
        sparta::Counter num_pf_mpb_hits_;              // Counter for total number of prefetch hits in miss_pending_buffer_

};

} // namespace olympia_mss
