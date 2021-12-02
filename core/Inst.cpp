// <Inst.cpp> -*- C++ -*-

#include "Inst.hpp"

namespace olympia_core
{
    // This pipeline supports around 135 outstanding example instructions.
    sparta::SpartaSharedPointerAllocator<Inst> inst_allocator(300, 250);
}
