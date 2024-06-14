#include "src.h"
#include "mavis/Mavis.h"

namespace core_test
{
    void Src::injectInsts()
    {
        sparta_assert(dut_credits_ > 0,
                      "Can't inject instructions with no credits!");

        olympia::InstGroupPtr inst_groups(new olympia::InstGroup);
        olympia::Inst::PtrType dinst;

        if (inst_generator_)
        {
            if (dinst = inst_generator_->getNextInst(getClock()),
                nullptr != dinst)
            {
                dinst->setUniqueID(unique_id_++);
                inst_groups->emplace_back(dinst);
                --dut_credits_;
            }
        }
        else if (test_type_ == "single")
        {
            mavis::ExtractorDirectInfo ex_info("add", {1, 2}, {3});
            dinst = mavis_facade_->makeInstDirectly(ex_info, getClock());
            dinst->setUniqueID(unique_id_++);
            inst_groups->emplace_back(dinst);
            --dut_credits_;
        }
        else if (test_type_ == "multiple")
        {
            auto inc_reg_num = [](uint32_t & inst_cnt) -> uint32_t
            {
                uint32_t ret = inst_cnt++;
                inst_cnt &= ~uint32_t(32);
                return ret;
            };

            // If num_pipelines == 1, only check pipeline 0 credits
            // Otherwise, check pipeline 0 and pipeline 1
            while (dut_credits_ > 0)
            {
                mavis::ExtractorDirectInfo ex_info{
                    "add",
                    {inc_reg_num(inst_cnt_), inc_reg_num(inst_cnt_)},
                    {inst_cnt_}};
                dinst =
                    mavis_facade_->makeInstDirectly(ex_info, getClock());
                dinst->setUniqueID(unique_id_++);
                inst_groups->emplace_back(dinst);
                --dut_credits_;
            }
        }
        else
        {
            sparta_assert(false, "What test is this? " << test_type_);
        }

        // Send inst group to the DUT
        if (!inst_groups->empty())
        {
            info_logger_ << "Sending group: " << inst_groups;
            out_instgrp_write_.send(inst_groups);
        }
    }

} // namespace core_test
