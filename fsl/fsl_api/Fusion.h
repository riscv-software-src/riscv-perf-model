// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh, Condor Computing Corp.
//
//! \file Fusion.h  top level fusion API
#pragma once
#include "json.hpp"
#include "fsl_api/FieldExtractor.h"
#include "fsl_api/FusionContext.h"
#include "fsl_api/FusionExceptions.h"
#include "fsl_api/FusionGroup.h"
#include "fsl_api/FusionTypes.h"
#include "fsl_api/MachineInfo.h"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace fusion
{
    //! \class Fusion
    //!
    //! \brief top level fusion class
    //!
    //! In this implementation the allocators are placeholders
    //! for more complex use cases. They are provided for future
    //! extensions.
    //!
    //! Input needed to create a fusion 'context' can come
    //! from explicitly construction of FusionGroups, construction
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
    template <typename FusionGroupType,
              typename MachineInfoType,
              typename FieldExtractorType,
              typename FusionGroupTypeAlloc =
                  fusion::ShrPtrAlloc<FusionGroupType>,
              typename MachineInfoTypeAlloc =
                  fusion::ShrPtrAlloc<MachineInfoType>,
              typename FieldExtractorTypeAlloc =
                  fusion::ShrPtrAlloc<FieldExtractorType>>
    struct Fusion
    {
        //! \brief list of fusion groups
        using FusionGroupListType = std::vector<FusionGroupType>;

        //! \brief the fusion context container type
        //!
        //! \see FusionContext.h
        using FusionGroupContainerType =
            std::unordered_map<fusion::HashType, FusionGroupType>;

        //! \brief group cfg is a ctor helper class
        using FusionGroupCfgType =
            fusion::FusionGroupCfg<MachineInfoType, FieldExtractorType>;

        //! \brief list of group cfg instances
        using FusionGroupCfgListType = std::vector<FusionGroupCfgType>;

        //! \brief transformation function object type
        //!
        //! support for future feature
        using TransformFuncType = bool (*)(FusionGroupType &,
                                           InstPtrListType &,
                                           InstPtrListType &);
        //! \brief context type is based on fusion groups and instr ptrs
        using FusionContextType
                = fusion::FusionContext<FusionGroupType,InstPtrType>;

        //! \brief custom fusion operator signature type
        using FusionFuncType = std::function<void(
            Fusion &, InstPtrListType &, InstPtrListType &)>;
      
        //! \brief list of fusion group match attributes
        //!
        //! Multiple matches may occur, not all will meet their 
        //! constraints. This list is sorted longest match to shorter.
        //! Longest match that meets constraints is selected
        using MatchInfoListType = std::vector<fusion::FusionGroupMatchInfo> ;

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
        //! Supports FSL or JSON, type is inferred from file extension
        //! All files in the list must be one or the other.
        Fusion(const FileNameListType &txt_file_list) :
            Fusion({}, {}, txt_file_list, FusionGroupTypeAlloc(),
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
        //! \brief initialize from a group cfg list
        void initialize(const FusionGroupCfgListType & grp_list)
        {
            for (auto cfg : grp_list)
            {
                FusionGroupType f(cfg);
                registerGroup(f);
            }
        }
        //! \brief initialize from a text file list
        //!
        //! For simplicity assume all files are the same type
        //! as first file. I do not see a need at the moment
        //! for mixing types.
        //!
        //! Future: JSON/DSL syntax is being created and reviewed.
        void initialize(const FileNameListType &txt_file_list)
        {
            if (txt_file_list.size() == 0) return;

            //Uses file extension, relies parsing to capture
            //cases the contents are not as indicated by the extension
            //FIXME: in this draft version of the code only handling
            //the first JSON file.
            bool useJson = hasExtension(txt_file_list[0],".json");
            if (useJson)
            {
                FusionGroupCfgListType cfgGroups;
                std::string fn = txt_file_list[0];
                parseJsonFusionGroups(txt_file_list[0],cfgGroups);
            } 
            else
            {
                //Future feature
                //fp.setInputFiles(txt_file_list);
                //int ret = fp.parse();
                //if (ret)
                //{
                //    // Future feature - needs additional handling
                //    throw fusion::FslSyntaxError(fp.errMsg, fp.lineNo);
                //}
                throw fusion::FslSyntaxError("FSL parsing is not supported",0);
            }
        }
        //! \brief populate a list of FusionGroups from a JSON file
        void parseJsonFusionGroups(const std::string fn,
                                   FusionGroupCfgListType &cfgGroups)
        {
            std::ifstream in(fn);
            if(!in.is_open())
            {
                throw JsonRuntimeError(
                      std::string("Could not open file '" + fn +"'"));
            }

            nlohmann::json jparser;

            try
            {
                in >> jparser;
            }
            catch(const std::exception&) 
            {
                throw JsonSyntaxError();
            }

            for(const auto& item : jparser["fusiongroups"])
            {
                checkForJsonField(item,"name");
                checkForJsonField(item,"uids");
                checkForJsonField(item,"tx");

                string grpName = item["name"];
                string txName  = item["tx"];

                FusionGroupCfgType fg {
                     .name = grpName,
                     .uids = std::nullopt,
                     .transformName = txName
                };

                InstUidListType uids;
                for(const auto& uid_str : item["uids"])
                {
                    int uid = std::stoi(uid_str.get<std::string>(),
                                        nullptr,16);
                    uids.push_back((uint32_t) uid);
                }
                fg.uids = uids;
                cfgGroups.push_back(std::move(fg));
            }
            initialize(cfgGroups);
        }

        //! \brief throw if field is missing
        void checkForJsonField(const nlohmann::json& item,
                               const std::string& fieldName) const
        {
            if (!item.contains(fieldName))
            {
                throw JsonRuntimeError("missing field "+fieldName);
            }
        }

        //! \brief check file extension 
        bool hasExtension(const std::string filePath,
                          const std::string ext) const
        {
            std::filesystem::path fPath(filePath);
            return fPath.extension()  == ext;
        }

        //! \brief alias for context_.insertGroup()
        void registerGroup(const FusionGroupType & grp)
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

        //! \brief return a ref to current context
        //!
        //! There is support for multiple named contexts. There
        //! is need for only one context in the current implementation.
        //!
        //! \see FusionContext.h
        FusionContextType & getCurrentContext() const
        {
            return context_;
        }

        //! \brief return a ref to the group container in the current context
        //!
        //! Consider friendship to skip a level of indirection, but
        //! at the moment there is no indication this matters for speed.
        //! And this is a cleaner form of encapsulation.
        //!
        //! \see FusionContext.h
        FusionGroupContainerType& getFusionGroupContainer()
        {
            return context_.getFusionGroupContainer();
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

        //! \brief report fusion groups
        //!
        //! Dump the group info to a file
        void reportGroups(const std::string reportFileName) const
        {
          std::ofstream out(reportFileName.c_str());
          if(!out.is_open()) throw fusion::FileIoError("open",reportFileName);
          reportGroups(out);
          out.close();
        }

        void reportGroups(std::ostream & os) const
        {
            os<<"Fusion groups: "<<std::endl;
            for(auto & g : context_.getFusionGroupContainer())
            {
                os << g << std::endl;
            }
        }

        //! \brief assign the functor handle with a custom operator
        void setFusionOpr(const FusionFuncType customOpr)
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
        //   typename FusionGroupType::PtrType makeFusionGroup(
        //       const std::string name,
        //       const InstUidListType &uidlist,
        //             TransformFuncType func)
        //   {
        //     FusionGroupType grp(name,uidlist,func);
        //     return context_.rtrie()->insert(grp);
        //   }

        //! \brief DSL parser
        //!
        //! At present this performs syntax checking of the input
        //! files. The remaining operations have not been implemented
        //! Future: DSL syntax is being redesigned
        //FslParser fslParser;

        //! \brief default fusion operator appends in to out and clears
        //! out.
        static void defaultFusionOpr(Fusion & inst, InstPtrListType & in,
                                     InstPtrListType & out)
        {
            out.insert(out.end(), in.begin(), in.end());
            in.clear();
        }

        //! \brief emit group container groups 
        void infoGroups(std::ostream &os,
                        const FusionGroupContainerType &fgroups) const
        {
            os<<"INFO fusionGroups "<<std::endl;
            for(auto & fg : fgroups) os<<fg<<std::endl;
        }

        //! \brief emit uid list
        void infoUids(std::ostream &os,
                      const InstUidListType &inputUids,
                      const std::string spacer) const
        {
            os<<"INFO in-uids"<<spacer;
            for(auto & uid : inputUids)
            {
                os<<" 0x"<<hex<<setw(2)<<setfill('0')<<uid;
            }
        }

        //! \brief emit fusion group match info
        void info(std::ostream &os,
                  const MatchInfoListType &matches,
                  const InstUidListType &inputUids)
        {
            os<<"INFO matches "<<matches.size()<<std::endl;
            infoUids(os,inputUids); os << std::endl;
            for(auto & mtch : matches) {
              os<<"INFO match: ";
              os <<mtch;
              for(auto & mid : mtch.matchedUids) {
                os<<" 0x"<<hex<<setw(2)<<setfill('0')<<mid;
              }
              os<<std::endl;
            }
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
