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

        enum TargetUnit : std::uint16_t{
            ALU,
            FPU,
            BR,
            LSU,
            ROB, // Instructions that go right to retire
            NONE,
            UNKNOWN = NONE
        };
        static constexpr uint32_t N_TARGET_UNITS = static_cast<uint32_t>(TargetUnit::UNKNOWN);

        enum class TargetPipe : std::uint16_t{
            BR,
            CMOV,
            DIV,
            FADDSUB,
            FLOAT,
            FMAC,
            I2F,
            F2I,
            INT,
            LSU,
            MUL,
            SYS,
            UNKNOWN
        };
        static constexpr uint32_t N_TARGET_PIPES = static_cast<uint32_t>(TargetPipe::UNKNOWN);

        using TargetUnitMap = std::map<std::string, TargetUnit>;
        static const TargetUnitMap dispatch_target_map;

        using TargetPipeMap = std::map<std::string, TargetPipe>;
        static const TargetPipeMap execution_pipe_map;

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

        //! Return the target unit for this instruction type
        TargetPipe getTargetPipe() const { return tgt_pipe_; }

        //! Return the execution time (latency) of the instruction
        uint32_t   getExecutionTime() const { return execute_time_; }

        //! Is this instruction a load/store type?
        bool       isLoadStore() const { return is_load_store_; }

    private:
        TargetUnit tgt_unit_ = TargetUnit::UNKNOWN;
        TargetPipe tgt_pipe_ = TargetPipe::UNKNOWN;
        uint32_t   execute_time_ = 0;
        bool       is_load_store_ = false;
    };
    /*
    CMOV,
            DIV,
            FADDSUB,
            FLOAT,
            FMAC,
            I2F,
            F2I,
            INT,
            LSU,
            MUL,
            SYS,
            UNKNOWN
    */
    inline std::ostream & operator<<(std::ostream & os, const InstArchInfo::TargetPipe & unit) {
        switch(unit) {
            case InstArchInfo::TargetPipe::BR:
                os << "BR";
                break;
            case InstArchInfo::TargetPipe::CMOV:
                os << "CMOV";
                break;
            case InstArchInfo::TargetPipe::DIV:
                os << "DIV";
                break;
            case InstArchInfo::TargetPipe::FADDSUB:
                os << "FADDSUB";
                break;
            case InstArchInfo::TargetPipe::FLOAT:
                os << "FLOAT";
                break;
            case InstArchInfo::TargetPipe::FMAC:
                os << "FMAC";
                break;
            case InstArchInfo::TargetPipe::I2F:
                os << "I2F";
                break;
            case InstArchInfo::TargetPipe::F2I:
                os << "F2I";
                break;
            case InstArchInfo::TargetPipe::INT:
                os << "INT";
                break;
            case InstArchInfo::TargetPipe::LSU:
                os << "LSU";
                break;
            case InstArchInfo::TargetPipe::MUL:
                os << "MUL";
                break;
            case InstArchInfo::TargetPipe::SYS:
                os << "SYS";
                break;
                
            case InstArchInfo::TargetPipe::UNKNOWN:
                throw sparta::SpartaException("Got UNKNOWN/NONE target pipe.");
        }
        return os;
    }

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
            case InstArchInfo::TargetUnit::UNKNOWN:
                throw sparta::SpartaException("Got UNKNOWN/NONE target unit.");
        }
        return os;
    }

}
