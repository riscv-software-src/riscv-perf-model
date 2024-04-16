#pragma once

#include "CacheFuncModel.hpp"

namespace olympia
{
    class MSHREntryInfo
    {
      public:
        MSHREntryInfo(const uint64_t & line_size, const sparta::Clock* clock) :
            line_fill_buffer_(line_size)
        {
            line_fill_buffer_.setValid(true);
        }

        ~MSHREntryInfo() {}

        SimpleCacheLine & getLineFillBuffer() { return line_fill_buffer_; }

        bool isValid() const { return line_fill_buffer_.isValid(); }

        void setValid(bool v) { line_fill_buffer_.setValid(v); }

        bool isModified() const { return line_fill_buffer_.isModified(); }

        void setModified(bool m) { line_fill_buffer_.setModified(m); }

        void setDataArrived(bool v) { data_arrived_ = v; }

        bool getDataArrived() { return data_arrived_; }

        void setMemRequest(const MemoryAccessInfoPtr & new_memory_access_info)
        {
            memory_access_info = new_memory_access_info;
        }

        MemoryAccessInfoPtr & getMemRequest() { return memory_access_info; }

      private:
        SimpleCacheLine line_fill_buffer_;
        MemoryAccessInfoPtr  memory_access_info;
        bool data_arrived_ = false;
    };
} // namespace olympia