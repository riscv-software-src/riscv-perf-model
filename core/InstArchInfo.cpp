// <InstArchInfo.cpp> -*- C++ -*-

#include "InstArchInfo.hpp"
#include "mavis/DecoderTypes.h"

namespace olympia
{
    const InstArchInfo::TargetPipeMap InstArchInfo::execution_pipe_map = {
        {"br", InstArchInfo::TargetPipe::BR},       {"cmov", InstArchInfo::TargetPipe::CMOV},
        {"div", InstArchInfo::TargetPipe::DIV},     {"faddsub", InstArchInfo::TargetPipe::FADDSUB},
        {"float", InstArchInfo::TargetPipe::FLOAT}, {"fmac", InstArchInfo::TargetPipe::FMAC},
        {"i2f", InstArchInfo::TargetPipe::I2F},     {"f2i", InstArchInfo::TargetPipe::F2I},
        {"int", InstArchInfo::TargetPipe::INT},     {"lsu", InstArchInfo::TargetPipe::LSU},
        {"mul", InstArchInfo::TargetPipe::MUL},     {"vint", InstArchInfo::TargetPipe::VINT},
        {"vset", InstArchInfo::TargetPipe::VSET},   {"vmul", InstArchInfo::TargetPipe::VMUL},
        {"vdiv", InstArchInfo::TargetPipe::VDIV},   {"sys", InstArchInfo::TargetPipe::SYS}};

    void InstArchInfo::update(const nlohmann::json & jobj)
    {
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

        is_load_store_ = (tgt_pipe_ == TargetPipe::LSU);
        is_vset_ = {tgt_pipe_ == TargetPipe::VSET};

        is_vector_ = {tgt_pipe_ == TargetPipe::VSET || tgt_pipe_ == TargetPipe::VINT || tgt_pipe_ == InstArchInfo::TargetPipe::VDIV || tgt_pipe_ == InstArchInfo::TargetPipe::VMUL};
    }

} // namespace olympia
