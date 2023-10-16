
#include "Dispatcher.hpp"
#include "Dispatch.hpp"

namespace olympia
{
    void Dispatcher::receiveCredits_(const uint32_t & credits)
    {
        unit_credits_ += credits;
        ILOG(name_ << " got " << credits << " credits, total: " << unit_credits_);

        dispatch_->scheduleDispatchSession();
    }

} // namespace olympia
