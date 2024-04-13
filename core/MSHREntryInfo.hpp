#pragma once

#include "CacheFuncModel.hpp"

namespace olympia {
    class MSHREntryInfo
    {
      public:
        MSHREntryInfo(const uint64_t & block_address, const uint64_t & line_size,
                      const uint32_t load_miss_queue_size, const sparta::Clock* clock) :
            line_fill_buffer_(line_size),
            block_address_(block_address),
            load_miss_queue_("load_miss_queue", load_miss_queue_size, clock)
        {
            line_fill_buffer_.setValid(true);
        }

        ~MSHREntryInfo() {}

        const uint64_t & getBlockAddress() const { return block_address_; }

        void setBlockAddress(uint64_t block_address) { block_address_ = block_address; }

        SimpleCacheLine & getLineFillBuffer() { return line_fill_buffer_; }

        bool isValid() const { return line_fill_buffer_.isValid(); }

        void setValid(bool v) { line_fill_buffer_.setValid(v); }

        bool isModified() const { return line_fill_buffer_.isModified(); }

        void setModified(bool m) { line_fill_buffer_.setModified(m); }

        void setDataArrived(bool v) { data_arrived_ = v; }

        const bool & getDataArrived() { return data_arrived_; }

        void enqueueLoad(MemoryAccessInfoPtr mem_access_info_ptr)
        {
            load_miss_queue_.push(mem_access_info_ptr);
        }

        MemoryAccessInfoPtr dequeueLoad()
        {
            if (load_miss_queue_.empty())
                return nullptr;

            MemoryAccessInfoPtr mem_access_info_ptr = load_miss_queue_.front();
            load_miss_queue_.pop();

            return mem_access_info_ptr;
        }

        MemoryAccessInfoPtr peekLoad()
        {
            if (load_miss_queue_.empty())
                return nullptr;

            MemoryAccessInfoPtr mem_access_info_ptr = load_miss_queue_.front();
            return mem_access_info_ptr;
        }

        bool isLoadMissQueueFull() const { return (load_miss_queue_.numFree() == 0); }

      private:
        SimpleCacheLine line_fill_buffer_;
        uint64_t block_address_;
        sparta::Queue<MemoryAccessInfoPtr> load_miss_queue_;
        bool data_arrived_ = false;
    };
}