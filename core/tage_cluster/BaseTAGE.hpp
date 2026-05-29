#pragma once
#include "Inst.hpp"
#include "sparta/log/MessageSource.hpp"

namespace olympia {
    
    using Addr = sparta::memory::addr_t;

    class BaseTAGE {
    public:
        virtual ~BaseTAGE() = default;

        void setBpLogger(sparta::log::MessageSource &logger) { branch_prediction_stat_logger_ = &logger; }
        void setBrLogPc(sparta::memory::addr_t log_pc) { br_log_pc_ = log_pc; }

    protected:
    
        // BP Logger
        sparta::log::MessageSource *branch_prediction_stat_logger_ = nullptr;
        sparta::memory::addr_t br_log_pc_ = 0x0;
    };
}
