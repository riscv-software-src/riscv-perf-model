
#include "InstGenerator.hpp"
#include "json.hpp"  // From Mavis
#include "mavis/Mavis.h"

namespace olympia_core
{
    std::unique_ptr<InstGenerator> InstGenerator::createGenerator(MavisType * mavis_facade,
                                                                  const std::string & filename)
    {
        const std::string json_ext = "json";
        if((filename.size() > json_ext.size()) && filename.substr(filename.size()-json_ext.size()) == json_ext) {
            std::cout << "-- JSON file input detected" << std::endl;
            return std::unique_ptr<InstGenerator>(new JSONInstGenerator(mavis_facade, filename));
        }

        const std::string stf_ext = "stf";  // Should cover both zstf and stf
        if((filename.size() > stf_ext.size()) && filename.substr(filename.size()-stf_ext.size()) == stf_ext) {
            std::cout << "-- STF file input detected" << std::endl;
            return std::unique_ptr<InstGenerator>(new TraceInstGenerator(mavis_facade, filename));
        }

        // Dunno what it is...
        sparta_assert(false, "Unknown file extension for '" << filename
                      << "'.  Expected .json or .[z]stf");
        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // JSON Inst Generator
    JSONInstGenerator::JSONInstGenerator(MavisType * mavis_facade,
                                         const std::string & filename)
    {
        mavis_facade_ = mavis_facade;

        std::ifstream fs;
        std::ios_base::iostate exceptionMask = fs.exceptions() | std::ios::failbit;
        fs.exceptions(exceptionMask);

        try {
            fs.open(filename);
        } catch (const std::ifstream::failure &) {
            std::cerr << "ERROR: Issues opening " << filename << std::endl;
            throw;
        }

        jobj_.reset(new nlohmann::json);
        fs >> *jobj_;
        n_insts_ = jobj_->size();
    }

    Inst::InstPtr JSONInstGenerator::getNextInst(const sparta::Clock * clk)
    {
        if(curr_inst_index_ == n_insts_) { return nullptr; }

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
        addElement(srcs, "vs1", mavis::InstMetaData::OperandFieldID::RS1, mavis::InstMetaData::OperandTypes::VECTOR);
        addElement(srcs, "rs2", mavis::InstMetaData::OperandFieldID::RS2, mavis::InstMetaData::OperandTypes::LONG);
        addElement(srcs, "fs2", mavis::InstMetaData::OperandFieldID::RS2, mavis::InstMetaData::OperandTypes::DOUBLE);
        addElement(srcs, "vs2", mavis::InstMetaData::OperandFieldID::RS2, mavis::InstMetaData::OperandTypes::VECTOR);

        mavis::OperandInfo dests;
        addElement(dests, "rd", mavis::InstMetaData::OperandFieldID::RD, mavis::InstMetaData::OperandTypes::LONG);
        addElement(dests, "fd", mavis::InstMetaData::OperandFieldID::RD, mavis::InstMetaData::OperandTypes::DOUBLE);
        addElement(dests, "vd", mavis::InstMetaData::OperandFieldID::RD, mavis::InstMetaData::OperandTypes::VECTOR);

        Inst::InstPtr inst;
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
            inst->setVAddr(vaddr);
        }

        ++curr_inst_index_;
        if (inst != nullptr) {
            inst->setUniqueID(++unique_id_);
        }
        return inst;

    }
}
