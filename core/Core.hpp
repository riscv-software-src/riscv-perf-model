

#pragma once

#include <string>

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/functional/RegisterSet.hpp"
#include "sparta/simulation/ParameterSet.hpp"

namespace olympia
{
    // In Olympia the Core class is just a placeholder and a
    // full-blown unit.  In reality, it can just be a simple
    // sparta::TreeNode
    class Core : public sparta::Unit
    {
      public:
        //! \brief Parameters for Core model
        class CoreParameterSet : public sparta::ParameterSet
        {
          public:
            CoreParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
        };

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static constexpr char name[] = "core";

        /*!
         * \brief Core constructor
         * \note This signature is dictated by the sparta::UnitFactory created
         * to contain this node. Generally,
         * \param node TreeNode that is creating this core (always a
         * sparta::UnitTreeNode)
         * \param params Fully configured and validated params, which were
         * instantiate by the sparta::UnitFactory which is instnatiating this
         * resource. Note that the type of params is the ParamsT argument of
         * sparta::UnitFactory and defaults to this class' nested ParameterSet
         * type, NOT sparta::ParameterSet.
         * \param ports Fully configured ports for this component.
         */
        Core(sparta::TreeNode* node, const CoreParameterSet* params);

        ~Core() {}
    };
} // namespace olympia
