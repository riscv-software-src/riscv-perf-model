// <InstArchInfo.cpp> -*- C++ -*-

#include "InstArchInfo.hpp"
#include "mavis/DecoderTypes.h"

namespace olympia_core
{
    const InstArchInfo::TargetUnitMap dispatch_target_map = {
        {"alu",      InstArchInfo::TargetUnit::ALU},
        {"fpu",      InstArchInfo::TargetUnit::FPU},
        {"branch",   InstArchInfo::TargetUnit::BR},
        {"lsu",      InstArchInfo::TargetUnit::LSU},
        {"rob",      InstArchInfo::TargetUnit::ROB},
        {"invalid",  InstArchInfo::TargetUnit::UNKNOWN},
    };

    void InstArchInfo::update(const nlohmann::json& jobj)
    {
        // Get the dispatch target
        if (jobj.find("dispatch") != jobj.end())
        {
            mavis::UnitNameListType ulist = jobj["dispatch"].get<mavis::UnitNameListType>();
            sparta_assert(ulist.size() == 1, "This model cannot handle more than 1 dispatch target/instruction");
            const auto tgt = ulist[0];
            const auto itr = dispatch_target_map.find(tgt);
            sparta_assert(itr != dispatch_target_map.end(),
                          "Unknown target unit: " << tgt << " for inst: " << jobj["mnemonic"].get<std::string>());
            tgt_unit_ = itr->second;
        }
        sparta_assert(tgt_unit_ != TargetUnit::UNKNOWN,
                      "Unknown target unit (dispatch) for " << jobj["mnemonic"].get<std::string>());

        if (jobj.find("latency") != jobj.end()) {
            execute_time_ = jobj["latency"].get<uint32_t>();
        }
        sparta_assert(execute_time_ != 0,
                      "Unknown execution time (latency) for " << jobj["mnemonic"].get<std::string>());

        is_load_store_ = (tgt_unit_ == TargetUnit::LSU);
    }

}
