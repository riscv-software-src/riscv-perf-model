// <InstGroup.cpp> -*- C++ -*-

#include "InstGroup.hpp"

namespace olympia
{
    // This pipeline supports around 135 outstanding example instructions.
    sparta::SpartaSharedPointerAllocator<InstGroup> instgroup_allocator(300, 250);
}
