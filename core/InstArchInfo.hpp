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

        enum TargetPipe : uint16_t
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
            VMASK,
            VMUL,
            VDIV,
            VSET,
            VLSU,
            SYS,
            UNKNOWN
        };

        static constexpr uint32_t N_TARGET_PIPES = static_cast<uint32_t>(TargetPipe::UNKNOWN);

        using TargetPipeMap = std::map<std::string, TargetPipe>;
        static const TargetPipeMap execution_pipe_map;

        using TargetPipeStringMap = std::map<TargetPipe, std::string>;
        static const TargetPipeStringMap execution_pipe_string_map;

        enum class UopGenType
        {
            ARITH,
            ARITH_SINGLE_DEST,
            ARITH_WIDE_DEST,
            NONE,
            UNKNOWN
        };

        static constexpr uint32_t N_UOP_GEN_TYPES = static_cast<uint32_t>(UopGenType::NONE);

        using UopGenMap = std::map<std::string, UopGenType>;
        static const UopGenMap uop_gen_type_map;

        // Called by Mavis during its initialization
        explicit InstArchInfo(const nlohmann::json & jobj) { update(jobj); }

        // Called by Mavis if, during initialization, changes are
        // dynamically made to an instruction.
        void update(const nlohmann::json & jobj);

        //! Return the target unit for this instruction type
        TargetPipe getTargetPipe() const { return tgt_pipe_; }

        //! Return the execution time (latency) of the instruction
        uint32_t getExecutionTime() const { return execute_time_; }

        //! Return the vector uop generator type
        UopGenType getUopGenType() const { return uop_gen_; }

        //! Is this instruction a load/store type?
        bool isLoadStore() const { return is_load_store_; }

        //! Is this instruction a vset instruction type
        bool isVset() const { return is_vset_; }

      private:
        TargetPipe tgt_pipe_ = TargetPipe::UNKNOWN;
        uint32_t execute_time_ = 0;
        UopGenType uop_gen_ = UopGenType::UNKNOWN;
        bool is_load_store_ = false;
        bool is_vset_ = false;
    };

    inline std::ostream & operator<<(std::ostream & os, const InstArchInfo::TargetPipe & pipe)
    {
        if (pipe != InstArchInfo::TargetPipe::UNKNOWN)
        {
            os << InstArchInfo::execution_pipe_string_map.at(pipe);
        }
        else
        {
            throw sparta::SpartaException("Got UNKNOWN/NONE target pipe.");
        }
        return os;
    }
} // namespace olympia
