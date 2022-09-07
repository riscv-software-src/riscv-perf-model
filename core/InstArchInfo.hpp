// <InstArchInfo.hpp> -*- C++ -*-

//!
//! \file InstArchInfo.hpp
//! \brief The "static" information on an instruction.
//!
//! For each opcode/inst read from a program, there is static data
//! associated with that instruction.  Mavis keeps a copy of that
//! static data to prevent it from being created over and over again

#pragma once

#include <string>
#include <cinttypes>
#include <map>

#include "sparta/utils/SpartaSharedPointer.hpp"

#include "json.hpp"

namespace olympia
{
    /*!
     * \class InstArchInfo
     * \brief Static information about an instruction
     *
     * Class allocated/created by Mavis when the decode tables are
     * created.  This is "static" information about an instruction
     * such as latency, execution target unit, serialized, etc.  The
     * data is kept in micro-architecture files (JSON) in the
     * simulator arch directory.
     */
    class InstArchInfo
    {
    public:
        using PtrType = sparta::SpartaSharedPointer<InstArchInfo>;

        enum class TargetUnit : std::uint16_t{
            ALU,
            FPU,
            BR,
            LSU,
            ROB, // Instructions that go right to retire
            N_TARGET_UNITS,
            UNKNOWN = N_TARGET_UNITS
        };
        using TargetUnitMap = std::map<std::string, TargetUnit>;

        // Called by Mavis during its initialization
        explicit InstArchInfo(const nlohmann::json& jobj)
        {
            update(jobj);
        }

        // Called by Mavis if, during initialization, changes are
        // dynamically made to an instruction.
        void update(const nlohmann::json& jobj);

        //! Return the target unit for this instruction type
        TargetUnit getTargetUnit() const { return tgt_unit_; }

        //! Return the execution time (latency) of the instruction
        uint32_t   getExecutionTime() const { return execute_time_; }

        //! Is this instruction a load/store type?
        bool       isLoadStore() const { return is_load_store_; }

    private:
        TargetUnit tgt_unit_ = TargetUnit::UNKNOWN;
        uint32_t   execute_time_ = 0;
        bool       is_load_store_ = false;
    };

    inline std::ostream & operator<<(std::ostream & os, const InstArchInfo::TargetUnit & unit) {
        switch(unit) {
            case InstArchInfo::TargetUnit::ALU:
                os << "ALU";
                break;
            case InstArchInfo::TargetUnit::FPU:
                os << "FPU";
                break;
            case InstArchInfo::TargetUnit::BR:
                os << "BR";
                break;
            case InstArchInfo::TargetUnit::LSU:
                os << "LSU";
                break;
            case InstArchInfo::TargetUnit::ROB:
                os << "ROB";
                break;
            case InstArchInfo::TargetUnit::N_TARGET_UNITS:
                throw sparta::SpartaException("N_TARGET_UNITS cannot be a valid enum state.");
        }
        return os;
    }

}
