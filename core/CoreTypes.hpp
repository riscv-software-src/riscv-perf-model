#pragma once

#include <vector>

#include "sparta/resources/Queue.hpp"
#include "Inst.hpp"

namespace olympia
{
    //! Instruction Queue
    typedef sparta::Queue<InstPtr> InstQueue;

    //! Register file types
    enum RegFile {
        RF_INTEGER,
        RF_FLOAT,
        RF_INVALID,
        N_REGFIILES = RF_INVALID
    };

    static inline const char * const regfile_names[] = {
        "integer",
        "float"
    };

    namespace message_categories {
        const std::string INFO = "info";
        // More can be added here, with any identifier...
    }

    inline std::ostream & operator<<(std::ostream & os, const RegFile & rf)
    {
        sparta_assert(rf < RegFile::RF_INVALID,
                     "RF index off into the weeds" << static_cast<uint32_t>(rf));
        os << regfile_names[rf];
        return os;
    }
}
