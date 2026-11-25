#include "LoadStoreInstInfo.hpp"

#include "Inst.hpp"

namespace olympia
{
    bool LoadStoreInstInfo::isRetired() const
    {
        return getInstPtr()->getStatus() == Inst::Status::RETIRED;
    }
} // namespace olympia
