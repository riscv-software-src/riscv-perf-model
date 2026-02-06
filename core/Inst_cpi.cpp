// <Inst_cpi.cpp> -*- C++ -*-

#include "Inst.hpp"

namespace olympia
{
    void Inst::finalizeCPIBreakdown()
    {
        // Calculate cycle durations for each stage based on timestamps
        // Note: For simplicity, we attribute all cycles between stage transitions
        // to the appropriate category. More sophisticated attribution could
        // differentiate between productive work and stalls within a stage.
        
        auto& breakdown = cpi_breakdown_;
        const auto& ts = timestamps_;
        
        // Fetch stalls: From fetch to decode
        if (ts.decode_enter > ts.fetch_enter) {
            breakdown.fetch_stall_cycles = ts.decode_enter - ts.fetch_enter;
        }
        
        // Decode stalls: From decode to rename
        if (ts.rename_enter > ts.decode_enter) {
            breakdown.decode_stall_cycles = ts.rename_enter - ts.decode_enter;
        }
        
        // Rename stalls: From rename to dispatch
        if (ts.dispatch_enter > ts.rename_enter) {
            breakdown.rename_stall_cycles = ts.dispatch_enter - ts.rename_enter;
        }
        
        // Dispatch stalls: From dispatch to issue ready
        // (Time waiting in dispatch or for operands to be ready)
        if (ts.issue_ready > 0 && ts.issue_ready > ts.dispatch_enter) {
            // If we tracked issue_ready, we can differentiate dispatch vs issue queue stalls
            breakdown.dispatch_stall_cycles = ts.issue_ready - ts.dispatch_enter;
        } else if (ts.execute_start > ts.dispatch_enter) {
            // Otherwise, attribute time before execution as dispatch/issue stalls
            breakdown.dispatch_stall_cycles = ts.execute_start - ts.dispatch_enter;
        }
        
        // Execute cycles: Actual execution time
        if (ts.execute_complete > ts.execute_start) {
            breakdown.execute_cycles = ts.execute_complete - ts.execute_start;
        }
        
        // ROB stalls: From completion to retirement
        if (ts.retired > ts.execute_complete) {
            breakdown.rob_stall_cycles = ts.retired - ts.execute_complete;
        }
        
        // Note: Memory stalls are implicitly included in execute_cycles
        // for memory instructions. For more granular attribution, we would
        // need additional timestamps within the LSU (e.g., cache miss start/end)
    }
} // namespace olympia
