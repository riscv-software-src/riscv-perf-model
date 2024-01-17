/**
 * @file   Rename.cpp
 * @brief
 *
 *
 */

#include <algorithm>
#include "CoreUtils.hpp"
#include "Rename.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/report/DatabaseInterface.hpp"

namespace olympia
{
    const char Rename::name[] = "rename";

    Rename::Rename(sparta::TreeNode * node,
                   const RenameParameterSet * p) :
        sparta::Unit(node),
        uop_queue_("rename_uop_queue", p->rename_queue_depth,
                   node->getClock(), getStatisticSet()),
        num_to_rename_per_cycle_(p->num_to_rename),
        rename_histogram_(*getStatisticSet(), "rename_histogram", "Rename Stage Histogram",
                          [&p]() { std::vector<int> v(p->num_to_rename+1); std::iota(v.begin(), v.end(), 0); return v; }())
    {
        uop_queue_.enableCollection(node);

        // The path into the Rename block
        // - Instructions are received on the Uop Queue Append port
        // - Credits arrive on the dispatch queue credits port
        in_uop_queue_append_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Rename, decodedInstructions_, InstGroupPtr));
        in_dispatch_queue_credits_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Rename, creditsDispatchQueue_, uint32_t));
        in_reorder_flush_.
            registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Rename, handleFlush_, FlushManager::FlushingCriteria));
        in_rename_retire_ack_.registerConsumerHandler
                (CREATE_SPARTA_HANDLER_WITH_DATA(Rename, getAckFromROB_, InstPtr));
        sparta::StartupEvent(node, CREATE_SPARTA_HANDLER(Rename, setupRename_));
        auto setup_map = [this] (core_types::RegFile reg_file, const uint32_t num_renames) {
                        const uint32_t num_regs = 32;  // default risc-v ARF count
                        sparta_assert(num_regs < num_renames); // ensure we have more renames than 32 because first 32 renames are allocated at the beginning
                        // initialize the first 32 Float (31 for INT) regs, i.e x1 -> PRF1

                        // for x0 for RF_INTEGER, we don't want to set a PRF for it because x0 is hardwired to 0
                        uint32_t i = 0;
                        if (reg_file == core_types::RegFile::RF_INTEGER){
                            reference_counter_[reg_file].push_back(0);
                            freelist_[reg_file].push(0);
                            i = 1;
                        }
                        for(; i < num_regs; i++){
                            map_table_[reg_file][i] = i;
                            // for the first 32 Float (31 INT) registers, we mark their reference_counters 1, as they are the
                            // current "valid" PRF for that ARF
                            reference_counter_[reg_file].push_back(1);
                        }
                        for(uint32_t j = num_regs; j < num_renames; ++j) {
                            freelist_[reg_file].push(j);
                            reference_counter_[reg_file].push_back(0);
                        }
                    };

        setup_map(core_types::RegFile::RF_INTEGER, p->num_integer_renames);
        setup_map(core_types::RegFile::RF_FLOAT,   p->num_float_renames);

        static_assert(core_types::RegFile::N_REGFILES == 2, "New RF type added, but Rename not updated");
    }

    // Using the Rename factory, create the Scoreboards
    void RenameFactory::onConfiguring(sparta::ResourceTreeNode* node)
    {
        sparta::TreeNode * sb_tn = nullptr;
        sb_tns_.emplace_back(sb_tn =
                             new sparta::TreeNode(node, "scoreboards", "Scoreboards used by Rename"));

        // Set up the Scoreboard resources
        for(uint32_t rf = 0; rf < core_types::RegFile::N_REGFILES; ++rf)
        {
            const auto rf_name = core_types::regfile_names[rf];
            sb_tns_.emplace_back(new sparta::ResourceTreeNode(sb_tn,
                                                              rf_name,
                                                              sparta::TreeNode::GROUP_NAME_NONE,
                                                              sparta::TreeNode::GROUP_IDX_NONE,
                                                              rf_name + std::string(" Scoreboard"),
                                                              &sb_facts_[rf]));
        }
    }

    void Rename::setupRename_()
    {
        // Set up scoreboards
        auto sbs_tn = getContainer()->getChild("scoreboards");
        sparta_assert(sbs_tn != nullptr, "Expected to find 'scoreboards' node in Rename, got none");
        for(uint32_t rf = 0; rf < core_types::RegFile::N_REGFILES; ++rf) {
            // Get scoreboard resources
            auto sb_tn = sbs_tn->getChild(core_types::regfile_names[rf]);
            scoreboards_[rf] = sb_tn->getResourceAs<sparta::Scoreboard*>();

            // Initialize 32 scoreboard resources,
            // all ready.
            uint32_t reg = 0;
            if(rf == core_types::RegFile::RF_INTEGER){
                reg = 1;
            }
            constexpr uint32_t num_regs = 32;
            core_types::RegisterBitMask bits;
            for(; reg < num_regs; ++reg) {
                bits.set(reg);
            }
            scoreboards_[rf]->set(bits);
        }

        // Send the initial credit count
        out_uop_queue_credits_.send(uop_queue_.capacity());
    }

    void Rename::creditsDispatchQueue_(const uint32_t & credits)
    {
        sparta_assert(in_dispatch_queue_credits_.dataReceived());

        credits_dispatch_ += credits;
        if (uop_queue_.size() > 0) {
            ev_schedule_rename_.schedule();
        }
    }
    void Rename::getAckFromROB_(const InstPtr & inst_ptr)
    {
        sparta_assert(inst_ptr->getStatus() == Inst::Status::RETIRED,
                        "Get ROB Ack, but the inst hasn't retired yet!");
        auto const & dests = inst_ptr->getDestOpInfoList();
        if(dests.size() > 0)
        {
            sparta_assert(dests.size() == 1); // we should only have one destination
            const auto dest = dests[0];
            const auto rf  = olympia::coreutils::determineRegisterFile(dest);
            const auto num = dest.field_value;
            const bool is_x0 = (num == 0 && rf == core_types::RF_INTEGER);
            if (!is_x0)
            {
                auto const & original_dest = inst_ptr->getRenameData().getOriginalDestination();
                --reference_counter_[original_dest.rf][original_dest.val];
                // free previous PRF mapping if no references from srcs, there should be a new dest mapping for the ARF -> PRF
                // so we know it's free to be pushed to freelist if it has no other src references
                if(reference_counter_[original_dest.rf][original_dest.val] <= 0){
                    freelist_[original_dest.rf].push(original_dest.val);
                }
            }
        }

        const auto & srcs = inst_ptr->getRenameData().getSourceList();
        // decrement reference to data register
        if(inst_ptr->isLoadStoreInst()){
            const auto & data_reg = inst_ptr->getRenameData().getDataReg();
            if(data_reg.field_id == mavis::InstMetaData::OperandFieldID::RS2 && data_reg.is_x0 != true){
                --reference_counter_[data_reg.rf][data_reg.val];
                if(reference_counter_[data_reg.rf][data_reg.val] <= 0){
                    // freeing data register value, because it's not in the source list, so won't get caught below
                    freelist_[data_reg.rf].push(data_reg.val);
                }
            }
        }
        // freeing references to PRF
        for(const auto & src: srcs){
            --reference_counter_[src.rf][src.val];
            if(reference_counter_[src.rf][src.val] <= 0){
                // freeing a register in the case where it still has references and has already been retired
                // we wait until the last reference is retired to then free the prf
                // any "valid" PRF that is the true mapping of an ARF will have a reference_counter of at least 1,
                // and thus shouldn't be retired
                freelist_[src.rf].push(src.val);
            }
        }
        // Instruction queue bookkeeping
        if (SPARTA_EXPECT_TRUE(!inst_queue_.empty()))
        {
            const auto & oldest_inst = inst_queue_.front();
            sparta_assert(oldest_inst->getUniqueID() == inst_ptr->getUniqueID(), "ROB and rename inst_queue out of sync");
            inst_queue_.pop_front();
        }
        else
        {
            sparta_assert(false, "ROB and rename inst_queue out of sync");
        }
        if(credits_dispatch_ > 0 && (uop_queue_.size() > 0)){
            ev_schedule_rename_.schedule();
        }
        ILOG("Retired instruction: " << inst_ptr);
    }


    // Handle incoming flush
    void Rename::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Got a flush call for " << criteria);
        // Restore the rename map, reference counters and freelist by walking through the inst_queue
        while (inst_queue_.size())
        {
            const auto & inst_ptr = inst_queue_.back();
            if (!criteria.includedInFlush(inst_ptr))
            {
                break;
            }
            else
            {
                auto const & dests = inst_ptr->getDestOpInfoList();
                for (const auto & dest : dests)
                {
                    // restore rename table following a flush
                    const auto rf  = olympia::coreutils::determineRegisterFile(dest);
                    const auto num = dest.field_value;
                    const bool is_x0 = (num == 0 && rf == core_types::RF_INTEGER);
                    if (!is_x0)
                    {
                        auto const & original_dest = inst_ptr->getRenameData().getOriginalDestination();
                        map_table_[rf][num] = original_dest.val;

                        // free renamed PRF mapping when reference counter reaches zero
                        auto const & renamed_dest = inst_ptr->getRenameData().getDestination();
                        --reference_counter_[renamed_dest.rf][renamed_dest.val];
                        if(reference_counter_[renamed_dest.rf][renamed_dest.val] <= 0){
                            freelist_[renamed_dest.rf].push(renamed_dest.val);
                        }
                    }
                }

                const auto & srcs = inst_ptr->getRenameData().getSourceList();
                // decrement reference to data register
                if(inst_ptr->isLoadStoreInst()){
                    const auto & data_reg = inst_ptr->getRenameData().getDataReg();
                    if(data_reg.field_id == mavis::InstMetaData::OperandFieldID::RS2 && data_reg.is_x0 != true){
                        --reference_counter_[data_reg.rf][data_reg.val];
                        if(reference_counter_[data_reg.rf][data_reg.val] <= 0){
                            // freeing data register value, because it's not in the source list, so won't get caught below
                            freelist_[data_reg.rf].push(data_reg.val);
                        }
                    }
                }
                // freeing references to PRF
                for(const auto & src: srcs){
                    --reference_counter_[src.rf][src.val];
                    if(reference_counter_[src.rf][src.val] <= 0){
                        // freeing a register in the case where it still has references and has already been retired
                        // we wait until the last reference is retired to then free the prf
                        // any "valid" PRF that is the true mapping of an ARF will have a reference_counter of at least 1,
                        // and thus shouldn't be retired
                        freelist_[src.rf].push(src.val);
                    }
                }
                inst_queue_.pop_back();
            }
        }

        current_stall_ = NO_DECODE_INSTS;

        // Clean up buffers
        uop_queue_regcount_data_.clear();
        out_uop_queue_credits_.send(uop_queue_.size());
        uop_queue_.clear();
    }

    void Rename::decodedInstructions_(const InstGroupPtr & insts)
    {
        sparta_assert(in_uop_queue_append_.dataReceived());

        RegCountData current_counts;
        if(!uop_queue_regcount_data_.empty()){
            // if we have entries, use the most recent entered instruction counts
            current_counts = uop_queue_regcount_data_.back();
        }
        for(auto & i : *insts) {
            // create an index count for each instruction entered
            const auto & dests = i->getDestOpInfoList();
            if(dests.size() > 0){
                sparta_assert(dests.size() == 1); // we should only have one destination
                const auto rf = olympia::coreutils::determineRegisterFile(dests[0]);
                const auto num = dests[0].field_value;
                const bool is_x0 = (num == 0 && rf == core_types::RF_INTEGER);
                // if dest is x0, we don't need to count it towards cumulative register count
                if(!is_x0){
                    current_counts.cumulative_reg_counts[rf]++;
                }
            }
            uop_queue_.push(i);
            uop_queue_regcount_data_.push_back(current_counts);
        }

        if(credits_dispatch_ > 0) {
            ev_schedule_rename_.schedule();
        }
    }
    void Rename::scheduleRenaming_()
    {
        current_stall_ = StallReason::NOT_STALLED;

        // If we have credits from dispatch, schedule a rename session this cycle
        uint32_t num_rename = std::min(uop_queue_.size(), num_to_rename_per_cycle_);
        num_rename = std::min(credits_dispatch_, num_rename);
        if(credits_dispatch_ > 0)
        {
            RegCountData count_subtract;
            bool enough_rename = false;
            for(uint32_t i = num_rename; i > 0 ; --i)
            {
                if(enough_rename){
                    // once we know the number we can rename
                    // pop everything below it
                    uop_queue_regcount_data_.pop_front();
                }
                else{
                    uint32_t enough_freelists = 0;
                    for(int j = 0; j < core_types::RegFile::N_REGFILES; ++j){
                        if(uop_queue_regcount_data_[i-1].cumulative_reg_counts[j] <= freelist_[j].size()){
                            enough_freelists++;
                        }
                    }
                    if(enough_freelists == core_types::RegFile::N_REGFILES){
                        // we are good to process all instructions from this index
                        num_to_rename_ = i;
                        enough_rename = true;
                    }
                }
            }
            // decrement the rest of the entries in the uop_queue_reg_count_data_ accordingly
            if(enough_rename)
            {
                count_subtract = uop_queue_regcount_data_.front();
                uop_queue_regcount_data_.pop_front();
                for(uint32_t i = 0; i < uop_queue_regcount_data_.size(); ++i){
                    for(uint32_t j = 0; j < core_types::RegFile::N_REGFILES; ++j)
                    {
                        uop_queue_regcount_data_[i].cumulative_reg_counts[j] -=
                            count_subtract.cumulative_reg_counts[j];
                    }
                }
                ev_rename_insts_.schedule();
            }
            else{
                current_stall_ = StallReason::NO_RENAMES;
                num_to_rename_ = 0;
            }
        }
        else{
            current_stall_ = StallReason::NO_DISPATCH_CREDITS;
            num_to_rename_ = 0;
        }
        ILOG("current stall: " << current_stall_);

        rename_histogram_.addValue((int) num_to_rename_);
    }
    void Rename::renameInstructions_()
    {
        if(num_to_rename_ > 0)
        {
            // Pick instructions from uop queue to rename
            InstGroupPtr insts = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);

            for(uint32_t i = 0; i < num_to_rename_; ++i)
            {
                // Pick the oldest
                const auto & renaming_inst = uop_queue_.read(0);
                renaming_inst->setStatus(Inst::Status::RENAMED);
                ILOG("sending inst to dispatch: " << renaming_inst);

                const auto & srcs = renaming_inst->getSourceOpInfoList();
                const auto & dests = renaming_inst->getDestOpInfoList();
                for(const auto & src : srcs)
                {
                    const auto rf  = olympia::coreutils::determineRegisterFile(src);
                    const auto num = src.field_value;
                    // we check if src is RF_INTEGER x0, if so, we skip rename
                    const bool is_x0 = (num == 0 && rf == core_types::RF_INTEGER);
                    if (is_x0) {
                        // if x0 is a data operand for LSU op, we need to set it in DataReg when we check in LSU
                        // so we can still check the scoreboard, which will always return back ready for x0
                        if(src.field_id == mavis::InstMetaData::OperandFieldID::RS2){
                            renaming_inst->getRenameData().setDataReg({num, rf, src.field_id, is_x0});
                        }
                        continue;
                    }
                    // we check for load/store separately because address operand
                    // is always integer
                    else if(renaming_inst->isLoadStoreInst()) {
                        // check for data operand existing based on RS2 existence
                        // store data register info separately
                        if(src.field_id == mavis::InstMetaData::OperandFieldID::RS2) {
                            auto & bitmask = renaming_inst->getDataRegisterBitMask(rf);
                            const uint32_t prf = map_table_[rf][num];
                            reference_counter_[rf][prf]++;
                            renaming_inst->getRenameData().setDataReg({prf, rf, src.field_id, is_x0});
                            bitmask.set(prf);

                            ILOG("\tsetup store data register bit mask "
                                << sparta::printBitSet(bitmask)
                                << " for '" << rf << "' scoreboard");
                        }
                        else {
                            // address is always INTEGER
                            const auto addr_rf  = core_types::RF_INTEGER;
                            auto & bitmask = renaming_inst->getSrcRegisterBitMask(addr_rf);
                            const uint32_t prf = map_table_[addr_rf][num];
                            reference_counter_[addr_rf][prf]++;
                            renaming_inst->getRenameData().setSource({prf, addr_rf, src.field_id});
                            bitmask.set(prf);

                            ILOG("\tsetup source register bit mask "
                                << sparta::printBitSet(bitmask)
                                << " for '" << addr_rf << "' scoreboard");
                        }
                    }
                    else {
                        auto & bitmask = renaming_inst->getSrcRegisterBitMask(rf);
                        const uint32_t prf = map_table_[rf][num];
                        reference_counter_[rf][prf]++;
                        renaming_inst->getRenameData().setSource({prf, rf, src.field_id});
                        bitmask.set(prf);

                        ILOG("\tsetup source register bit mask "
                            << sparta::printBitSet(bitmask)
                            << " for '" << rf << "' scoreboard");
                    }
                }

                for(const auto & dest : dests)
                {
                    const auto rf  = olympia::coreutils::determineRegisterFile(dest);
                    const auto num = dest.field_value;
                    if (num == 0 && rf == core_types::RF_INTEGER) {
                        continue;
                    }
                    else{
                        auto & bitmask = renaming_inst->getDestRegisterBitMask(rf);
                        const uint32_t prf = freelist_[rf].front();
                        freelist_[rf].pop();
                        renaming_inst->getRenameData().setOriginalDestination({map_table_[rf][num], rf, dest.field_id});
                        renaming_inst->getRenameData().setDestination({prf, rf, dest.field_id});
                        map_table_[rf][num] = prf;
                        // we increase reference_counter_ for destinations to mark them as "valid",
                        // so the PRF in the reference_counter_ should have a value of 1
                        // once a PRF reference_counter goes to 0, we know that the PRF isn't the "valid"
                        // PRF for that ARF anymore and there are no sources referring to it
                        // so we can push it to freelist
                        reference_counter_[rf][prf]++;
                        bitmask.set(prf);
                        // clear scoreboard for the PRF we are allocating
                        scoreboards_[rf]->clearBits(bitmask);
                        ILOG("\tsetup destination register bit mask "
                            << sparta::printBitSet(bitmask)
                            << " for '" << rf << "' scoreboard");
                    }
                }
                // Remove it from uop queue
                insts->emplace_back(renaming_inst);
                inst_queue_.emplace_back(renaming_inst);
                uop_queue_.pop();
            }

            // Send insts to dispatch
            out_dispatch_queue_write_.send(insts);
            credits_dispatch_ -= num_to_rename_;

            // Replenish credits in the Decode unit
            out_uop_queue_credits_.send(num_to_rename_);
            num_to_rename_ = 0;
        }
        if (credits_dispatch_ > 0 && (uop_queue_.size() > 0)) {
            ev_schedule_rename_.schedule(1);
        }
    }
}