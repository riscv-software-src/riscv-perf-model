// <Inst.cpp> -*- C++ -*-

#include "Inst.hpp"

namespace olympia_core
{
    // This pipeline supports around 250 outstanding instructions.
    InstAllocator         inst_allocator(300, 250);
    InstArchInfoAllocator inst_arch_info_allocator(300, 250);
}
