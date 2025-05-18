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

#include "stf-inc/stf_inst_reader.hpp"

#include "InstArchInfo.hpp"
#include "CoreTypes.hpp"
#include "vector/VectorConfig.hpp"
#include "MiscUtils.hpp"

#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <unordered_map>
#include <variant>
#include <sstream>

namespace olympia
{
    /*!
     * \class Inst
     * \brief Example instruction that flows through the example/CoreModel
     */

    // Forward declaration of the Pair Definition class is must as we are friending it.
    class InstPairDef;

    class Inst
    {
      public:
        class RenameData
        {
          public:
            // A register consists of its value and its register file.
            struct Reg
            {
                uint32_t val = 0;
                core_types::RegFile rf = core_types::RegFile::RF_INVALID;
                mavis::InstMetaData::OperandFieldID field_id =
                    mavis::InstMetaData::OperandFieldID::NONE;
                bool is_x0 = false;
            };

            using RegList = std::vector<Reg>;

            void setOriginalDestination(const Reg & destination) { original_dest_ = destination; }

            void setDestination(const Reg & destination) { dest_ = destination; }

            void setDataReg(const Reg & data_reg) { data_reg_ = data_reg; }

            void setSource(const Reg & source) { src_.emplace_back(source); }

            const RegList & getSourceList() const { return src_; }

            const Reg & getOriginalDestination() const { return original_dest_; }

            const Reg & getDestination() const { return dest_; }

            const Reg & getDataReg() const { return data_reg_; }

          private:
            Reg original_dest_;
            Reg dest_;
            RegList src_;
            Reg data_reg_;
        };

        // Used by Mavis
        using PtrType = sparta::SpartaSharedPointer<Inst>;

        // The modeler needs to alias a type called
        // "SpartaPairDefinitionType" to the Pair Definition class of
        // itself
        using SpartaPairDefinitionType = InstPairDef;

        enum class Status : std::uint16_t
        {
            BEFORE_FETCH = 0,
            __FIRST = BEFORE_FETCH,
            FETCHED,
            DECODED,
            RENAMED,
            DISPATCHED,
            SCHEDULED,
            COMPLETED,
            RETIRED,
            FLUSHED,
            UNMOD, // no extended status
            FUSED,
            FUSION_GHOST,
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
        Inst(const mavis::OpcodeInfo::PtrType & opcode_info,
             const InstArchInfo::PtrType & inst_arch_info, const sparta::Clock* clk);

        // This is needed by Mavis as an optimization.  Try NOT to
        // implement it and let the compiler do it for us for speed.
        Inst(const Inst & other) = default;

        const Status & getStatus() const { return status_state_; }

        bool getCompletedStatus() const { return getStatus() == olympia::Inst::Status::COMPLETED; }

        bool getFlushedStatus() const { return getStatus() == olympia::Inst::Status::FLUSHED; }

        void setStatus(Status status)
        {
            sparta_assert(status_state_ != status,
                          "Status being set twice to the same value: " << status << " " << *this);
            sparta_assert(status > status_state_, "Cannot go backwards in status.  Current: "
                                                      << status_state_ << " New: " << status
                                                      << *this);
            status_state_ = status;
            if (getStatus() == Status::COMPLETED)
            {
                if (ev_retire_ != 0)
                {
                    ev_retire_->schedule();
                }
            }
        }

        const Status & getExtendedStatus() const { return extended_status_state_; }

        void setExtendedStatus(Status status)
        {
            sparta_assert(extended_status_state_ == Inst::Status::UNMOD
                              || extended_status_state_ == Inst::Status::FUSED
                              || extended_status_state_ == Inst::Status::FUSION_GHOST,
                          "Attempt to set unknown extended status : " << status << " " << *this);

            extended_status_state_ = status;
        }

        InstArchInfo::TargetPipe getPipe() const { return inst_arch_info_->getTargetPipe(); }

        // ROB handling -- mark this instruction as the oldest in the machine
        void setOldest(bool oldest, sparta::Scheduleable* rob_retire_event)
        {
            ev_retire_ = rob_retire_event;
            is_oldest_ = oldest;
        }

        bool isMarkedOldest() const { return is_oldest_; }

        // Rewind iterator used for going back in program simulation after flushes
        template <typename T> void setRewindIterator(T iter) { rewind_iter_ = iter; }

        template <typename T> T getRewindIterator() const { return std::get<T>(rewind_iter_); }

        // Set the instructions unique ID.  This ID in constantly
        // incremented and does not repeat.  The same instruction in a
        // trace can have different unique IDs (due to flushing)
        void setUniqueID(uint64_t uid) { unique_id_ = uid; }

        uint64_t getUniqueID() const { return unique_id_; }

        // Set the instruction's UOp ID. This ID is incremented based
        // off of number of Uops. The UOp instructions will all have the same
        // UID, but different UOp IDs.
        void setUOpID(uint64_t uopid) { uopid_ = uopid; }

        // Set the instruction's UOp ID. This ID is incremented based
        // off of number of Uops. The UOp instructions will all have the same
        // UID, but different UOp IDs.
        uint64_t getUOpID() const { return uopid_.isValid() ? uopid_.getValue() : 0; }

        void setBlockingVSET(bool is_blocking_vset) { is_blocking_vset_ = is_blocking_vset; }

        bool isBlockingVSET() const { return is_blocking_vset_; }

        // Set the instruction's Program ID.  This ID is specific to
        // an instruction's retire pointer.  The same instruction in a
        // trace will have the same program ID (as compared to
        // UniqueID). Fusion changes this a bit see below
        void setProgramID(uint64_t prog_id) { program_id_ = prog_id; }

        uint64_t getProgramID() const { return program_id_; }

        // A fused operation will modify the program_id_increment_ based on
        // the number of instructions fused. A-B-C-D -> fA  incr becomes 4
        // This is planned, but not currently used.
        void setProgramIDIncrement(uint64_t incr) { program_id_increment_ = incr; }

        // This is planned, but not currently used.
        uint64_t getProgramIDIncrement() const { return program_id_increment_; }

        // Set the instruction's PC
        void setPC(sparta::memory::addr_t inst_pc) { inst_pc_ = inst_pc; }

        sparta::memory::addr_t getPC() const { return inst_pc_; }

        // Set the instruction's target PC (branch target or load/store target)
        void setTargetVAddr(sparta::memory::addr_t target_vaddr) { target_vaddr_ = target_vaddr; }

        sparta::memory::addr_t getTargetVAddr() const { return target_vaddr_; }

        void setVectorConfig(const VectorConfigPtr input_vector_config)
        {
            vector_config_ = input_vector_config;
        }

        const VectorConfigPtr getVectorConfig() const { return vector_config_; }

        VectorConfigPtr getVectorConfig() { return vector_config_; }

        void setTail(bool has_tail) { has_tail_ = has_tail; }

        bool hasTail() const { return has_tail_; }

        void setUOpParent(sparta::SpartaWeakPointer<olympia::Inst> & parent_uop)
        {
            parent_uop_ = parent_uop;
        }

        sparta::SpartaWeakPointer<olympia::Inst> getUOpParent() { return parent_uop_; }

        // Branch instruction was taken (always set for JAL/JALR)
        void setTakenBranch(bool taken) { is_taken_branch_ = taken; }

        // Is this branch instruction mispredicted?
        bool isMispredicted() const { return is_mispredicted_; }

        void setMispredicted() { is_mispredicted_ = true; }

        // TBD -- add branch prediction
        void setSpeculative(bool spec) { is_speculative_ = spec; }

        // Last instruction within the cache block fetched from the ICache
        void setLastInFetchBlock(bool last) { last_in_fetch_block_ = last; }

        bool isLastInFetchBlock() const { return last_in_fetch_block_; }

        // Opcode information
        std::string getMnemonic() const { return opcode_info_->getMnemonic(); }

        std::string getDisasm() const { return opcode_info_->dasmString(); }

        uint32_t getOpCode() const { return static_cast<uint32_t>(opcode_info_->getOpcode()); }

        // Get the data size in bytes
        uint32_t getMemAccessSize() const { return opcode_info_->getDataSize() / 8; }  // opcode_info's data size is in bits

        mavis::InstructionUniqueID getMavisUid() const
        {
            return opcode_info_->getInstructionUniqueID();
        }

        // Operand information
        using OpInfoList = mavis::DecodedInstructionInfo::OpInfoList;

        const OpInfoList & getSourceOpInfoList() const
        {
            return opcode_info_->getSourceOpInfoList();
        }

        const OpInfoList & getDestOpInfoList() const { return opcode_info_->getDestOpInfoList(); }

        bool hasZeroRegSource() const
        {
            return std::any_of(getSourceOpInfoList().begin(), getSourceOpInfoList().end(),
                               [](const mavis::OperandInfo::Element & elem)
                               { return elem.field_value == 0; });
        }

        bool hasZeroRegDest() const
        {
            return std::any_of(getDestOpInfoList().begin(), getDestOpInfoList().end(),
                               [](const mavis::OperandInfo::Element & elem)
                               { return elem.field_value == 0; });
        }

        uint64_t getImmediate() const
        {
            sparta_assert(has_immediate_, "Instruction does not have an immediate!");
            return opcode_info_->getImmediate();
        }

        uint64_t getSpecialField(const mavis::OpcodeInfo::SpecialField sfid) const
        {
            return opcode_info_->getSpecialField(sfid);
        }

        bool getVectorMaskEnabled() const
        {
            try
            {
                // If vm bit is 0, masking is enabled
                const uint64_t vm_bit =
                    opcode_info_->getSpecialField(mavis::OpcodeInfo::SpecialField::VM);
                return vm_bit == 0;
            }
            catch (const mavis::UnsupportedExtractorSpecialFieldID & mavis_exception)
            {
                return false;
            }
            catch (const mavis::InvalidExtractorSpecialFieldID & mavis_exception)
            {
                return false;
            }
        }

        // Static instruction information
        bool isStoreInst() const { return is_store_; }

        bool isLoadStoreInst() const { return inst_arch_info_->isLoadStore(); }

        uint32_t getExecuteTime() const { return inst_arch_info_->getExecutionTime(); }

        InstArchInfo::UopGenType getUopGenType() const { return inst_arch_info_->getUopGenType(); }

        uint64_t getRAdr() const { return target_vaddr_ | 0x8000000; } // faked

        bool isSpeculative() const { return is_speculative_; }

        bool isTransfer() const { return is_transfer_; }

        bool isTakenBranch() const { return is_taken_branch_; }

        bool isBranch() const { return is_branch_; }

        bool isCondBranch() const { return is_condbranch_; }

        bool isCall() const { return is_call_; }

        bool isCSR() const { return is_csr_; }

        bool isReturn() const { return is_return_; }

        bool hasImmediate() const { return has_immediate_; }

        bool isVset() const { return inst_arch_info_->isVset(); }

        bool isVector() const { return is_vector_; }

        bool isVectorWholeRegister() const { return is_vector_whole_reg_; }

        void setCoF(const bool & cof) { is_cof_ = cof; }

        bool isCoF() const { return is_cof_; }

        // Rename information
        core_types::RegisterBitMask & getSrcRegisterBitMask(const core_types::RegFile rf)
        {
            return src_reg_bit_masks_[rf];
        }

        core_types::RegisterBitMask & getDestRegisterBitMask(const core_types::RegFile rf)
        {
            return dest_reg_bit_masks_[rf];
        }

        core_types::RegisterBitMask & getDataRegisterBitMask(const core_types::RegFile rf)
        {
            return store_data_mask_[rf];
        }

        const core_types::RegisterBitMask &
        getSrcRegisterBitMask(const core_types::RegFile rf) const
        {
            return src_reg_bit_masks_[rf];
        }

        const core_types::RegisterBitMask &
        getDestRegisterBitMask(const core_types::RegFile rf) const
        {
            return dest_reg_bit_masks_[rf];
        }

        const core_types::RegisterBitMask &
        getDataRegisterBitMask(const core_types::RegFile rf) const
        {
            return store_data_mask_[rf];
        }

        RenameData & getRenameData() { return rename_data; }

        const RenameData & getRenameData() const { return rename_data; }

        mavis::OpcodeInfo::PtrType getOpCodeInfo() { return opcode_info_; }

        // Duplicates stream operator but does not change EXPECT logs
        std::string info()
        {
            std::string rStatus = "DONTCARE";
            std::string eStatus = "UNKNOWN";

            if (getExtendedStatus() == Inst::Status::UNMOD)
            {
                eStatus = "UNMOD";
            }
            else if (getExtendedStatus() == Inst::Status::FUSED)
            {
                eStatus = "FUSED";
            }
            else if (getExtendedStatus() == Inst::Status::FUSION_GHOST)
            {
                eStatus = "GHOST";
            }

            std::stringstream ss;

            ss << "uid: " << std::dec                   //<< std::hex << std::setfill('0')
               << getUniqueID() << " pid: " << std::dec //<< std::hex << std::setfill('0')
               << getProgramID() << " mav: 0x" << std::hex << std::setw(2) << std::setfill('0')
               << getMavisUid() << " inc: " << std::dec << getProgramIDIncrement() << " pc: 0x"
               << std::hex << std::setw(8) << std::setfill('0') << getPC() << " " << std::setw(10)
               << std::setfill(' ') << rStatus << " " << std::setw(12) << std::setfill(' ')
               << eStatus << " '" << getDisasm() << "'";

            return ss.str();
        }

      private:
        mavis::OpcodeInfo::PtrType opcode_info_;
        InstArchInfo::PtrType inst_arch_info_;

        sparta::memory::addr_t inst_pc_ = 0; // Instruction's PC
        sparta::memory::addr_t target_vaddr_ =
            0; // Instruction's Target PC (for branches, loads/stores)
        bool is_oldest_ = false;
        uint64_t unique_id_ = 0;  // Supplied by Fetch
        uint64_t program_id_ = 0; // Supplied by a trace Reader or execution backend
        sparta::utils::ValidValue<uint64_t> uopid_; // Set in decode
        uint64_t program_id_increment_ = 1;
        bool is_speculative_ = false; // Is this instruction soon to be flushed?
        const bool is_store_;
        const bool is_transfer_; // Is this a transfer instruction (F2I/I2F)
        const bool is_branch_;
        const bool is_condbranch_;
        const bool is_call_;
        const bool is_csr_;
        const bool is_return_;
        // Is this instruction a change of flow?
        bool is_cof_ = false;
        const bool has_immediate_;

        // Vector
        const bool is_vector_;
        const bool is_vector_whole_reg_;

        VectorConfigPtr vector_config_{new VectorConfig};
        bool has_tail_ = false; // Does this vector uop have a tail?

        // blocking vset is a vset that needs to read a value from a register value. A blocking vset
        // can't be resolved until after execution, so we need to block on it due to UOp fracturing
        bool is_blocking_vset_ = false;

        sparta::SpartaWeakPointer<olympia::Inst> parent_uop_;

        // Did this instruction mispredict?
        bool is_mispredicted_ = false;
        bool is_taken_branch_ = false;
        bool last_in_fetch_block_ = false; // This is the last instruction in the fetch block
        sparta::Scheduleable* ev_retire_ = nullptr;
        Status status_state_;
        Status extended_status_state_{Inst::Status::UNMOD};

        using JSONIterator = uint64_t;
        using RewindIterator = std::variant<stf::STFInstReader::iterator, JSONIterator>;
        RewindIterator rewind_iter_;

        // Rename information
        using RegisterBitMaskArray =
            std::array<core_types::RegisterBitMask, core_types::RegFile::N_REGFILES>;
        RegisterBitMaskArray src_reg_bit_masks_;
        RegisterBitMaskArray dest_reg_bit_masks_;
        RegisterBitMaskArray store_data_mask_;
        RenameData rename_data;
        static const std::unordered_map<Inst::Status, std::string> status2String;
    };

    using InstPtr = Inst::PtrType;
    using InstQueue = sparta::Queue<InstPtr>;

    inline std::ostream & operator<<(std::ostream & os, const Inst::Status & status)
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
        // Used in extended_status_state as default case
        case Inst::Status::UNMOD:
            os << "UNMOD";
            break;
        // The new opcode/instruction as a result of fusion
        case Inst::Status::FUSED:
            os << "FUSED";
            break;
        // The opcodes/instruction no longer present due to fusion
        case Inst::Status::FUSION_GHOST:
            os << "FUSION_GHOST";
            break;
        case Inst::Status::__LAST:
            throw sparta::SpartaException("__LAST cannot be a valid enum state.");
        }
        return os;
    }

    // Expect log info system uses simple diff
    //   - any changes here will break EXPECT
    inline std::ostream & operator<<(std::ostream & os, const Inst & inst)
    {
        os << "uid:" << inst.getUniqueID() << " " << std::setw(10) << inst.getStatus() << " "
           << std::hex << inst.getPC() << std::dec << " pid:" << inst.getProgramID()
           << " uopid:" << inst.getUOpID() << " '" << inst.getDisasm() << "' ";
        return os;
    }

    inline std::ostream & operator<<(std::ostream & os, const InstPtr & inst)
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

    /*!
     * \class InstPairDef
     * \brief Pair Definition class of the instruction that flows through the Olympia
     *
     * This is the definition of the PairDefinition class of Inst.
     * It's mostly used for pipeline collection (-z option).  This
     * PairDefinition class could be named anything but it needs to
     * inherit publicly from sparta::PairDefinition templatized on the
     * actual class Inst.
     */
    class InstPairDef : public sparta::PairDefinition<Inst>
    {
      public:
        // The SPARTA_ADDPAIRs APIs must be called during the construction of the PairDefinition
        // class
        InstPairDef() : PairDefinition<Inst>() { SPARTA_INVOKE_PAIRS(Inst); }
        SPARTA_REGISTER_PAIRS(SPARTA_ADDPAIR("DID",
                                             &Inst::getUniqueID), // Used by Argos to color code
                              SPARTA_ADDPAIR("uid", &Inst::getUniqueID),
                              SPARTA_ADDPAIR("mnemonic", &Inst::getMnemonic),
                              SPARTA_ADDPAIR("complete", &Inst::getCompletedStatus),
                              SPARTA_ADDPAIR("pipe", &Inst::getPipe),
                              SPARTA_ADDPAIR("latency", &Inst::getExecuteTime),
                              SPARTA_ADDPAIR("raddr", &Inst::getRAdr, std::ios::hex),
                              SPARTA_ADDPAIR("tgt_vaddr", &Inst::getTargetVAddr, std::ios::hex))
    };

    /*!
     * \class PEventPairs
     * \brief Pair Definitions for pevents
     */
    class InstPEventPairs : public sparta::PairDefinition<Inst>
    {
      public:
        using TypeCollected = Inst;

        InstPEventPairs() : sparta::PairDefinition<Inst>()
        {
            addPEventsPair("uid", &Inst::getUniqueID);
            addPEventsPair("pc", &Inst::getPC, std::ios::hex);
        }
    };

    // Instruction allocators
    using InstAllocator = sparta::SpartaSharedPointerAllocator<Inst>;
    using InstArchInfoAllocator = sparta::SpartaSharedPointerAllocator<InstArchInfo>;
} // namespace olympia
