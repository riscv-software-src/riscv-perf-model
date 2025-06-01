// <Rename.cpp> -*- C++ -*-

/**
 * @file  Rename.cpp
 * @brief Implementation of a simple rename block
 */

#include <algorithm>
#include <ranges>

#include "CoreUtils.hpp"
#include "Rename.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/app/FeatureConfiguration.hpp"

namespace olympia
{
    const char Rename::name[] = "rename";

    Rename::Rename(sparta::TreeNode* node, const RenameParameterSet* p) :
        sparta::Unit(node),
        num_to_rename_per_cycle_(p->num_to_rename),
        partial_rename_(p->partial_rename),
        enable_move_elimination_(p->move_elimination),
        rename_histogram_(*getStatisticSet(), "rename_histogram", "Rename Stage Histogram",
                          [&p]()
                          {
                              std::vector<int> v(p->num_to_rename + 1);
                              std::iota(v.begin(), v.end(), 0);
                              return v;
                          }()),
        rename_event_("RENAME", getContainer(), getClock()),
        uop_queue_("rename_uop_queue", p->rename_queue_depth, node->getClock(), getStatisticSet())
    {
        uop_queue_.enableCollection(node);

        // The path into the Rename block
        // - Instructions are received on the Uop Queue Append port
        // - Credits arrive on the dispatch queue credits port
        in_uop_queue_append_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Rename, decodedInstructions_, InstGroupPtr));
        in_dispatch_queue_credits_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Rename, creditsDispatchQueue_, uint32_t));
        in_reorder_flush_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Rename, handleFlush_, FlushManager::FlushingCriteria));
        in_rename_retire_ack_.registerConsumerHandler(
            CREATE_SPARTA_HANDLER_WITH_DATA(Rename, getAckFromROB_, InstGroupPtr));

        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Rename, setupRename_));

        auto setup_map = [this](auto reg_file, uint32_t num_regs_reserved)
        {
            // for x0 for RF_INTEGER, we don't want to set a PRF for it because x0 is hardwired to 0
            const auto staring_arf = (reg_file == core_types::RegFile::RF_INTEGER ? 1 : 0);
            for (uint32_t areg = staring_arf; areg < NUM_RISCV_REGS_; areg++)
            {
                map_table_[reg_file][areg] = num_regs_reserved++;
            }

            return num_regs_reserved;
        };

        auto setup_freelists =
            [this](auto reg_file, const uint32_t num_renames, const uint32_t num_regs_reserved)
            {
                auto & rcomp = regfile_components_[reg_file];
                auto & reference_counter = rcomp.reference_counter;

                // for x0 for RF_INTEGER, we don't want to set a PRF for it because x0 is hardwired to 0
                uint32_t i = 0;
                if (reg_file == core_types::RegFile::RF_INTEGER)
                {
                    reference_counter.emplace_back(0);
                    i = 1;
                }
                for (; i < num_regs_reserved; i++)
                {
                    // for the first 32 Float (31 INT) registers, we mark
                    // their reference_counters 1, as they are the current
                    // "valid" PRF for that ARF.  This can be
                    // parameterized to NOT do this if running a trace
                    // that is "bare metal"
                    reference_counter.emplace_back(1);
                }
                for (uint32_t j = num_regs_reserved; j < num_renames; ++j)
                {
                    rcomp.freelist.emplace(j);
                    reference_counter.emplace_back(0);
                }
            };

        for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
        {
            uint32_t regs_reserved = (reg_file == core_types::RegFile::RF_INTEGER) ? 1 : 0;
            regs_reserved = setup_map(reg_file, regs_reserved);

            uint32_t num_renames = 0;
            switch (reg_file)
            {
                case core_types::RegFile::RF_INTEGER:
                    num_renames = p->num_integer_renames;
                    break;
                case core_types::RegFile::RF_FLOAT:
                    num_renames = p->num_float_renames;
                    break;
                case core_types::RegFile::RF_VECTOR:
                    num_renames = p->num_vector_renames;
                    break;
                case core_types::RegFile::RF_INVALID:
                    throw sparta::SpartaException("Invalid register file type.");
            }

            setup_freelists(reg_file, num_renames, regs_reserved);
        }

        // This Rename unit might not support all register files, so have to ignore a few params
        // just in case
        p->num_integer_renames.ignore();
        p->num_float_renames.ignore();
        p->num_vector_renames.ignore();
    }

    // Using the Rename factory, create the Scoreboards
    void RenameFactory::onConfiguring(sparta::ResourceTreeNode* node)
    {
        sparta::TreeNode* sb_tn = nullptr;
        sb_tns_.emplace_back(
            sb_tn = new sparta::TreeNode(node, "scoreboards", "Scoreboards used by Rename"));

        // Set up the Scoreboard resources
        for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
        {
            const auto reg_file_name = core_types::regfile_names[reg_file];
            sb_tns_.emplace_back(new sparta::ResourceTreeNode(
                                     sb_tn, reg_file_name, sparta::TreeNode::GROUP_NAME_NONE,
                                     sparta::TreeNode::GROUP_IDX_NONE, reg_file_name + std::string(" Scoreboard"),
                                     &sb_facts_[reg_file]));
        }
    }

    void Rename::setupRename_()
    {
        // Set up scoreboards
        auto sbs_tn = getContainer()->getChild("scoreboards");
        sparta_assert(sbs_tn != nullptr, "Expected to find 'scoreboards' node in Rename, got none");
        for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
        {
            // Get scoreboard resources
            auto sb_tn = sbs_tn->getChild(core_types::regfile_names[reg_file]);
            auto & scoreboard = regfile_components_[reg_file].scoreboard;

            scoreboard = sb_tn->getResourceAs<sparta::Scoreboard*>();

            // Initialize 32 scoreboard resources,
            // all ready.
            uint32_t reg = 0;
            if (reg_file == core_types::RegFile::RF_INTEGER)
            {
                reg = 1;
            }
            const uint32_t num_regs = NUM_RISCV_REGS_;
            core_types::RegisterBitMask bits;
            for (; reg < num_regs; ++reg)
            {
                bits.set(reg);
            }
            scoreboard->set(bits);
        }

        // Send the initial credit count
        out_uop_queue_credits_.send(uop_queue_.capacity());
        stall_counters_[current_stall_].startCounting();

        // ev_sanity_check_.schedule(1);
    }

    void Rename::creditsDispatchQueue_(const uint32_t & credits)
    {
        sparta_assert(in_dispatch_queue_credits_.dataReceived());

        credits_dispatch_ += credits;
        if (uop_queue_.size() > 0)
        {
            ev_schedule_rename_.schedule();
        }
        else
        {
            setStall_(StallReason::NO_DECODE_INSTS);
        }
        DLOG("credits from dispatch.  Total: " << credits_dispatch_);
    }

    void Rename::getAckFromROB_(const InstGroupPtr & inst_grp_ptr)
    {
        ILOG("Retired instructions: " << inst_grp_ptr);

        auto reclaim_rename = [this](const InstPtr & inst_ptr, const core_types::RegFile reg_file,
                                     const RenameData::Reg & dest)
        {
            if (SPARTA_EXPECT_TRUE(!dest.op_info.is_x0))
            {
                const auto prev_dest = dest.prev_dest;
                sparta_assert(prev_dest != std::numeric_limits<uint32_t>::max());
                auto & rcomp = regfile_components_[dest.op_info.reg_file];
                auto & ref = rcomp.reference_counter[prev_dest];
                sparta_assert(ref.cnt != 0, "reclaim had a 0 ref for " << inst_ptr);
                DLOG("\t\treclaiming: " << reg_file << " val:" << dest.phys_reg);
                --ref.cnt;
                // free previous PRF mapping if no references
                // from srcs, there should be a new dest
                // mapping for the ARF -> PRF so we know it's
                // free to be pushed to freelist if it has no
                // other src references
                if (ref.cnt == 0)
                {
                    ILOG("\tpushing " << dest.op_info.reg_file << " " << prev_dest
                         << " on freelist for uid:" << inst_ptr->getUniqueID());
                    rcomp.freelist.emplace(prev_dest);
                }
            }
        };

        for (const auto & inst_ptr : *inst_grp_ptr)
        {
            sparta_assert(inst_ptr->getStatus() == Inst::Status::RETIRED,
                          "Get ROB Ack, but the inst hasn't retired yet! " << inst_ptr);

            ILOG("\treclaiming: " << inst_ptr);

            for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
            {
                for (const auto & dest : inst_ptr->getRenameData().getDestList(
                         static_cast<core_types::RegFile>(reg_file)))
                {
                    reclaim_rename(inst_ptr, static_cast<core_types::RegFile>(reg_file), dest);
                }
            }

            sparta_assert(!inst_queue_.empty(), "ROB and rename inst_queue out of sync");

            const auto & oldest_inst = inst_queue_.front();
            sparta_assert(oldest_inst->getUniqueID() == inst_ptr->getUniqueID(),
                          "ROB and rename inst_queue out of sync");
            inst_queue_.pop_front();
        }

        if (credits_dispatch_ > 0 && (uop_queue_.size() > 0))
        {
            ev_schedule_rename_.schedule();
        }
    }

    // Handle incoming flush
    void Rename::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Got a flush call for " << criteria);

        // Restore the rename map, reference counters and freelist by
        // walking through the inst_queue_
        // inst_queue_.erase((++it).base()) will advance the
        // reverse_iterator and then erase the element it previously
        // pointed to. it remains valid because we're erasing from the
        // back of the deque
        for (auto it = inst_queue_.rbegin(); it != inst_queue_.rend();
             inst_queue_.erase((++it).base()))
        {
            const auto & inst_ptr = *it;
            if (!criteria.includedInFlush(inst_ptr))
            {
                ILOG("\t" << inst_ptr << " not included in flush ")
                    break;
            }
            else
            {
                ILOG("\treclaiming: " << inst_ptr);

                for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
                {
                    auto const & dests = inst_ptr->getRenameData().getDestList(
                        static_cast<core_types::RegFile>(reg_file));
                    for (const auto & dest : dests)
                    {
                        // restore rename table following a flush
                        if (!dest.op_info.is_x0)
                        {
                            map_table_[dest.op_info.reg_file][dest.op_info.field_value] =
                                dest.prev_dest;

                            // free renamed PRF mapping when reference counter reaches zero
                            ILOG("\t\treclaiming: " << dest.op_info.reg_file
                                 << " val:" << dest.phys_reg);
                            auto & rcomp = regfile_components_[dest.op_info.reg_file];
                            auto & ref = rcomp.reference_counter[dest.phys_reg];
                            sparta_assert(ref.cnt != 0, "reclaim had a 0 ref for " << inst_ptr);
                            --ref.cnt;
                            if (ref.cnt == 0)
                            {
                                sparta_assert(dest.op_info.reg_file
                                              != core_types::RegFile::RF_INVALID);
                                rcomp.freelist.emplace(dest.phys_reg);
                            }
                        }
                    }

                    inst_ptr->getRenameData().clear(static_cast<core_types::RegFile>(reg_file));
                }
            }
        }

        setStall_(NO_DECODE_INSTS);
        if (false == uop_queue_.empty())
        {
            out_uop_queue_credits_.send(uop_queue_.size());
            uop_queue_.clear();
        }
    }

    void Rename::decodedInstructions_(const InstGroupPtr & insts)
    {
        for (auto & i : *insts)
        {
            DLOG("Received inst: " << i);
            uop_queue_.push_back(i);
        }
        updateRegcountData_();
        ev_schedule_rename_.schedule();
    }

    void Rename::updateRegcountData_()
    {
        // Clear the data
        uop_queue_regcount_data_ = RegCountData();

        if (false == uop_queue_.empty())
        {
            for (const auto & inst : uop_queue_)
            {
                // create an index count for each instruction entered
                const auto & dests = inst->getDestOpInfoListWithRegfile();
                for (const auto & dest : dests)
                {
                    const auto reg_file = dest.reg_file;
                    // if dest is x0, we don't need to count it towards
                    // cumulative register count
                    if (false == dest.is_x0)
                    {
                        ++uop_queue_regcount_data_.cumulative_reg_counts[reg_file];
                    }
                }
                if (partial_rename_)
                {
                    // just need to update register counts for the
                    // first instruction in the queue
                    break;
                }
            }
        }
    }

    std::pair<bool, Rename::StallReason> Rename::enoughRenames_() const
    {
        // Determine if rename has enough destination renames to
        // rename this instruction.
        bool enough_renames = true;

        static constexpr StallReason stall_reason_map[core_types::RegFile::N_REGFILES] = {
            StallReason::NO_INTEGER_RENAMES,
            StallReason::NO_FLOAT_RENAMES,
            StallReason::NO_VECTOR_RENAMES,
        };

        StallReason stall_reason = StallReason::NOT_STALLED;

        for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
        {
            if (uop_queue_regcount_data_.cumulative_reg_counts[reg_file]
                > regfile_components_[reg_file].freelist.size())
            {
                enough_renames = false;
                stall_reason = stall_reason_map[reg_file];
                break;
            }
        }
        return {enough_renames, stall_reason};
    }

    void Rename::scheduleRenaming_()
    {
        setStall_(StallReason::NOT_STALLED);

        const auto disp_size = uop_queue_.size();

        if (disp_size == 0)
        {
            setStall_(NO_DECODE_INSTS);
            return;
        }

        bool have_dispatch_credits = false;
        if (false == partial_rename_)
        {
            // If we're not doing partial renaming, we need full BW
            // from dispatch.
            if (credits_dispatch_ < disp_size)
            {
                DLOG("not enough disp credits");
                setStall_(StallReason::NO_DISPATCH_CREDITS);
            }
            else
            {
                have_dispatch_credits = true;
            }
        }
        else
        {
            have_dispatch_credits = (credits_dispatch_ > 0);
        }

        // If we have credits from dispatch.  If so schedule a rename
        // session this cycle if we have enough Renames for at least
        // the oldest instruction in Rename
        if (have_dispatch_credits)
        {
            const auto [enough_renames, stalled_regfile] = enoughRenames_();
            if (enough_renames)
            {
                ev_rename_insts_.schedule();
            }
            else
            {
                DLOG("not enough renames");
                setStall_(stalled_regfile);
            }
        }
        else
        {
            if (0 == credits_dispatch_)
            {
                setStall_(StallReason::NO_DISPATCH_CREDITS);
            }
        }
        ILOG("current stall: " << getStall_());
    }

    void Rename::renameInstructions_()
    {
        // Pick instructions from uop queue to rename
        InstGroupPtr insts = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);

        uint32_t num_to_rename = std::min(uop_queue_.size(), num_to_rename_per_cycle_);
        num_to_rename = std::min(credits_dispatch_, num_to_rename);

        sparta_assert(num_to_rename > 0,
                      "Not sure why we're renaming if there are no credits and/or no insts");
        do
        {
            const auto & inst_to_rename = uop_queue_.access(0);

            ILOG("Renaming " << inst_to_rename);

            if (partial_rename_)
            {
                const auto [enough_renames, stalled_regfile] = enoughRenames_();
                if (false == enough_renames)
                {
                    setStall_(stalled_regfile);
                    ILOG("\tStall: Not enough renames " << inst_to_rename);
                    break;
                }
            }

            // Rename the instruction
            renameSources_(inst_to_rename);
            renameDests_(inst_to_rename);
            inst_to_rename->setStatus(Inst::Status::RENAMED);
            insts->emplace_back(std::move(uop_queue_.access(0)));

            // Remove it from uop queue
            uop_queue_.erase(0);

            inst_queue_.emplace_back(inst_to_rename);
            rename_event_.collect(*inst_to_rename);

            if (partial_rename_)
            {
                if (false == uop_queue_.empty())
                {
                    updateRegcountData_();
                }
            }

            --num_to_rename;

        } while (num_to_rename != 0);

        if (false == partial_rename_)
        {
            sparta_assert(num_to_rename == 0,
                          "Still have instructions to rename, but we're not partial to that. HA!");
            sparta_assert(uop_queue_.empty(), "How is the uop queue not empty?")
                }

        if (false == insts->empty())
        {
            const uint32_t num_renamed = insts->size();
            // Send insts to dispatch that were renamed
            ILOG("sending insts to dispatch: " << insts);
            out_dispatch_queue_write_.send(insts);
            credits_dispatch_ -= num_renamed;

            // Replenish credits in the Decode unit
            out_uop_queue_credits_.send(num_renamed);
            rename_histogram_.addValue(num_renamed);
        }

        if (credits_dispatch_ > 0 && (uop_queue_.size() > 0))
        {
            ev_schedule_rename_.schedule(1);
        }
        else if (credits_dispatch_ == 0)
        {
            setStall_(StallReason::NO_DISPATCH_CREDITS);
        }
        else
        {
            setStall_(StallReason::NO_DECODE_INSTS);
        }
    }

    void Rename::renameSources_(const InstPtr & renaming_inst)
    {
        const auto & srcs = renaming_inst->getSrcOpInfoListWithRegfile();
        for (const auto & src : srcs)
        {
            const auto reg_file = src.reg_file;
            const auto arch_num = src.field_value;
            const bool is_x0 = src.is_x0;
            const bool is_rs2 = src.field_id == mavis::InstMetaData::OperandFieldID::RS2;
            const bool is_rs3 = src.field_id == mavis::InstMetaData::OperandFieldID::RS3;

            // we check if src is RF_INTEGER x0, if so, we skip rename
            if (is_x0)
            {
                // if x0 is a data operand for LSU agen, we need to
                // set it in DataReg when we check in LSU so we can
                // still check the scoreboard, which will always
                // return back ready for x0
                if (is_rs2)
                {
                    renaming_inst->setDataRegister({arch_num, src});
                }
                continue;
            }

            const uint32_t prf = map_table_[reg_file][arch_num];

            // For load/store, check if producing inst was a load
            if (renaming_inst->isLoadStoreInst())
            {
                auto & rcomp = regfile_components_[reg_file];
                auto & reference_counter = rcomp.reference_counter[prf];
                if (reference_counter.producer_is_load)
                {
                    renaming_inst->setLoadProducer(true);
                    DLOG("Renaming ld/st that has a load producer: " << renaming_inst);
                }
            }

            // we check for load/store separately because address
            // operand is always integer
            if ((is_rs2 || is_rs3) && renaming_inst->isLoadStoreInst())
            {
                renaming_inst->setDataRegister({prf, src});
                ILOG("\tls data rename " << arch_num << " -> " << prf);
                continue;
            }

            ILOG("\tsource rename " << reg_file << " " << arch_num << " -> " << prf);
            renaming_inst->addSrcRegister({prf, src});
        }
    }

    void Rename::renameDests_(const InstPtr & renaming_inst)
    {
        const auto & dests = renaming_inst->getDestOpInfoListWithRegfile();
        for (const auto & dest : dests)
        {
            const auto reg_file = dest.reg_file;
            const auto arch_num = dest.field_value;
            const bool is_x0 = dest.is_x0;

            if (false == is_x0)
            {
                auto & rcomp = regfile_components_[reg_file];
                auto & freelist = rcomp.freelist;

                sparta_assert(!freelist.empty(), "Freelist should never be empty");

                uint32_t prf = std::numeric_limits<uint32_t>::max();
                bool update_scoreboard = true;

                bool move_eliminated = false;
                if (enable_move_elimination_ && renaming_inst->isMove())
                {
                    // Get the source PRF
                    const auto & src_list = renaming_inst->getRenameData().getSourceList(reg_file);

                    // Check for FP move operations that have 2
                    // source operands.  These are short hand fmv
                    // operations: fsgnj rx, ry, ry
                    if (src_list.size() > 1)
                    {
                        sparta_assert(
                            src_list[0].phys_reg == src_list[0].phys_reg,
                            "MOV inst with 2 sources are not equivalent: " << renaming_inst);
                    }

                    // Check for moves from 1 register file type to
                    // another.  Can't do that.  In this case, the
                    // source list will be empty for that register
                    // file type
                    if (false == src_list.empty())
                    {
                        prf = src_list[0].phys_reg;
                        renaming_inst->setTargetROB();
                        ILOG("\tMove elim: mapping " << arch_num << " to " << prf);
                        update_scoreboard = false;
                        ++move_eliminations_;
                        move_eliminated = true;
                    }
                }

                if (SPARTA_EXPECT_TRUE(false == move_eliminated))
                {
                    prf = freelist.front();
                    DLOG("popping: " << prf);
                    freelist.pop();
                }

                sparta_assert(prf != std::numeric_limits<uint32_t>::max(),
                              "PRF not assigned neither from the freelist nor move elim (if enabled)");

                const uint32_t prev_dest = map_table_[reg_file][arch_num];
                RenameData::Reg renamed_dst({prf, dest, prev_dest});

                map_table_[reg_file][arch_num] = prf;

                // we increase reference_counter_ for destinations to mark them as "valid",
                // so the PRF in the reference_counter_ should have a value of 1
                // once a PRF reference_counter goes to 0, we know that the PRF isn't the
                // "valid" PRF for that ARF anymore and there are no sources referring to it
                // so we can push it to freelist
                auto & reference_counter = rcomp.reference_counter[renamed_dst.phys_reg];
                ++reference_counter.cnt;
                reference_counter.producer_id = renaming_inst->getUniqueID();
                reference_counter.producer = renaming_inst;
                reference_counter.producer_is_load = renaming_inst->isLoadInst();
                ILOG("\tdest rename " << renamed_dst);
                if (SPARTA_EXPECT_TRUE(update_scoreboard))
                {
                    renaming_inst->addDestRegisterWithScoreboardUpdate(
                        std::forward<RenameData::Reg>(renamed_dst),
                        regfile_components_[reg_file].scoreboard);
                }
                else
                {
                    renaming_inst->addDestRegister(std::forward<RenameData::Reg>(renamed_dst));
                }
            }
        }
    }

    void Rename::sanityCheck_()
    {
        // Check for duplications in the freelist
        for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
        {
            const auto & rcomp = regfile_components_[reg_file];
            if (false == rcomp.freelist.empty())
            {
                std::vector<uint32_t> sorted_fl;
                // copy
                auto flq = rcomp.freelist;
                while (false == flq.empty())
                {
                    sorted_fl.emplace_back(flq.back());
                    flq.pop();
                }
                std::sort(sorted_fl.begin(), sorted_fl.end());
                uint32_t reg = sorted_fl.front();
                auto next_reg = sorted_fl.begin();
                ++next_reg;
                while (next_reg != sorted_fl.end())
                {
                    sparta_assert(reg != *next_reg,
                                  "Duplicate reg " << reg << " in regfile " << reg_file);
                    reg = *next_reg;
                }
            }
        }

        ev_sanity_check_.schedule(1);
    }

    template <class OStreamT> void Rename::dumpRenameContent_(OStreamT & output) const
    {
        output << "Rename Contents" << std::endl;
        output << "\tcurrent stall: " << current_stall_ << std::endl;
        output << "\tdisp credits: " << credits_dispatch_ << std::endl;
        output << "\tUop Queue" << std::endl;
        for (const auto & inst : uop_queue_)
        {
            output << "\t\t" << inst << " S_INT"
                   << sparta::printBitSet(
                       inst->getSrcRegisterBitMask(core_types::RegFile::RF_INTEGER))
                   << " S_DAT"
                   << sparta::printBitSet(
                       inst->getDataRegisterBitMask(core_types::RegFile::RF_INTEGER))
                   << " D_INT"
                   << sparta::printBitSet(
                       inst->getDestRegisterBitMask(core_types::RegFile::RF_INTEGER))
                   << std::endl;
        }
        output << "\n\toutstanding insts (waiting for retire)" << std::endl;
        for (const auto & inst : inst_queue_)
        {
            output << "\t\t" << inst << std::endl;
        }
        output << "\n\trename maps" << std::endl;

        for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
        {
            output << "\t\t" << core_types::regfile_names[reg_file] << ": map table:" << std::endl;
            const auto & reg_array = map_table_[reg_file];
            const auto & ref_counts = regfile_components_[reg_file].reference_counter;
            for (uint32_t reg = 0; reg < NUM_RISCV_REGS_; ++reg)
            {
                const auto & ref_count_ref = ref_counts.at(reg_array[reg]);
                output << "\t\t\t" << std::setw(2) << reg << " -> " << std::setw(3)
                       << reg_array[reg] << " refc: " << std::setw(2) << ref_count_ref.cnt
                       << " prod: " << std::setw(8) << ref_count_ref.producer_id << " ";
                if (ref_count_ref.producer.expired())
                {
                    output << "<inst retired>";
                }
                else
                {
                    output << ref_count_ref.producer.lock();
                }
                output << std::endl;
            }
        }

        // Cumulative counts
        output << "Cumulative reg counts for current uop queue:";
        for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
        {
            output << "\n\t" << reg_file << " "
                   << uop_queue_regcount_data_.cumulative_reg_counts[reg_file];
        }

        output << "\nfree lists";
        for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
        {
            output << "\n\t" << core_types::regfile_names[reg_file] << ":";
            char comma = ' ';
            auto fln = regfile_components_[reg_file].freelist;
            while (false == fln.empty())
            {
                output << comma << fln.front();
                comma = ',';
                fln.pop();
            }
        }
        output << "\nref cnts:";
        for (auto reg_file = 0; reg_file < core_types::RegFile::N_REGFILES; ++reg_file)
        {
            output << "\n\t" << core_types::regfile_names[reg_file] << ":";
            auto ref_cnts = regfile_components_[reg_file].reference_counter;

            for (uint32_t idx = 0; idx < ref_cnts.size(); ++idx)
            {
                output << "\n\t\t" << idx << ": " << ref_cnts[idx].cnt;
            }
        }
        output << std::endl;
    }

    void Rename::dumpDebugContent_(std::ostream & output) const { dumpRenameContent_(output); }

    void Rename::dumpDebugContentHearbeat_()
    {
        dumpRenameContent_(info_logger_);
        ev_debug_rename_.schedule(1);
    }

} // namespace olympia
