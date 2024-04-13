#pragma once

#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/resources/Pipeline.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "CacheFuncModel.hpp"
#include "Inst.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "MemoryAccessInfo.hpp"
#include "MSHREntryInfo.hpp"

namespace olympia
{
    class DCache : public sparta::Unit
    {
      public:
        class CacheParameterSet : public sparta::ParameterSet
        {
          public:
            CacheParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

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
        DCache(sparta::TreeNode* n, const CacheParameterSet* p);

      private:
        ////////////////////////////////////////////////////////////////////////////////
        // L1 Cache Handling
        ////////////////////////////////////////////////////////////////////////////////
        using L1Handle = CacheFuncModel::Handle;
        L1Handle l1_cache_;
        const bool l1_always_hit_;
        const uint32_t cache_latency_;
        const uint64_t cache_line_size_;
        const sparta::cache::AddrDecoderIF* addr_decoder_;
        // Keep track of the instruction that causes current outstanding cache miss
        //        MemoryAccessInfoPtr cache_pending_inst_ = nullptr;

        const uint32_t num_mshr_entries_;
        const uint32_t load_miss_queue_size_;

        void setupL1Cache_(const CacheParameterSet* p);

        bool dataLookup_(const MemoryAccessInfoPtr & mem_access_info_ptr);

        void reloadCache_(uint64_t phy_addr);

        uint64_t getBlockAddr(const MemoryAccessInfoPtr & mem_access_info_ptr);

        // To arbitrate between incoming request from LSU and Cache refills from BIU
        //        bool incoming_cache_refill_ = false;
        MemoryAccessInfoPtr incoming_cache_refill_ = nullptr;

        using MSHREntryInfoPtr = sparta::SpartaSharedPointer<MSHREntryInfo>;
        using MSHREntryIterator = sparta::Buffer<MSHREntryInfoPtr>::const_iterator;
        // Ongoing Refill request
        MSHREntryIterator current_refill_mshr_entry_;

        // Cache Pipeline
        enum class PipelineStage
        {
            LOOKUP = 0,
            DATA_READ = 1,
            DEALLOCATE = 2,
            NUM_STAGES
        };

        sparta::Pipeline<MemoryAccessInfoPtr> cache_pipeline_{
            "DCachePipeline", static_cast<uint32_t>(PipelineStage::NUM_STAGES), getClock()};

        void handleLookup_();
        void handleDataRead_();
        void handleDeallocate_();

        ////////////////////////////////////////////////////////////////////////////////
        // Handle requests
        ////////////////////////////////////////////////////////////////////////////////

        void getInstsFromLSU_(const MemoryAccessInfoPtr & memory_access_info_ptr);

        void getAckFromL2Cache_(const uint32_t & ack);

        void getRespFromL2Cache_(const MemoryAccessInfoPtr & memory_access_info_ptr);

        void freePipelineAppend_();

        bool busy_ = false;

        bool pipelineFree_ = true;

        // Credit bool for sending miss request to L2Cache
        uint32_t dcache_l2cache_credits_ = 0;

        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<MemoryAccessInfoPtr> in_lsu_lookup_req_{&unit_port_set_,
                                                                   "in_lsu_lookup_req", 0};

        sparta::DataInPort<uint32_t> in_l2cache_ack_{&unit_port_set_, "in_l2cache_ack", 1};

        sparta::DataInPort<MemoryAccessInfoPtr> in_l2cache_resp_{&unit_port_set_, "in_l2cache_resp",
                                                                 1};

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::SignalOutPort out_lsu_free_req_{&unit_port_set_, "out_lsu_free_req", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_lookup_ack_{&unit_port_set_,
                                                                     "out_lsu_lookup_ack", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_lookup_req_{&unit_port_set_,
                                                                     "out_lsu_lookup_req", 1};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_l2cache_req_{&unit_port_set_,
                                                                  "out_l2cache_req", 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Events
        ////////////////////////////////////////////////////////////////////////////////
        sparta::UniqueEvent<> uev_free_pipeline_{
            &unit_event_set_, "issue_inst", CREATE_SPARTA_HANDLER(DCache, freePipelineAppend_)};
        ////////////////////////////////////////////////////////////////////////////////
        // Counters
        ////////////////////////////////////////////////////////////////////////////////
        sparta::Counter dl1_cache_hits_{getStatisticSet(), "dl1_cache_hits",
                                        "Number of DL1 cache hits", sparta::Counter::COUNT_NORMAL};

        sparta::Counter dl1_cache_misses_{getStatisticSet(), "dl1_cache_misses",
                                          "Number of DL1 cache misses",
                                          sparta::Counter::COUNT_NORMAL};


        sparta::Buffer<MSHREntryInfoPtr> mshr_file_;
        MSHREntryInfoAllocator & mshr_entry_allocator;
        void allocateMSHREntry_(const MemoryAccessInfoPtr & mem_access_info_ptr);
        MSHREntryIterator mshrLookup_(const uint64_t block_address);
        void replyLSU(const MemoryAccessInfoPtr & mem_access_info_ptr);
    };

} // namespace olympia
