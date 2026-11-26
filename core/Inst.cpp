// <Inst.cpp> -*- C++ -*-

#include "Inst.hpp"
#include "InstGenerator.hpp"
#include "rename/RenameData.hpp"
#include "CoreUtils.hpp"
#include <iomanip>
#include <unordered_map>

namespace olympia
{
    std::ostream & operator<<(std::ostream & os, const Inst::Status & status)
    {
        switch (status)
        {
        case Inst::Status::BEFORE_FETCH:
            os << "BEFORE_FETCH";
            break;
        case Inst::Status::FETCHED:
            os << "FETCHED";
            break;
        case Inst::Status::DECODED:
            os << "DECODED";
            break;
        case Inst::Status::RENAMED:
            os << "RENAMED";
            break;
        case Inst::Status::DISPATCHED:
            os << "DISPATCHED";
            break;
        case Inst::Status::SCHEDULED:
            os << "SCHEDULED";
            break;
        case Inst::Status::COMPLETED:
            os << "COMPLETED";
            break;
        case Inst::Status::RETIRED:
            os << "RETIRED";
            break;
        case Inst::Status::FLUSHED:
            os << "FLUSHED";
            break;
        case Inst::Status::UNMOD:
            os << "UNMOD";
            break;
        case Inst::Status::FUSED:
            os << "FUSED";
            break;
        case Inst::Status::FUSION_GHOST:
            os << "FUSION_GHOST";
            break;
        case Inst::Status::__LAST:
            throw sparta::SpartaException("__LAST cannot be a valid enum state.");
        }
        return os;
    }

    std::ostream & operator<<(std::ostream & os, const Inst & inst)
    {
        os << "uid:" << inst.getUniqueID() << " " << std::setw(10) << inst.getStatus() << " "
           << std::hex << inst.getPC() << std::dec << " pid:" << inst.getProgramID()
           << " uopid:" << inst.getUOpID() << " '" << inst.getDisasm() << "' ";
        return os;
    }

    std::ostream & operator<<(std::ostream & os, const InstPtr & inst)
    {
        if (inst)
        {
            os << *inst;
        }
        else
        {
            os << "nullptr";
        }
        return os;
    }

    RenameData::OpInfoWithRegfile::OpInfoWithRegfile(const OpInfoList::value_type & op_info) :
        field_value(op_info.field_value),
        field_id(op_info.field_id),
        reg_file(coreutils::determineRegisterFile(op_info)),
        is_x0(field_value == 0 && reg_file == core_types::RF_INTEGER)
    {
    }

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

    template <class OpInfoListType>
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
        clk_(clk),
        dest_opcode_info_with_reg_file_(
            getOpcodeInfoWithRegFileInfo<RenameData::DestOpInfoWithRegfileList>(
                opcode_info_->getDestOpInfoList())),
        src_opcode_info_with_reg_file_(
            getOpcodeInfoWithRegFileInfo<RenameData::SrcOpInfoWithRegfileList>(
                opcode_info_->getSourceOpInfoList())),
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
        sparta_assert(clk_ != nullptr, "Inst requires a valid clock");
        sparta_assert(inst_arch_info_ != nullptr,
                      "Mavis decoded the instruction, but Olympia has no uarch data for it: "
                          << getDisasm() << " " << std::hex << " opc: 0x" << getOpCode());

        // Check that instruction is supported
        sparta_assert(getPipe() != InstArchInfo::TargetPipe::UNKNOWN,
                      "Unknown target pipe (execution) for " << getMnemonic());
        sparta_assert(getExecuteTime() != 0,
                      "Unknown execution time (latency) for " << getMnemonic());
    }
#ifdef EDM_ENABLED

    void Inst::notifyFlush(const InstPtr & self)
    {
        if (edm_generator_)
        {
            edm_generator_->onFlush(self);
        }
    }

    void Inst::notifyRetire(const InstPtr & self)
    {
        if (edm_generator_)
        {
            edm_generator_->onRetire(self);
        }
    }

    void Inst::notifyStoreCommit(const InstPtr & self)
    {
        if (edm_generator_)
        {
            edm_generator_->onRetireStore(self);
        }
    }

    void Inst::notifyStoreDrop(const InstPtr & self)
    {
        if (edm_generator_)
        {
            edm_generator_->onDropStore(self);
        }
    }
#endif

    void Inst::finalizeCPIBreakdown()
    {
        constexpr std::array<Status, static_cast<size_t>(CPIStallType::NUM_TYPES) + 1>
            cpi_statuses = {Status::FETCHED,    Status::DECODED,   Status::RENAMED,
                            Status::DISPATCHED, Status::SCHEDULED, Status::COMPLETED,
                            Status::RETIRED};

        sparta_assert(getStatus() == Status::RETIRED,
                      "CPI breakdown can only be finalized for retired instructions: " << *this);

        for (size_t i = 0; i < cpi_breakdown_.cycles.size(); ++i)
        {
            const auto start_status = cpi_statuses[i];
            const auto end_status = cpi_statuses[i + 1];
            const auto start_cycle = getTimestamp(start_status);
            const auto end_cycle = getTimestamp(end_status);

            sparta_assert(end_cycle >= start_cycle,
                          "CPI timestamp for status "
                              << static_cast<uint32_t>(end_status) << " precedes status "
                              << static_cast<uint32_t>(start_status) << " for inst " << *this);
            cpi_breakdown_.cycles[i] = end_cycle - start_cycle;
        }
    }
} // namespace olympia
