// <InstGroup.cpp> -*- C++ -*-

#include "InstGroup.hpp"

#include <sstream>

#include "Inst.hpp"
#include "sparta/utils/LogUtils.hpp"

namespace olympia
{
    // This pipeline supports around 135 outstanding example instructions.
    sparta::SpartaSharedPointerAllocator<InstGroup> instgroup_allocator(300, 250);

    std::ostream & operator<<(std::ostream & os, const InstGroup & inst_grp)
    {
        std::string next = "";
        std::stringstream str;
        for (auto inst : inst_grp)
        {
            str << next << HEX8(inst->getPC())
                << " UID(" << inst->getUniqueID() << ") "
                << " PID(" << inst->getProgramID() << ") "
                << " " << inst->getMnemonic();
            next = ", ";
        }
        return os << str.str();
    }

    std::ostream & operator<<(std::ostream & os, const InstGroupPtr & inst_grp_ptr)
    {
        return os << *inst_grp_ptr;
    }
}
