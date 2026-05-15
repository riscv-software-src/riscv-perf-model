// <MavisUnit.hpp> -*- C++ -*-

//!
//! \file MavisUnit.hpp
//! \brief A functional unit of Mavis, placed in the Sparta Tree for any unit to grab/use
//!

#pragma once

#include <string>
#include <vector>

#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/ResourceFactory.hpp"
#include "sparta/simulation/ResourceFactory.hpp"

#include "mavis/DecoderTypes.h"
#include "mavis/extension_managers/RISCVExtensionManager.hpp"

#include "inst/Inst.hpp"
#include "inst/InstAllocation.hpp"

// To reduce compile time and binary bloat, foward declare Mavis
template<typename InstType,
         typename AnnotationType,
         typename InstTypeAllocator,
         typename AnnotationTypeAllocator>
class Mavis;

namespace olympia
{
    using MavisType = Mavis<Inst,
                            InstArchInfo,
                            InstPtrAllocator<InstAllocator>,
                            InstPtrAllocator<InstArchInfoAllocator>>;

    // Handy UIDs that the modeler can assign to an instruction for
    // compare
    enum MavisUIDs : mavis::InstructionUniqueID
    {
        MAVIS_UID_NOP = 1,
        MAVIS_UID_VSETIVLI,
        MAVIS_UID_VSETVLI,
        MAVIS_UID_VSETVL,
        MAVIS_UID_VMV1R,
        MAVIS_UID_VMV2R,
        MAVIS_UID_VMV4R,
        MAVIS_UID_VMV8R,
        MAVIS_UID_VZEXTVF2,
        MAVIS_UID_VSEXTVF2,
        MAVIS_UID_VZEXTVF4,
        MAVIS_UID_VSEXTVF4,
        MAVIS_UID_VZEXTVF8,
        MAVIS_UID_VSEXTVF8
    };

    // This is a sparta tree node wrapper around the Mavis facade object
    // Used to provide global access to the facade
    class MavisUnit : public sparta::Unit {
    public:
        //! Mavis parameters
        class MavisParameters : public sparta::ParameterSet {
        public:
            explicit MavisParameters(sparta::TreeNode *n) :
                sparta::ParameterSet(n)
            {}

            PARAMETER(std::string,   isa_file_path,    "mavis_isa_files", "Where are the mavis isa files?")
            PARAMETER(std::string,   uarch_file_path,  "arches/isa_json", "Where are the mavis uarch files?")
            PARAMETER(std::string,   pseudo_file_path,  "", "Where are the mavis pseudo isa/usarch files? (default: uarch_file_path)")
            PARAMETER(std::string,   isa_string,
                      "rv64gbcv",
                      "The supported ISA string by Olympia.  This is directly tied to the JSON files in arches/isa_json")
            PARAMETER(std::string,   uarch_overrides_json, "", "JSON uArch overrides")
            PARAMETER(std::vector<std::string>, uarch_overrides, {}, R"(uArch overrides.
    Format : <mnemonic>, <attribute> : <value>
    Example: -p .....params.uarch_overrides "[ "add, latency : 100", "lw, dispatch : ["iex","lsu"] ]"
")")
        };

        static constexpr char name[] = "mavis";

        //! Mavis's factory class
        class Factory : public sparta::ResourceFactory<MavisUnit, MavisParameters>
        {
        public:
            //void onConfiguring(sparta::ResourceTreeNode* node) override;
            Factory() = default;
        };

        // Constructor
        MavisUnit(sparta::TreeNode *, const MavisParameters*);

        // Destructor
        ~MavisUnit();

        // Access the mavis facade
        MavisType* getFacade() {
            return mavis_facade_.get();
        }

    private:

        //! Mavis Instruction ID's that we want to use in Olympia
        static inline mavis::InstUIDList mavis_uid_list_ {
            { "nop",      MAVIS_UID_NOP},
            { "vsetivli", MAVIS_UID_VSETIVLI},
            { "vsetvli",  MAVIS_UID_VSETVLI},
            { "vsetvl",   MAVIS_UID_VSETVL},
            {"vmv1r.v",   MAVIS_UID_VMV1R},
            {"vmv2r.v",   MAVIS_UID_VMV2R},
            {"vmv4r.v",   MAVIS_UID_VMV4R},
            {"vmv8r.v",   MAVIS_UID_VMV8R},
            {"vzext.vf2", MAVIS_UID_VZEXTVF2},
            {"vsext.vf2", MAVIS_UID_VSEXTVF2},
            {"vzext.vf4", MAVIS_UID_VZEXTVF4},
            {"vsext.vf4", MAVIS_UID_VSEXTVF4},
            {"vzext.vf8", MAVIS_UID_VZEXTVF8},
            {"vsext.vf8", MAVIS_UID_VSEXTVF8}
	};

        const std::string          pseudo_file_path_; ///< Path to olympia pseudo ISA/uArch JSON files
        const mavis::extension_manager::riscv::RISCVExtensionManager ext_man_;
        std::unique_ptr<MavisType> mavis_facade_;     ///< Mavis facade object
    };

    using MavisFactory = sparta::ResourceFactory<MavisUnit,
                                                MavisUnit::MavisParameters>;

    MavisType *getMavis(sparta::TreeNode *);

} // namespace olympia
