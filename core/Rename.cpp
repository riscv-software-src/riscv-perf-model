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
#include "mavis/ExtractorDirectInfo.h"
namespace olympia
{
    inline core_types::RegFile determineRegisterFile(const mavis::OperandInfo::Element & reg)
    {
        const bool is_float = (reg.operand_type == mavis::InstMetaData::OperandTypes::SINGLE) ||
                              (reg.operand_type == mavis::InstMetaData::OperandTypes::DOUBLE);
        return is_float ? core_types::RegFile::RF_FLOAT : core_types::RegFile::RF_INTEGER;
    }

    const char Rename::name[] = "rename";

    int free_insts = 0;
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

        // initialize freelist
        for(unsigned int i = 0; i < (int) sparta::Scoreboard::MAX_REGISTERS; i++){
            freelist[core_types::RegFile::RF_INTEGER].push(i); // INTEGER
            freelist[core_types::RegFile::RF_FLOAT].push(i); // FLOAT
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

        core_types::RegFile rf;
        auto dests = inst_ptr->getDestOpInfoList();
        if(dests.size() > 0){
            rf = determineRegisterFile(dests[0]);   
        }
        else{
            // in the case of no dest register, it will be a NOP
            rf = core_types::RegFile::RF_INTEGER;
        }
        
        // freeing references to PRF
        auto dest_register_bit_mask = inst_ptr->getDestRegisterBitMask(rf);
        auto src_register_bit_mask = inst_ptr->getSrcRegisterBitMask(rf);
        for(unsigned int i = 0; i < sparta::Scoreboard::MAX_REGISTERS; i++){
            if(dest_register_bit_mask.test(i)){
                if(reference_counter[rf][i] <= 0){
                    // register is free now
                    freelist[rf].push(i);
                    free_insts++;
                }
                // popping the front of the map table because the instruction is now retired
                // we don't want subsequent instructions to reference a retired PRF 
                map_table[rf][reverse_map_table[rf][i].front()].pop();
                reverse_map_table[rf][i].pop();
            }
            if(src_register_bit_mask.test(i)){
                // we only free the register if this reference is the last one
                if(src_map_table[rf][i].size() == 0){
                    --reference_counter[rf][i];
                    if(reference_counter[rf][i] <= 0){
                        // freeing a register in the case where it has references
                        // we wait until the last reference is retired to then free the prf
                        freelist[rf].push(i);
                        free_insts++;
                    }
                }
                else{
                    src_map_table[rf][i].pop();
                }
            }
        }
        if(freelist[rf].size() > 512){
            freelist[rf].size();
        }
        ILOG("Get Ack from ROB in Rename Stage! Retired store instruction: " << inst_ptr);
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
        unsigned int rf_counters[2] = {0, 0};
        for(uint32_t i = 0; i < num_rename; ++i)
        {
            const auto & renaming_inst = uop_queue_.read(i);
            const auto & dests = renaming_inst->getDestOpInfoList();
            for(const auto & dest : dests)
            {
                const auto rf  = determineRegisterFile(dest);
                rf_counters[rf]++;
            }
        }
        if (credits_dispatch_ > 0 && (freelist[core_types::RegFile::RF_FLOAT].size() >= rf_counters[core_types::RegFile::RF_FLOAT] && freelist[core_types::RegFile::RF_INTEGER].size() >= rf_counters[core_types::RegFile::RF_INTEGER])) {
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
                for(const auto & src : srcs)
                {
                    const auto rf  = determineRegisterFile(src);
                    const auto num = src.field_value;
                    auto & bitmask = renaming_inst->getSrcRegisterBitMask(rf);
                    unsigned int prf;

                    if(map_table[rf][num].size() == 0){
                        prf = num;
                        // we store the mapping, so when processing the retired instruction,
                        // we know that there was no real mapping
                        src_map_table[rf][num].push(prf);
                    }
                    else{
                        prf = map_table[rf][num].back();
                        // only increase reference counter for real references
                        reference_counter[rf][prf]++;
                    }
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
                    
                    unsigned int prf = freelist[rf].front();
                    freelist[rf].pop();
                    map_table[rf][num].push(prf);
                    // reverse map table is used for mapping the PRF -> ARF
                    // so when we get a retired instruction, we know to pop the map table entry
                    reverse_map_table[rf][prf].push(num);
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