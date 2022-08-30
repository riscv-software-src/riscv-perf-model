#pragma once

#include <vector>

#include "sparta/resources/Queue.hpp"
#include "Inst.hpp"

namespace olympia
{
    //! Instruction Queue
    typedef sparta::Queue<InstPtr> InstQueue;

    namespace message_categories {
        const std::string INFO = "info";
        // More can be added here, with any identifier...
    }

}
