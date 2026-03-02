#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <regex>
#include <cstdint>

#include "sparta/utils/SpartaException.hpp"


namespace olympia_mss
{
    struct MappedDevice
    {
        uint64_t addr = 0;
        uint32_t size = 0;
        std::string device_name;

        // to compare two devices
        bool operator==(const MappedDevice& other) const {
            return addr == other.addr && size == other.size && device_name == other.device_name;
        }
    };

    std::istream& operator>>(std::istream& is, MappedDevice& md);
    std::ostream& operator<<(std::ostream& os, const MappedDevice& md);
}
