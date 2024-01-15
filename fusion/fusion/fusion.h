// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh, Condor Computing Corp.
//
//! \file fusion.h  top level fusion API
#pragma once
#include "qparser.h"
#include "fusioncontext.h"
#include "fusionexceptions.h"
#include "fusiontypes.h"
#include "fusiongroup.h"
#include "machineinfo.h"
#include "fieldextractor.h"
#include <unordered_map>
#include <stdexcept>
#include <vector>
#include <memory>

namespace fusion
{
    //! \class Fusion
    //!
    //! \brief top level fusion class
    //!
    //! In this implementation the allocators are placeholder for
    //! more complex use cases. They are provided for future
    //! extensions.
    //!
    //! Input needed to create a fusion 'context' can come
    //! from explicity construction of FusionGroups, construction
    //! from the helper class FusionGroupCfg, and eventually
    //! from the DSL or from Json.
    //!
    //! Both the DSL and JSON are future features. With the DSL
    //! having a parser and a defined syntax. The JSON form has
    //! no definition at the moment, the JSON form could be a simple
    //! syntax variation of the DSL form. The linkage to the
    //! transform function object needs to be defined for JSON.
    //!
    //! There is a single context assumed although there are
    //! stubs for multiple context support. At the moment it
    //! is not clear if multiple context are actually a
    //! useful feature for fusion enabled instruction
    //! decoders in the existing performance models.
    //!
    //! For usage see the testbench.cpp in fusion/test. For the
    //! final PR there will more usage examples.
    template <typename FusionGroupType, typename MachineInfoType,
              typename FieldExtractorType,
              typename FusionGroupTypeAlloc =
                  fusion::ShrPtrAlloc<FusionGroupType>,
              typename MachineInfoTypeAlloc =
                  fusion::ShrPtrAlloc<MachineInfoType>,
              typename FieldExtractorTypeAlloc =
                  fusion::ShrPtrAlloc<FieldExtractorType>>
    struct Fusion
    {
        //! \brief ...
        using FusionGroupListType = std::vector<FusionGroupType>;
        //! \brief ...
        using FusionGroupCfgType =
            fusion::FusionGroupCfg<MachineInfoType, FieldExtractorType>;
        //! \brief ...
        using FusionGroupCfgListType = std::vector<FusionGroupCfgType>;
        //! \brief ...
        using TransformFuncType = bool (*)(FusionGroupType &,
                                           InstPtrListType &,
                                           InstPtrListType &);
        //! \brief ...
        using FusionContextType = fusion::FusionContext<FusionGroupType>;

        //! \brief ...
        using FusionFuncType = std::function<void(
            Fusion &, InstPtrListType &, InstPtrListType &)>;

        //! \brief main ctor
        Fusion(const FusionGroupListType & fusiongroup_list,
               const FusionGroupCfgListType & fusiongroupcfg_list,
               const FileNameListType & txt_file_list,
               const FusionGroupTypeAlloc fusiongroup_alloc =
                   fusion::ShrPtrAlloc<FusionGroupType>(),
               const MachineInfoTypeAlloc machine_info_alloc =
                   fusion::ShrPtrAlloc<MachineInfoType>(),
               const FieldExtractorType field_extractor_alloc =
                   fusion::ShrPtrAlloc<FieldExtractorType>()) :
            fusiongroup_alloc_(fusiongroup_alloc),
            machine_info_alloc_(machine_info_alloc),
            fusionOpr(defaultFusionOpr)
        {
            if (fusiongroup_list.size() > 0)
                initialize(fusiongroup_list);
            else if (fusiongroupcfg_list.size() > 0)
                initialize(fusiongroupcfg_list);
            else if (txt_file_list.size() > 0)
                initialize(txt_file_list);
            context_.makeContext("fbase", fusiongroup_list);
        }

        //! \brief ctor from group list
        Fusion(const FusionGroupListType & fusiongroup_list) :
            Fusion(fusiongroup_list, {}, {}, FusionGroupTypeAlloc(),
                   MachineInfoTypeAlloc(), FieldExtractorType())
        {
        }

        //! \brief ctor from cfg group list
        Fusion(const FusionGroupCfgListType & fusiongroupcfg_list) :
            Fusion({}, fusiongroupcfg_list, {}, FusionGroupTypeAlloc(),
                   MachineInfoTypeAlloc(), FieldExtractorType())
        {
        }

        //! \brief ctor from txt file list
        //!
        //! This is for future support of the DSL or JSON.
        //! The plan a constructor helper determines file type
        Fusion(const FileNameListType & txt_file_list) :
            Fusion({}, {}, txt_file_list, FusionGroupTypeAlloc(),
                   MachineInfoTypeAlloc(), FieldExtractorType())
        {
        }

        //! \brief bare ctor
        Fusion() :
            Fusion({}, {}, {}, FusionGroupTypeAlloc(),
                   MachineInfoTypeAlloc(), FieldExtractorType())
        {
        }

        //! \brief initialize state from a group list
        void initialize(const FusionGroupListType & fusiongroup_list)
        {
            for (auto grp : fusiongroup_list)
            {
                registerGroup(grp);
            }
        }

        //! \brief initialize from a text file list
        //!
        //! For simplicity assume all files are the same type
        //! as first file. I do not see a need at the moment
        //! for mixing types.
        void initialize(const FileNameListType & txt_file_list)
        {
            if (txt_file_list.size() == 0)
                return;
            bool useJson = isJsonFile(txt_file_list[0]);
            if (useJson)
            {
            }
            else
            {
                qp.setInputFiles(txt_file_list);
                int ret = qp.parse();
                if (ret)
                {
                    // throw fusion::DslSyntaxError(qp.errMsg, qp.lineNo);
                }
            }
        }

        //! \brief future file type detector
        bool isJsonFile(std::string fn) { return false; }

        //! \brief initialize from a group cfg list
        void initialize(const FusionGroupCfgListType & grp_list)
        {
            for (auto cfg : grp_list)
            {
                FusionGroupType f(cfg);
                registerGroup(f);
            }
        }

        //! \brief alias for context_.insertGroup()
        void registerGroup(FusionGroupType & grp)
        {
            context_.insertGroup(grp);
        }

        //! \brief create a single context from a list of fusiongroups
        //!
        //! This is here to support generality but I have
        //! not seen an immediate need for dynamic switching
        //! between multiple fusion contexts in a simulation.
        //! Something to consider for the future.
        void makeContext(const std::string & name,
                         const FusionGroupListType & fusiongroup_list)
        {
            context_.makeContext(name, fusiongroup_list);
        }

        //! \brief interface to the fusion operation
        //!
        //! This is the principle interface to the fusion operation.
        //! The operator can modify both input and output as needed.
        //! The default operator appends in to out and clears in.
        //!
        //! fusionOpr can be assigned with a user function.
        void fusionOperator(InstPtrListType & in, InstPtrListType & out)
        {
            fusionOpr(*this, in, out);
        }

        //! \brief assign the functor handle with a custom operator
        void setFusionOpr(FusionFuncType customOpr)
        {
            fusionOpr = customOpr;
        }

        // Future
        //   typename FusionGroupType::PtrType makeFusionGroup(const
        //   FusionGroupCfgType &cfg)
        //   {
        //     FusionGroupType grp(cfg);
        //     return context_.rtrie()->insert(grp);
        //   }
        //
        //   typename FusionGroupType::PtrType makeFusionGroup(const
        //   std::string name,
        //                                                     const
        //                                                     InstUidListType
        //                                                     &uidlist,
        //                                                     TransformFuncType
        //                                                     func)
        //   {
        //     FusionGroupType grp(name,uidlist,func);
        //     return context_.rtrie()->insert(grp);
        //   }

        //! \brief DSL parser
        //!
        //! At present this performs syntax checking of the input
        //! files. The remaining operations have not been implemented
        QParser qp;

        // JParser lohmann;

        //! \brief default fusion operator appends in to out and clears
        //! out.
        static void defaultFusionOpr(Fusion & inst, InstPtrListType & in,
                                     InstPtrListType & out)
        {
            out.insert(out.end(), in.begin(), in.end());
            in.clear();
        }

        //! \brief future feature
        FusionGroupTypeAlloc fusiongroup_alloc_;
        //! \brief future feature
        MachineInfoTypeAlloc machine_info_alloc_;
        //! \brief the current fusion state
        //!
        //! There is a single context in this version of the
        //! code. This could expand to support multiple
        //! simultaneous contexts if there is a use case.
        FusionContextType context_;
        //! \brief the fusion operation handle
        FusionFuncType fusionOpr;
    };

} // namespace fusion
