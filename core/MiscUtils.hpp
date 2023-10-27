// <MiscUtils.hpp> -*- C++ -*-

#pragma once

namespace olympia::miscutils
{
    // Is var in the set of vals
    // https://stackoverflow.com/questions/15181579/c-most-efficient-way-to-compare-a-variable-to-multiple-values
    // Usage: bool in = isOneOf(x, 1, 5, 7, 9, 23);
    template<typename Var, typename ... Val>
    bool isOneOf(Var&& var, Val&& ... val) {
        return ((var == val) || ...);
    }
}
