#include "NextLinePrefetchEngine.hpp"

namespace olympia {

  NextLinePrefetchEngine::NextLinePrefetchEngine(unsigned int num_lines_to_prefetch, unsigned int cache_line_size) :
    num_lines_to_prefetch_(num_lines_to_prefetch),
    cache_line_size_(cache_line_size),
    prefetch_queue_(num_lines_to_prefetch)
  {
    // do nothing
  }

  bool NextLinePrefetchEngine::isPrefetchReady() const {
    return !prefetch_queue_.empty();
  }

  bool NextLinePrefetchEngine::handleMemoryAccess(const MemoryAccessInfoPtr &access) {
      sparta::memory::addr_t addr = access->getVAddr();
      // get address from access
      // remove existing prefetches from queue
      prefetch_queue_.clear();
      for (uint64_t i=0; i<num_lines_to_prefetch_; i++) {
        addr += cache_line_size_;
        MemoryAccessInfoPtr prefetch_access(access);
        // TBD set prefetcher type
        prefetch_access->getInstPtr()->setTargetVAddr(addr);
        prefetch_queue_.emplace_back(prefetch_access);
      }
      return true;
    }

  const MemoryAccessInfoPtr NextLinePrefetchEngine::getPrefetchMemoryAccess() const {
    sparta_assert(isPrefetchReady());
    auto access = prefetch_queue_.front();
    return access;
  }

  void NextLinePrefetchEngine::popPrefetchMemoryAccess() {
    sparta_assert(prefetch_queue_.size()>0);
    prefetch_queue_.pop_front();
  }
}
