// <CPU.h> -*- C++ -*-

#pragma once

#include <string>

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

namespace olympia
{

    /**
     * @file  CPU.h
     * @brief CPU Unit acts as a logical unit containing multiple cores
     */
    class CPU : public sparta::Unit
    {
      public:
        //! \brief Parameters for CPU model
        class CPUParameterSet : public sparta::ParameterSet
        {
          public:
            CPUParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
        };

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static constexpr char name[] = "cpu";

        /**
         * @brief Constructor for CPU
         *
         * @param node The node that represents (has a pointer to) the CPU
         * @param p The CPU's parameter set
         */
        CPU(sparta::TreeNode* node, const CPUParameterSet* params);

        //! \brief Destructor of the CPU Unit
        ~CPU();

    }; // class CPU
} // namespace olympia
