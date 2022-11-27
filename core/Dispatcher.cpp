
#include "Dispatcher.hpp"
#include "Dispatch.hpp"

namespace olympia
{
    void Dispatcher::receiveCredits_(const uint32_t & credits) {
        unit_credits_ += credits;
        if(SPARTA_EXPECT_FALSE(info_logger_)) {
            info_logger_ << name_ << " got " << credits << " credits, total: " << unit_credits_;
        }

        dispatch_->scheduleDispatchSession();
    }

}
