// <CoreUtils.hpp> -*- C++ -*-

#pragma once

#include <vector>
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/utils/Utils.hpp"

#include "mavis/InstMetaData.h"
#include "mavis/OperandInfo.hpp"

#include "CoreExtensions.hpp"
#include "CoreTypes.hpp"

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
                {mavis::InstMetaData::OperandTypes::QUAD, core_types::RegFile::RF_INTEGER},
                {mavis::InstMetaData::OperandTypes::VECTOR, core_types::RegFile::RF_VECTOR}};
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


// Get direct child of core node (core is an ancestor of this node)
// - if child_name == "", return the core node.  If core node can't be found, return nullptr
//   - we never return the root node because this is not a ResourceTreeNode
// - if child_name != "":
//   - if core node doesn't exist, treat root node as core node.  Then:
//   - if child doesn't exist, return nullptr
//   - if child exists, return child node
inline sparta::ResourceTreeNode *getCoreChildNode(sparta::TreeNode *node, const std::string &child_name)
{
    sparta::TreeNode *core_node = node->findAncestorByName("core*");  // glob-style pattern

    if (child_name == "") {
        // We're supposed to return the core node itself

        // If the core node can't be found, we return nullptr.  We don't return the root node
        // because the root node is not a ResourceTreeNode

        return static_cast<sparta::ResourceTreeNode *>(core_node);
    }

    // We're supposed to find a direct child of the core node

    if (core_node == nullptr) {
        // We couldn't find the core node, so treat the root node as the core node
        // (for unit tests)
        core_node = node->getRoot();
    }

    static const bool MUST_EXIST = false;  // If child not found, return nullptr

    return static_cast<sparta::ResourceTreeNode *>(core_node->getChild(child_name, MUST_EXIST));
}  // coreutils::getCoreChildNode()


// If core node can't be found, return the default value
// If child_name == "", get param from the core node
// - Note:  Only finds direct children of the core node, not other descendants
template<typename ParamT>
inline ParamT getUnitParam(sparta::TreeNode *node, const std::string &child_name,
                           const std::string &param_name, ParamT default_value)
{
    const sparta::ResourceTreeNode *unit_node = getCoreChildNode(node, child_name);
    if (unit_node == nullptr) {
        return default_value;
    }

    return unit_node->getParameterSet()->getParameter(param_name)->getValueAs<ParamT>();
}  // coreutils::getUnitParam()

// Note:  the value returned does not consider warmup;
// this is the count from the beginning of simulation
//
// If you want to incorporate warmup, do something like this:
//
//    sparta::Counter rob_retired_{
//        getStatisticSet(), "rob_retired",
//        "Number of ROB retires", sparta::Counter::COUNT_NORMAL
//    };
//
//    rob_retired_ += coreutils::getNumRetiredInsts(getContainer()) - rob_retired_.get();
//
inline sparta::Counter::counter_type
getUnitCounter(sparta::TreeNode *node, const std::string &child_name,
               const std::string &counter_name)
{
    const sparta::ResourceTreeNode *unit_node = getCoreChildNode(node, child_name);
    if (unit_node == nullptr) {
        // XXX Should we assert instead?
        return 0;
    }

    static const bool MUST_EXIST = true;  // If stats not found, assert
    const sparta::StatisticSet *unit_stat_set =
        static_cast<const sparta::StatisticSet *>(unit_node->getChild("stats", MUST_EXIST));

    return unit_stat_set->getCounter(counter_name)->get();
}  // coreutils::getUnitCounter()

// Get num insts retired
inline sparta::Counter::counter_type getNumRetiredInsts(sparta::TreeNode *node)
{
    return getUnitCounter(node, "rob", "num_retired");
}  // coreutils::getNumRetiredInsts()

// Get max_inflight_preds param from fetch unit
inline uint32_t getMaxInflightPreds(sparta::TreeNode *node)
{
    return getUnitParam<uint32_t>(node, "fetch", "max_inflight_predictions", 128);
}  // coreutils::getMaxInflightPreds()

// Get id0_num_stages param from decode unit
inline uint32_t getDecodeId0NumStages(sparta::TreeNode *node)
{
    return getUnitParam<uint32_t>(node, "decode", "id0_num_stages", 1);
}  // coreutils::getDecodeId0NumStages()


// Get scoreboard parent node, which must be a child of the core node
inline sparta::TreeNode *getScoreboardParentNode(sparta::TreeNode *node)
{
    const bool test_slices = getSMTestSlices(node);
    const std::string sb_parent_str = test_slices ? "slice_manager" : "id1";

    sparta::TreeNode *sb_parent_node = getCoreChildNode(node, sb_parent_str);

    sparta_assert(sb_parent_node != nullptr,
                  "Can't find scoreboard parent: " << sb_parent_str);

    return sb_parent_node;
}  // coreutils::getScoreboardParentNode()


inline void setupScoreboardViews(sparta::TreeNode *node,
                                 core_types::ScoreboardViews &views, const std::string &view_name)
{
    sparta::TreeNode *sb_parent = getScoreboardParentNode(node);

    // Setup scoreboard view upon register file
    std::vector<core_types::RegFile> reg_files = {
        core_types::RegFile::RF_INTEGER,
        core_types::RegFile::RF_FLOAT
    };

    for (const auto &rf : reg_files) {
        views[rf].reset(new core_types::ScoreboardView(
            view_name, core_types::regfile_names[rf], sb_parent));
    }
}  // coreutils::setupScoreboardViews()


} // namespace olympia::coreutils
