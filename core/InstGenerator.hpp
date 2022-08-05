// <InstGenerator.hpp> -*- C++ -*-

//!
//! \file InstGenerator.hpp
//! \brief Definition of the CoreModel InstGenerator
//!

#pragma once

#include "Inst.hpp"
#include "sparta/utils/SpartaAssert.hpp"

namespace olympia_core
{
    /*
     * \class InstGenerator
     * \brief Instruction generator base class
     *
     * Base class used to fetch an instruction to send down the core
     * pipe.  Possible derivations are STF Instruction reader, JSON
     * generator, etc
     */
    class InstGenerator
    {
    public:
        virtual ~InstGenerator() {}
        virtual Inst::InstPtr getNextInst() {
            sparta_assert(false, "The base generator does nothing");
            return nullptr;
        }
    };
}
