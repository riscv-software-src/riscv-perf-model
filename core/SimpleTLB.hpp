#pragma once

#include <cinttypes>
#include "cache/TreePLRUReplacement.hpp"
#include "cache/BasicCacheItem.hpp"
#include "cache/SimpleCache2.hpp"
#include "cache/ReplacementIF.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace olympia {

class SimpleTLBEntry : public sparta::cache::BasicCacheItem {
public:
    SimpleTLBEntry() = delete;

    SimpleTLBEntry(uint64_t page_size) :
        page_size_(page_size),
        valid_(false)
    {
        sparta_assert(sparta::utils::is_power_of_2(page_size),
                      "TLBEntry: Page size must be a power of 2. page_size=" << page_size);
    }

    // Copy constructor
    SimpleTLBEntry(const SimpleTLBEntry & rhs) :
        BasicCacheItem(rhs),
        page_size_(rhs.page_size_),
        valid_(rhs.valid_)
    {}

    // Copy assignment operator
    SimpleTLBEntry &operator=(const SimpleTLBEntry & rhs) {
        BasicCacheItem::operator=(rhs);
        page_size_ = rhs.page_size_;
        valid_ = rhs.valid_;
        return *this;
    }

    virtual ~SimpleTLBEntry() {}

    // Required by SimpleCache2
    void reset(uint64_t addr) {
        setValid(true);
        BasicCacheItem::setAddr(addr);
    }

    // Required by SimpleCache2
    void setValid(bool v) { valid_ = v; }

    // Required by BasicCacheSet
    bool isValid() const { return valid_; }

    // Required by SimpleCache2
    void setModified(bool m) { (void) m; }

    // Required by SimpleCache2
    bool read(uint64_t offset, uint32_t size, uint32_t *buf) const {
        (void) offset;
        (void) size;
        (void) buf;
        sparta_assert(false);
        return true;
    }

    // Required by SimpleCache2
    bool write(uint64_t offset, uint32_t size, uint32_t *buf) const {
        (void) offset;
        (void) size;
        (void) buf;
        sparta_assert(false);
        return true;
    }

private:
    uint64_t page_size_ = 0;
    bool valid_ = false;

}; // class SimpleTLBEntry

class SimpleTLB : public sparta::cache::SimpleCache2<SimpleTLBEntry> {
public:
    SimpleTLB(uint64_t tlb_page_size, uint64_t tlb_num_entries, uint64_t tlb_associativity) :
        sparta::cache::SimpleCache2<SimpleTLBEntry> ((tlb_page_size * tlb_num_entries) >> 10,
                                                     tlb_page_size,
                                                     tlb_page_size,
                                                     SimpleTLBEntry(tlb_page_size),
                                                     sparta::cache::TreePLRUReplacement(tlb_associativity))
    {}
}; // class SimpleTLB

} // namespace olympia
