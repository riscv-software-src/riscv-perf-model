// Support methods for the fusion decoder
// 2024.07.25 Jeff Nye

#include "fsl_api/FusionTypes.h"
#include "Decode.hpp"

#include "sparta/events/StartupEvent.hpp"
#include "sparta/utils/LogUtils.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <vector>

using namespace std;
using namespace fusion;

namespace olympia
{
    // -------------------------------------------------------------------
    // Remove the ghost fusion ops
    // Kept, but currently not called
    // -------------------------------------------------------------------
    void Decode::whereIsEgon_(InstGroupPtr & insts, uint32_t & numGhosts)
    {
        if (insts == nullptr)
        {
            // DLOG("whereIsEgon() called with insts null");
            return;
        }

        // DLOG("BEFORE # --------------------------------------");
        // infoInsts(cout,insts);

        for (auto it = insts->begin(); it != insts->end();)
        {
            if ((*it)->getExtendedStatus() == olympia::Inst::Status::FUSION_GHOST)
            {
                ++numGhosts;
                it = insts->erase(it);
            }
            else
            {
                ++it;
            }
        }
        // DLOG("AFTER  # --------------------------------------");
        // infoInsts(cout,insts);
        // DLOG("DONE   # --------------------------------------");
    }

    // -------------------------------------------------------------------
    // -------------------------------------------------------------------
    void Decode::processMatches_(MatchInfoListType & matches, InstGroupPtr & insts,
                                 const InstUidListType & inputUids)
    {
        if (matches.size() == 0)
            return;

        // Kept for upcoming additional work on reducing ROB entry requirements
        // DLOG("PROCESS MATCHES "<<matches.size());
        // DLOG("FUSION: REF   UIDS:");
        // fuser->infoUids(cout,inputUids,"before "); cout<<endl;
        // fuser->info(cout,matches,inputUids); cout<<endl;
        // size_t idx = 0;

        for (const auto & thisMatch : matches)
        {
            // Kept for upcoming additional work on reducing ROB entry requirements
            // DLOG("FUSION: M("<<idx++<<")  UIDS:");
            // string gap(thisMatch.startIdx*5,' ');
            // DLOG(gap);
            // for(const auto &mu:thisMatch.matchedUids) DLOG(" 0x"<<hex<<setw(2)<<mu);
            // DLOG("\n");

            InstGroup::iterator itr = insts->begin();
            std::advance(itr, thisMatch.startIdx);
            if ((*itr)->getExtendedStatus() == Inst::Status::UNMOD)
            {
                (*itr)->setExtendedStatus(Inst::Status::FUSED);
                ++fusion_num_fuse_instructions_;
                // Kept for upcoming additional work on reducing ROB
                // entry requirements. This manages the sequential expectations
                // of the ROB for program ID.
                //(*itr)->setProgramIDIncrement(thisMatch.size());

                for (size_t i = 1; i < thisMatch.size(); ++i)
                {
                    ++itr;
                    sparta_assert(itr != insts->end(),
                                  "processMatches inst iterator exceeded range");
                    (*itr)->setExtendedStatus(Inst::Status::FUSION_GHOST);
                    ++fusion_num_ghost_instructions_;
                    ++fusion_pred_cycles_saved_;
                }
                updateFusionGroupUtilization_(thisMatch.name);
            }
        }

        matches.clear();
    }

    // ----------------------------------------------------------------------
    // TODO: this performs well for the FusionGroup set sizes tested, ~256.
    // More testing is intended for 2048 (10x any practical size).
    // Further improvement is deferred until a decsion on uop/trace cache
    // implementations. Regardless, if warranted for model performance,
    // an abstracted uop$ can be built using what is below adding the more
    // conventional address indexing but retaining the UIDs as abstractions
    // to fully decoded instrs.
    // ----------------------------------------------------------------------
    void Decode::matchFusionGroups_(MatchInfoListType & matches, InstGroupPtr & insts,
                                    InstUidListType & inputUids,
                                    FusionGroupContainerType & fusionGroups)
    {
        matches.clear(); // Clear any existing matches

        // The cache is a map of cachelines, indexed by size,
        //          <size, list of <index,hash>
        //
        //   3   1:hash  2:hash 3:hash  ... modulo size, inputUids.size() % 3
        hcache_.clear();

        for (auto & fgPair : fusionGroups)
        {
            auto & fGrp = fgPair.second;
            size_t grpSize = fGrp.uids().size();

            // grpSize should be less than the input for matching fusion pairs
            if(grpSize > inputUids.size())
            {
                continue;
            }

            hcache_.buildHashCacheEntry(inputUids, grpSize);
        }

        for (auto & fgPair : fusionGroups)
        {

            HashType grpHash = fgPair.first;
            auto & fGrp = fgPair.second;
            size_t grpSize = fGrp.uids().size();

            // No match possible if the fg is bigger than the input
            if (grpSize > inputUids.size())
            {
                /*++filtered;*/ continue;
            }

            // If the grpSize is not in the hash cache we need to calculate it
            if (hcache_.find(grpSize) == hcache_.end())
            {
                hcache_.buildHashCacheEntry(inputUids, grpSize);
            }

            // If this fires something unexplained happened, the entry hit
            // in the first place or it was constructed on a miss,
            // either way this should never fire.
            sparta_assert(hcache_.find(grpSize) != hcache_.end())

                // pairs is from the cache entries with the same size as fGrp.
                // a pair is <index,hash>, index is the position in the input
                HashPairListType pairs = hcache_[grpSize];

            // For each of the pairs that have the same size as fGrp.
            for (auto & ep : pairs)
            {
                // if the pair.hash matches the group's hash
                if (ep.second == grpHash)
                {

                    // DLOG("HASH HIT, index "<<dec<<ep.first<<" hash 0x"<<setw(8)
                    //                        <<hex<<setfill('0')<<grpHash);
                    size_t startIdx = ep.first;
                    bool match = true;
                    size_t j = 0;

                    for (size_t i = startIdx; i < grpSize; ++i)
                    {
                        if (inputUids[i] != fGrp.uids()[j])
                        {
                            match = false;
                            break;
                        }
                        ++j;
                    }

                    if (match)
                    {
                        matches.emplace_back(fGrp.name(), startIdx, 0, fGrp.uids());
                        // DLOG("SEQ MATCH "<<fGrp.name()<<" "<<dec<<grpSize);
                    }
                }
            }
        }

        // FIXME: make this an assignable function object
        // Sort by size descending, sort by startIdx ascending
        std::sort(matches.begin(), matches.end(),
                  [](const FusionGroupMatchInfo & lhs, const FusionGroupMatchInfo & rhs)
                  {
                      if (lhs.size() == rhs.size())
                      {
                          return lhs.startIdx < rhs.startIdx;
                      }
                      return lhs.size() > rhs.size();
                  });
    }

    // ------------------------------------------------------------------------
    // If we get here we know name has been matched, update the stats
    // ------------------------------------------------------------------------
    void Decode::updateFusionGroupUtilization_(std::string name)
    {
        // update the map containing per group utilization counts
        // rely on the default behavior of map[] for new entries
        matchedFusionGroups_[name]++;
        // groups used more than once still only count as one in this stat
        fusion_num_groups_utilized_ = matchedFusionGroups_.size();
    }

    // ------------------------------------------------------------------------
    // ------------------------------------------------------------------------
    void Decode::infoInsts_(std::ostream & os, const InstGroupPtr & insts)
    {
        InstGroup::iterator instItr;
        os << "INSTS: ";
        for (instItr = insts->begin(); instItr != insts->end(); ++instItr)
        {
            os << (*instItr)->info() << endl;
        }
    }

} // namespace olympia
