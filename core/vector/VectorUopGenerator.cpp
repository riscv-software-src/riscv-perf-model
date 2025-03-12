#include "vector/VectorUopGenerator.hpp"
#include "InstArchInfo.hpp"
#include "mavis/Mavis.h"
#include "mavis/InstMetaData.h"
#include "sparta/utils/LogUtils.hpp"

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
        //    Uop 1: vadd.vv v12, v4, v8
        //    Uop 2: vadd.vv v13, v5, v9
        //    Uop 3: vadd.vv v14, v6, v10
        //    Uop 4: vadd.vv v15, v7, v11
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::ELEMENTWISE,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::ELEMENTWISE>);

        // Vector single dest uop generator, only increment all src register numbers
        // For a "vmseq.vv v12, v4, v8" with an LMUL of 4:
        //    Uop 1: vmseq.vv v12, v4, v8
        //    Uop 2: vmseq.vv v12, v5, v9
        //    Uop 3: vmseq.vv v12, v6, v10
        //    Uop 4: vmseq.vv v12, v7, v11
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::SINGLE_DEST,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::SINGLE_DEST>);

        // Vector single src uop generator, only increment dst register numbers
        // For a "viota.m v0, v8" with an LMUL of 4:
        //    Uop 1: viota.m v0, v8
        //    Uop 2: viota.m v0, v9
        //    Uop 3: viota.m v0, v10
        //    Uop 4: viota.m v0, v11
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::SINGLE_SRC,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::SINGLE_SRC>);

        // Vector wide uop generator, only increment src register numbers for even
        // uops
        // For a "vwmul.vv v12, v4, v8" with an LMUL of 4:
        //    Uop 1: vwmul.vv v12, v4, v8
        //    Uop 2: vwmul.vv v13, v4, v8
        //    Uop 3: vwmul.vv v14, v6, v10
        //    Uop 4: vwmul.vv v15, v6, v10
        //    Uop 5: vwmul.vv v16, v8, v12
        //    Uop 6: vwmul.vv v17, v8, v12
        //    Uop 7: vwmul.vv v18, v10, v14
        //    Uop 8: vwmul.vv v19, v10, v14
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::WIDENING,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::WIDENING>);

        // Vector wide mixed uop generator
        // For a "vwaddu.wv v12, v4, v8" with an LMUL of 4:
        //    Uop 1: vwaddu.wv v12, v4, v8
        //    Uop 2: vwaddu.wv v13, v5, v8
        //    Uop 3: vwaddu.wv v14, v6, v10
        //    Uop 4: vwaddu.wv v15, v7, v10
        //    Uop 5: vwaddu.wv v16, v8, v12
        //    Uop 6: vwaddu.wv v17, v9, v12
        //    Uop 7: vwaddu.wv v18, v10, v14
        //    Uop 8: vwaddu.wv v19, v11, v14
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::WIDENING_MIXED,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::WIDENING_MIXED>);

        // Vector arithmetic multiply-add uop generator, add dest as source
        // For a "vmacc.vv v12, v4, v8" with an LMUL of 4:
        //    Uop 1: vmacc.vv v12, v4, v8, v12
        //    Uop 2: vmacc.vv v13, v5, v9, v13
        //    Uop 3: vmacc.vv v14, v6, v10, v14
        //    Uop 4: vmacc.vv v15, v7, v11, v15
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::MAC,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::MAC>);

        // Vector multiply-add wide dest uop generator
        // For a "vwmacc.vv v12, v4, v8" with an LMUL of 4:
        //    Uop 1: vwmacc.vv v12, v4, v8, v12
        //    Uop 2: vwmacc.vv v13, v4, v8, v13
        //    Uop 3: vwmacc.vv v14, v5, v9, v14
        //    Uop 4: vwmacc.vv v15, v5, v9, v15
        //    Uop 5: vwmacc.vv v16, v6, v10, v16
        //    Uop 6: vwmacc.vv v17, v6, v10, v17
        //    Uop 7: vwmacc.vv v18, v7, v11, v18
        //    Uop 8: vwmacc.vv v19, v7, v11, v19
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::MAC_WIDE,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::MAC_WIDE>);

        // Vector fixed point clip narrow uop generator
        // For a "vnclipu.wv v0, v4, v8" with an LMUL of 4:
        //    Uop 1: vnclipu.wv v0, v4, v5, v12
        //    Uop 2: vnclipu.wv v1, v6, v7, v13
        //    Uop 3: vnclipu.wv v2, v8, v9, v14
        //    Uop 4: vnclipu.wv v3, v10, v11, v15
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::NARROWING,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::NARROWING>);

        // Vector reduction uop generator
        // For a "vredsum.vs v12, v8, v4" with an LMUL of 4:
        //    Uop 1: vredsum.vs v12, v8, v4
        //    Uop 2: vredsum.vs v13, v9, v5, v12
        //    Uop 3: vredsum.vs v14, v10, v6, v13
        //    Uop 4: vredsum.vs v15, v11, v7, v14
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::REDUCTION,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::REDUCTION>);

        // Vector wide reduction uop generator
        // For a "vwredsum.vs v20, v12, v4" with an LMUL of 4:
        //    Uop 1: vredsum.vs v20, v12, v4
        //    Uop 2: vredsum.vs v21, v12, v5, v20
        //    Uop 3: vredsum.vs v22, v13, v6, v21
        //    Uop 4: vredsum.vs v23, v13, v7, v22
        //    Uop 5: vredsum.vs v24, v14, v8, v23
        //    Uop 6: vredsum.vs v25, v14, v9, v24
        //    Uop 7: vredsum.vs v26, v15, v10, v25
        //    Uop 8: vredsum.vs v27, v15, v11, v26
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::REDUCTION_WIDE,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::REDUCTION_WIDE>);

        // Vector fixed point clip narrow uop generator
        // For a "vzext.vf4 v0, v4" with an LMUL of 4:
        //    Uop 1: vzext.vf4 v0, v4
        //    Uop 2: vzext.vf4 v1, v4
        //    Uop 3: vzext.vf4 v2, v4
        //    Uop 4: vzext.vf4 v3, v4
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::INT_EXT,
            &VectorUopGenerator::generateUops_<InstArchInfo::UopGenType::INT_EXT>);

        // Vector slide 1 up uop generator
        // For a "vslide1up.vx v4, v8, x1" with an LMUL of 4:
        //    Uop 1: vslide1up.vx v4, v8, x1
        //    Uop 2: vslide1up.vx v4, v9, v8
        //    Uop 3: vslide1up.vx v4, v10, v9
        //    Uop 4: vslide1up.vx v4, v11, v10
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::SLIDE1UP,
            &VectorUopGenerator::generateSlideUops_<InstArchInfo::UopGenType::SLIDE1UP>);

        // Vector slide 1 down uop generator
        // For a "vslide1ddown.vx v4, v8, x1" with an LMUL of 4:
        //    Uop 1: vslide1down.vx v4, v8, v9
        //    Uop 2: vslide1down.vx v4, v9, v10
        //    Uop 3: vslide1down.vx v4, v10, v11
        //    Uop 4: vslide1down.vx v4, v11, x1
        uop_gen_function_map_.emplace(
            InstArchInfo::UopGenType::SLIDE1DOWN,
            &VectorUopGenerator::generateSlideUops_<InstArchInfo::UopGenType::SLIDE1DOWN>);

        // Vector permute uop generator
        // For a "vrgather.vv v20, v8, v4" with an LMUL of 4:
        //    Load Uop 1: vrgather.vv v4, v5
        //    Load Uop 1: vrgather.vv v6, v7
        //     Exe Uop 1: vrgather.vv v20, v8
        //     Exe Uop 2: vrgather.vv v21, v9
        //     Exe Uop 3: vrgather.vv v22, v10
        //     Exe Uop 4: vrgather.vv v23, v11
        uop_gen_function_map_.emplace(InstArchInfo::UopGenType::PERMUTE,
                                      &VectorUopGenerator::generatePermuteUops_);
    }

    void VectorUopGenerator::onBindTreeLate_() { mavis_facade_ = getMavis(getContainer()); }

    void VectorUopGenerator::setInst(const InstPtr & inst)
    {
        sparta_assert(current_inst_.isValid() == false,
                      "Cannot start generating uops for a new vector instruction, "
                      "current instruction has not finished: "
                          << current_inst_);

        const auto uop_gen_type = inst->getUopGenType();
        sparta_assert(uop_gen_type != InstArchInfo::UopGenType::UNKNOWN,
                      "Inst: " << inst << " uop gen type is unknown");
        sparta_assert(uop_gen_type != InstArchInfo::UopGenType::NONE,
                      "Inst: " << inst << " uop gen type is none");

        const auto mavis_uid = inst->getMavisUid();
        if (uop_gen_type == InstArchInfo::UopGenType::INT_EXT)
        {
            if ((mavis_uid == MAVIS_UID_VZEXTVF2) || (mavis_uid == MAVIS_UID_VSEXTVF2))
            {
                addModifier("viext", 2);
            }
            else if ((mavis_uid == MAVIS_UID_VZEXTVF4) || (mavis_uid == MAVIS_UID_VSEXTVF4))
            {
                addModifier("viext", 4);
            }
            else if ((mavis_uid == MAVIS_UID_VZEXTVF8) || (mavis_uid == MAVIS_UID_VSEXTVF8))
            {
                addModifier("viext", 8);
            }
        }

        // Number of vector elements processed by each uop
        const VectorConfigPtr & vector_config = inst->getVectorConfig();
        const uint64_t num_elems_per_uop = VectorConfig::VLEN / vector_config->getSEW();
        // FIXME: In some scenarios, we may need to generate uops that contain all tail elements,
        // for now let's optimize by generating uops based on the VL
        num_uops_to_generate_ = std::ceil((float)vector_config->getVL() / num_elems_per_uop);

        if (uop_gen_type == InstArchInfo::UopGenType::WIDENING
            || uop_gen_type == InstArchInfo::UopGenType::WIDENING_MIXED
            || uop_gen_type == InstArchInfo::UopGenType::MAC_WIDE
            || uop_gen_type == InstArchInfo::UopGenType::REDUCTION_WIDE)
        {
            sparta_assert(vector_config->getLMUL() <= 4, "LMUL must be lower or equal to 4.\n"
                                                         "These modes set EMUL = 2 * LMUL <= 8.");
            // TODO: Add parameter to support dual dests
            num_uops_to_generate_ *= 2;
        }
        else if (inst->isVectorWholeRegister())
        {
            // For vector move, load and store whole register instructions
            if (mavis_uid == MAVIS_UID_VMV1R)
            {
                num_uops_to_generate_ = 1;
            }
            else if (mavis_uid == MAVIS_UID_VMV2R)
            {
                num_uops_to_generate_ = 2;
            }
            else if (mavis_uid == MAVIS_UID_VMV4R)
            {
                num_uops_to_generate_ = 4;
            }
            else if (mavis_uid == MAVIS_UID_VMV8R)
            {
                num_uops_to_generate_ = 8;
            }
            else
            {
                num_uops_to_generate_ = inst->getSpecialField(mavis::OpcodeInfo::SpecialField::NF);
            }
        }

        sparta_assert(num_uops_to_generate_ <= InstArchInfo::N_VECTOR_UOPS,
                      "Cannot generate more than " << std::dec << InstArchInfo::N_VECTOR_UOPS
                                                   << " vector uops: " << inst);
        current_inst_ = inst;
        ILOG(current_inst_ << " (" << vector_config << ") is being split into "
                           << num_uops_to_generate_ << " UOPs");
    }

    InstPtr VectorUopGenerator::generateUop()
    {
        sparta_assert(current_inst_.isValid(),
                      "Cannot generate uops, current instruction is not set");
        const auto uop_gen_type = current_inst_.getValue()->getUopGenType();
        sparta_assert(uop_gen_type <= InstArchInfo::UopGenType::NONE,
                      "Inst: " << current_inst_ << " uop gen type is unknown");

        // Generate uop
        auto uop_gen_func = uop_gen_function_map_.at(uop_gen_type);
        const InstPtr uop = uop_gen_func(this);

        // setting UOp instructions to have the same UID and PID as parent instruction
        uop->setUniqueID(current_inst_.getValue()->getUniqueID());
        uop->setProgramID(current_inst_.getValue()->getProgramID());

        const VectorConfigPtr & vector_config = current_inst_.getValue()->getVectorConfig();
        uop->setVectorConfig(vector_config);
        uop->setUOpID(num_uops_generated_);

        // Set weak pointer to parent vector instruction
        sparta::SpartaWeakPointer<olympia::Inst> parent_weak_ptr = current_inst_.getValue();
        uop->setUOpParent(parent_weak_ptr);

        ++num_uops_generated_;
        ++vuops_generated_;

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

    template <InstArchInfo::UopGenType Type> InstPtr VectorUopGenerator::generateUops_()
    {
        sparta_assert(current_inst_.isValid(),
                      "Cannot generate uops, current instruction is not set");
        auto srcs = current_inst_.getValue()->getSourceOpInfoList();

        mavis::OperandInfo::Element src_rs3;
        for (auto & src : srcs)
        {
            if (src.operand_type != mavis::InstMetaData::OperandTypes::VECTOR
                || Type == InstArchInfo::UopGenType::SINGLE_SRC)
            {
                continue;
            }

            if constexpr (Type == InstArchInfo::UopGenType::ELEMENTWISE
                          || Type == InstArchInfo::UopGenType::MAC
                          || Type == InstArchInfo::UopGenType::REDUCTION)
            {
                src.field_value += num_uops_generated_;
            }
            else if constexpr (Type == InstArchInfo::UopGenType::WIDENING
                               || Type == InstArchInfo::UopGenType::MAC_WIDE)
            {
                src.field_value += num_uops_generated_ / 2;
            }
            else if constexpr ((Type == InstArchInfo::UopGenType::WIDENING_MIXED)
                               || (Type == InstArchInfo::UopGenType::REDUCTION_WIDE))
            {
                if (src.field_id == mavis::InstMetaData::OperandFieldID::RS2)
                {
                    src.field_value += num_uops_generated_;
                }
                else if (src.field_id == mavis::InstMetaData::OperandFieldID::RS1)
                {
                    src.field_value += num_uops_generated_ / 2;
                }
            }
            else if constexpr (Type == InstArchInfo::UopGenType::NARROWING)
            {
                if (src.field_id == mavis::InstMetaData::OperandFieldID::RS2)
                {
                    src_rs3 = src;
                    src.field_value += num_uops_generated_ * 2;
                    src_rs3.field_value = src.field_value + 1;
                }
                else if (src.field_id == mavis::InstMetaData::OperandFieldID::RS1)
                {
                    src.field_value += num_uops_generated_;
                }
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

        // For narrowing insturction,
        if constexpr (Type == InstArchInfo::UopGenType::NARROWING)
        {
            sparta_assert(src_rs3.field_id != mavis::InstMetaData::OperandFieldID::NONE,
                          "Vector narrowing instructions need to include an RS3 operand!");
            srcs.emplace_back(mavis::InstMetaData::OperandFieldID::RS3, src_rs3.operand_type,
                              src_rs3.field_value);
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

        auto dests = current_inst_.getValue()->getDestOpInfoList();
        if constexpr (Type != InstArchInfo::UopGenType::SINGLE_DEST)
        {
            for (auto & dest : dests)
            {
                dest.field_value += num_uops_generated_;

                if constexpr (Type == InstArchInfo::UopGenType::MAC
                              || Type == InstArchInfo::UopGenType::MAC_WIDE)
                {
                    add_dest_as_src(srcs, dest);
                }
                if constexpr ((Type == InstArchInfo::UopGenType::REDUCTION)
                              || (Type == InstArchInfo::UopGenType::REDUCTION_WIDE))
                {
                    if (num_uops_generated_ != 0)
                    {
                        auto prev_dest = dest;
                        prev_dest.field_value -= 1;
                        add_dest_as_src(srcs, prev_dest);
                    }
                }
            }
        }

        return makeInst_(srcs, dests);
    }

    template <InstArchInfo::UopGenType Type> InstPtr VectorUopGenerator::generateSlideUops_()
    {
        static_assert((Type == InstArchInfo::UopGenType::SLIDE1UP)
                      || (Type == InstArchInfo::UopGenType::SLIDE1DOWN));
        sparta_assert(current_inst_.isValid(),
                      "Cannot generate uops, current instruction is not set");
        auto orig_srcs = current_inst_.getValue()->getSourceOpInfoList();

        mavis::OperandInfo::ElementList srcs;
        for (auto & src : orig_srcs)
        {
            if constexpr (Type == InstArchInfo::UopGenType::SLIDE1UP)
            {
                // For vslide1up, first uop sources the scalar register
                if (src.operand_type != mavis::InstMetaData::OperandTypes::VECTOR)
                {
                    if (num_uops_generated_ == 0)
                    {
                        srcs.emplace_back(src);
                    }
                }
                else
                {
                    srcs.emplace_back(src.field_id, src.operand_type,
                                      src.field_value + num_uops_generated_);
                    if (num_uops_generated_ != 0)
                    {
                        using OperandFieldID = mavis::InstMetaData::OperandFieldID;
                        const OperandFieldID field_id = OperandFieldID::RS3;
                        srcs.emplace_back(field_id, src.operand_type,
                                          src.field_value + num_uops_generated_ - 1);
                    }
                }
            }
            else if constexpr (Type == InstArchInfo::UopGenType::SLIDE1DOWN)
            {
                // For vslide1down, last uop sources the scalar register
                if (src.operand_type != mavis::InstMetaData::OperandTypes::VECTOR)
                {
                    if ((num_uops_generated_ + 1) == num_uops_to_generate_)
                    {
                        srcs.emplace_back(src);
                    }
                }
                else
                {
                    srcs.emplace_back(src.field_id, src.operand_type,
                                      src.field_value + num_uops_generated_);
                    if ((num_uops_generated_ + 1) != num_uops_to_generate_)
                    {
                        using OperandFieldID = mavis::InstMetaData::OperandFieldID;
                        const OperandFieldID field_id = OperandFieldID::RS3;
                        srcs.emplace_back(field_id, src.operand_type,
                                          src.field_value + num_uops_generated_ + 1);
                    }
                }
            }
        }

        auto dests = current_inst_.getValue()->getDestOpInfoList();
        for (auto & dest : dests)
        {
            dest.field_value += num_uops_generated_;
        }

        return makeInst_(srcs, dests);
    }

    InstPtr VectorUopGenerator::generatePermuteUops_()
    {
        sparta_assert(false, "Vector permute uop generation is currently not supported!");
    }

    InstPtr VectorUopGenerator::makeInst_(const mavis::OperandInfo::ElementList & srcs,
                                          const mavis::OperandInfo::ElementList & dests)
    {
        // Create uop
        InstPtr uop;
        if (current_inst_.getValue()->hasImmediate())
        {
            mavis::ExtractorDirectOpInfoList ex_info(current_inst_.getValue()->getMnemonic(), srcs,
                                                     dests,
                                                     current_inst_.getValue()->getImmediate());
            uop = mavis_facade_->makeInstDirectly(ex_info, getClock());
        }
        else
        {
            mavis::ExtractorDirectOpInfoList ex_info(current_inst_.getValue()->getMnemonic(), srcs,
                                                     dests);
            uop = mavis_facade_->makeInstDirectly(ex_info, getClock());
        }

        return uop;
    }

    void VectorUopGenerator::handleFlush(const FlushManager::FlushingCriteria & flush_criteria)
    {
        if (current_inst_.isValid() && flush_criteria.includedInFlush(current_inst_))
        {
            reset_();
        }
    }

    void VectorUopGenerator::dumpDebugContent_(std::ostream & output) const
    {
        output << "Current Vector Instruction: " << current_inst_ << std::endl;
        output << "Num Uops Generated: " << num_uops_generated_ << std::endl;
        output << "Num Uops Remaining: " << (num_uops_to_generate_ - num_uops_generated_)
               << std::endl;
    }
} // namespace olympia
