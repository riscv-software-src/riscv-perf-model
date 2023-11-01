#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/resources/Pipeline.hpp"
#include "sparta/resources/Buffer.hpp"
#include "SimpleDL1.hpp"
#include "Inst.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "MemoryAccessInfo.hpp"
#include <cache/AddrDecoderIF.hpp>
#include <sparta/events/SchedulingPhases.hpp>
#include <sparta/utils/SpartaSharedPointer.hpp>

namespace olympia {
    class DCache : public sparta::Unit {
    public:
        class CacheParameterSet : public sparta::ParameterSet {
        public:
            CacheParameterSet(sparta::TreeNode *n) : sparta::ParameterSet(n) {}

            // Parameters for the DL1 cache
            PARAMETER(uint32_t, l1_line_size, 64, "DL1 line size (power of 2)")
            PARAMETER(uint32_t, l1_size_kb, 32, "Size of DL1 in KB (power of 2)")
            PARAMETER(uint32_t, l1_associativity, 8, "DL1 associativity (power of 2)")
            PARAMETER(uint32_t, cache_latency, 1, "Assumed latency of the memory system")
            PARAMETER(bool, l1_always_hit, false, "DL1 will always hit")
            PARAMETER(uint32_t, mshr_entries, 8, "Number of MSHR Entries")
            PARAMETER(uint32_t, load_miss_queue_size, 8, "Load miss queue size")
        };

        static const char name[];
        DCache(sparta::TreeNode *n, const CacheParameterSet *p);

        ~DCache();

        class MSHREntryInfo;
        
        using MSHREntryInfoPtr = sparta::SpartaSharedPointer<MSHREntryInfo>;

        enum class PipelineStage
        {
            LOOKUP = 0,
            DATA_READ = 1,
            NUM_STAGES
        };

        class MSHREntryInfo {
        public:
            MSHREntryInfo(const uint64_t& block_address, const uint64_t& line_size, const uint32_t load_miss_queue_size, const sparta::Clock* clock):
                line_fill_buffer_(line_size), 
                block_address_(block_address),
                load_miss_queue_("load_miss_queue", load_miss_queue_size, clock)
            { line_fill_buffer_.setValid(true); }

            ~MSHREntryInfo() {
            }

            const uint64_t & getBlockAddress() const {
                return block_address_;
            }

            void setBlockAddress(uint64_t block_address) {
                block_address_ = block_address;
                line_fill_buffer_.setAddr(block_address);
            }

            SimpleCacheLine& getLineFillBuffer() {
                return line_fill_buffer_;
            }

            bool isValid() {
                return line_fill_buffer_.isValid();
            }

            void setValid(bool v) {
                line_fill_buffer_.setValid(v);
            }

            bool isModified() {
                return line_fill_buffer_.isModified();
            }

            void setModified(bool m) {
                line_fill_buffer_.setModified(m);
            }

            void setDataArrived(bool v) {
                data_arrived_ = v;
            }

            const bool& getDataArrived() {
                return data_arrived_;
            }

            void enqueueLoad(MemoryAccessInfoPtr mem_access_info_ptr) {
                load_miss_queue_.push(mem_access_info_ptr);
            }

            MemoryAccessInfoPtr dequeueLoad() {
                if (load_miss_queue_.empty())
                    return nullptr;

                MemoryAccessInfoPtr mem_access_info_ptr = load_miss_queue_.front();
                load_miss_queue_.pop();

                return mem_access_info_ptr;
            }

            bool isLoadMissQueueFull() const{
                return (load_miss_queue_.numFree() == 0);
            }
            
        private:
            SimpleCacheLine line_fill_buffer_;
            uint64_t block_address_;
            sparta::Queue<MemoryAccessInfoPtr> load_miss_queue_;
            bool data_arrived_ = false;
        };

        // allocator
        sparta::SpartaSharedPointerAllocator<MSHREntryInfo> mshr_entry_allocator;

    private:
        using MSHRFile = sparta::Buffer<MSHREntryInfoPtr>;
        MSHRFile mshr_file_;

        using L1Handle = SimpleDL1::Handle;
        L1Handle l1_cache_;
        const sparta::cache::AddrDecoderIF *addr_decoder_;
        const bool l1_always_hit_;
        const uint32_t cache_latency_;
        const uint64_t cache_line_size_;
        const uint32_t max_mshr_entries_;
        const uint32_t load_miss_queue_size_;

        // To keep track of cache refill requests
        bool ongoing_cache_refill_ = false;

        // To arbitrate between incoming request from LSU and Cache refills from BIU
        bool incoming_cache_refill_ = false;

        // Ongoing Refill request
        sparta::Buffer<MSHREntryInfoPtr>::iterator current_refill_mshr_entry_;

        // Cache Lookup
        bool cacheLookup_(const MemoryAccessInfoPtr &mem_access_info_ptr);

        // MSHR entry lookup in MSHR File
        sparta::Buffer<MSHREntryInfoPtr>::iterator mshrLookup_(const uint64_t &block_address);

        // Callback function for handling incoming Cache requests from LSU
        void processInstsFromLSU_(const MemoryAccessInfoPtr &memory_access_info_ptr);

        // Callback function for handling incoming cache refill data from BIU
        void getRefillFromBIU_(const InstPtr &inst_ptr);

        void incomingRefillBIU_();

        // Allocate MSHR entry in MSHR File
        sparta::Buffer<MSHREntryInfoPtr>::iterator allocateMSHREntry_(uint64_t block_address);

        // Cache Lookup Stage
        void lookupHandler_();

        // Cache Data Read Stage
        void dataHandler_();

        // Issue cache refill request to BIU
        void issueDownstreamRead_();

        // Write Line Buffer data to Cache
        void cacheRefillWrite_();

        // Cache Pipeline
        sparta::Pipeline<MemoryAccessInfoPtr> cache_pipeline_
            {"CachePipeline", static_cast<uint32_t>(PipelineStage::NUM_STAGES), getClock()};

        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<MemoryAccessInfoPtr> in_lsu_lookup_req_
                {&unit_port_set_, "in_lsu_lookup_req", 0};

        sparta::DataInPort<InstPtr> in_biu_ack_
                {&unit_port_set_, "in_biu_ack", sparta::SchedulingPhase::Update, 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::SignalOutPort out_lsu_lookup_nack_
                {&unit_port_set_, "out_lsu_lookup_nack", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_lookup_resp_
                {&unit_port_set_, "out_lsu_lookup_resp", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_data_ready_
                {&unit_port_set_, "out_lsu_data_ready", 0};

        sparta::DataOutPort<InstPtr> out_biu_req_
                {&unit_port_set_, "out_biu_req",0};

        ////////////////////////////////////////////////////////////////////////////////
        // Events
        ////////////////////////////////////////////////////////////////////////////////
        sparta::UniqueEvent<sparta::SchedulingPhase::Update> uev_issue_downstream_read_{&unit_event_set_, "issue_downstream_read",
                CREATE_SPARTA_HANDLER(DCache, issueDownstreamRead_)};

        sparta::UniqueEvent<sparta::SchedulingPhase::Update> uev_cache_refill_write_{&unit_event_set_, "cache_refill_write",
                CREATE_SPARTA_HANDLER(DCache, cacheRefillWrite_)};

        sparta::UniqueEvent<sparta::SchedulingPhase::Update> uev_biu_incoming_refill_{&unit_event_set_, "biu_incoming_refill",
                CREATE_SPARTA_HANDLER(DCache, incomingRefillBIU_)};

        ////////////////////////////////////////////////////////////////////////////////
        // Counters
        ////////////////////////////////////////////////////////////////////////////////
        sparta::Counter dl1_cache_hits_{
                getStatisticSet(), "dl1_cache_hits",
                "Number of DL1 cache hits", sparta::Counter::COUNT_NORMAL
        };

        sparta::Counter dl1_cache_misses_{
                getStatisticSet(), "dl1_cache_misses",
                "Number of DL1 cache misses", sparta::Counter::COUNT_NORMAL
        };
    };

}
