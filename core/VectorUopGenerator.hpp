// <VectorUopGenerator.hpp> -*- C++ -*-
//! \file VectorUopGenerator.hpp
#pragma once

#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/TreeNode.hpp"
#include "sparta/simulation/ParameterSet.hpp"

#include "Inst.hpp"
#include "FlushManager.hpp"
#include "MavisUnit.hpp"

#include <optional>

namespace olympia
{

    /**
     * @file VectorUopGenerator.hpp
     * @brief TODO
     */
    class VectorUopGenerator : public sparta::Unit
    {
      public:
        class Modifier
        {
          public:
            Modifier(std::string name, uint32_t value) : name_{name}, data_{value} {}

            std::string getName() const { return name_; }

            uint32_t getValue() const { return data_; }

            void setValue(uint32_t newValue) { data_ = newValue; }

          private:
            std::string name_;
            uint32_t data_;
        };

        //! \brief Parameters for VectorUopGenerator model
        class VectorUopGeneratorParameterSet : public sparta::ParameterSet
        {
          public:
            VectorUopGeneratorParameterSet(sparta::TreeNode* n) : sparta::ParameterSet(n) {}

            //! \brief Generate uops for widening vector instructions with two dests
            // PARAMETER(bool, widening_dual_dest, false,
            //     "Generate uops for widening vector instructions with two dests")
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

        template <InstArchInfo::UopGenType Type> const InstPtr generateUops();

        uint64_t getNumUopsRemaining() const { return num_uops_to_generate_; }

        void handleFlush(const FlushManager::FlushingCriteria &);

      private:
        void onBindTreeLate_() override;

        MavisType* mavis_facade_;

        // typedef std::function<const InstPtr (VectorUopGenerator*)> FUNC;
        typedef std::function<const InstPtr(VectorUopGenerator*)> UopGenFunctionType;
        typedef std::map<InstArchInfo::UopGenType, UopGenFunctionType> UopGenFunctionMapType;
        UopGenFunctionMapType uop_gen_function_map_;

        // TODO: Use Sparta ValidValue
        InstPtr current_inst_ = nullptr;
        std::vector<Modifier> current_inst_modifiers_;
        // UopGenFunctionMapType::iterator current_uop_gen_function_;

        sparta::Counter vuops_generated_;

        uint64_t num_uops_generated_ = 0;
        uint64_t num_uops_to_generate_ = 0;

        void reset_()
        {
            current_inst_ = nullptr;
            current_inst_modifiers_.clear();
            num_uops_generated_ = 0;
            num_uops_to_generate_ = 0;
        }

        void addModifier(const std::string & name, uint32_t value)
        {
            current_inst_modifiers_.emplace_back(name, value);
        }

        std::optional<uint32_t> getModifier(const std::string & name)
        {
            for (auto & m : current_inst_modifiers_)
            {
                if (m.getName() == name)
                {
                    return m.getValue();
                }
            }
            return {};
        }

        friend class VectorUopGeneratorTester;
    };

    class VectorUopGeneratorTester;
} // namespace olympia
