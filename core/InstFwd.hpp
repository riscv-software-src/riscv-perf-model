#pragma once

#include "sparta/utils/SpartaSharedPointer.hpp"

namespace olympia
{
    class Inst;
    using InstPtr = sparta::SpartaSharedPointer<Inst>;

    class InstGroup;
    using InstGroupPtr = sparta::SpartaSharedPointer<InstGroup>;
} // namespace olympia
