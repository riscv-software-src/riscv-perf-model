
#include "InstGenerator.hpp"
#include "json.hpp"  // From Mavis
#include "mavis/Mavis.h"

namespace olympia
{
    std::unique_ptr<InstGenerator> InstGenerator::createGenerator(MavisType * mavis_facade,
                                                                  const std::string & filename,
                                                                  const bool skip_nonuser_mode)
    {
        const std::string json_ext = "json";
        if((filename.size() > json_ext.size()) && filename.substr(filename.size()-json_ext.size()) == json_ext) {
            std::cout << "olympia: JSON file input detected" << std::endl;
            return std::unique_ptr<InstGenerator>(new JSONInstGenerator(mavis_facade, filename));
        }

        const std::string stf_ext = "stf";  // Should cover both zstf and stf
        if((filename.size() > stf_ext.size()) && filename.substr(filename.size()-stf_ext.size()) == stf_ext) {
            std::cout << "olympia: STF file input detected" << std::endl;
            return std::unique_ptr<InstGenerator>(new TraceInstGenerator(mavis_facade, filename, skip_nonuser_mode));
        }

        // Dunno what it is...
        sparta_assert(false, "Unknown file extension for '" << filename
                      << "'.  Expected .json or .[z]stf");
        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // JSON Inst Generator
    JSONInstGenerator::JSONInstGenerator(MavisType * mavis_facade,
                                         const std::string & filename) :
        InstGenerator(mavis_facade)
    {
        std::ifstream fs;
        std::ios_base::iostate exceptionMask = fs.exceptions() | std::ios::failbit;
        fs.exceptions(exceptionMask);

        try {
            fs.open(filename);
        } catch (const std::ifstream::failure &e) {
            throw sparta::SpartaException("ERROR: Issues opening ") << filename << ": " << e.what();
        }

        jobj_.reset(new nlohmann::json);
        fs >> *jobj_;
        n_insts_ = jobj_->size();
    }

    bool JSONInstGenerator::isDone() const {
        return (curr_inst_index_ >= n_insts_);
    }

    void JSONInstGenerator::reset(const InstPtr & inst_ptr, const bool skip = false)
    {
        curr_inst_index_ = inst_ptr->getRewindIterator<uint64_t>();
        program_id_ = inst_ptr->getProgramID();
        if (skip)
        {
            ++curr_inst_index_;
            ++program_id_;
        }
    }

    InstPtr JSONInstGenerator::getNextInst(const sparta::Clock * clk)
    {
        if(SPARTA_EXPECT_FALSE(isDone())) {
            return nullptr;
        }

        // Get the JSON record at the current index
        nlohmann::json jinst = jobj_->at(curr_inst_index_);

        if (jinst.find("mnemonic") == jinst.end()) {
            throw sparta::SpartaException() << "Missing mnemonic at " << curr_inst_index_;
        }
        const std::string mnemonic = jinst["mnemonic"];

        auto addElement =  [&jinst] (mavis::OperandInfo & operands,
                                     const std::string & key,
                                     const mavis::InstMetaData::OperandFieldID operand_field_id,
                                     const mavis::InstMetaData::OperandTypes operand_type) {
                               if(jinst.find(key) != jinst.end()) {
                                   operands.addElement(operand_field_id,
                                                       operand_type,
                                                       jinst[key].get<uint64_t>());
                               }
                           };

        mavis::OperandInfo srcs;
        addElement(srcs, "rs1", mavis::InstMetaData::OperandFieldID::RS1, mavis::InstMetaData::OperandTypes::LONG);
        addElement(srcs, "fs1", mavis::InstMetaData::OperandFieldID::RS1, mavis::InstMetaData::OperandTypes::DOUBLE);
        addElement(srcs, "rs2", mavis::InstMetaData::OperandFieldID::RS2, mavis::InstMetaData::OperandTypes::LONG);
        addElement(srcs, "fs2", mavis::InstMetaData::OperandFieldID::RS2, mavis::InstMetaData::OperandTypes::DOUBLE);

        mavis::OperandInfo dests;
        addElement(dests, "rd", mavis::InstMetaData::OperandFieldID::RD, mavis::InstMetaData::OperandTypes::LONG);
        addElement(dests, "fd", mavis::InstMetaData::OperandFieldID::RD, mavis::InstMetaData::OperandTypes::DOUBLE);

        InstPtr inst;
        if(jinst.find("imm") != jinst.end()) {
            const uint64_t imm = jinst["imm"].get<uint64_t>();
            mavis::ExtractorDirectOpInfoList ex_info(mnemonic, srcs, dests, imm);
            inst = mavis_facade_->makeInstDirectly(ex_info, clk);
        }
        else {
            mavis::ExtractorDirectOpInfoList ex_info(mnemonic, srcs, dests);
            inst = mavis_facade_->makeInstDirectly(ex_info, clk);
        }

        if (jinst.find("vaddr") != jinst.end()) {
            uint64_t vaddr = std::strtoull(jinst["vaddr"].get<std::string>().c_str(), nullptr, 0);
            inst->setTargetVAddr(vaddr);
        }

        if (jinst.find("taken") != jinst.end()) {
            const bool taken = jinst["taken"].get<bool>();
            inst->setTakenBranch(taken);
        }

        inst->setRewindIterator<uint64_t>(curr_inst_index_);
        inst->setUniqueID(++unique_id_);
        inst->setProgramID(program_id_++);
        ++curr_inst_index_;
        return inst;

    }

    ////////////////////////////////////////////////////////////////////////////////
    // STF Inst Generator
    TraceInstGenerator::TraceInstGenerator(MavisType * mavis_facade,
                                           const std::string & filename,
                                           const bool skip_nonuser_mode) :
        InstGenerator(mavis_facade)
    {
        std::ifstream fs;
        std::ios_base::iostate exceptionMask = fs.exceptions() | std::ios::failbit;
        fs.exceptions(exceptionMask);

        try {
            fs.open(filename);
        } catch (const std::ifstream::failure &e) {
            throw sparta::SpartaException("ERROR: Issues opening ") << filename << ": " << e.what();
        }

        // If true, search for an stf-pte file alongside this trace.
        constexpr bool CHECK_FOR_STF_PTE = false;

        // Filter out mode change events regardless of skip_nonuser_mode
        // value. Required for traces that stay in machine mode the entire
        // time
        constexpr bool FILTER_MODE_CHANGE_EVENTS = true;
        constexpr size_t BUFFER_SIZE             = 4096;
        reader_.reset(new stf::STFInstReader(filename,
                                             skip_nonuser_mode,
                                             CHECK_FOR_STF_PTE,
                                             FILTER_MODE_CHANGE_EVENTS,
                                             BUFFER_SIZE));

        next_it_ = reader_->begin();
    }

    bool TraceInstGenerator::isDone() const {
        return next_it_ == reader_->end();
    }

    void TraceInstGenerator::reset(const InstPtr & inst_ptr, const bool skip = false)
    {
        next_it_ = inst_ptr->getRewindIterator<stf::STFInstReader::iterator>();
        program_id_ = inst_ptr->getProgramID();
        if (skip)
        {
            ++next_it_;
            ++program_id_;
        }
    }

    InstPtr TraceInstGenerator::getNextInst(const sparta::Clock * clk)
    {
        if(SPARTA_EXPECT_FALSE(isDone())) {
            return nullptr;
        }

        mavis::Opcode opcode = next_it_->opcode();

        try {
            InstPtr inst = mavis_facade_->makeInst(opcode, clk);
            inst->setPC(next_it_->pc());
            inst->setUniqueID(++unique_id_);
            inst->setProgramID(program_id_++);
            inst->setRewindIterator<stf::STFInstReader::iterator>(next_it_);
            if (const auto& mem_accesses = next_it_->getMemoryAccesses(); !mem_accesses.empty())
            {
                using VectorAddrType = std::vector<sparta::memory::addr_t>;
                VectorAddrType addrs;
                std::for_each(next_it_->getMemoryAccesses().begin(),
                              next_it_->getMemoryAccesses().end(),
                              [&addrs] (const auto & ma) {
                                  addrs.emplace_back(ma.getAddress());
                              });
                inst->setTargetVAddr(addrs.front());
                //For misaligns, more than 1 address is provided
                //inst->setVAddrVector(std::move(addrs));
            }
            if (next_it_->isBranch())
            {
                inst->setTakenBranch(next_it_->isTakenBranch());
                inst->setTargetVAddr(next_it_->branchTarget());
            }
            ++next_it_;
            return inst;
        }
        catch(std::exception & excpt) {
            std::cerr << "ERROR: Mavis failed decoding: 0x"
                      << std::hex << opcode << " for STF It PC: 0x"
                      << next_it_->pc() << " STFID: " << std::dec
                      << next_it_->index() << " err: "
                      << excpt.what() << std::endl;
            throw;
        }
        return nullptr;
    }

}
