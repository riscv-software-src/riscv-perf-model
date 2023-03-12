/**
 * @file   Rename.cpp
 * @brief
 *
 *
 */

#include <algorithm>
#include "Rename.hpp"
#include "sparta/events/StartupEvent.hpp"
#include "sparta/app/FeatureConfiguration.hpp"
#include "sparta/report/DatabaseInterface.hpp"

namespace olympia
{
    inline core_types::RegFile determineRegisterFile(const mavis::OperandInfo::Element & reg)
    {
        const bool is_float = (reg.operand_type == mavis::InstMetaData::OperandTypes::SINGLE) ||
                              (reg.operand_type == mavis::InstMetaData::OperandTypes::DOUBLE);
        return is_float ? core_types::RegFile::RF_FLOAT : core_types::RegFile::RF_INTEGER;
    }

    const char Rename::name[] = "rename";

    Rename::Rename(sparta::TreeNode * node,
                   const RenameParameterSet * p) :
        sparta::Unit(node),
        uop_queue_("rename_uop_queue", p->rename_queue_depth,
                   node->getClock(), getStatisticSet()),
        num_to_rename_per_cycle_(p->num_to_rename)
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
                        for(uint32_t i = 0; i < num_renames; ++i) {
                            freelist_[reg_file].emplace(i);
                        }
                        map_table_[reg_file].reset(new int32_t[num_renames]());
                        for(uint32_t i = 0; i < num_renames; i++){
                            // mark all map_table values as invalid
                            map_table_[reg_file][i] = -1;
                        }
                        reference_counter_[reg_file].reset(new int32_t[num_renames]());
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

            // Initialize scoreboard resources, make 32 registers
            // all ready.
            constexpr uint32_t num_regs = 32;
            core_types::RegisterBitMask bits;
            for(uint32_t reg = 0; reg < num_regs; ++reg) {
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
            ev_rename_insts_.schedule();
        }
    }
    void Rename::getAckFromROB_(const InstPtr & inst_ptr)
    {
        sparta_assert(inst_ptr->getStatus() == Inst::Status::RETIRED,
                        "Get ROB Ack, but the inst hasn't retired yet!");

        core_types::RegFile rf = core_types::RegFile::RF_INTEGER;
        auto const & dests = inst_ptr->getDestOpInfoList();
        if(dests.size() > 0){
            sparta_assert(dests.size() == 1); // we should only have one destination
            rf = determineRegisterFile(dests[0]);   
        }

        auto const & dest = inst_ptr->getRenameData().getDestination();
        auto const & original_dest = inst_ptr->getRenameData().getOriginalDestination();
        auto const & srcs = inst_ptr->getRenameData().getSource();

        if(dests.size() > 0){
            if(reference_counter_[rf][dest] <= 0){
                freelist_[rf].push(dest);
            }
            map_table_[rf][original_dest] = -1;
        }
        
        // freeing references to PRF
        for(auto src: srcs){
            if(src.second == false){
                --reference_counter_[rf][src.first];
                if(reference_counter_[rf][src.first] <= 0){
                    // freeing a register in the case where it has references
                    // we wait until the last reference is retired to then free the prf
                    freelist_[rf].push(src.first);
                }
            }
        }
        ILOG("Get Ack from ROB in Rename Stage! Retired instruction: " << inst_ptr);
    }

    
    // Handle incoming flush
    void Rename::handleFlush_(const FlushManager::FlushingCriteria & criteria)
    {
        ILOG("Got a flush call for " << criteria);
        out_uop_queue_credits_.send(uop_queue_.size());
        uop_queue_.clear();
    }

    void Rename::decodedInstructions_(const InstGroupPtr & insts)
    {
        sparta_assert(in_uop_queue_append_.dataReceived());

        for(auto & i : *insts) {
            uop_queue_.push(i);
        }

        // If we have credits from dispatch, schedule a rename session this cycle
        uint32_t num_rename = std::min(uop_queue_.size(), num_to_rename_per_cycle_);
        num_rename = std::min(credits_dispatch_, num_rename);
        uint32_t rf_counters[core_types::N_REGFILES] = {};
        for(uint32_t i = 0; i < num_rename; ++i)
        {
            const auto & renaming_inst = uop_queue_.read(i);
            const auto & dests = renaming_inst->getDestOpInfoList();
            if(dests.size() > 0){
                sparta_assert(dests.size() == 1); // we should only have one destination
                const auto rf = determineRegisterFile(dests[0]);  
                rf_counters[rf]++; 
            }
        }

        int num_valid_freelist_ = 0;
        for(int i = 0; i < core_types::N_REGFILES; ++i){
            if(freelist_[i].size() >= rf_counters[i]){
                ++num_valid_freelist_;
            }
        }
        if(credits_dispatch_ > 0 && num_valid_freelist_ == core_types::N_REGFILES) {
            ev_rename_insts_.schedule();
        }
    }

    void Rename::renameInstructions_()
    {
        uint32_t num_rename = std::min(uop_queue_.size(), num_to_rename_per_cycle_);
        num_rename = std::min(credits_dispatch_, num_rename);

        if(num_rename > 0)
        {
            // Pick instructions from uop queue to rename
            InstGroupPtr insts = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);


            for(uint32_t i = 0; i < num_rename; ++i)
            {
                // Pick the oldest
                const auto & renaming_inst = uop_queue_.read(0);
                ILOG("sending inst to dispatch: " << renaming_inst);

                // TODO: Register renaming for sources
                const auto & srcs = renaming_inst->getSourceOpInfoList();
                if(srcs.size() > 1){
                    int i = 0;
                    i++;
                }
                for(const auto & src : srcs)
                {
                    const auto rf  = determineRegisterFile(src);
                    const auto num = src.field_value;
                    auto & bitmask = renaming_inst->getSrcRegisterBitMask(rf);
                    uint32_t prf;

                    bool no_prf = false;
                    if(map_table_[rf][num] < 0){
                        prf = num;
                        // we store the mapping, so when processing the retired instruction,
                        // we know that there was no real mapping, so we mark it as no_prf
                        no_prf = true;
                    }
                    else{
                        prf = (uint32_t) map_table_[rf][num];
                        // only increase reference counter for real references
                        reference_counter_[rf][prf]++;
                    }
                    olympia::Inst::RenameData::SourceReg rename_data_src(prf, no_prf);
                    renaming_inst->getRenameData().setSource(rename_data_src);
                    bitmask.set(prf);
                    scoreboards_[rf]->set(bitmask);
                    ILOG("\tsetup source register bit mask "
                        << sparta::printBitSet(bitmask)
                        << " for '" << rf << "' scoreboard");
                }

                // TODO: Register renaming for destinations
                const auto & dests = renaming_inst->getDestOpInfoList();
                for(const auto & dest : dests)
                {
                    const auto rf  = determineRegisterFile(dest);
                    const auto num = dest.field_value;
                    auto & bitmask = renaming_inst->getDestRegisterBitMask(rf);
                    
                    uint32_t prf = freelist_[rf].front();
                    freelist_[rf].pop();
                    map_table_[rf][num] = (int) prf;
                    // reverse map table is used for mapping the PRF -> ARF
                    // so when we get a retired instruction, we know to pop the map table entry

                    renaming_inst->getRenameData().setDestination(prf);
                    renaming_inst->getRenameData().setOriginalDestination(num);
                    bitmask.set(prf);
                    scoreboards_[rf]->set(bitmask);
                    ILOG("\tsetup destination register bit mask "
                    << sparta::printBitSet(bitmask)
                    << " for '" << rf << "' scoreboard");
                }

                // Remove it from uop queue
                insts->emplace_back(uop_queue_.read(0));
                uop_queue_.pop();
            }   

            // Send insts to dispatch
            out_dispatch_queue_write_.send(insts);
            credits_dispatch_ -= num_rename;

            // Replenish credits in the Decode unit             
            out_uop_queue_credits_.send(num_rename);
        }

        if (credits_dispatch_ > 0 && (uop_queue_.size() > 0)) {
            ev_rename_insts_.schedule(1);
        }
    }

}