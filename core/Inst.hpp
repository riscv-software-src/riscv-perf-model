// <Inst.h> -*- C++ -*-

#pragma once

#include "sparta/memory/AddressTypes.hpp"
#include "sparta/resources/Scoreboard.hpp"
#include "sparta/resources/Queue.hpp"
#include "sparta/pairs/SpartaKeyPairs.hpp"
#include "sparta/simulation/State.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"
#include "mavis/OpcodeInfo.h"

#include "InstArchInfo.hpp"
#include "CoreTypes.hpp"
#include "MiscUtils.hpp"

#include <cstdlib>
#include <ostream>
#include <map>

namespace olympia
{
    /*!
     * \class Inst
     * \brief Example instruction that flows through the example/CoreModel
     */

    // Forward declaration of the Pair Definition class is must as we are friending it.
    class InstPairDef;

    class Inst {
    public:

        class RenameData
        {
        public:
            // A register consists of its value and its register file.
            struct Reg {
                uint32_t val = 0;
                core_types::RegFile rf = core_types::RegFile::RF_INVALID;
                mavis::InstMetaData::OperandFieldID field_id = mavis::InstMetaData::OperandFieldID::NONE;
            };
            using RegList = std::vector<Reg>;

            void setOriginalDestination(const Reg & destination){
                original_dest_ = destination;
            }
            void setDataReg(const Reg & data_reg){
                data_reg_ = data_reg;
            }
            void setSource(const Reg & source){
                src_.emplace_back(source);
            }
            const RegList & getSourceList() const {
                return src_;
            }
            const Reg & getOriginalDestination() const {
                return original_dest_;
            }
            const Reg & getDataReg() const {
                return data_reg_;
            }
        private:
            Reg original_dest_;
            RegList src_;
            Reg data_reg_;
        };

        // Used by Mavis
        using PtrType = sparta::SpartaSharedPointer<Inst>;

        // The modeler needs to alias a type called
        // "SpartaPairDefinitionType" to the Pair Definition class of
        // itself
        using SpartaPairDefinitionType = InstPairDef;

        enum class Status : std::uint16_t{
            FETCHED = 0,
            __FIRST = FETCHED,
            DECODED,
            RENAMED,
            DISPATCHED,
            SCHEDULED,
            COMPLETED,
            RETIRED,
            __LAST
        };

        /*!
         * \brief Construct an Instruction
         * \param opcode_info    Mavis Opcode information
         * \param inst_arch_info Pointer to the static data of instruction
         * \param clk            Core clock
         *
         * Called by Mavis when an opcode is decoded to a particular
         * instruction.
        */
        Inst(const mavis::OpcodeInfo::PtrType& opcode_info,
             const InstArchInfo::PtrType     & inst_arch_info,
             const sparta::Clock             * clk);

        // This is needed by Mavis as an optimization.  Try NOT to
        // implement it and let the compiler do it for us for speed.
        Inst(const Inst& other) = default;

        const Status & getStatus() const {
            return status_state_;
        }

        bool getCompletedStatus() const {
            return getStatus() == olympia::Inst::Status::COMPLETED;
        }

        void setStatus(Status status) {
            status_state_ = status;
            if(getStatus() == Status::COMPLETED) {
                if(ev_retire_ != 0) {
                    ev_retire_->schedule();
                }
            }
        }

        InstArchInfo::TargetUnit getUnit() const {
            return inst_arch_info_->getTargetUnit();
        }

        InstArchInfo::TargetPipe getPipe() const {
            return inst_arch_info_->getTargetPipe();
        }

        void setOldest(bool oldest, sparta::Scheduleable * rob_retire_event) {
            ev_retire_ = rob_retire_event;
            is_oldest_ = oldest;
        }

        bool isMarkedOldest() const { return is_oldest_; }

        // Set the instructions unique ID.  This ID in constantly
        // incremented and does not repeat.  The same instruction in a
        // trace can have different unique IDs (due to flushing)
        void     setUniqueID(uint64_t uid) { unique_id_ = uid; }
        uint64_t getUniqueID() const       { return unique_id_; }

        // Set the instruction's Program ID.  This ID is specific to
        // an instruction's retire pointer.  The same instruction in a
        // trace will have the same program ID (as compared to
        // UniqueID).
        void     setProgramID(uint64_t prog_id) { program_id_ = prog_id; }
        uint64_t getProgramID() const           { return program_id_; }

        // Set the instruction's PC
        void setPC(sparta::memory::addr_t inst_pc) { inst_pc_ = inst_pc; }
        sparta::memory::addr_t getPC() const       { return inst_pc_; }

        // Set the instruction's target PC (branch target or load/store target)
        void     setTargetVAddr(sparta::memory::addr_t target_vaddr) { target_vaddr_ = target_vaddr; }
        sparta::memory::addr_t getTargetVAddr() const                { return target_vaddr_; }

        // TBD -- add branch prediction
        void setSpeculative(bool spec) { is_speculative_ = spec; }

        // Opcode information
        std::string getMnemonic() const { return opcode_info_->getMnemonic(); }
        std::string getDisasm()   const { return opcode_info_->dasmString(); }
        uint32_t    getOpCode()   const { return static_cast<uint32_t>(opcode_info_->getOpcode()); }

        // Operand information
        using OpInfoList = mavis::DecodedInstructionInfo::OpInfoList;
        const OpInfoList& getSourceOpInfoList() const { return opcode_info_->getSourceOpInfoList(); }
        const OpInfoList& getDestOpInfoList()   const { return opcode_info_->getDestOpInfoList(); }

        // Static instruction information
        bool        isStoreInst() const    { return is_store_; }
        bool        isLoadStoreInst() const {return inst_arch_info_->isLoadStore(); }
        uint32_t    getExecuteTime() const { return inst_arch_info_->getExecutionTime(); }

        uint64_t    getRAdr() const        { return target_vaddr_ | 0x8000000; } // faked
        bool        isSpeculative() const  { return is_speculative_; }
        bool        isTransfer() const     { return is_transfer_; }

        // Rename information
        core_types::RegisterBitMask & getSrcRegisterBitMask(const core_types::RegFile rf) {
            return src_reg_bit_masks_[rf];
        }
        core_types::RegisterBitMask & getDestRegisterBitMask(const core_types::RegFile rf) {
            return dest_reg_bit_masks_[rf];
        }
        core_types::RegisterBitMask & getDataRegisterBitMask(const core_types::RegFile rf) {
            return store_data_mask_[rf];
        }
        const core_types::RegisterBitMask & getSrcRegisterBitMask(const core_types::RegFile rf) const {
            return src_reg_bit_masks_[rf];
        }
        const core_types::RegisterBitMask & getDestRegisterBitMask(const core_types::RegFile rf) const {
            return dest_reg_bit_masks_[rf];
        }
        const core_types::RegisterBitMask & getDataRegisterBitMask(const core_types::RegFile rf) const {
            return store_data_mask_[rf];
        }
        RenameData & getRenameData() {
            return rename_data;
        }
        const RenameData & getRenameData() const{
            return rename_data;
        }
    private:
        mavis::OpcodeInfo::PtrType opcode_info_;
        InstArchInfo::PtrType      inst_arch_info_;

        sparta::memory::addr_t inst_pc_       = 0; // Instruction's PC
        sparta::memory::addr_t target_vaddr_  = 0; // Instruction's Target PC (for branches, loads/stores)
        bool                   is_oldest_       = false;
        uint64_t               unique_id_     = 0; // Supplied by Fetch
        uint64_t               program_id_    = 0; // Supplied by a trace Reader or execution backend
        bool                   is_speculative_ = false; // Is this instruction soon to be flushed?
        const bool             is_store_;
        const bool             is_transfer_;  // Is this a transfer instruction (F2I/I2F)
        sparta::Scheduleable * ev_retire_    = nullptr;
        Status                 status_state_;

        // Rename information
        using RegisterBitMaskArray = std::array<core_types::RegisterBitMask, core_types::RegFile::N_REGFILES>;
        RegisterBitMaskArray src_reg_bit_masks_;
        RegisterBitMaskArray dest_reg_bit_masks_;
        RegisterBitMaskArray store_data_mask_;
        RenameData rename_data;
    };

    using InstPtr = Inst::PtrType;
    using InstQueue = sparta::Queue<InstPtr>;

    inline std::ostream & operator<<(std::ostream & os, const Inst::Status & status) {
        switch(status) {
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
            case Inst::Status::__LAST:
                throw sparta::SpartaException("__LAST cannot be a valid enum state.");
        }
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os, const Inst & inst) {
        os << "uid: " << inst.getUniqueID()
           << " " << std::setw(10) << inst.getStatus()
           << " " << std::hex << inst.getPC() << std::dec
           << " pid: " << inst.getProgramID() << " '" << inst.getDisasm() << "' ";
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os, const InstPtr & inst) {
        os << *inst;
        return os;
    }

    /*!
     * \class InstPairDef
     * \brief Pair Definition class of the Example instruction that flows through the example/CoreModel
     */
    // This is the definition of the PairDefinition class of Inst.
    // This PairDefinition class could be named anything but it needs to
    // inherit publicly from sparta::PairDefinition templatized on the actual class Inst.
    class InstPairDef : public sparta::PairDefinition<Inst>{
    public:

        // The SPARTA_ADDPAIRs APIs must be called during the construction of the PairDefinition class
        InstPairDef() : PairDefinition<Inst>(){
            SPARTA_INVOKE_PAIRS(Inst);
        }
        SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("DID",       &Inst::getUniqueID),
                              SPARTA_ADDPAIR("uid",       &Inst::getUniqueID),
                              SPARTA_ADDPAIR("mnemonic",  &Inst::getMnemonic),
                              SPARTA_ADDPAIR("complete",  &Inst::getCompletedStatus),
                              SPARTA_ADDPAIR("unit",      &Inst::getUnit),
                              SPARTA_ADDPAIR("latency",   &Inst::getExecuteTime),
                              SPARTA_ADDPAIR("raddr",     &Inst::getRAdr, std::ios::hex),
                              SPARTA_ADDPAIR("tgt_vaddr", &Inst::getTargetVAddr, std::ios::hex))
    };

    // Instruction allocators
    using InstAllocator         = sparta::SpartaSharedPointerAllocator<Inst>;
    using InstArchInfoAllocator = sparta::SpartaSharedPointerAllocator<InstArchInfo>;
}
