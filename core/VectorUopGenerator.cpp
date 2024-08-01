#include "VectorUopGenerator.hpp"
#include "mavis/Mavis.h"
#include "sparta/utils/LogUtils.hpp"

namespace olympia
{
    constexpr char VectorUopGenerator::name[];

    VectorUopGenerator::VectorUopGenerator(sparta::TreeNode* node, const VectorUopGeneratorParameterSet* p) :
        sparta::Unit(node)
    {
        // Vector arithmetic uop generator, increment all src and dest register numbers
        // For a "vadd.vv v12, v4,v8" with an LMUL of 4:
        //      Uop 1: vadd.vv v12, v4, v8
        //      Uop 2: vadd.vv v13, v5, v9
        //      Uop 3: vadd.vv v14, v6, v10
        //      Uop 4: vadd.vv v15, v7, v11
        {
            constexpr bool SINGLE_DEST = false;
            constexpr bool WIDE_DEST = false;
            uop_gen_function_map_.emplace(InstArchInfo::UopGenType::ARITH,
                &VectorUopGenerator::generateArithUop<SINGLE_DEST, WIDE_DEST>);
        }

        // Vector arithmetic single dest uop generator, only increment all src register numbers
        // For a "vmseq.vv v12, v4,v8" with an LMUL of 4:
        //      Uop 1: vadd.vv v12, v4, v8
        //      Uop 2: vadd.vv v12, v5, v9
        //      Uop 3: vadd.vv v12, v6, v10
        //      Uop 4: vadd.vv v12, v7, v11
        {
            constexpr bool SINGLE_DEST = true;
            constexpr bool WIDE_DEST = false;
            uop_gen_function_map_.emplace(InstArchInfo::UopGenType::ARITH_SINGLE_DEST,
                &VectorUopGenerator::generateArithUop<SINGLE_DEST, WIDE_DEST>);
        }

        // Vector arithmetic wide dest uop generator, only increment src register numbers for even uops
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
            constexpr bool SINGLE_DEST = false;
            constexpr bool WIDE_DEST = true;
            uop_gen_function_map_.emplace(InstArchInfo::UopGenType::ARITH_WIDE_DEST,
                &VectorUopGenerator::generateArithUop<SINGLE_DEST, WIDE_DEST>);
        }
    }

    void VectorUopGenerator::setInst(const InstPtr & inst)
    {
        sparta_assert(current_inst_ == nullptr,
            "Cannot start generating uops for a new vector instruction, "
            "current instruction has not finished: " << current_inst_);

        const auto uop_gen_type = inst->getUopGenType();
        sparta_assert(uop_gen_type != InstArchInfo::UopGenType::UNKNOWN,
            "Inst: " << current_inst_ << " uop gen type is unknown");

        if(uop_gen_type != InstArchInfo::UopGenType::NONE)
        {
            // Number of vector elements processed by each uop
            const Inst::VCSRs * current_vcsrs = inst->getVCSRs();
            const uint64_t num_elems_per_uop = Inst::VLEN / current_vcsrs->sew;
            // TODO: For now, generate uops for all elements even if there is a tail
            num_uops_to_generate_ = std::ceil(current_vcsrs->vlmax / num_elems_per_uop);

            if(uop_gen_type == InstArchInfo::UopGenType::ARITH_WIDE_DEST)
            {
                // TODO: Add parameter to support dual dests
                num_uops_to_generate_ *= 2;
            }
        }

        if(num_uops_to_generate_ > 1)
        {
            // Original instruction will act as the first UOp
            inst->setUOpID(0); // set UOpID()   
            current_inst_ = inst;
            ILOG("Inst: " << current_inst_ << " is being split into "
                          << num_uops_to_generate_ << " UOPs");
        }
        else
        {
            ILOG("Inst: " << inst << " does not need to generate uops");
        }
        // Inst counts as the first uop
        --num_uops_to_generate_;
    }

    const InstPtr VectorUopGenerator::generateUop()
    {
        const auto uop_gen_type = current_inst_->getUopGenType();
        sparta_assert(uop_gen_type <= InstArchInfo::UopGenType::NONE,
            "Inst: " << current_inst_ << " uop gen type is unknown");

        // Generate uop
        ILOG("test")
        auto uop_gen_func = uop_gen_function_map_.at(uop_gen_type);
        const InstPtr uop = uop_gen_func(this);
        ++num_uops_generated_;

        // setting UOp instructions to have the same UID and PID as parent instruction
        uop->setUniqueID(current_inst_->getUniqueID());
        uop->setProgramID(current_inst_->getProgramID());

        const Inst::VCSRs * current_vcsrs = current_inst_->getVCSRs();
        uop->setVCSRs(current_vcsrs);
        uop->setUOpID(num_uops_generated_);

        // Set weak pointer to parent vector instruction (first uop)
        sparta::SpartaWeakPointer<olympia::Inst> parent_weak_ptr = current_inst_;
        uop->setUOpParent(parent_weak_ptr);

        // Handle last uop
        if(num_uops_generated_ == num_uops_to_generate_)
        {
            const uint32_t num_elems = current_vcsrs->vl / current_vcsrs->sew;
            uop->setTail(num_elems < current_vcsrs->vlmax);

            reset_();
        }

        ILOG("Generated uop: " << uop);

        return uop;
    }

    template<bool SINGLE_DEST, bool WIDE_DEST>
    const InstPtr VectorUopGenerator::generateArithUop()
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
                src.field_value += (num_uops_generated_ % 2) ? num_uops_generated_ - 1
                                                             : num_uops_generated_;
            }
            else
            {
                src.field_value += num_uops_generated_;
            }
        }

        auto dests = current_inst_->getDestOpInfoList();
        if constexpr (SINGLE_DEST == false)
        {
            for (auto & dest : dests)
            {
                dest.field_value += num_uops_generated_;
            }
        }

        // Create uop
        mavis::ExtractorDirectOpInfoList ex_info(current_inst_->getMnemonic(),
                                                 srcs,
                                                 dests,
                                                 current_inst_->getImmediate());
        InstPtr uop = mavis_facade_->makeInstDirectly(ex_info, getClock());

        // setting UOp instructions to have the same UID and PID as parent instruction
        uop->setUniqueID(current_inst_->getUniqueID());
        uop->setProgramID(current_inst_->getProgramID());

        const Inst::VCSRs * current_VCSRs = current_inst_->getVCSRs();
        uop->setVCSRs(current_VCSRs);
        uop->setUOpID(num_uops_generated_);

        // Set weak pointer to parent vector instruction (first uop)
        sparta::SpartaWeakPointer<olympia::Inst> weak_ptr_inst = current_inst_;
        uop->setUOpParent(weak_ptr_inst);
        uop->setEEW(current_inst_->getEEW());
        uop->setMOP(current_inst_->getMOP());
        uop->setStride(current_inst_->getStride());
        if(uop->isLoadStoreInst()){
            // set base address according to LMUL, i.e if we're on the 3rd
            // LMUL Uop, it's base address should be base address + 3 * EEW
            uop->setTargetVAddr(uop->getTargetVAddr() + uop->getEEW() * uop->getUOpID());
        }

        // Handle last uop
        if(num_uops_generated_ == num_uops_to_generate_)
        {
            const uint32_t num_elems = current_VCSRs->vl / current_VCSRs->sew;
            uop->setTail(num_elems < current_VCSRs->vlmax);

            reset_();
        }
        
        ILOG("Generated uop: " << uop);
        return uop;
    }

    void VectorUopGenerator::handleFlush(const FlushManager::FlushingCriteria & flush_criteria)
    {
        if(current_inst_ && flush_criteria.includedInFlush(current_inst_))
        {
            reset_();
        }
    }
} // namespace olympia
