#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/simulation/ParameterSet.hpp"
#include "sparta/utils/LogUtils.hpp"
#include "sparta/resources/Pipeline.hpp"
#include "sparta/resources/Array.hpp"
#include "SimpleDL1.hpp"
#include "Inst.hpp"
#include "cache/TreePLRUReplacement.hpp"
#include "MemoryAccessInfo.hpp"
#include <cache/AddrDecoderIF.hpp>
#include <sparta/resources/Queue.hpp>

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
        };

        static const char name[];
        DCache(sparta::TreeNode *n, const CacheParameterSet *p);

        enum class PipelineStage
        {
            LOOKUP = 0,
            DATA_READ = 1,
            NUM_STAGES
        };

        class MSHREntry {
        public:
            MSHREntry();
            MSHREntry(const uint64_t& block_address, const uint64_t& line_size):
                line_fill_buffer_(line_size), block_address_(block_address){}

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

            void setDataArrived(bool v) {
                data_arrived_ = v;
            }

            const bool& getDataArrived() {
                return data_arrived_;
            }
            
        private:
            uint64_t block_address_;
            SimpleCacheLine line_fill_buffer_;
            bool data_arrived_ = false;
        };

    private:
        using MSHRFile = sparta::Array<MSHREntry>;
        MSHRFile mshr_file_;

        using L1Handle = SimpleDL1::Handle;
        L1Handle l1_cache_;
        const sparta::cache::AddrDecoderIF *addr_decoder_;
        const bool l1_always_hit_;
        bool busy_;
        uint32_t cache_latency_;
        uint32_t max_mshr_entries_;

        // To keep track of cache refill requests
        bool ongoing_cache_refill_ = false;

        // To arbitrate between incoming request from LSU and Cache refills from BIU
        bool incoming_cache_refill_ = false;

        bool cacheLookup_(const MemoryAccessInfoPtr &mem_access_info_ptr);

        const uint32_t mshrLookup_(const MemoryAccessInfoPtr &mem_access_info_ptr);

        void reloadCache_(uint64_t phy_addr);

        void getInstsFromLSU_(const MemoryAccessInfoPtr &memory_access_info_ptr);

        void getAckFromBIU_(const InstPtr &inst_ptr);

        const uint32_t allocateMSHREntry_(uint64_t block_address);

        void lookupHandler_();

        void dataHandler_();

        void issueDownstreamRead_();

        // Cache Pipeline
        sparta::Pipeline<MemoryAccessInfoPtr> cache_pipeline_
            {"CachePipeline", static_cast<uint32_t>(PipelineStage::NUM_STAGES), getClock()};

        ////////////////////////////////////////////////////////////////////////////////
        // Input Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::DataInPort<MemoryAccessInfoPtr> in_lsu_lookup_req_
                {&unit_port_set_, "in_lsu_lookup_req", 0};

        sparta::DataInPort<InstPtr> in_biu_ack_
                {&unit_port_set_, "in_biu_ack", 0};

        ////////////////////////////////////////////////////////////////////////////////
        // Output Ports
        ////////////////////////////////////////////////////////////////////////////////
        sparta::SignalOutPort out_lsu_lookup_nack_
                {&unit_port_set_, "out_lsu_lookup_nack", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_lookup_ack_
                {&unit_port_set_, "out_lsu_lookup_ack", 0};

        sparta::DataOutPort<MemoryAccessInfoPtr> out_lsu_lookup_req_
                {&unit_port_set_, "out_lsu_lookup_req", 0};

        sparta::DataOutPort<InstPtr> out_biu_req_
                {&unit_port_set_, "out_biu_req",0};

        ////////////////////////////////////////////////////////////////////////////////
        // Events
        ////////////////////////////////////////////////////////////////////////////////
        
        sparta::UniqueEvent<> uev_issue_downstream_read_{&unit_event_set_, "issue_downstream_read",
                CREATE_SPARTA_HANDLER(DCache, issueDownstreamRead_)};

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
