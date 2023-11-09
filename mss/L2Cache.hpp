// <L2Cache.h> -*- C++ -*-



#pragma once

#include <algorithm>
#include <math.h>

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
#include "sparta/ports/SyncPort.hpp"
#include "sparta/resources/Pipe.hpp"
#include "sparta/resources/Pipeline.hpp"

#include "Inst.hpp"
#include "CoreTypes.hpp"
#include "MemoryAccessInfo.hpp"

#include "SimpleDL1.hpp"
#include "LSU.hpp"

namespace olympia_mss
{
    class L2Cache : public sparta::Unit
    {
    public:
        //! Parameters for L2Cache model
        class L2CacheParameterSet : public sparta::ParameterSet {
        public:
            // Constructor for L2CacheParameterSet
            L2CacheParameterSet(sparta::TreeNode* n):
                sparta::ParameterSet(n)
            { }

            PARAMETER(uint32_t, dcache_req_queue_size, 8, "DCache request queue size")
            PARAMETER(uint32_t, dcache_resp_queue_size, 4, "DCache resp queue size")
            PARAMETER(uint32_t, il1_req_queue_size, 8, "IL1 request queue size")
            PARAMETER(uint32_t, il1_resp_queue_size, 4, "IL1 resp queue size")
            PARAMETER(uint32_t, biu_req_queue_size, 4, "BIU request queue size")
            PARAMETER(uint32_t, biu_resp_queue_size, 4, "BIU resp queue size")
            PARAMETER(uint32_t, pipeline_req_queue_size, 64, "Pipeline request buffer size")
            PARAMETER(uint32_t, miss_pending_buffer_size, 64, "Pipeline request buffer size")
            
            // Parameters for the L2 cache
            PARAMETER(uint32_t, l2_line_size, 64, "L2 line size (power of 2)")
            PARAMETER(uint32_t, l2_size_kb, 512, "Size of L2 in KB (power of 2)")
            PARAMETER(uint32_t, l2_associativity, 16, "L2 associativity (power of 2)")
            PARAMETER(bool, l2_always_hit, false, "L2 will always hit")

            PARAMETER(uint32_t, l2cache_biu_credits, 1, "Starting credits for BIU")
            PARAMETER(uint32_t, l2cache_latency, 7, "Cache Lookup HIT latency")
        };

        // Constructor for L2Cache
        // node parameter is the node that represent the L2Cache and p is the L2Cache parameter set
        L2Cache(sparta::TreeNode* node, const L2CacheParameterSet* p);

        // name of this resource.
        static const char name[];

        ////////////////////////////////////////////////////////////////////////////////
        // Type Name/Alias Declaration
        ////////////////////////////////////////////////////////////////////////////////

    private:

        // Friend class used in rename testing
        friend class L2CacheTester;

        ////////////////////////////////////////////////////////////////////////////////
        // Statistics and Counters
        ////////////////////////////////////////////////////////////////////////////////
        // sparta::StatisticDef       cycles_per_instr_;          // A simple expression to calculate IPC
        sparta::Counter num_reqs_from_dcache_;        // Counter of number instructions received from DCache
        sparta::Counter num_reqs_from_il1_;        // Counter of number instructions received from IL1
        sparta::Counter num_reqs_to_biu_;          // Counter of number instructions forwarded to BIU -- Totals misses
        sparta::Counter num_acks_from_biu_;        // Counter of number acks received from BIU
        sparta::Counter num_acks_to_il1_;          // Counter of number acks provided to IL1
        sparta::Counter num_acks_to_dcache_;          // Counter of number acks provided to DCache

        sparta::Counter num_resps_from_biu_;       // Counter of number responses received from BIU
        sparta::Counter num_resps_to_il1_;         // Counter of number responses provided to IL1
        sparta::Counter num_resps_to_dcache_;         // Counter of number responses provided to DCache

        sparta::Counter l2_cache_hits_;            // Counter of number L2 Cache Hits
        sparta::Counter l2_cache_misses_;          // Counter of number L2 Cache Misses

        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::DataInPort<olympia::InstPtr> in_dcache_l2cache_req_
            {&unit_port_set_, "in_dcache_l2cache_req", 1};

        sparta::DataInPort<olympia::InstPtr> in_il1_l2cache_req_
            {&unit_port_set_, "in_il1_l2cache_req", 1};

        sparta::DataInPort<olympia::InstPtr> in_biu_resp_
            {&unit_port_set_, "in_biu_l2cache_resp", 1};

        sparta::DataInPort<bool> in_biu_ack_
            {&unit_port_set_, "in_biu_l2cache_ack", 1};


        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////

        sparta::DataOutPort<olympia::InstPtr> out_biu_req_
            {&unit_port_set_, "out_l2cache_biu_req"};

        sparta::DataOutPort<olympia::InstPtr> out_l2cache_il1_resp_
            {&unit_port_set_, "out_l2cache_il1_resp"};

        sparta::DataOutPort<olympia::InstPtr> out_l2cache_dcache_resp_
            {&unit_port_set_, "out_l2cache_dcache_resp"};

        sparta::DataOutPort<bool> out_l2cache_il1_ack_
            {&unit_port_set_, "out_l2cache_il1_ack"};

        sparta::DataOutPort<bool> out_l2cache_dcache_ack_
            {&unit_port_set_, "out_l2cache_dcache_ack"};


        ////////////////////////////////////////////////////////////////////////////////
        // Internal States
        ////////////////////////////////////////////////////////////////////////////////

        
        using CacheRequestQueue = std::vector<olympia::InstPtr>;

        // Buffers for the incoming requests from DCache and IL1
        CacheRequestQueue dcache_req_queue_;
        CacheRequestQueue il1_req_queue_;

        const uint32_t dcache_req_queue_size_;
        const uint32_t il1_req_queue_size_;

        // Buffers for the outgoing requests from L2Cache
        CacheRequestQueue biu_req_queue_;

        const uint32_t biu_req_queue_size_;

        // Buffers for the incoming resps from BIU
        CacheRequestQueue biu_resp_queue_;

        const uint32_t biu_resp_queue_size_;

        // Buffers for the outgoing resps to DCache and IL1
        CacheRequestQueue dcache_resp_queue_;
        CacheRequestQueue il1_resp_queue_;

        const uint32_t dcache_resp_queue_size_;
        const uint32_t il1_resp_queue_size_;

        // Channels enum
        enum class Channel : uint32_t
        {
            NO_ACCESS = 0,
            __FIRST = NO_ACCESS,
            BIU,
            IL1,
            DCACHE,
            NUM_CHANNELS,
            __LAST = NUM_CHANNELS
        };
        

        // Cache Pipeline
        class PipelineStages {
            public:
                PipelineStages(uint32_t latency) : NUM_STAGES(latency),
                                                   HIT_MISS_HANDLING(NUM_STAGES - 1),
                                                   CACHE_LOOKUP(HIT_MISS_HANDLING - 1) {}

            const uint32_t NO_ACCESS = 0;
            const uint32_t CACHE_LOOKUP;
            const uint32_t HIT_MISS_HANDLING;
            const uint32_t NUM_STAGES;
        };
        
	    // Skipping the use of SpartaSharedPointer due to allocator bug
	    // Instead using std::shared_ptr. That works cleanly.
        using L2MemoryAccessInfoPtr = std::shared_ptr<olympia::MemoryAccessInfo>;
        using L2UnitName = olympia::MemoryAccessInfo::UnitName;
        using L2CacheState = olympia::MemoryAccessInfo::CacheState;
        using L2CachePipeline = sparta::Pipeline<L2MemoryAccessInfoPtr>;
        
        PipelineStages stages_;
        L2CachePipeline l2cache_pipeline_;
        
        sparta::Queue<L2MemoryAccessInfoPtr> pipeline_req_queue_;
        const uint32_t pipeline_req_queue_size_;
        uint32_t inFlight_reqs_ = 0;
        
        sparta::Buffer<std::shared_ptr<olympia::MemoryAccessInfo>> miss_pending_buffer_;
        const uint32_t miss_pending_buffer_size_;
        

        // L2 Cache
        using CacheHandle = olympia::SimpleDL1::Handle;
        const uint32_t l2_lineSize_;
        const uint32_t shiftBy_;
        CacheHandle l2_cache_;
        const bool l2_always_hit_;

        // Local state variables
        uint32_t l2cache_biu_credits_ = 0;
        sparta::State<Channel> channel_select_ = Channel::IL1;
        const uint32_t l2cache_latency_;

        ////////////////////////////////////////////////////////////////////////////////
        // Event Handlers
        ////////////////////////////////////////////////////////////////////////////////

        // Event to handle L2Cache request from DCache
        sparta::UniqueEvent<> ev_handle_dcache_l2cache_req_
            {&unit_event_set_, "ev_handle_dcache_l2cache_req", CREATE_SPARTA_HANDLER(L2Cache, handle_DCache_L2Cache_Req_)};

        // Event to handle L2Cache request from IL1
        sparta::UniqueEvent<> ev_handle_il1_l2cache_req_
            {&unit_event_set_, "ev_handle_il1_l2cache_req", CREATE_SPARTA_HANDLER(L2Cache, handle_IL1_L2Cache_Req_)};

        // Event to handle L2Cache resp for IL1
        sparta::UniqueEvent<> ev_handle_l2cache_il1_resp_
            {&unit_event_set_, "ev_handle_l2cache_il1_resp", CREATE_SPARTA_HANDLER(L2Cache, handle_L2Cache_IL1_Resp_)};

        // Event to handle L2Cache resp for DCache
        sparta::UniqueEvent<> ev_handle_l2cache_dcache_resp_
            {&unit_event_set_, "ev_handle_l2cache_dcache_resp", CREATE_SPARTA_HANDLER(L2Cache, handle_L2Cache_DCache_Resp_)};

        // Event to handle L2Cache request to BIU
        sparta::UniqueEvent<> ev_handle_l2cache_biu_req_
            {&unit_event_set_, "ev_handle_l2cache_biu_req", CREATE_SPARTA_HANDLER(L2Cache, handle_L2Cache_BIU_Req_)};

        // Event to handle L2Cache request to BIU
        sparta::UniqueEvent<> ev_handle_biu_l2cache_resp_
            {&unit_event_set_, "ev_handle_biu_l2cache_resp", CREATE_SPARTA_HANDLER(L2Cache, handle_BIU_L2Cache_Resp_)};

        // Event to handle L2Cache ack for IL1
        sparta::UniqueEvent<> ev_handle_l2cache_il1_ack_
            {&unit_event_set_, "ev_handle_l2cache_il1_ack", CREATE_SPARTA_HANDLER(L2Cache, handle_L2Cache_IL1_Ack_)};

        // Event to handle L2Cache ack for DCache
        sparta::UniqueEvent<> ev_handle_l2cache_dcache_ack_
            {&unit_event_set_, "ev_handle_l2cache_dcache_ack", CREATE_SPARTA_HANDLER(L2Cache, handle_L2Cache_DCache_Ack_)};

        // Event to create request for pipeline and feed it to the pipeline_req_queue_
        sparta::UniqueEvent<> ev_create_req_
            {&unit_event_set_, "create_req", CREATE_SPARTA_HANDLER(L2Cache, create_Req_)};
        
        // Event to issue request to pipeline
        sparta::UniqueEvent<> ev_issue_req_
            {&unit_event_set_, "issue_req", CREATE_SPARTA_HANDLER(L2Cache, issue_Req_)};

        ////////////////////////////////////////////////////////////////////////////////
        // Callbacks
        ////////////////////////////////////////////////////////////////////////////////

        // Receive new L2Cache request from DCache
        void getReqFromDCache_(const olympia::InstPtr &);

        // Receive new L2Cache request from IL1
        void getReqFromIL1_(const olympia::InstPtr &);

        // Receive BIU access Response
        void getRespFromBIU_(const olympia::InstPtr &);

        // Receive BIU ack Response
        void getAckFromBIU_(const bool &);

        // Handle L2Cache request from DCache
        void handle_DCache_L2Cache_Req_();

        // Handle L2Cache request from IL1
        void handle_IL1_L2Cache_Req_();

        // Handle L2Cache request to BIU
        void handle_L2Cache_BIU_Req_();

        // Handle L2Cahe resp for IL1
        void handle_L2Cache_IL1_Resp_();

        // Handle L2Cahe resp for DCache
        void handle_L2Cache_DCache_Resp_();

        // Handle L2Cahe ack for IL1
        void handle_L2Cache_IL1_Ack_();

        // Handle L2Cahe ack for DCache
        void handle_L2Cache_DCache_Ack_();

        // Handle BIU resp to L2Cache
        void handle_BIU_L2Cache_Resp_();


        // Pipeline request create callback
        void create_Req_();
        
        // Pipeline request issue callback
        void issue_Req_();
        

        // Pipeline callbacks
        // Stage 1
        void handleCacheAccessRequest_();

        // Stage 2
        void handleCacheAccessResult_();


        ////////////////////////////////////////////////////////////////////////////////
        // Regular Function/Subroutine Call
        ////////////////////////////////////////////////////////////////////////////////

        // Append L2Cache request queue for reqs from DCache
        void appendDCacheReqQueue_(const olympia::InstPtr &);

        // Append L2Cache request queue for reqs from IL1
        void appendIL1ReqQueue_(const olympia::InstPtr &);

        // Append L2Cache request queue for reqs to BIU
        void appendBIUReqQueue_(const olympia::InstPtr &);

        // Append L2Cache resp queue for resps from BIU
        void appendBIURespQueue_(const olympia::InstPtr &);

        // Append L2Cache resp queue for resps to DCache BIU
        void appendDCacheRespQueue_(const olympia::InstPtr &);

        // Append L2Cache resp queue for resps to IL1
        void appendIL1RespQueue_(const olympia::InstPtr &);


        
    	// Select the channel to pick the request from
        // Current options : 
        //       BIU - P0
        //       DCache - P1 - RoundRobin Candidate
        //       DL1 - P1 - RoundRobin Candidate
        Channel arbitrateL2CacheAccessReqs_();
        
	    // Cache lookup for a HIT or MISS on a given request 
        L2CacheState cacheLookup_(std::shared_ptr<olympia::MemoryAccessInfo>);
        
	    // Allocating the cacheline in the L2 bbased on return from BIU/L3
        void reloadCache_(uint64_t);

        // Return the resp to the master units
        void sendOutResp_(const L2UnitName&, const olympia::InstPtr&);

        // Send the request to the slave units
        void sendOutReq_(const L2UnitName&, const olympia::InstPtr&);

        // Check if there are enough credits for the request to be issued to the l2cache_pipeline_
        bool hasCreditsForPipelineIssue_();
    };

    class L2CacheTester;
}
