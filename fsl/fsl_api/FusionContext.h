// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file FusionContext.h  FusionGroup set context
#pragma once
#include "fsl_api/Instruction.h"
#include "fsl_api/FusionExceptions.h"
#include "fsl_api/FusionTypes.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fusion
{
// ----------------------------------------------------------------------
//! \class FusionGroupMatchInfo
//
//! \brief fusion group match return structure 
//!
//! Signed integer for startIdx is for no-match reporting without
//! increasing the ctor signature.
// ----------------------------------------------------------------------
struct FusionGroupMatchInfo {
    FusionGroupMatchInfo(std::string name_,
                         int32_t startIdx_,
                         uint32_t groupIdx_,
                         InstUidListType _matchedUids)
        : name(name_),
          startIdx(startIdx_),
          groupIdx(groupIdx_),
          matchedUids(_matchedUids)
    { }

    //this is only kept for stats reporting, fgroup utilization maps, etc
    std::string name  = "";
    int32_t  startIdx = -1;
    int32_t  groupIdx = -1;
    size_t   size() const { return matchedUids.size(); }

    InstUidListType matchedUids;

    friend std::ostream& operator<<(std::ostream& os,
                                    const FusionGroupMatchInfo& matchInfo)
    {
        os << std::dec << std::setfill(' ')
           <<  " name "     << matchInfo.name
           <<  " groupIdx " << std::setw(3) << matchInfo.groupIdx
           <<  " startIdx " << std::setw(3) << matchInfo.startIdx
           << " size "      << std::setw(3) << matchInfo.size();
        return os;
    }
};
// -----------------------------------------------------------------------
//! \class FusionContext
//!
//!
//! \brief Holds an searchable list of the current FusionGroups
//!
//!  In this implementation the groups are held and searched
//!  within a uo/map. There is a trie implementation but that
//!  is not used currently. Before adding the trie if will be
//!  useful to spend more time with large fusion group definitions
//!  and see how the map performs vs the trie (or alternatives).
//!
//!  Future abstration : typename container or typename tree:
//!     typename FusionGroupContainerType =
//!     std::unordered_map<fusion::HashType,FusionGroupType>,
//!        typename LookupType = fusion::ShrPtrAlloc<RadixTrie>
// -----------------------------------------------------------------------
template <typename FusionGroupType,
          typename InstPtrType>
class FusionContext
{
    //! \brief for now this is a uo/map with uint key
    using FusionGroupListType = std::vector<FusionGroupType>;
    //! \brief for now this is a uo/map with uint key
    using FusionGroupContainerType =
        std::unordered_map<fusion::HashType, FusionGroupType>;
    //! \brief return struct for matching groups during context search
    using MatchInfoListType = std::vector<FusionGroupMatchInfo>;
    //! \brief mavis assigned UID list type
    using InstUidListType = fusion::InstUidListType;

  public:
    FusionContext() = default;

    //! \brief basic ctor
    //!
    //! name_ member is assigned in makeContext
    FusionContext(const std::string & name,
                  const FusionGroupListType & groups)
    {
        makeContext(name_, groups);
    }

    //! \brief insert each group into the (only/current) context
    void makeContext(const std::string & n,
                     const FusionGroupListType & groups)
    {
        name_ = n;
        for (const auto & grp : groups)
        {
            insertGroup(grp);
        }
    }

    //! \brief insert each group w/ catch
    void insertGroup(const FusionGroupType & group)
    {
        fusion::HashType hash = group.hash();
        if (hash == 0)
        {
            throw fusion::HashIllegalValueError(group.name(),
                                                group.hash());
        }

        if (container_.find(hash) != container_.end())
        {
            throw fusion::HashDuplicateError(group.name(), group.hash());
        }

        container_.insert(std::make_pair(hash, group));
    }

    //! \brief public function to get group list from container
    FusionGroupContainerType& getFusionGroupContainer()
    {
      return container_;
    }

  private:
    //! \brief context name
    //!
    //! When this is used in (future) multi-context implementations
    //! the name must be unique
    std::string name_{""};
    //! \brief this is an alias for map in this version
    FusionGroupContainerType container_;
};

}
