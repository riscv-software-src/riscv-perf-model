#pragma once

#include "sparta/simulation/Clock.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace olympia {

class OncePerCycleChecker {
public:
    OncePerCycleChecker(const sparta::Clock* clk) :
        clk_{clk}
    {}

    void check() {
        sparta_assert (clk_->currentCycle() > last_cycle_,
                       "Did not expect to be called multiple times on the same cycle");
        last_cycle_ = clk_->currentCycle();
    }

private:
    const sparta::Clock* clk_;
    sparta::Clock::Cycle last_cycle_{0};
};

}
