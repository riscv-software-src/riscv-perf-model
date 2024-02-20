// <InstGroup.hpp> -*- C++ -*-

#pragma once

#include <vector>
#include <iostream>
#include "CoreTypes.hpp"

#include "sparta/utils/LogUtils.hpp"
#include "sparta/utils/SpartaSharedPointer.hpp"
#include "sparta/utils/SpartaSharedPointerAllocator.hpp"

#include "Inst.hpp"

namespace olympia
{
    //! \brief An instruction group is the data type passed between units
    //!
    //! It's faster/easier to copy pointers to an instruction group
    //! than to pass copies of the vectors contained
    class InstGroup
    {
        using InstVector = std::vector<InstQueue::value_type>;
    public:
        using iterator       = InstVector::iterator;
        using const_iterator = InstVector::const_iterator;

        void emplace_back(const typename InstQueue::value_type & inst) {
            insts_.emplace_back(inst);
        }

        iterator begin() { return insts_.begin(); }
        iterator end()   { return insts_.end(); }

        const_iterator begin() const { return insts_.begin(); }
        const_iterator end()   const { return insts_.end(); }

        std::size_t size() const  { return insts_.size(); }
        bool        empty() const { return insts_.empty(); }

        InstQueue::value_type front() const { return insts_.front(); }
        InstQueue::value_type back() const { return insts_.back(); }

        void erase(iterator first, iterator last)
        {
            insts_.erase(first, last);
        }

    private:
        InstVector insts_;
    };
    extern sparta::SpartaSharedPointerAllocator<InstGroup> instgroup_allocator;
    using InstGroupPtr = sparta::SpartaSharedPointer<InstGroup>;

    inline std::ostream & operator<<(std::ostream & os, const InstGroup & inst_grp) {
        std::string next = "";
        std::stringstream str;
        for(auto inst : inst_grp) {
            str << next << HEX8(inst->getPC())
               << " UID(" << inst->getUniqueID() << ") "
               << " PID(" << inst->getProgramID() << ") "
               << " " << inst->getMnemonic();
            next = ", ";
        }
        return (os << str.str());
    }
    inline std::ostream & operator<<(std::ostream & os, const InstGroupPtr & inst_grp_ptr) {
        return os << *inst_grp_ptr;
    }
}
