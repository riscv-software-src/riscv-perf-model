// <VectorUopGenerator.hpp> -*- C++ -*-
//! \file VectorUopGenerator.hpp
#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include "Inst.hpp"
#include "FlushManager.hpp"
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

            //! \brief Generate uops for widening vector instructions with two dests
            //PARAMETER(bool, widening_dual_dest, false,
            //    "Generate uops for widening vector instructions with two dests")
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

        void setInst(const InstPtr & inst);

        const InstPtr generateUop();

        template<bool SINGLE_DEST, bool WIDE_DEST, bool ADD_DEST_AS_SRC>
        const InstPtr generateArithUop();

        uint64_t getNumUopsRemaining() const { return num_uops_to_generate_; }

        void handleFlush(const FlushManager::FlushingCriteria &);

    private:
        void onBindTreeLate_() override;

        MavisType * mavis_facade_;

        //typedef std::function<const InstPtr (VectorUopGenerator*)> FUNC;
        typedef std::function<const InstPtr (VectorUopGenerator*)> UopGenFunctionType;
        typedef std::map<InstArchInfo::UopGenType, UopGenFunctionType> UopGenFunctionMapType;
        UopGenFunctionMapType uop_gen_function_map_;

        // TODO: Use Sparta ValidValue
        InstPtr current_inst_ = nullptr;
        //UopGenFunctionMapType::iterator current_uop_gen_function_;

        uint64_t num_uops_generated_ = 0;
        uint64_t num_uops_to_generate_ = 0;

        void reset_()
        {
            current_inst_ = nullptr;
            num_uops_generated_ = 0;
            num_uops_to_generate_ = 0;
        }
    };
} // namespace olympia
