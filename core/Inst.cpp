// <Inst.cpp> -*- C++ -*-

#include "Inst.hpp"

namespace olympia
{

    /*!
     * \brief Construct an Instruction
     * \param opcode_info    Mavis Opcode information
     * \param inst_arch_info Pointer to the static data of instruction
     * \param clk            Core clock
     *
     * Called by Mavis when an opcode is decoded to a particular
     * instruction.
     */
    Inst::Inst(const mavis::OpcodeInfo::PtrType& opcode_info,
               const InstArchInfo::PtrType     & inst_arch_info,
               const sparta::Clock             * clk) :
        opcode_info_    (opcode_info),
        inst_arch_info_ (inst_arch_info),
        is_store_(opcode_info->isInstType(mavis::OpcodeInfo::InstructionTypes::STORE)),
        is_transfer_(miscutils::isOneOf(inst_arch_info_->getTargetPipe(),
                                        InstArchInfo::TargetPipe::I2F,
                                        InstArchInfo::TargetPipe::F2I)),

        status_state_(Status::FETCHED)
    {
        sparta_assert(inst_arch_info_ != nullptr,
                      "Mavis decoded the instruction, but Olympia has no uarch data for it: "
                      << getDisasm() << " " << std::hex << " opc: 0x" << getOpCode());
    }
}
