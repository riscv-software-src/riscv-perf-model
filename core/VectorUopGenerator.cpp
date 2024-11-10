#include "VectorUopGenerator.hpp"
#include "mavis/Mavis.h"
#include "sparta/utils/LogUtils.hpp"

namespace olympia
{
    constexpr char VectorUopGenerator::name[];

    VectorUopGenerator::VectorUopGenerator(sparta::TreeNode* node,
                                           const VectorUopGeneratorParameterSet* p) :
        sparta::Unit(node)
    {
        // Vector uop generator, increment all src and dest register numbers
        // For a "vadd.vv v12, v4,v8" with an LMUL of 4:
        //      Uop 1: vadd.vv v12, v4, v8
        //      Uop 2: vadd.vv v13, v5, v9
        //      Uop 3: vadd.vv v14, v6, v10
        //      Uop 4: vadd.vv v15, v7, v11
        {
            constexpr bool SINGLE_DEST = false;
            constexpr bool WIDE_DEST = false;
            constexpr bool ADD_DEST_AS_SRC = false;
            constexpr bool NARROW_DEST = false;
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::ELEMENTWISE,
                &VectorUopGenerator::generateUops<SINGLE_DEST, WIDE_DEST, ADD_DEST_AS_SRC,
                                                  NARROW_DEST>);
        }

        // Vector single dest uop generator, only increment all src register numbers
        // For a "vmseq.vv v12, v4, v8" with an LMUL of 4:
        //      Uop 1: vmseq.vv v12, v4, v8
        //      Uop 2: vmseq.vv v12, v5, v9
        //      Uop 3: vmseq.vv v12, v6, v10
        //      Uop 4: vmseq.vv v12, v7, v11
        {
            constexpr bool SINGLE_DEST = true;
            constexpr bool WIDE_DEST = false;
            constexpr bool ADD_DEST_AS_SRC = false;
            constexpr bool NARROW_DEST = false;
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::SINGLE_DEST,
                &VectorUopGenerator::generateUops<SINGLE_DEST, WIDE_DEST, ADD_DEST_AS_SRC,
                                                  NARROW_DEST>);
        }

        // Vector wide dest uop generator, only increment src register numbers for even
        // uops For a "vwmul.vv v12, v4, v8" with an LMUL of 4:
        //     Uop 1: vwmul.vv v12, v4, v8
        //     Uop 2: vwmul.vv v13, v4, v8
        //     Uop 3: vwmul.vv v14, v6, v10
        //     Uop 4: vwmul.vv v15, v6, v10
        //     Uop 5: vwmul.vv v16, v8, v12
        //     Uop 6: vwmul.vv v17, v8, v12
        //     Uop 7: vwmul.vv v18, v10, v14
        //     Uop 8: vwmul.vv v19, v10, v14
        {
            constexpr bool SINGLE_DEST = false;
            constexpr bool WIDENING = true;
            constexpr bool ADD_DEST_AS_SRC = false;
            constexpr bool NARROWING = false;
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::WIDENING,
                &VectorUopGenerator::generateUops<SINGLE_DEST, WIDENING, ADD_DEST_AS_SRC,
                                                  NARROWING>);
        }

        // Vector narrowing uop generator, only increment src register numbers for even
        // uops For a "vnsrl.wv v12, v4, v8" with an LMUL of 4:
        //     Uop 1: vnsrl.wv v12, v4, v8
        //     Uop 2: vnsrl.wv v12, v5, v9
        //     Uop 3: vnsrl.wv v13, v6, v10
        //     Uop 4: vnsrl.wv v13, v7, v11
        //     Uop 5: vnsrl.wv v14, v8, v12
        //     Uop 6: vnsrl.wv v14, v9, v13
        //     Uop 7: vnsrl.wv v15, v10, v14
        //     Uop 8: vnsrl.wv v15, v11, v15
        {
            constexpr bool SINGLE_DEST = false;
            constexpr bool WIDENING = true;
            constexpr bool ADD_DEST_AS_SRC = false;
            constexpr bool NARROWING = false;
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::WIDENING,
                &VectorUopGenerator::generateUops<SINGLE_DEST, WIDENING, ADD_DEST_AS_SRC,
                                                  NARROWING>);
        }

        // Vector arithmetic multiply-add uop generator, add dest as source
        // For a "vmacc.vv v12, v4, v8" with an LMUL of 4:
        //      Uop 1: vmacc.vv v12, v4, v8, v12
        //      Uop 2: vmacc.vv v13, v5, v9, v13
        //      Uop 3: vmacc.vv v14, v6, v10, v14
        //      Uop 4: vmacc.vv v15, v7, v11, v15
        {
            constexpr bool SINGLE_DEST = false;
            constexpr bool WIDE_DEST = false;
            constexpr bool ADD_DEST_AS_SRC = true;
            constexpr bool NARROW_DEST = false;
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::MAC,
                &VectorUopGenerator::generateUops<SINGLE_DEST, WIDE_DEST, ADD_DEST_AS_SRC,
                                                  NARROW_DEST>);
        }

        // Vector multiply-add wide dest uop generator, add dest as source
        // For a "vwmacc.vv v12, v4, v8" with an LMUL of 4:
        //      Uop 1: vwmacc.vv v12, v4, v8, v12
        //      Uop 2: vwmacc.vv v13, v4, v8, v13
        //      Uop 3: vwmacc.vv v14, v5, v9, v14
        //      Uop 4: vwmacc.vv v15, v5, v9, v15
        //      Uop 5: vwmacc.vv v16, v6, v10, v16
        //      Uop 6: vwmacc.vv v17, v6, v10, v17
        //      Uop 7: vwmacc.vv v18, v7, v11, v18
        //      Uop 8: vwmacc.vv v19, v7, v11, v19
        {
            constexpr bool SINGLE_DEST = false;
            constexpr bool WIDE_DEST = true;
            constexpr bool ADD_DEST_AS_SRC = true;
            constexpr bool NARROW_DEST = false;
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::MAC_WIDE,
                &VectorUopGenerator::generateUops<SINGLE_DEST, WIDE_DEST, ADD_DEST_AS_SRC,
                                                  NARROW_DEST>);
        }
    }

    void VectorUopGenerator::onBindTreeLate_() { mavis_facade_ = getMavis(getContainer()); }

    void VectorUopGenerator::setInst(const InstPtr & inst)
    {
        sparta_assert(current_inst_ == nullptr,
                      "Cannot start generating uops for a new vector instruction, "
                      "current instruction has not finished: "
                          << current_inst_);

        const auto uop_gen_type = inst->getUopGenType();
        sparta_assert(uop_gen_type != InstArchInfo::UopGenType::UNKNOWN,
                      "Inst: " << current_inst_ << " uop gen type is unknown");
        sparta_assert(uop_gen_type != InstArchInfo::UopGenType::NONE,
                      "Inst: " << current_inst_ << " uop gen type is none");

        // Number of vector elements processed by each uop
        const VectorConfigPtr & vector_config = inst->getVectorConfig();
        const uint64_t num_elems_per_uop = VectorConfig::VLEN / vector_config->getSEW();
        // TODO: For now, generate uops for all elements even if there is a tail
        num_uops_to_generate_ = std::ceil(vector_config->getVLMAX() / num_elems_per_uop);

        if ((uop_gen_type == InstArchInfo::UopGenType::WIDENING)
            || (uop_gen_type == InstArchInfo::UopGenType::MAC_WIDE))
        {
            // TODO: Add parameter to support dual dests
            num_uops_to_generate_ *= 2;
        }

        current_inst_ = inst;
        ILOG("Inst: " << current_inst_ << " is being split into " << num_uops_to_generate_
                      << " UOPs");
    }

    const InstPtr VectorUopGenerator::generateUop()
    {
        const auto uop_gen_type = current_inst_->getUopGenType();
        sparta_assert(uop_gen_type <= InstArchInfo::UopGenType::NONE,
                      "Inst: " << current_inst_ << " uop gen type is unknown");

        // Generate uop
        auto uop_gen_func = uop_gen_function_map_.at(uop_gen_type);
        const InstPtr uop = uop_gen_func(this);

        // setting UOp instructions to have the same UID and PID as parent instruction
        uop->setUniqueID(current_inst_->getUniqueID());
        uop->setProgramID(current_inst_->getProgramID());

        const VectorConfigPtr & vector_config = current_inst_->getVectorConfig();
        uop->setVectorConfig(vector_config);
        uop->setUOpID(num_uops_generated_);
        ++num_uops_generated_;

        // Set weak pointer to parent vector instruction (first uop)
        sparta::SpartaWeakPointer<olympia::Inst> parent_weak_ptr = current_inst_;
        uop->setUOpParent(parent_weak_ptr);

        // Does this uop contain tail elements?
        const uint32_t num_elems_per_uop = vector_config->getVLMAX() / vector_config->getSEW();
        uop->setTail((num_elems_per_uop * num_uops_generated_) > vector_config->getVL());

        // Handle last uop
        if (num_uops_generated_ == num_uops_to_generate_)
        {
            reset_();
        }

        ILOG("Generated uop: " << uop);

        return uop;
    }

    template <bool SINGLE_DEST, bool WIDE_DEST, bool ADD_DEST_AS_SRC, bool NARROW_DEST>
    const InstPtr VectorUopGenerator::generateUops()
    {
        // Increment source and destination register values
        auto srcs = current_inst_->getSourceOpInfoList();
        for (auto & src : srcs)
        {
            // Do not increment scalar sources for transfer instructions
            if (src.operand_type != mavis::InstMetaData::OperandTypes::VECTOR)
            {
                continue;
            }

            if constexpr (WIDE_DEST == true)
            {
                // Only increment source values for even uops
                src.field_value += num_uops_generated_ / 2;
            }
            else
            {
                src.field_value += num_uops_generated_;
            }
        }

        // Add a destination to the list of sources
        auto add_dest_as_src = [](auto & srcs, auto & dest)
        {
            // OperandFieldID is an enum with RS1 = 0, RS2 = 1, etc. with a max RS of RS4
            using OperandFieldID = mavis::InstMetaData::OperandFieldID;
            const OperandFieldID field_id = static_cast<OperandFieldID>(srcs.size());
            sparta_assert(
                field_id <= OperandFieldID::RS_MAX,
                "Mavis does not support instructions with more than "
                    << std::dec
                    << static_cast<std::underlying_type_t<OperandFieldID>>(OperandFieldID::RS_MAX)
                    << " sources");
            srcs.emplace_back(field_id, dest.operand_type, dest.field_value);
        };

        auto dests = current_inst_->getDestOpInfoList();
        if constexpr (SINGLE_DEST == false)
        {
            for (auto & dest : dests)
            {

                if constexpr (NARROW_DEST == true)
                {
                    // Only increment destination values for even uops
                    dest.field_value += num_uops_generated_ / 2;
                }
                else
                {
                    dest.field_value += num_uops_generated_;
                }

                if constexpr (ADD_DEST_AS_SRC == true)
                {
                    add_dest_as_src(srcs, dest);
                }
            }
        }

        // If uop contains tail or masked elements that need to be left undisturbed, we need to add
        // the destination registers as source registers
        if constexpr (ADD_DEST_AS_SRC == false)
        {
            const VectorConfigPtr & vector_config = current_inst_->getVectorConfig();
            const uint32_t num_elems_per_uop = vector_config->getVLMAX() / vector_config->getSEW();
            const bool uop_contains_tail_elems =
                (num_elems_per_uop * num_uops_generated_) > vector_config->getVL();

            if (uop_contains_tail_elems && (vector_config->getVTA() == false))
            {
                for (auto & dest : dests)
                {
                    add_dest_as_src(srcs, dest);
                }
            }
        }

        // Create uop
        InstPtr uop;
        if (current_inst_->hasImmediate())
        {
            mavis::ExtractorDirectOpInfoList ex_info(current_inst_->getMnemonic(), srcs, dests,
                                                     current_inst_->getImmediate());
            uop = mavis_facade_->makeInstDirectly(ex_info, getClock());
        }
        else
        {
            mavis::ExtractorDirectOpInfoList ex_info(current_inst_->getMnemonic(), srcs, dests);
            uop = mavis_facade_->makeInstDirectly(ex_info, getClock());
        }

        return uop;
    }

    void VectorUopGenerator::handleFlush(const FlushManager::FlushingCriteria & flush_criteria)
    {
        if (current_inst_ && flush_criteria.includedInFlush(current_inst_))
        {
            reset_();
        }
    }
} // namespace olympia
