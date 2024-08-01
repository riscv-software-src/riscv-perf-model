// <InstArchInfo.cpp> -*- C++ -*-

#include "InstArchInfo.hpp"
#include "mavis/DecoderTypes.h"

namespace olympia
{
    const InstArchInfo::TargetPipeMap InstArchInfo::execution_pipe_map = {
        {"br",   InstArchInfo::TargetPipe::BR},   
        {"cmov", InstArchInfo::TargetPipe::CMOV},
        {"div",   InstArchInfo::TargetPipe::DIV},  
        {"faddsub", InstArchInfo::TargetPipe::FADDSUB},
        {"float",   InstArchInfo::TargetPipe::FLOAT},
        {"fmac", InstArchInfo::TargetPipe::FMAC},
        {"i2f",   InstArchInfo::TargetPipe::I2F},  
        {"f2i", InstArchInfo::TargetPipe::F2I},
        {"int",   InstArchInfo::TargetPipe::INT},  
        {"lsu", InstArchInfo::TargetPipe::LSU},
        {"mul",   InstArchInfo::TargetPipe::MUL},  
        {"vint", InstArchInfo::TargetPipe::VINT},
        {"vmask",   InstArchInfo::TargetPipe::VMASK},
        {"vset",   InstArchInfo::TargetPipe::VSET}, 
        {"vmul", InstArchInfo::TargetPipe::VMUL},
        {"vlsu", InstArchInfo::TargetPipe::VLSU},   {"vdiv",   InstArchInfo::TargetPipe::VDIV},
      
        {"sys", InstArchInfo::TargetPipe::SYS},    {"?",       InstArchInfo::TargetPipe::UNKNOWN}
    };

    const InstArchInfo::TargetPipeStringMap InstArchInfo::execution_pipe_string_map = {
        {InstArchInfo::TargetPipe::BR,      "BR"},
        {InstArchInfo::TargetPipe::CMOV,    "CMOV"},
        {InstArchInfo::TargetPipe::DIV,     "DIV"},
        {InstArchInfo::TargetPipe::FADDSUB, "FADDSUB"},
        {InstArchInfo::TargetPipe::FLOAT,   "FLOAT"},
        {InstArchInfo::TargetPipe::FMAC,    "FMAC"},
        {InstArchInfo::TargetPipe::I2F,     "I2F"},
        {InstArchInfo::TargetPipe::F2I,     "F2I"},
        {InstArchInfo::TargetPipe::INT,     "INT"},
        {InstArchInfo::TargetPipe::LSU,     "LSU"},
        {InstArchInfo::TargetPipe::VLSU,    "VLSU"},
        {InstArchInfo::TargetPipe::MUL,     "MUL"},
        {InstArchInfo::TargetPipe::VINT,    "VINT"},
        {InstArchInfo::TargetPipe::VMASK,   "VMASK"},
        {InstArchInfo::TargetPipe::VSET,    "VSET"},
        {InstArchInfo::TargetPipe::VMUL,    "VMUL"},
        {InstArchInfo::TargetPipe::VDIV,    "VDIV"},
        {InstArchInfo::TargetPipe::SYS,     "SYS"},
        {InstArchInfo::TargetPipe::UNKNOWN, "?"}
    };

    const InstArchInfo::UopGenMap InstArchInfo::uop_gen_type_map = {
        {"ARITH",               InstArchInfo::UopGenType::ARITH},
        {"ARITH_SINGLE_DEST",   InstArchInfo::UopGenType::ARITH_SINGLE_DEST},
        {"ARITH_WIDE_DEST",     InstArchInfo::UopGenType::ARITH_WIDE_DEST},
        {"ARITH_MAC",           InstArchInfo::UopGenType::ARITH_MAC},
        {"ARITH_MAC_WIDE_DEST", InstArchInfo::UopGenType::ARITH_MAC_WIDE_DEST},
        {"NONE",                InstArchInfo::UopGenType::NONE}
    };

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

        if (jobj.find("latency") != jobj.end())
        {
            execute_time_ = jobj["latency"].get<uint32_t>();
        }

        if (jobj.find("uop_gen") != jobj.end())
        {
            auto uop_gen_name = jobj["uop_gen"].get<std::string>();
            const auto itr = uop_gen_type_map.find(uop_gen_name);
            sparta_assert(itr != uop_gen_type_map.end(),
                "Unknown uop gen: " << uop_gen_name << " for inst: "
                                    << jobj["mnemonic"].get<std::string>());
            uop_gen_ = itr->second;
        }

        if (jobj.find("uop_gen") != jobj.end())
        {
            auto uop_gen_name = jobj["uop_gen"].get<std::string>();
            const auto itr = uop_gen_type_map.find(uop_gen_name);
            sparta_assert(itr != uop_gen_type_map.end(),
                "Unknown uop gen: " << uop_gen_name << " for inst: "
                                    << jobj["mnemonic"].get<std::string>());
            uop_gen_ = itr->second;
        }
        is_load_store_ = (tgt_pipe_ == TargetPipe::LSU || tgt_pipe_ == TargetPipe::VLSU);
        is_vset_ = {tgt_pipe_ == TargetPipe::VSET};
    }

} // namespace olympia
