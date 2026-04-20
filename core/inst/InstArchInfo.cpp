// <InstArchInfo.cpp> -*- C++ -*-

#include "InstArchInfo.hpp"
#include "mavis/DecoderTypes.h"

namespace olympia
{
    const InstArchInfo::TargetPipeMap InstArchInfo::execution_pipe_map = {
        {"br", InstArchInfo::TargetPipe::BR},
        {"cmov", InstArchInfo::TargetPipe::CMOV},
        {"div", InstArchInfo::TargetPipe::DIV},
        {"faddsub", InstArchInfo::TargetPipe::FADDSUB},
        {"float", InstArchInfo::TargetPipe::FLOAT},
        {"fmac", InstArchInfo::TargetPipe::FMAC},
        {"i2f", InstArchInfo::TargetPipe::I2F},
        {"f2i", InstArchInfo::TargetPipe::F2I},
        {"int", InstArchInfo::TargetPipe::INT},
        {"lsu", InstArchInfo::TargetPipe::LSU},
        {"mul", InstArchInfo::TargetPipe::MUL},
        {"vint", InstArchInfo::TargetPipe::VINT},
        {"vdiv", InstArchInfo::TargetPipe::VDIV},
        {"vmul", InstArchInfo::TargetPipe::VMUL},
        {"vfixed", InstArchInfo::TargetPipe::VFIXED},
        {"vmask", InstArchInfo::TargetPipe::VMASK},
        {"vmv", InstArchInfo::TargetPipe::VMV},
        {"v2s", InstArchInfo::TargetPipe::V2S},
        {"vfloat", InstArchInfo::TargetPipe::VFLOAT},
        {"vfdiv", InstArchInfo::TargetPipe::VFDIV},
        {"vfmul", InstArchInfo::TargetPipe::VFMUL},
        {"vpermute", InstArchInfo::TargetPipe::VPERMUTE},
        {"vload", InstArchInfo::TargetPipe::VLOAD},
        {"vstore", InstArchInfo::TargetPipe::VSTORE},
        {"vset", InstArchInfo::TargetPipe::VSET},
        {"rob", InstArchInfo::TargetPipe::ROB},
        {"sys", InstArchInfo::TargetPipe::SYS},
        {"?", InstArchInfo::TargetPipe::UNKNOWN}};

    const InstArchInfo::TargetPipeStringMap InstArchInfo::execution_pipe_string_map = {
        {InstArchInfo::TargetPipe::BR, "BR"},
        {InstArchInfo::TargetPipe::CMOV, "CMOV"},
        {InstArchInfo::TargetPipe::DIV, "DIV"},
        {InstArchInfo::TargetPipe::FADDSUB, "FADDSUB"},
        {InstArchInfo::TargetPipe::FLOAT, "FLOAT"},
        {InstArchInfo::TargetPipe::FMAC, "FMAC"},
        {InstArchInfo::TargetPipe::I2F, "I2F"},
        {InstArchInfo::TargetPipe::F2I, "F2I"},
        {InstArchInfo::TargetPipe::INT, "INT"},
        {InstArchInfo::TargetPipe::LSU, "LSU"},
        {InstArchInfo::TargetPipe::MUL, "MUL"},
        {InstArchInfo::TargetPipe::VINT, "VINT"},
        {InstArchInfo::TargetPipe::VDIV, "VDIV"},
        {InstArchInfo::TargetPipe::VMUL, "VMUL"},
        {InstArchInfo::TargetPipe::VFIXED, "VFIXED"},
        {InstArchInfo::TargetPipe::VMASK, "VMASK"},
        {InstArchInfo::TargetPipe::VMV, "VMV"},
        {InstArchInfo::TargetPipe::V2S, "V2S"},
        {InstArchInfo::TargetPipe::VFLOAT, "VFLOAT"},
        {InstArchInfo::TargetPipe::VFDIV, "VFDIV"},
        {InstArchInfo::TargetPipe::VFMUL, "VFMUL"},
        {InstArchInfo::TargetPipe::VPERMUTE, "VPERMUTE"},
        {InstArchInfo::TargetPipe::VLOAD, "VLOAD"},
        {InstArchInfo::TargetPipe::VSTORE, "VSTORE"},
        {InstArchInfo::TargetPipe::VSET, "VSET"},
        {InstArchInfo::TargetPipe::ROB, "ROB"},
        {InstArchInfo::TargetPipe::SYS, "SYS"},
        {InstArchInfo::TargetPipe::UNKNOWN, "?"}};

    const InstArchInfo::UopGenMap InstArchInfo::uop_gen_type_map = {
        {"ELEMENTWISE", InstArchInfo::UopGenType::ELEMENTWISE},
        {"SINGLE_DEST", InstArchInfo::UopGenType::SINGLE_DEST},
        {"SINGLE_SRC", InstArchInfo::UopGenType::SINGLE_SRC},
        {"WIDENING", InstArchInfo::UopGenType::WIDENING},
        {"WIDENING_MIXED", InstArchInfo::UopGenType::WIDENING_MIXED},
        {"NARROWING", InstArchInfo::UopGenType::NARROWING},
        {"MAC", InstArchInfo::UopGenType::MAC},
        {"MAC_WIDE", InstArchInfo::UopGenType::MAC_WIDE},
        {"REDUCTION", InstArchInfo::UopGenType::REDUCTION},
        {"REDUCTION_WIDE", InstArchInfo::UopGenType::REDUCTION_WIDE},
        {"INT_EXT", InstArchInfo::UopGenType::INT_EXT},
        {"SLIDE1UP", InstArchInfo::UopGenType::SLIDE1UP},
        {"SLIDE1DOWN", InstArchInfo::UopGenType::SLIDE1DOWN},
        {"PERMUTE", InstArchInfo::UopGenType::PERMUTE},
        {"NONE", InstArchInfo::UopGenType::NONE}};

    void InstArchInfo::update(const boost::json::object & jobj)
    {
        if (const auto it = jobj.find("pipe"); it != jobj.end())
        {
            auto pipe_name = it->value().as_string();
            const auto itr = execution_pipe_map.find(pipe_name);
            sparta_assert(itr != execution_pipe_map.end(),
                          "Unknown pipe target: " << pipe_name << " for inst: "
                                                  << jobj.at("mnemonic").as_string());
            tgt_pipe_ = itr->second;
        }

        if (const auto it = jobj.find("latency"); it != jobj.end())
        {
            execute_time_ = boost::json::value_to<uint32_t>(it->value());
        }

        if (const auto it = jobj.find("uop_gen"); it != jobj.end())
        {
            auto uop_gen_name = it->value().as_string();
            const auto itr = uop_gen_type_map.find(uop_gen_name);
            sparta_assert(itr != uop_gen_type_map.end(),
                          "Unknown uop gen: " << uop_gen_name << " for inst: "
                                              << jobj.at("mnemonic").as_string());
            uop_gen_ = itr->second;
        }

        is_load_store_ = (tgt_pipe_ == TargetPipe::LSU);
        is_vset_ = {tgt_pipe_ == TargetPipe::VSET};
    }

} // namespace olympia
