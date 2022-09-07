// <Inst.cpp> -*- C++ -*-

#include "Inst.hpp"

namespace olympia
{
    // This pipeline supports around 250 outstanding instructions.
    InstAllocator         inst_allocator          (3000, 2500);
    InstArchInfoAllocator inst_arch_info_allocator(3000, 2500);
}
