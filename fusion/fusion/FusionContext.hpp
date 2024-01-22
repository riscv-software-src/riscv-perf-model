// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file FusionContext.hpp  FusionGroup set context
#pragma once
#include "FusionExceptions.hpp"
#include "FusionTypes.hpp"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fusion
{
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
template <typename FusionGroupType>
class FusionContext
{
    //! \brief for now this is a uo/map with uint key
    using FusionGroupListType = std::vector<FusionGroupType>;
    //! \brief for now this is a uo/map with uint key
    using FusionGroupContainerType =
        std::unordered_map<fusion::HashType, FusionGroupType>;
    // Future : using LookupType = RadixTrie<4>;

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

  private:
    //! \brief context name
    //!
    //! When this is used in (future) multi-context implementations
    //! the name must be unique
    std::string name_{""};
    //! \brief this is an alias for map in this version
    FusionGroupContainerType container_;
    // Future:  LookupType lookup_;
};

}
