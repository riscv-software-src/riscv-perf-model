// <InstGroup.hpp> -*- C++ -*-

#pragma once

#include <iosfwd>
#include <vector>

#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"
#include "InstFwd.hpp"

namespace olympia
{
    //! \brief An instruction group is the data type passed between units
    //!
    //! It's faster/easier to copy pointers to an instruction group
    //! than to pass copies of the vectors contained
    class InstGroup
    {
        using InstVector = std::vector<InstPtr>;
    public:
        using iterator       = InstVector::iterator;
        using const_iterator = InstVector::const_iterator;

        void emplace_back(const InstPtr & inst) {
            insts_.emplace_back(inst);
        }

        iterator erase(iterator first, iterator last) {
            return insts_.erase(first, last);
        }

        iterator erase(iterator pos) {
            return insts_.erase(pos);
        }

        iterator begin() { return insts_.begin(); }
        iterator end()   { return insts_.end(); }

        const_iterator begin() const { return insts_.begin(); }
        const_iterator end()   const { return insts_.end(); }

        std::size_t size() const  { return insts_.size(); }
        bool        empty() const { return insts_.empty(); }

    private:
        InstVector insts_;
    };
    extern sparta::SpartaSharedPointerAllocator<InstGroup> instgroup_allocator;

    std::ostream & operator<<(std::ostream & os, const InstGroup & inst_grp);
    std::ostream & operator<<(std::ostream & os, const InstGroupPtr & inst_grp_ptr);
}
