// <InstArchInfo.cpp> -*- C++ -*-

#include "InstArchInfo.hpp"
#include "mavis/DecoderTypes.h"

namespace olympia
{
    const InstArchInfo::TargetUnitMap InstArchInfo::dispatch_target_map = {
        {"alu", InstArchInfo::TargetUnit::ALU},         {"fpu", InstArchInfo::TargetUnit::FPU},
        {"br", InstArchInfo::TargetUnit::BR},           {"lsu", InstArchInfo::TargetUnit::LSU},
        {"rob", InstArchInfo::TargetUnit::ROB},         {"none", InstArchInfo::TargetUnit::NONE},
        {"unknown", InstArchInfo::TargetUnit::UNKNOWN},
    };

    const InstArchInfo::TargetPipeMap InstArchInfo::execution_pipe_map = {
        {"br", InstArchInfo::TargetPipe::BR},       {"cmov", InstArchInfo::TargetPipe::CMOV},
        {"div", InstArchInfo::TargetPipe::DIV},     {"faddsub", InstArchInfo::TargetPipe::FADDSUB},
        {"float", InstArchInfo::TargetPipe::FLOAT}, {"fmac", InstArchInfo::TargetPipe::FMAC},
        {"i2f", InstArchInfo::TargetPipe::I2F},     {"f2i", InstArchInfo::TargetPipe::F2I},
        {"int", InstArchInfo::TargetPipe::INT},     {"lsu", InstArchInfo::TargetPipe::LSU},
        {"mul", InstArchInfo::TargetPipe::MUL},     {"sys", InstArchInfo::TargetPipe::SYS}};

    void InstArchInfo::update(const nlohmann::json & jobj)
    {
        // Get the dispatch target
        if (jobj.find("dispatch") != jobj.end())
        {
            mavis::UnitNameListType ulist = jobj["dispatch"].get<mavis::UnitNameListType>();
            sparta_assert(ulist.size() == 1,
                          "This model cannot handle more than 1 dispatch target/instruction");
            const auto tgt = ulist[0];
            const auto itr = dispatch_target_map.find(tgt);
            sparta_assert(itr != dispatch_target_map.end(),
                          "Unknown target unit: " << tgt << " for inst: "
                                                  << jobj["mnemonic"].get<std::string>());
            tgt_unit_ = itr->second;
        }
        sparta_assert(tgt_unit_ != TargetUnit::UNKNOWN,
                      "Unknown target unit (dispatch) for " << jobj["mnemonic"].get<std::string>());

        if (jobj.find("pipe") != jobj.end())
        {
            auto pipe_name = jobj["pipe"].get<std::string>();
            const auto itr = execution_pipe_map.find(pipe_name);
            sparta_assert(itr != execution_pipe_map.end(),
                          "Unknown pipe target: " << pipe_name << " for inst: "
                                                  << jobj["mnemonic"].get<std::string>());
            tgt_pipe_ = itr->second;
        }
        sparta_assert(tgt_pipe_ != TargetPipe::UNKNOWN, "Unknown target pipe (execution) for "
                                                            << jobj["mnemonic"].get<std::string>());

        if (jobj.find("latency") != jobj.end())
        {
            execute_time_ = jobj["latency"].get<uint32_t>();
        }
        sparta_assert(execute_time_ != 0, "Unknown execution time (latency) for "
                                              << jobj["mnemonic"].get<std::string>());

        is_load_store_ = (tgt_unit_ == TargetUnit::LSU);
    }

} // namespace olympia
