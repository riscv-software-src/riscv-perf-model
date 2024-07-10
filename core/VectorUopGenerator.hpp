// <VectorUopGenerator.hpp> -*- C++ -*-
//! \file VectorUopGenerator.hpp
#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include "Inst.hpp"
#include "MavisUnit.hpp"

namespace olympia
{

    /**
     * @file VectorUopGenerator.hpp
     * @brief TODO
     */
    class VectorUopGenerator : public sparta::Unit
    {
    public:
        //! \brief Parameters for VectorUopGenerator model
        class VectorUopGeneratorParameterSet : public sparta::ParameterSet
        {
          public:
            VectorUopGeneratorParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}
        };

        /**
         * @brief Constructor for VectorUopGenerator
         *
         * @param node The node that represents (has a pointer to) the VectorUopGenerator
         * @param p The VectorUopGenerator's parameter set
         */
        VectorUopGenerator(sparta::TreeNode* node, const VectorUopGeneratorParameterSet* p);

        //! \brief Name of this resource. Required by sparta::UnitFactory
        static constexpr char name[] = "vec_uop_gen";

        void setMavis(MavisType * mavis) { mavis_facade_ = mavis; }

        void setInst(const InstPtr & inst);

        const InstPtr genUop();

        bool keepGoing() const { return num_uops_to_generate_ > 1; }

    private:
        MavisType * mavis_facade_;

        // TODO: Use Sparta ValidValue
        InstPtr current_inst_ = nullptr;

        uint64_t num_uops_generated_ = 0;
        uint64_t num_uops_to_generate_ = 0;
    };
} // namespace olympia
