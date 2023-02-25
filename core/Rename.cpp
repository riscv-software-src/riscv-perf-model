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
        for(const auto & dest : dests){
            rf = determineRegisterFile(dest);
        }
        
        auto dest_register_bit_mask = inst_ptr->getDestRegisterBitMask(rf);
        for(unsigned int i = 0; i < sparta::Scoreboard::MAX_REGISTERS; i++){
            if(dest_register_bit_mask.test(i)){
                if(reference_counter[rf][i] <= 0){
                    // register is free now
                    freelist[rf].push(i);
                }
            }
        }

        // freeing references to PRF
        auto src_register_bit_mask = inst_ptr->getSrcRegisterBitMask(rf);
        for(unsigned int i = 0; i < sparta::Scoreboard::MAX_REGISTERS; i++){
            if(src_register_bit_mask.test(i) && reference_counter[rf][i] > 0){
                // we only free the register if this reference is the last one
                --reference_counter[rf][i];
                if(reference_counter[rf][i] <= 0){
                    // freeing a register in the case where it has references
                    // we wait until the last reference is retired to then free the prf
                    freelist[rf].push(i);
                }
                ILOG("Decrementing reference counter on prf " << i << " ");
            }
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

        // If we have credits from dispatch, schedule a rename session
        // this cycle
        if (credits_dispatch_ > 0) {
            ev_rename_insts_.schedule();
        }
    }

    void Rename::renameInstructions_()
    {
        uint32_t num_rename = std::min(uop_queue_.size(), num_to_rename_per_cycle_);
        num_rename = std::min(credits_dispatch_, num_rename);
        bool freelist_full = false;

        if(num_rename > 0)
        {
            // Pick instructions from uop queue to rename
            InstGroupPtr insts = sparta::allocate_sparta_shared_pointer<InstGroup>(instgroup_allocator);
            std::queue<std::pair<int, unsigned int>> prfs_popped;

            //see if there will be enough prfs for renaming
            for(uint32_t i = 0; i < num_rename; ++i)
            {
                // Pick the oldest
                const auto & renaming_inst = uop_queue_.read(i);

                // TODO: Register renaming for destinations
                const auto & dests = renaming_inst->getDestOpInfoList();
                int count = 0;
                for(const auto & dest : dests)
                {
                    count++;
                    const auto rf  = determineRegisterFile(dest);
                    if(freelist[rf].empty()){
                        // freelist is full
                        freelist_full = true;
                        break; // stop processing this instruction group
                    } else{
                        unsigned int prf = freelist[rf].front();
                        freelist[rf].pop();
                        prfs_popped.push(std::make_pair(rf, prf));
                    }
                    
                }
                if(freelist_full){
                    break;
                }
            }
            if(freelist_full){
                // restore freelist to original state, as we are not going to process the instruction group yet.
                while(!prfs_popped.empty()){
                    freelist[prfs_popped.front().first].push(prfs_popped.front().second);
                    prfs_popped.pop();
                }
            }
            else{
                // we have enough renames to process this instruction group
                for(uint32_t i = 0; i < num_rename; ++i)
                {
                    // Pick the oldest
                    const auto & renaming_inst = uop_queue_.read(0);
                    ILOG("sending inst to dispatch: " << renaming_inst);

                    // TODO: Register renaming for destinations
                    const auto & dests = renaming_inst->getDestOpInfoList();
                    for(const auto & dest : dests)
                    {
                        const auto rf  = determineRegisterFile(dest);
                        const auto num = dest.field_value;
                        auto & bitmask = renaming_inst->getDestRegisterBitMask(rf);
                        
                        // we already defined a prf register, so we can just use the original value
                        unsigned int prf = prfs_popped.front().second;
                        prfs_popped.pop();
                        map_table[rf][num].push(prf);
                        bitmask.set(prf);
                        scoreboards_[rf]->set(bitmask);
                        ILOG("\tsetup destination register bit mask "
                        << sparta::printBitSet(bitmask)
                        << " for '" << rf << "' scoreboard");
                    }

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
                        }
                        else{
                            prf = map_table[rf][num].back();
                        }
                        reference_counter[rf][prf]++;
                        ILOG("Increasing reference counter on prf " << prf << " and rf " << (int) rf);
                        bitmask.set(prf);
                        scoreboards_[rf]->set(bitmask);
                        ILOG("\tsetup source register bit mask "
                            << sparta::printBitSet(bitmask)
                            << " for '" << rf << "' scoreboard");
                    }

                    // Remove it from uop queue
                    insts->emplace_back(uop_queue_.read(0));
                    uop_queue_.pop();
                }   
            }

            if(!freelist_full){
                // Send insts to dispatch
                out_dispatch_queue_write_.send(insts);
                credits_dispatch_ -= num_rename;

                // Replenish credits in the Decode unit             
                out_uop_queue_credits_.send(num_rename);
            }
        }

        if (credits_dispatch_ > 0 && (uop_queue_.size() > 0)) {
            ev_rename_insts_.schedule(1);
        }
    }

}