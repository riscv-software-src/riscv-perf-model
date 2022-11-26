// <CoreTypes.hpp> -*- C++ -*-

#pragma once

#include <vector>

#include "sparta/resources/Queue.hpp"
#include "Inst.hpp"

namespace olympia
{
    //! Instruction Queue
    typedef sparta::Queue<InstPtr> InstQueue;
}
