// <InstGroup.cpp> -*- C++ -*-

#include "InstGroup.hpp"

namespace olympia_core
{
    // This pipeline supports around 135 outstanding example instructions.
    sparta::SpartaSharedPointerAllocator<InstGroup> instgroup_allocator(300, 250);
}
