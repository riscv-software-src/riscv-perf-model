
#include "MappedDevice.hpp"

namespace olympia_mss
{
    std::istream& operator>>(std::istream& is, MappedDevice& md) {
        std::string s;
        char c;
        if (!(is >> std::ws) || is.peek() != '[') {
            is.setstate(std::ios::failbit);
            return is;
        }

        int balance = 0;
        while (is.get(c)) {
            s += c;
            if (c == '[') balance++;
            else if (c == ']') balance--;
            if (balance == 0) break;
        }

        // the regex matches [addr, size, "name"] or [addr, size, name]
        // addr and size can be hex (0x...) or dec
        static const std::regex device_re("\\[\\s*(0x[0-9a-fA-F]+|[0-9]+)\\s*,\\s*(0x[0-9a-fA-F]+|[0-9]+)\\s*,\\s*(?:\"([^\"]*)\"|([^,\\]\\s]+))\\s*\\]");
        std::match_results<std::string::const_iterator> match;
        if (std::regex_match(s, match, device_re)) {
            try {
                md.addr = std::stoull(match[1].str(), nullptr, 0);
                md.size = std::stoul(match[2].str(), nullptr, 0);
                md.device_name = match[3].matched ? match[3].str() : match[4].str();
                return is;
            } catch (const std::exception & e) {
                throw sparta::SpartaException("Malformed parameter for mapped device: ") << s
                    << ". Expected: [addr, size, \"name\"]. Internal error: " << e.what();
            }
        }
        throw sparta::SpartaException("Malformed parameter for mapped device: ") << s
            << ". Expected: [addr, size, \"name\"]";
    }

    std::ostream& operator<<(std::ostream& os, const MappedDevice& md) {
        os << "[" << std::hex << md.addr << ", " << md.size << ", \"" << md.device_name << "\"]" << std::dec;
        return os;
    }
}
