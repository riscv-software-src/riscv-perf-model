#pragma once

#include "sparta/resources/Scoreboard.hpp"

namespace olympia
{
    using RegisterBitMask = sparta::Scoreboard::RegisterBitMask;

    //! Register file types
    enum RegFile : uint8_t {
        RF_INTEGER,
        RF_FLOAT,
        RF_INVALID,
        N_REGFILES = RF_INVALID
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
                     "RF index off into the weeds " << rf);
        os << regfile_names[rf];
        return os;
    }
}
