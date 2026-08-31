#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <limits>

namespace olympia::edm
{
    using Addr = uint64_t;
    using Opcode = uint32_t;
    using CoreId = uint32_t;
    using HartId = uint32_t;

    struct RegAccess
    {
        uint32_t reg_id = 0;
        std::string reg_name;
        std::vector<uint8_t> value;
        std::vector<uint8_t> prev_value;
    };

    struct MemAccess
    {
        Addr paddr = 0;
        Addr vaddr = 0;
        size_t size = 0;
        std::vector<uint8_t> value;
        std::vector<uint8_t> prev_value;
        bool is_write = false;
    };

    class InstructionInfo
    {
      public:
        InstructionInfo() = default;

        template <typename EventAccessorT>
        explicit InstructionInfo(EventAccessorT & evt)
        {
            iss_uid_ = evt.getEuid();

            const auto* event = evt.get();
            if (!event)
            {
                return;
            }

            pc_ = event->getPc();
            next_pc_ = event->getNextPc();
            dasm_ = event->getDisassemblyStr();
            opcode_ = event->getOpcode();
            is_branch_ = event->isChangeOfFlowEvent();

            if (is_branch_)
            {
                alt_next_pc_ = evt->getAltNextPc();
            }

            is_load_ = !evt->getMemoryReads().empty();
            is_store_ = !evt->getMemoryWrites().empty();
            ends_simulation_ = evt->isLastEvent();

            for (const auto & src : evt->getRegisterReads())
            {
                RegAccess r;
                r.reg_name = src.reg_id.reg_name;
                r.value = src.value;
                reg_reads_.push_back(std::move(r));
            }
            for (const auto & dst : evt->getRegisterWrites())
            {
                RegAccess r;
                r.reg_name = dst.reg_id.reg_name;
                r.value = dst.value;
                r.prev_value = dst.prev_value;
                reg_writes_.push_back(std::move(r));
            }
            for (const auto & mr : evt->getMemoryReads())
            {
                MemAccess m;
                m.paddr = mr.paddr;
                m.vaddr = mr.vaddr;
                m.size = mr.size;
                m.value = mr.value;
                m.is_write = false;
                mem_reads_.push_back(std::move(m));
            }
            for (const auto & mw : evt->getMemoryWrites())
            {
                MemAccess m;
                m.paddr = mw.paddr;
                m.vaddr = mw.vaddr;
                m.size = mw.size;
                m.value = mw.value;
                m.prev_value = mw.prev_value;
                m.is_write = true;
                mem_writes_.push_back(std::move(m));
            }
        }

        Addr getPC() const { return pc_; }
        Addr getNextPC() const { return next_pc_; }
        const std::optional<Addr> & getAltNextPC() const { return alt_next_pc_; }
        Opcode getOpcode() const { return opcode_; }
        uint32_t getOpcodeSize() const { return opcode_size_; }
        const std::string & getDasm() const { return dasm_; }
        bool isBranch() const { return is_branch_; }
        bool isTaken() const { return is_taken_; }
        bool isStore() const { return is_store_; }
        bool isLoad() const { return is_load_; }
        bool endsSimulation() const { return ends_simulation_; }
        bool isWrongPath() const { return is_wrong_path_; }
        bool isFaulted() const { return faulted_; }
        uint64_t getIssUid() const { return iss_uid_; }
        const std::vector<RegAccess> & getRegReads() const { return reg_reads_; }
        const std::vector<RegAccess> & getRegWrites() const { return reg_writes_; }
        const std::vector<MemAccess> & getMemReads() const { return mem_reads_; }
        const std::vector<MemAccess> & getMemWrites() const { return mem_writes_; }

        void setPC(Addr pc) { pc_ = pc; }
        void setNextPC(Addr next_pc) { next_pc_ = next_pc; }
        void setAltNextPC(Addr alt) { alt_next_pc_ = alt; }
        void setOpcode(Opcode op) { opcode_ = op; }
        void setOpcodeSize(uint32_t sz) { opcode_size_ = sz; }
        void setDasm(std::string dasm) { dasm_ = std::move(dasm); }
        void setBranch(bool b) { is_branch_ = b; }
        void setTaken(bool t) { is_taken_ = t; }
        void setStore(bool s) { is_store_ = s; }
        void setLoad(bool l) { is_load_ = l; }
        void setEndsSimulation(bool e) { ends_simulation_ = e; }
        void setWrongPath(bool w) { is_wrong_path_ = w; }
        void setFaulted(bool f) { faulted_ = f; }
        void setIssUid(uint64_t u) { iss_uid_ = u; }
        void addRegRead(RegAccess r) { reg_reads_.push_back(std::move(r)); }
        void addRegWrite(RegAccess r) { reg_writes_.push_back(std::move(r)); }
        void addMemRead(MemAccess m) { mem_reads_.push_back(std::move(m)); }
        void addMemWrite(MemAccess m) { mem_writes_.push_back(std::move(m)); }

      private:
        Addr pc_ = 0;
        Addr next_pc_ = 0;
        std::optional<Addr> alt_next_pc_;

        Opcode opcode_ = 0;
        uint32_t opcode_size_ = 4;
        std::string dasm_;

        bool is_branch_ = false;
        bool is_taken_ = false;
        bool is_store_ = false;
        bool is_load_ = false;
        bool ends_simulation_ = false;
        bool is_wrong_path_ = false;
        bool faulted_ = false;

        uint64_t iss_uid_ = std::numeric_limits<uint64_t>::max();

        std::vector<RegAccess> reg_reads_;
        std::vector<RegAccess> reg_writes_;
        std::vector<MemAccess> mem_reads_;
        std::vector<MemAccess> mem_writes_;
    };

    struct EDMCheckpoint
    {
        CoreId core_id = 0;
        HartId hart_id = 0;
        uint64_t olympia_inst_uid = 0;
        uint64_t iss_uid = std::numeric_limits<uint64_t>::max();
        Addr branch_pc = 0;
        Addr correct_path_pc = 0;
        Addr current_path_pc = 0;
        bool is_wrong_path_injection = false;
    };

    struct SteeringDecision
    {
        enum class Action
        {
            STEP_NORMAL,
            STEP_WITH_OVERRIDE
        };

        Action action = Action::STEP_NORMAL;
        std::optional<Addr> override_pc;
        bool is_wrong_path = false;
    };
} // namespace olympia::edm
