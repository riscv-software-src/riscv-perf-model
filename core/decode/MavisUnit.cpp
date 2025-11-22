// <MavisUnit.cpp> -*- C++ -*-

//!
//! \file MavisUnit.cpp
//! \brief A functiona unit of Mavis, placed in the Sparta Tree for any unit to grab/use
//!

#include "mavis/Mavis.h"
#include "MavisUnit.hpp"


#include "OlympiaAllocators.hpp"

namespace olympia
{

    std::vector<std::string> getUArchFiles(sparta::TreeNode *n, const MavisUnit::MavisParameters* p,
                                           const std::string & uarch_file_path, const std::string& pseudo_file_path)
    {
        std::vector<std::string> uarch_files = {uarch_file_path + "/olympia_uarch_rv64g.json",
                                                uarch_file_path + "/olympia_uarch_rv64c.json",
                                                uarch_file_path + "/olympia_uarch_rv64b.json",
                                                uarch_file_path + "/olympia_uarch_rv64v.json"};

        if(false == std::string(p->uarch_overrides_json).empty()) {
            uarch_files.emplace_back(p->uarch_overrides_json);
        }

        return uarch_files;
    }

    MavisType::AnnotationOverrides getUArchAnnotationOverrides(const MavisUnit::MavisParameters* p)
    {
        MavisType::AnnotationOverrides annotations;

        const std::vector<std::string> vals = p->uarch_overrides;
        for(auto overde : vals)
        {
            sparta_assert(overde.find(',') != std::string::npos,  "Malformed uarch override: " << overde);
            const std::string mnemonic  = overde.substr(0, overde.find(','));
            const std::string attribute_pair = overde.substr(overde.find(',')+1);
            sparta_assert(!mnemonic.empty() and !attribute_pair.empty(), "Malformed uarch override: " << overde);
            annotations.emplace_back(std::make_pair(mnemonic, attribute_pair));
        }

        return annotations;
    }

    /**
     * \brief Construct a new Mavis unit
     * \param n Tree node parent for this unit
     * \param p Unit parameters
     */
    MavisUnit::MavisUnit(sparta::TreeNode *n, const MavisParameters* p) :
        sparta::Unit(n),
        pseudo_file_path_(std::string(p->pseudo_file_path).empty() ? p->uarch_file_path : p->pseudo_file_path),
        ext_man_(mavis::extension_manager::riscv::RISCVExtensionManager::fromISA(p->isa_string,
                                                                                 std::string(p->isa_file_path) + "/riscv_isa_spec.json",
                                                                                 p->isa_file_path)),
        mavis_facade_ (std::make_unique<MavisType>(ext_man_.constructMavis<Inst, InstArchInfo, InstPtrAllocator<InstAllocator>, InstPtrAllocator<InstArchInfoAllocator>>
                                                       (getUArchFiles(n, p, p->uarch_file_path, pseudo_file_path_),
                                                        mavis_uid_list_,
                                                        getUArchAnnotationOverrides(p),
                                                        InstPtrAllocator<InstAllocator> (sparta::notNull(OlympiaAllocators::getOlympiaAllocators(n))->inst_allocator),
                                                        InstPtrAllocator<InstArchInfoAllocator> (sparta::notNull(OlympiaAllocators::getOlympiaAllocators(n))->inst_arch_info_allocator))))
    {}

    /**
     * \brief Destruct a mavis unit
     */
    MavisUnit::~MavisUnit() {}

    /**
     * \brief Sparta-visible global function to find a mavis node and provide the mavis facade
     * \param node Tree node to start the search (recurses up the tree from here until a mavis unit is found)
     * \return Pointer to Mavis facade object
     */
    MavisType* getMavis(sparta::TreeNode *node)
    {
        MavisUnit * mavis_unit = nullptr;
        if (node)
        {
            if (node->hasChild(MavisUnit::name)) {
                mavis_unit = node->getChild(MavisUnit::name)->getResourceAs<MavisUnit>();
            }
            else {
                return getMavis(node->getParent());
            }
        }
        sparta_assert(mavis_unit != nullptr, "Mavis unit was not found");
        // cppcheck-suppress nullPointer
        return mavis_unit->getFacade();
    }

} // namespace olympia
