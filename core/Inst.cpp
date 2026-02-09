// <Inst.cpp> -*- C++ -*-

#include "Inst.hpp"
#include "rename/RenameData.hpp"
#include "CoreUtils.hpp"
#include <unordered_map>

namespace olympia
{
    const std::unordered_map<Inst::Status, std::string> Inst::status2String = {
        {Inst::Status::BEFORE_FETCH, "BEFORE_FETCH"},
        {Inst::Status::FETCHED, "FETCHED"},
        {Inst::Status::DECODED, "DECODED"},
        {Inst::Status::RENAMED, "RENAMED"},
        {Inst::Status::DISPATCHED, "DISPATCHED"},
        {Inst::Status::SCHEDULED, "SCHEDULED"},
        {Inst::Status::COMPLETED, "COMPLETED"},
        {Inst::Status::RETIRED, "RETIRED"},
        {Inst::Status::FLUSHED, "FLUSHED"},
        {Inst::Status::UNMOD, "UNMOD"},
        {Inst::Status::FUSED, "FUSED"},
        {Inst::Status::FUSION_GHOST, "FUSION_GHOST"}};

    RenameData::OpInfoWithRegfile::OpInfoWithRegfile(const OpInfoList::value_type & op_info) :
        field_value(op_info.field_value),
        field_id(op_info.field_id),
        reg_file(coreutils::determineRegisterFile(op_info)),
        is_x0(field_value == 0 && reg_file == core_types::RF_INTEGER)
    { }


    bool isCallInstruction(const mavis::OpcodeInfo::PtrType & opcode_info)
    {
        if (opcode_info->isInstType(mavis::OpcodeInfo::InstructionTypes::JAL)
            || opcode_info->isInstType(mavis::OpcodeInfo::InstructionTypes::JALR))
        {
            const int dest =
                opcode_info->getDestOpInfo().getFieldValue(mavis::InstMetaData::OperandFieldID::RD);
            return miscutils::isOneOf(dest, 1, 5);
        }
        return false;
    }

    bool isReturnInstruction(const mavis::OpcodeInfo::PtrType & opcode_info)
    {
        if (opcode_info->isInstType(mavis::OpcodeInfo::InstructionTypes::JALR))
        {
            const int dest =
                opcode_info->getDestOpInfo().getFieldValue(mavis::InstMetaData::OperandFieldID::RD);
            const int src = opcode_info->getSourceOpInfo().getFieldValue(
                mavis::InstMetaData::OperandFieldID::RS1);
            if (dest != src)
            {
                return miscutils::isOneOf(src, 1, 5);
            }
        }
        return false;
    }

    template<class OpInfoListType>
    OpInfoListType getOpcodeInfoWithRegFileInfo(const Inst::OpInfoList & mavis_opcode_info)
    {
        OpInfoListType op_list;
        for (const auto & op : mavis_opcode_info)
        {
            op_list.emplace_back(op);
        }
        return op_list;
    }

    /*!
     * \brief Construct an Instruction
     * \param opcode_info    Mavis Opcode information
     * \param inst_arch_info Pointer to the static data of instruction
     * \param clk            Core clock
     *
     * Called by Mavis when an opcode is decoded to a particular
     * instruction.
     */
    Inst::Inst(const mavis::OpcodeInfo::PtrType & opcode_info,
               const InstArchInfo::PtrType & inst_arch_info, const sparta::Clock* clk) :
        opcode_info_(opcode_info),
        inst_arch_info_(inst_arch_info),
        dest_opcode_info_with_reg_file_(
            getOpcodeInfoWithRegFileInfo<RenameData::DestOpInfoWithRegfileList>(opcode_info_->getDestOpInfoList())),
        src_opcode_info_with_reg_file_(
            getOpcodeInfoWithRegFileInfo<RenameData::SrcOpInfoWithRegfileList>(opcode_info_->getSourceOpInfoList())),
        is_store_(opcode_info->isInstType(mavis::OpcodeInfo::InstructionTypes::STORE)),
        is_load_(opcode_info->isInstType(mavis::OpcodeInfo::InstructionTypes::LOAD)),
        is_move_(opcode_info->isInstTypeAnyOf(mavis::OpcodeInfo::InstructionTypes::MOVE)),
        is_transfer_(miscutils::isOneOf(inst_arch_info_->getTargetPipe(),
                                        InstArchInfo::TargetPipe::I2F,
                                        InstArchInfo::TargetPipe::F2I)),
        is_branch_(opcode_info_->isInstType(mavis::OpcodeInfo::InstructionTypes::BRANCH)),
        is_condbranch_(opcode_info_->isInstType(mavis::OpcodeInfo::InstructionTypes::CONDITIONAL)),
        is_call_(isCallInstruction(opcode_info)),
        is_csr_(opcode_info->isInstType(mavis::OpcodeInfo::InstructionTypes::CSR)),
        is_return_(isReturnInstruction(opcode_info)),
        has_immediate_(opcode_info_->hasImmediate()),
        rob_targeted_(getPipe() == InstArchInfo::TargetPipe::ROB),
        is_vector_(opcode_info->isInstType(mavis::OpcodeInfo::InstructionTypes::VECTOR)),
        is_vector_whole_reg_(
            is_vector_ && opcode_info->isInstType(mavis::OpcodeInfo::InstructionTypes::WHOLE)),
        status_state_(Status::BEFORE_FETCH)
    {
        sparta_assert(inst_arch_info_ != nullptr,
                      "Mavis decoded the instruction, but Olympia has no uarch data for it: "
                          << getDisasm() << " " << std::hex << " opc: 0x" << getOpCode());

        // Check that instruction is supported
        sparta_assert(getPipe() != InstArchInfo::TargetPipe::UNKNOWN,
                      "Unknown target pipe (execution) for " << getMnemonic());
        sparta_assert(getExecuteTime() != 0,
                      "Unknown execution time (latency) for " << getMnemonic());
    }
} // namespace olympia
