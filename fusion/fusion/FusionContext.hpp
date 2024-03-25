// HEADER PLACEHOLDER
// contact Jeff Nye, jeffnye-gh
//
//! \file FusionContext.hpp  FusionGroup set context
#pragma once
#include "Instruction.hpp"
#include "FusionExceptions.hpp"
#include "FusionTypes.hpp"

#include <string>
#include <unordered_map>
#include <utility>
#include <iostream>
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
    //! \brief ...
    using HashPair = pair<size_t,fusion::HashType>;
    //! \brief ...
    using HashPairListType = std::vector<HashPair>;
    //! \brief TODO: ordered seems best, verify against unordered
    using HashCacheType = std::map<size_t,HashPairListType>;

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

    // -------------------------------------------------------------
    // Functions supporting the hash-cache lookup mechanism
    // -------------------------------------------------------------
    void infoHCache(std::ostream &os,HashCacheType &hashCache)
    {
        os<<"INFO hashCache";
        for(auto & hc : hashCache)
        {
            os<<" "<<hc.first;
    
            for(auto & snd : hc.second) //hashPairListType
            {
                os<<" "<<dec<<snd.first<<":"
                       <<"0x"<<hex<<setw(8)<<setfill('0')<<snd.second;
            }
            os<<endl;
        }
    }

    //! \brief ...
    void clearHCache() { hcache.clear(); }

    //! \brief test for existance of index: grpSize
    bool groupSizeLookup(const size_t grpSize)
    {
        return hcache.find(grpSize) == hcache.end();
    }

    //! \brief test for existance or create new entry for index grpSize
    void groupSizeLookup(const InstUidListType &inputUids,
                         const size_t grpSize)
    {
        if(!groupSizeLookup(grpSize)) 
        {
            createHCacheEntry(inputUids,grpSize);
        }
    }

    //! \brief return the specified hcache entry
    HashPairListType getEntry(const size_t grpSize)
    {
        if(groupSizeLookup(grpSize))
        {
            return hcache[grpSize];
        } 
        else  //TODO: consider throwing HashCacheAccessError instead
        {
            return HashPairListType();
        } 
    }

    //! \brief add entry to hcache
    //!
    //! Creates a size-indexed entry for a hash of inputUid fragments of
    //! length grpSize. This entry is added to the hashCache.
    //!
    //! On entry into this function a walking hash is created for the fusion 
    //! group size. e.g. if grpSize is three, and the input is length 5, three
    //! hashes will be created.
    //!
    //!    a b c d e    input
    //!    F F F        hash 1
    //!      F F F      hash 2
    //!        F F F    hash 3
    //!
    //! These hashes are cached, indexed by length.
    //!
    //! A cache line is a list of pairs, a list of <index,hash>
    //! The cache is a map of cachelines, indexed by size,
    //!          <size, list of <index,hash>
    void createHCacheEntry(const InstUidListType &inputUids, size_t grpSize)
    {
        //create the sub-divisions of length grpSize
        vector<InstUidListType> fragments;
        subDivideUids(fragments,inputUids,grpSize);

        //this will be the 'cacheline' of hcache, calculate the
        //hash of each fragment. pair.first is the index into fragments
        HashPairListType cacheLine;

        size_t i=0;
        for(auto & uidVec : fragments) {
            fusion::HashType hash = FusionGroupType::jenkins_1aat(uidVec);
            cacheLine.push_back(make_pair(i++,hash));
        }

        hcache.insert(make_pair(grpSize,cacheLine));

        //DLOG("Created new cachedHash entry, size is now ",
        //       << hcache.size());
    }

    //! \brief ...
    void subDivideUids(std::vector<InstUidListType> &output,
                       const InstUidListType &inputUids,
                       const size_t length)
    {
        if(length == 0 || inputUids.size() == 0) return;

        output.clear();

        // Calculate how many vectors of reference's length can be generated
        size_t numVectors = inputUids.size() - length + 1;

        for (size_t i = 0; i < numVectors; ++i)
        {
            InstUidListType temp;

            // Extract elements from input starting at index i, 
            // with the same length as reference
            for (size_t j = 0; j < length; ++j)
            {
                temp.push_back(inputUids[i + j]);
            }
            output.push_back(temp);
        }
    }

  private:
    //! \brief context name
    //!
    //! When this is used in (future) multi-context implementations
    //! the name must be unique
    std::string name_{""};
    //! \brief this is an alias for map in this version
    FusionGroupContainerType container_;
    //! \brief ...
    HashCacheType hcache;
};

}
