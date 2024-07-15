#include "VectorUopGenerator.hpp"
#include "mavis/Mavis.h"
#include "sparta/utils/LogUtils.hpp"

namespace olympia
{
    constexpr char VectorUopGenerator::name[];

    VectorUopGenerator::VectorUopGenerator(sparta::TreeNode* node, const VectorUopGeneratorParameterSet* p) :
        sparta::Unit(node)
    {}

    void VectorUopGenerator::setInst(const InstPtr & inst)
    {
        sparta_assert(current_inst_ == nullptr,
            "Cannot start generating uops for a new vector instruction, "
            "current instruction has not finished: " << current_inst_);

        // Number of vector elements processed by each uop
        const Inst::VCSRs * current_VCSRs = inst->getVCSRs();
        const uint64_t num_elems_per_uop = Inst::VLEN / current_VCSRs->sew;
        // TODO: For now, generate uops for all elements even if there is a tail
        num_uops_to_generate_ = std::ceil(current_VCSRs->vlmax / num_elems_per_uop);

        // Does the instruction have tail elements?
        const uint32_t num_elems = current_VCSRs->vl / current_VCSRs->sew;
        inst->setTail(num_elems < current_VCSRs->vlmax);

        if(num_uops_to_generate_ > 1)
        {
            current_inst_ = inst;
            current_inst_->setUOpCount(num_uops_to_generate_);
            ILOG("Inst: " << current_inst_ << " is being split into " << num_uops_to_generate_ << " UOPs");
            // Inst counts as the first uop
            --num_uops_to_generate_;
        }
        else
        {
            ILOG("Inst: " << inst << " does not need to generate uops");
        }
    }

    const InstPtr VectorUopGenerator::generateUop()
    {
        ++num_uops_generated_;

        // Increment source and destination register values
        // TODO: Different generators will handle this differently
        auto srcs = current_inst_->getSourceOpInfoList();
        for (auto & src : srcs)
        {
            src.field_value += num_uops_generated_;
        }
        auto dests = current_inst_->getDestOpInfoList();
        for (auto & dest : dests)
        {
            dest.field_value += num_uops_generated_;
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
