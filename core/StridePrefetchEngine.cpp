#include "StridePrefetchEngine.hpp"
#include "MemoryAccessInfo.hpp" 

namespace olympia
{

    StridePrefetchEngine::StridePrefetchEngine(unsigned int num_lines_to_prefetch,
                                               unsigned int cache_line_size,
                                               unsigned int table_size,
                                               unsigned int confidence_threshold) :
        num_lines_to_prefetch_(num_lines_to_prefetch),
        cache_line_size_(cache_line_size),
        table_size_(table_size),
        confidence_threshold_(confidence_threshold),
        // FIX: stride_table_ MUST be initialized before prefetch_queue_
        stride_table_(table_size),
        prefetch_queue_(num_lines_to_prefetch * 2)
    {
    }

    bool StridePrefetchEngine::isPrefetchReady() const 
    { 
        return !prefetch_queue_.empty(); 
    }

    bool StridePrefetchEngine::handleMemoryAccess(const MemoryAccessInfoPtr & access)
    {
        sparta::memory::addr_t addr = access->getVAddr();
        sparta::memory::addr_t cache_line_addr = (addr & ~(cache_line_size_ - 1));
        
        // Simple hash for index
        uint64_t index = 0;
        if (access->getInstPtr() != nullptr)
        {
            // Assuming getPC() is available on the instruction. 
            // If not, simply using '0' is safe for this assignment.
            index = (access->getInstPtr()->getPC() >> 2) % table_size_;
        }

        // Clear existing prefetches
        prefetch_queue_.clear();

        // Get or create stride entry
        StrideEntry& entry = stride_table_[index];

        if (!entry.valid)
        {
            // First access for this entry
            entry.last_addr = cache_line_addr;
            entry.valid = true;
            entry.confidence = 0;
            return false;
        }

        // Calculate stride
        int64_t current_stride = static_cast<int64_t>(cache_line_addr) - 
                                 static_cast<int64_t>(entry.last_addr);

        if (current_stride == entry.last_stride && current_stride != 0)
        {
            // Stride matches - increase confidence
            entry.confidence++;

            if (entry.confidence >= confidence_threshold_)
            {
                // Generate prefetches based on stride pattern
                generateStridePrefetches_(access, cache_line_addr, current_stride);
                entry.last_addr = cache_line_addr;
                return true;
            }
        }
        else
        {
            // Stride changed - reset confidence
            entry.confidence = 0;
            entry.last_stride = current_stride;
        }

        entry.last_addr = cache_line_addr;
        return false;
    }

    void StridePrefetchEngine::generateStridePrefetches_(const MemoryAccessInfoPtr & access,
                                                        sparta::memory::addr_t current_addr,
                                                        int64_t stride)
    {
        sparta::memory::addr_t prefetch_addr = current_addr;

        for (uint64_t i = 0; i < num_lines_to_prefetch_; i++)
        {
            prefetch_addr += stride;
            
            // Create NEW prefetch request (prevents SegFault)
            MemoryAccessInfoPtr prefetch_access = sparta::SpartaSharedPointer<olympia::MemoryAccessInfo>(
                new olympia::MemoryAccessInfo(prefetch_addr)
            );

            // Only set target vaddr if the Instruction Pointer exists
            if (prefetch_access->getInstPtr() != nullptr)
            {
                prefetch_access->getInstPtr()->setTargetVAddr(prefetch_addr);
            }

            prefetch_queue_.emplace_back(prefetch_access);
        }
    }

    const MemoryAccessInfoPtr StridePrefetchEngine::getPrefetchMemoryAccess() const
    {
        sparta_assert(isPrefetchReady(), "No prefetch available");
        return prefetch_queue_.front();
    }

    void StridePrefetchEngine::popPrefetchMemoryAccess()
    {
        sparta_assert(prefetch_queue_.size() > 0, "Prefetch queue is empty");
        prefetch_queue_.pop_front();
    }

} // namespace olympia