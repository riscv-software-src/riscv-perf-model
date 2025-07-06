// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file MachineInfo.h processor implementation details
#pragma once
#include <cstdint>
#include <iostream>
#include <ostream>
#include <string>

//! \brief Placeholder for uarch and implementation details
//!
//! I have not followed up on the discussion on using
//! extension.core_extensions from the yaml for this information.
//! This is future work.
struct MachineInfo
{
    //! \brief there is only a default constructor provided
    MachineInfo() {}

    //! \brief access number of implemented integer write ports
    uint32_t maxIntWrPorts() const { return maxIntWrPorts_; }

    //! \brief access number of implemented float write ports
    uint32_t maxFpWrPorts() const { return maxFpWrPorts_; }

    //! \brief access number of implemented vector write ports
    uint32_t maxVecWrPorts() const { return maxVecWrPorts_; }

    //! \brief access number of implemented integer read ports
    uint32_t maxIntRdPorts() const { return maxIntRdPorts_; }

    //! \brief access number of implemented float read ports
    uint32_t maxFpRdPorts() const { return maxFpRdPorts_; }

    //! \brief access number of implemented vector read ports
    uint32_t maxVecRdPorts() const { return maxVecRdPorts_; }

    //! \brief Is there a location available to execute a fused operator?
    //!
    //! Obviously, a placeholder. This would be a debug function
    //! trap dispatch to execute pipes that do not implement the fused opr.
    bool compSiteImplemented(uint32_t magic) const { return true; }

    //! \brief how many cycles to wait for the InstrQueue to fill
    //!
    //! Since fusion operates on 1 or more opcodes there are cases
    //! where the InstrQueue may not have enough opcodes to validate
    //! fusion groups. This is more of an issue for N-tuples > 2.
    //! At some point the cycles saved by fusion are spent waiting
    //! for fusable opportunities. This limits the impact.
    uint32_t allowedLatency() const { return allowedLatency_; }

    //! \brief ...
    void setName(std::string n) { name_ = n; }

    //! \brief ...
    std::string name() const { return name_; }

  private:
    //! \brief arbitrary name
    std::string name_{"noname"};
    //! \brief ...
    uint32_t maxIntWrPorts_{2};
    //! \brief ...
    uint32_t maxFpWrPorts_{2};
    //! \brief ...
    uint32_t maxVecWrPorts_{2};
    //! \brief ...
    uint32_t maxIntRdPorts_{4};
    //! \brief ...
    uint32_t maxFpRdPorts_{4};
    //! \brief ...
    uint32_t maxVecRdPorts_{4};
    //! \brief ...
    uint32_t allowedLatency_{1};
};
