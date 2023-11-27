#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include "sparta/utils/MathUtils.hpp"

#include "cache/TreePLRUReplacement.hpp"
#include "cache/BasicCacheItem.hpp"
#include "cache/Cache.hpp"
#include "cache/BasicCacheSet.hpp"

#include "MiscUtils.hpp"


namespace olympia
{

    /**
     * @file   BTB.hpp
     * @brief The Branch Target Buffer -- Cache like structure for branch instructions
     *
     * Based on SimpleCache, this unit defines a way associate cache like structure
     * designed to be used as a way of steering fetch ahead.
     *
     * Each set covers an entire block of memory, there may be multiple branches with
     * the same block address. We use a full tag to hit on the right line.
     *
     * We store the branch PC, target, type and a 2 bit saturating counter to predict
     * direction on conditional branches.
     */

    class BTBEntry : public sparta::cache::BasicCacheItem
    {
    public:

        enum class BranchType : std::uint16_t {
            CONDITIONAL,
            __FIRST = CONDITIONAL,
            DIRECT,
            INDIRECT,
            RETURN,
            __LAST
        };

        BTBEntry() :
            valid_(false)
        { };

        BTBEntry(const BTBEntry &rhs) :
            BasicCacheItem(rhs),
            valid_(rhs.valid_),
            target_(rhs.target_),
            is_call_(rhs.is_call_),
            branchtype_(rhs.branchtype_),
            lhist_counter_(rhs.lhist_counter_)
        { };

        BTBEntry &operator=(const BTBEntry &rhs)
        {
            if (&rhs != this) {
                BasicCacheItem::operator=(rhs);
                valid_ = rhs.valid_;
                target_ = rhs.target_;
                is_call_ = rhs.is_call_;
                branchtype_ = rhs.branchtype_;
                lhist_counter_ = rhs.lhist_counter_;
            }
            return *this;
        }

        void reset(uint64_t addr)
        {
            setValid(true);
            setAddr(addr);
            resetLHistCounter_();
        }

        void setValid(bool v) { valid_ = v; }
        bool isValid() const { return valid_; }

        void setTarget(uint64_t t) { target_ = t; }
        uint64_t getTarget() const { return target_; }

        void setBranchType(BranchType branchtype, bool is_call = false)
        {
            sparta_assert(!is_call ||
                          miscutils::isOneOf(branchtype, BranchType::INDIRECT,
                                                         BranchType::DIRECT,
                                                         BranchType::RETURN),
                          "Invalid branch and call combination");
            branchtype_ = branchtype;
            is_call_ = is_call;
        }

        BranchType getBranchType() const { return branchtype_; }
        bool isCall() const { return is_call_; }

        bool predictDirection() {
            if (branchtype_ == BranchType::CONDITIONAL)
            {
                return lhist_counter_ >= 0;
            }
            return true;
        }

        void updateDirection(bool taken)
        {
            if (taken)
            {
                lhist_counter_ = std::min(lhist_count_max_, ++lhist_counter_);
            }
            else
            {
                lhist_counter_ = std::max(lhist_count_min_, --lhist_counter_);
            }
        }


    private:
        bool                 valid_ = false;
        uint64_t             target_;
        bool                 is_call_ = false;
        BranchType           branchtype_;
        int32_t              lhist_counter_ = 0;
        static constexpr int32_t lhist_count_max_ = 1; // 2 bit saturating counter.
        static constexpr int32_t lhist_count_min_ = -2;

        void resetLHistCounter_() { lhist_counter_ = 0; }

    };


    class BTBAddrDecoder : public sparta::cache::AddrDecoderIF
    {
    public:
        BTBAddrDecoder(uint32_t entries, uint32_t stride, uint32_t num_ways)
        {
            index_mask_ = (entries/num_ways)-1;
            index_shift_ = sparta::utils::floor_log2(stride);
        }
        virtual uint64_t calcTag(uint64_t addr) const { return addr; }
        virtual uint32_t calcIdx(uint64_t addr) const { return (addr >> index_shift_) & index_mask_; }
        virtual uint64_t calcBlockAddr(uint64_t addr) const { return addr; }
        virtual uint64_t calcBlockOffset(uint64_t addr) const { return 0;}
    private:
        uint32_t index_mask_;
        uint32_t index_shift_;
    };

    class BTB : public sparta::Unit
    {
    public:
        static constexpr const char * name = "btb";

        // Parameters
        //! \brief Parameters for BTB
        class BTBParameterSet : public sparta::ParameterSet
        {
        public:
            BTBParameterSet(sparta::TreeNode* n) :
                sparta::ParameterSet(n)
            {}

            PARAMETER(uint64_t, num_of_entries, 4096, "BTB # of entries (power of 2)")
            PARAMETER(uint32_t, block_size, 64, "BTB Search stride in bytes (power of 2)")
            PARAMETER(uint32_t, associativity, 8, "BTB associativity (power of 2)")
        };


        BTB(sparta::TreeNode * node, const BTBParameterSet *p) : sparta::Unit(node)
        {
            cache_.reset(new sparta::cache::Cache(p->num_of_entries,
                                                  1,
                                                  p->block_size,
                                                  BTBEntry(),
                                                  sparta::cache::TreePLRUReplacement(p->associativity),
                                                  false));
            cache_->setAddrDecoder(new BTBAddrDecoder(p->num_of_entries, p->block_size, p->associativity));
        }

        /**
         *\return whether an addr is in the cache
            */
        bool isHit(uint64_t addr) const
        {
            const BTBEntry *line = cache_->peekItem(addr);
            return ( line != nullptr );
        }

        // Get a line for replacement
        BTBEntry &getLineForReplacementWithInvalidCheck(uint64_t addr)
        {
            return cache_->getCacheSet(addr).getItemForReplacementWithInvalidCheck();
        }

        /**
         *\return Pointer to line with addr.  nullptr is returned if not fould
        */
        BTBEntry * getLine(uint64_t addr)
        {
            return cache_->getItem(addr);
        }

        /**
         *\return Pointer to line with addr.  nullptr is returned if not fould
        */
        const BTBEntry * peekLine(uint64_t addr) const
        {
            return cache_->peekItem(addr);
        }

        void touchLRU(const BTBEntry &line)
        {
            auto *rep = cache_->getCacheSetAtIndex( line.getSetIndex() ).getReplacementIF();
            rep->touchLRU( line.getWay() );
        }

        void touchMRU(const BTBEntry &line)
        {
            auto *rep = cache_->getCacheSetAtIndex( line.getSetIndex() ).getReplacementIF();
            rep->touchMRU( line.getWay() );
        }

        // Allocate 'line' as having the new 'addr'
        // 'line' doesn't know anything about NT
        void allocateWithMRUUpdate(BTBEntry &line,
                                    uint64_t   addr)
        {
            line.reset( addr );
            touchMRU( line );
        }


        void invalidateLineWithLRUUpdate(BTBEntry &line)
        {
            static const uint64_t addr=0;
            line.reset( addr );
            line.setValid( false );
            touchLRU( line );
        }

        void invalidateAll()
        {
            auto set_it = cache_->begin();
            for (; set_it != cache_->end(); ++set_it) {
                auto line_it = set_it->begin();
                for (; line_it != set_it->end(); ++line_it) {
                    line_it->setValid(false);
                }
                set_it->getReplacementIF()->reset();

            }
        }

        /**
         * determine if there are any open ways in the set.
         */
        bool hasOpenWay(const uint64_t addr)
        {
            return cache_->getCacheSet(addr).hasOpenWay();
        }

        uint32_t getNumWays() const {
            return cache_->getNumWays();
        }

        uint32_t getNumSets() const {
            return cache_->getNumSets();
        }


    private:
        std::unique_ptr<sparta::cache::Cache<BTBEntry>> cache_;
    };
};