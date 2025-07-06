// Header placeholder
// contact jeff at condor
//! \file Hcache.h
#pragma once
//TODO: do general performance tests for map, uo_map, array etc.
//TODO: template
#include "fsl_api/FusionTypes.h"

#include <map>
#include <ostream>
#include <vector>

namespace fusion
{

//! \class HCache
//!
//! \brief Length indexed fusion group hash lookup structure
//!
//! HCache provides performance benefit to the model's execution when
//! there are a large number of FusionGroups to compare UID sequences
//! against.
struct HCache
{
    //! \brief 'words' in the $ 'line' are pairs, length and resulting hash
    using HashPair = pair<size_t,fusion::HashType>;
    //! \brief value type for cache
    using HashPairListType = std::vector<HashPair>;
    //! \brief type used by the 'cache' array
    using HCacheType = std::map<size_t,HashPairListType>;
    //! \brief non-const iterator
    using HCacheItrType = HCacheType::iterator;
    //! \brief function object type
    using HashFuncType
           = fusion::HashType (*)(const std::vector<fusion::UidType>&);
    //! \brief ctor with hash function object argument
    HCache(HashFuncType func = nullptr)
        : hashFunc(func)
    {}

    // ------------------------------------------------------------------
    //! \brief create an entry in the cache
    //!
    // Create a size-indexed entry for a hash of inputUid fragments of
    // length grpSize. This entry is added to the hashCache.
    //
    // On entry into this function a walking hash is created for the 
    // fusion group size. e.g. if grpSize is three, and the input is 
    // length 5, three hashes will be created.
    //
    //    a b c d e    input
    //    F F F        hash 1
    //      F F F      hash 2
    //        F F F    hash 3
    //
    // These hashes are cached, indexed by length.
    //
    // A cache line is a list of pairs, a list of <index,hash>
    // The cache is a map of cachelines, indexed by size,
    //          <size, list of <index,hash>
    // ------------------------------------------------------------------
    void buildHashCacheEntry(const InstUidListType &inputUids,size_t grpSize)
    {
        //create the sub-divisions of length grpSize
        vector<InstUidListType> fragments;
        subDivideUids(fragments,inputUids,grpSize);

        //this will be the 'cacheline' of hcache, calculate the
        //hash of each fragment. pair.first is the index into fragments
        HashPairListType cacheLine;

        size_t i=0;
        for(auto & uidVec : fragments) {
            fusion::HashType hash = hashFunc(uidVec);
            cacheLine.push_back(make_pair(i++,hash));
        }

        hcache.insert(make_pair(grpSize,cacheLine));
    }

    // ------------------------------------------------------------------
    //! \brief prepare a vector of uids for the hash operation
    //!
    //! Uids groups based on the length of the input. The hash will
    //! be formed for each sub-division
    // ------------------------------------------------------------------
    void subDivideUids(std::vector<InstUidListType> &output,
                       const InstUidListType &inputUids,size_t length)
    {
        if(length == 0 || inputUids.size() == 0) return;

        output.clear();

        // Calculate how many vectors of reference's length can be generated
        size_t numVectors = inputUids.size() - length + 1;

        for (size_t i = 0; i < numVectors; ++i) {
            InstUidListType temp;

            // Extract elements from input starting at index i, 
            // with the same length as reference
            for (size_t j = 0; j < length; ++j) {
                temp.push_back(inputUids[i + j]);
            }

            output.push_back(temp);
        }
    }

    // ------------------------------------------------------------------
    //! \brief output cache entries to a stream
    // ------------------------------------------------------------------
    void infoHCache(std::ostream &os)
    {
        os<<"INFO hcache";
        for(auto & hc : hcache)
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

    //! \brief beginning non-const iterator getter method
    HCacheItrType begin() { return hcache.begin(); }

    //! \brief ending non-const iterator getter method
    HCacheItrType end()   { return hcache.end();   }

    //! \brief ...
    size_t size()  const  { return hcache.size();  }

    //! \brief ...
    void clear() { hcache.clear(); }

    //! \brief  ...
    HCacheItrType find(size_t s) { return hcache.find(s); }

    //! \brief ...
    HashPairListType& operator[](const size_t& key)
    {
        auto it = hcache.find(key);
        if(it == hcache.end()) 
        {
            throw std::out_of_range("Hash key not found in hcache");
        }

        return it->second;
    }

private:
    //! \brief function object for hash creation
    HashFuncType hashFunc;

    //! \brief future feature: support for template version of HCache
    std::map<size_t,HashPairListType> hcache;
};

}
