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

#include "mavis/DecoderTypes.h"

#include "Inst.hpp"
#include "InstAllocation.hpp"

// To reduce compile time and binary bloat, foward declare Mavis
template<typename InstType,
         typename AnnotationType,
         typename InstTypeAllocator,
         typename AnnotationTypeAllocator>
class Mavis;

namespace olympia_core
{
    using MavisType = Mavis<Inst,
                            InstArchInfo,
                            InstPtrAllocator<InstAllocator>,
                            InstPtrAllocator<InstArchInfoAllocator>>;

    // Handy UIDs that the modeler can assign to an instruction for
    // compare
    constexpr mavis::InstructionUniqueID MAVIS_UID_NOP        = 1;

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
            { "nop",             MAVIS_UID_NOP },
                };

        const std::string          pseudo_file_path_; ///< Path to olympia pseudo ISA/uArch JSON files
        std::unique_ptr<MavisType> mavis_facade_;     ///< Mavis facade object
    };

    MavisType *getMavis(sparta::TreeNode *);

} // namespace olympia
