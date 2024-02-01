// <CoreUtils.hpp> -*- C++ -*-

#pragma once

#include <vector>
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Utils.hpp"

#include "CPUTopology.hpp"

namespace olympia::coreutils
{
    inline auto getPipeTopology(sparta::TreeNode* node, const std::string & pipe_name)
    {
        auto core_extension = node->getExtension(olympia::CoreExtensions::name);
        auto core_extension_params = sparta::notNull(core_extension)->getParameters();
        auto pipe_topology_param = sparta::notNull(core_extension_params)->getParameter(pipe_name);
        return sparta::notNull(pipe_topology_param)
            ->getValueAs<olympia::CoreExtensions::PipeTopology>();
    }

    inline core_types::RegFile determineRegisterFile(const mavis::OperandInfo::Element & reg)
    {
        static const std::map<mavis::InstMetaData::OperandTypes, core_types::RegFile>
            mavis_optype_to_regfile = {
                // mapping of supported types ...
                {mavis::InstMetaData::OperandTypes::SINGLE, core_types::RegFile::RF_FLOAT},
                {mavis::InstMetaData::OperandTypes::DOUBLE, core_types::RegFile::RF_FLOAT},
                {mavis::InstMetaData::OperandTypes::WORD, core_types::RegFile::RF_INTEGER},
                {mavis::InstMetaData::OperandTypes::LONG, core_types::RegFile::RF_INTEGER},
                {mavis::InstMetaData::OperandTypes::QUAD, core_types::RegFile::RF_INTEGER}};
        if (auto match = mavis_optype_to_regfile.find(reg.operand_type);
            match != mavis_optype_to_regfile.end())
        {
            return match->second;
        }
        sparta_assert(false, "Unknown reg type: " << static_cast<uint32_t>(reg.operand_type));
        return core_types::RegFile::RF_INVALID;
    }

    inline core_types::RegFile determineRegisterFile(const std::string & target_name)
    {
        if (target_name == "alu" || target_name == "br")
        {
            return core_types::RF_INTEGER;
        }
        else if (target_name == "fpu")
        {
            return core_types::RF_FLOAT;
        }
        sparta_assert(false, "Not supported this target: " << target_name);
        return core_types::RF_INVALID;
    }

    inline core_types::RegFile determineRegisterFile(const InstArchInfo::TargetUnit & target_unit){
        if(target_unit == InstArchInfo::TargetUnit::ALU || target_unit == InstArchInfo::TargetUnit::BR){
            return core_types::RF_INTEGER;
        }
        else if(target_unit == InstArchInfo::TargetUnit::FPU){
            return core_types::RF_FLOAT;
        }
        sparta_assert(false, "Not supported this target unit: " << target_unit);
        return core_types::RF_INVALID;
    }

} // namespace olympia::coreutils
