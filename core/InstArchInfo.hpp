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

        enum TargetPipe : std::uint16_t
        {
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
            VINT,
            VMUL,
            VDIV,
            VSET,
            SYS,
            UNKNOWN
        };

        static constexpr uint32_t N_TARGET_PIPES = static_cast<uint32_t>(TargetPipe::UNKNOWN);

        using TargetPipeMap = std::map<std::string, TargetPipe>;
        static const TargetPipeMap execution_pipe_map;

        // Called by Mavis during its initialization
        explicit InstArchInfo(const nlohmann::json & jobj) { update(jobj); }

        // Called by Mavis if, during initialization, changes are
        // dynamically made to an instruction.
        void update(const nlohmann::json & jobj);

        //! Return the target unit for this instruction type
        TargetPipe getTargetPipe() const { return tgt_pipe_; }

        //! Return the execution time (latency) of the instruction
        uint32_t getExecutionTime() const { return execute_time_; }

        //! Is this instruction a load/store type?
        bool isLoadStore() const { return is_load_store_; }

        //! Is this instruction a vector type
        bool isVector() const { return is_vector_; }

        //! Is this instruction a vset instruction type
        bool isVset() const { return is_vset_; }

      private:
        TargetPipe tgt_pipe_ = TargetPipe::UNKNOWN;
        uint32_t execute_time_ = 0;
        bool is_load_store_ = false;
        bool is_vset_ = false;
        bool is_vector_ = false;
    };

    inline std::ostream & operator<<(std::ostream & os, const InstArchInfo::TargetPipe & pipe)
    {
        switch (pipe)
        {
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
        case InstArchInfo::TargetPipe::VINT:
            os << "VINT";
            break;
        case InstArchInfo::TargetPipe::VMUL:
            os << "VMUL";
            break;
        case InstArchInfo::TargetPipe::VDIV:
            os << "VDIV";
            break;
        case InstArchInfo::TargetPipe::VSET:
            os << "VINT";
            break;
        case InstArchInfo::TargetPipe::SYS:
            os << "SYS";
            break;

        case InstArchInfo::TargetPipe::UNKNOWN:
            throw sparta::SpartaException("Got UNKNOWN/NONE target pipe.");
        }
        return os;
    }
} // namespace olympia
