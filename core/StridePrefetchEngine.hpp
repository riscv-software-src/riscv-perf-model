#pragma once

#include "sparta/memory/AddressTypes.hpp"
#include "sparta/utils/FastList.hpp"
#include "PrefetchEngineIF.hpp"
#include <unordered_map>

namespace olympia
{
    /*!
     * \class StridePrefetchEngine
     * \brief Stride-based prefetch engine
     *
     * This class implements a stride prefetcher that detects regular access patterns
     * and generates prefetches based on the detected stride. It maintains a table
     * of recent accesses to track strides.
     *
     * Key features:
     * - Detects stride patterns in memory accesses
     * - Configurable number of prefetches per detected stride
     * - Configurable table size for tracking access patterns
     */
    class StridePrefetchEngine : public PrefetchEngineIF<>
    {
      public:
        /*!
         * \brief Construct a StridePrefetchEngine instance
         * \param num_lines_to_prefetch Number of cache lines to prefetch per detected stride
         * \param cache_line_size Size of cache line (in bytes)
         * \param table_size Size of the stride tracking table
         * \param confidence_threshold Number of matching strides before triggering prefetch
         */
        StridePrefetchEngine(unsigned int num_lines_to_prefetch = 2,
                            unsigned int cache_line_size = 64,
                            unsigned int table_size = 256,
                            unsigned int confidence_threshold = 2);

        /*!
         * \brief Checks if prefetch is ready
         * \return Returns true if one or more prefetches are ready, false otherwise
         */
        bool isPrefetchReady() const override;

        /*!
         * \brief Input memory access to prefetcher
         * \param access Const reference to MemoryAccessInfoPtr
         * \return Returns true if a stride pattern was detected and prefetches generated
         */
        bool handleMemoryAccess(const MemoryAccessInfoPtr & access) override;

        /*!
         * \brief Get next prefetch access
         * \return MemoryAccessInfoPtr containing the prefetch address
         *
         * This function will assert if no prefetches are available.
         * Calls to this function should be guarded by isPrefetchReady() in the same clock cycle
         */
        const MemoryAccessInfoPtr getPrefetchMemoryAccess() const override;

        /*!
         * \brief Remove the first prefetch memory access
         */
        void popPrefetchMemoryAccess() override;

      private:
        /*!
         * \brief Structure to track stride information
         */
        struct StrideEntry
        {
            sparta::memory::addr_t last_addr = 0;      //! Last accessed address
            int64_t last_stride = 0;                    //! Last computed stride
            uint32_t confidence = 0;                    //! Confidence counter
            bool valid = false;                         //! Entry validity flag

            StrideEntry() = default;
        };

        //! Get cache line address (aligned to cache line boundary)
        sparta::memory::addr_t getCacheLineAddr(sparta::memory::addr_t addr) const
        {
            return (addr / cache_line_size_) * cache_line_size_;
        }

        //! Generate prefetch requests for detected stride
        void generateStridePrefetches_(const MemoryAccessInfoPtr & access,
                                      sparta::memory::addr_t current_addr,
                                      int64_t stride);

        const uint64_t num_lines_to_prefetch_;      //! Number of prefetch lines to generate
        const uint64_t cache_line_size_;            //! Size of cache line in bytes
        const uint64_t table_size_;                 //! Size of stride tracking table
        const uint32_t confidence_threshold_;       //! Confidence threshold for prefetching

        //! Stride tracking table (indexed by PC or simplified hash)
        std::unordered_map<uint64_t, StrideEntry> stride_table_;

        //! Queue to hold prefetches generated
        sparta::utils::FastList<MemoryAccessInfoPtr> prefetch_queue_;
    };

} // namespace olympia