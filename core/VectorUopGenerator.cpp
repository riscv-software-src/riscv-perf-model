#include "VectorUopGenerator.hpp"
#include "Inst.hpp"
#include "InstArchInfo.hpp"
#include "mavis/Mavis.h"
#include "sparta/utils/LogUtils.hpp"
#include <mavis/InstMetaData.h>

namespace olympia
{
    constexpr char VectorUopGenerator::name[];

    VectorUopGenerator::VectorUopGenerator(sparta::TreeNode* node,
                                           const VectorUopGeneratorParameterSet* p) :
        sparta::Unit(node),
        vuops_generated_(&unit_stat_set_, "vector_uops_generated",
                         "Number of vector uops generated", sparta::Counter::COUNT_NORMAL)
    {
        // Vector elementwise uop generator, increment all src and dest register numbers
        // For a "vadd.vv v12, v4,v8" with an LMUL of 4:
        //      Uop 1: vadd.vv v12, v4, v8
        //      Uop 2: vadd.vv v13, v5, v9
        //      Uop 3: vadd.vv v14, v6, v10
        //      Uop 4: vadd.vv v15, v7, v11
        {
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::ELEMENTWISE,
                &VectorUopGenerator::generateUops<InstArchInfo::UopGenType::ELEMENTWISE>);
        }

        // Vector single dest uop generator, only increment all src register numbers
        // For a "vmseq.vv v12, v4, v8" with an LMUL of 4:
        //      Uop 1: vmseq.vv v12, v4, v8
        //      Uop 2: vmseq.vv v12, v5, v9
        //      Uop 3: vmseq.vv v12, v6, v10
        //      Uop 4: vmseq.vv v12, v7, v11
        {
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::SINGLE_DEST,
                &VectorUopGenerator::generateUops<InstArchInfo::UopGenType::SINGLE_DEST>);
        }

        // Vector wide uop generator, only increment src register numbers for even
        // uops
        // For a "vwmul.vv v12, v4, v8" with an LMUL of 4:
        //     Uop 1: vwmul.vv v12, v4, v8
        //     Uop 2: vwmul.vv v13, v4, v8
        //     Uop 3: vwmul.vv v14, v6, v10
        //     Uop 4: vwmul.vv v15, v6, v10
        //     Uop 5: vwmul.vv v16, v8, v12
        //     Uop 6: vwmul.vv v17, v8, v12
        //     Uop 7: vwmul.vv v18, v10, v14
        //     Uop 8: vwmul.vv v19, v10, v14
        {
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::WIDENING,
                &VectorUopGenerator::generateUops<InstArchInfo::UopGenType::WIDENING>);
        }

        // Vector wide mixed uop generator
        // For a "vwaddu.wv v12, v4, v8" with an LMUL of 4:
        //     Uop 1: vwaddu.wv v12, v4, v8
        //     Uop 2: vwaddu.wv v13, v5, v8
        //     Uop 3: vwaddu.wv v14, v6, v10
        //     Uop 4: vwaddu.wv v15, v7, v10
        //     Uop 5: vwaddu.wv v16, v8, v12
        //     Uop 6: vwaddu.wv v17, v9, v12
        //     Uop 7: vwaddu.wv v18, v10, v14
        //     Uop 8: vwaddu.wv v19, v11, v14
        {
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::WIDENING_MIXED,
                &VectorUopGenerator::generateUops<InstArchInfo::UopGenType::WIDENING_MIXED>);
        }

        // Vector arithmetic multiply-add uop generator, add dest as source
        // For a "vmacc.vv v12, v4, v8" with an LMUL of 4:
        //      Uop 1: vmacc.vv v12, v4, v8, v12
        //      Uop 2: vmacc.vv v13, v5, v9, v13
        //      Uop 3: vmacc.vv v14, v6, v10, v14
        //      Uop 4: vmacc.vv v15, v7, v11, v15
        {
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::MAC,
                &VectorUopGenerator::generateUops<InstArchInfo::UopGenType::MAC>);
        }

        // Vector multiply-add wide dest uop generator
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
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::MAC_WIDE,
                &VectorUopGenerator::generateUops<InstArchInfo::UopGenType::MAC_WIDE>);
        }

        // Vector fixed point clip narrow uop generator
        // For a "vnclipu.wv v0, v4, v8" with an LMUL of 4:
        //      Uop 1: vnclipu.wv v0, v4, v12
        //      Uop 2: vnclipu.wv v0, v5, v12
        //      Uop 3: vnclipu.wv v1, v6, v13
        //      Uop 4: vnclipu.wv v1, v7, v13
        //      Uop 5: vnclipu.wv v2, v8, v14
        //      Uop 6: vnclipu.wv v2, v9, v14
        //      Uop 7: vnclipu.wv v3, v10, v15
        //      Uop 8: vnclipu.wv v3, v11, v15
        {
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::NARROWING,
                &VectorUopGenerator::generateUops<InstArchInfo::UopGenType::NARROWING>);
        }

        // Vector fixed point clip narrow uop generator
        // For a "vzext.vf4 v0, v4" with an LMUL of 4:
        //      Uop 1: vzext.vf4 v0, v4
        //      Uop 2: vzext.vf4 v1, v4
        //      Uop 3: vzext.vf4 v2, v4
        //      Uop 4: vzext.vf4 v3, v4
        {
            uop_gen_function_map_.emplace(
                InstArchInfo::UopGenType::INT_EXT,
                &VectorUopGenerator::generateUops<InstArchInfo::UopGenType::INT_EXT>);
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

        if (uop_gen_type == InstArchInfo::UopGenType::INT_EXT)
        {
            auto mnemonic = inst->getMnemonic();
            auto modifier = mnemonic.substr(mnemonic.find(".") + 1);

            if (modifier == "vf2")
            {
                addModifier("viext", 2);
            }
            else if (modifier == "vf4")
            {
                addModifier("viext", 4);
            }
            else if (modifier == "vf8")
            {
                addModifier("viext", 8);
            }
        }

        // Number of vector elements processed by each uop
        const VectorConfigPtr & vector_config = inst->getVectorConfig();
        const uint64_t num_elems_per_uop = VectorConfig::VLEN / vector_config->getSEW();
        // TODO: For now, generate uops for all elements even if there is a tail
        num_uops_to_generate_ = std::ceil(vector_config->getVLMAX() / num_elems_per_uop);

        if (uop_gen_type == InstArchInfo::UopGenType::WIDENING
            || uop_gen_type == InstArchInfo::UopGenType::WIDENING_MIXED
            || uop_gen_type == InstArchInfo::UopGenType::MAC_WIDE
            || uop_gen_type == InstArchInfo::UopGenType::NARROWING)
        {
            sparta_assert(vector_config->getLMUL() <= 4,
                          "LMUL must be lower or equal to 4.\n" 
                          "These modes set EMUL = 2 * LMUL <= 8.");
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
        ++vuops_generated_;

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

    template <InstArchInfo::UopGenType Type> const InstPtr VectorUopGenerator::generateUops()
    {
        auto srcs = current_inst_->getSourceOpInfoList();
        for (auto & src : srcs)
        {
            if (src.operand_type != mavis::InstMetaData::OperandTypes::VECTOR)
                continue;

            if constexpr (Type == InstArchInfo::UopGenType::ELEMENTWISE
                          || Type == InstArchInfo::UopGenType::MAC)
            {
                src.field_value += num_uops_generated_;
            }
            else if constexpr (Type == InstArchInfo::UopGenType::WIDENING
                               || Type == InstArchInfo::UopGenType::MAC_WIDE)
            {
                src.field_value += num_uops_generated_ / 2;
            }
            else if constexpr (Type == InstArchInfo::UopGenType::WIDENING_MIXED)
            {
                if (src.field_id == mavis::InstMetaData::OperandFieldID::RS2)
                    src.field_value += num_uops_generated_;
                else if (src.field_id == mavis::InstMetaData::OperandFieldID::RS1)
                    src.field_value += num_uops_generated_ / 2;
            }
            else if constexpr (Type == InstArchInfo::UopGenType::NARROWING)
            {
                if (src.field_id == mavis::InstMetaData::OperandFieldID::RS2)
                    src.field_value += num_uops_generated_ / 2;
                else if (src.field_id == mavis::InstMetaData::OperandFieldID::RS1)
                    src.field_value += num_uops_generated_;
            }
            else if constexpr (Type == InstArchInfo::UopGenType::INT_EXT)
            {
                auto ext = getModifier("viext");
                if (!ext)
                {
                    throw sparta::SpartaException("Modifier at current instruction doesnt exist.");
                }
                src.field_value += num_uops_generated_ / *ext;
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
        if constexpr (Type != InstArchInfo::UopGenType::SINGLE_DEST)
        {
            for (auto & dest : dests)
            {
                if constexpr (Type == InstArchInfo::UopGenType::NARROWING)
                {
                    dest.field_value += num_uops_generated_ / 2;
                }
                else
                {
                    dest.field_value += num_uops_generated_;
                }

                if constexpr (Type == InstArchInfo::UopGenType::MAC
                              || Type == InstArchInfo::UopGenType::MAC_WIDE)
                {
                    add_dest_as_src(srcs, dest);
                }
            }
        }

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
