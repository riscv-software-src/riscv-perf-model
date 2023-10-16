// <CoreUtils.hpp> -*- C++ -*-

#pragma once

#include <vector>

#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Utils.hpp"

#include "CPUTopology.hpp"

namespace olympia::coreutils
{
    // Get the extension 'execution_topology' from the CoreExtension
    // extension.  The node passed is expected to be the node
    // containing the extension (for example, core0)
    inline auto getExecutionTopology(sparta::TreeNode* node)
    {
        auto core_extension = node->getExtension(olympia::CoreExtensions::name);
        auto core_extension_params = sparta::notNull(core_extension)->getParameters();
        auto execution_topology_param
            = sparta::notNull(core_extension_params)->getParameter("execution_topology");
        return sparta::notNull(execution_topology_param)
            ->getValueAs<olympia::CoreExtensions::ExecutionTopology>();
    }

    inline core_types::RegFile determineRegisterFile(const mavis::OperandInfo::Element & reg)
    {
        static const std::map<mavis::InstMetaData::OperandTypes, core_types::RegFile>
            mavis_optype_to_regfile
            = {// mapping of supported types ...
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

} // namespace olympia::coreutils
