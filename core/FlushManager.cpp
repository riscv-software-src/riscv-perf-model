#include "FlushManager.hpp"

#include "Inst.hpp"

namespace olympia
{
    bool FlushManager::FlushingCriteria::includedInFlush(const InstPtr & other) const
    {
        return isInclusiveFlush() ? inst_ptr_->getUniqueID() <= other->getUniqueID()
                                  : inst_ptr_->getUniqueID() < other->getUniqueID();
    }

    std::ostream & operator<<(std::ostream & os, const FlushManager::FlushingCriteria & criteria)
    {
        os << criteria.getInstPtr() << " " << criteria.getCause();
        return os;
    }
} // namespace olympia
