// <CPUTopology.cpp> -*- C++ -*-

#include "CPUTopology.hpp"
#include "CoreUtils.hpp"

#include "sparta/utils/SpartaException.hpp"

/**
 * @br0ief Constructor for CPUTopology_1
 */
olympia::CoreTopologySimple::CoreTopologySimple(){

    //! Instantiating units of this topology
    units = {
        {
            "core*",
            "cpu",
            "Core *",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->core_rf
        },
        {
            "flushmanager",
            "cpu.core*",
            "Flush Manager",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->flushmanager_rf
        },
        {
            "fetch",
            "cpu.core*",
            "Fetch Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->fetch_rf
        },
        {
            "decode",
            "cpu.core*",
            "Decode Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->decode_rf
        },
        {
            "rename",
            "cpu.core*",
            "Rename Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->rename_rf
        },
        {
            "dispatch",
            "cpu.core*",
            "Dispatch Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->dispatch_rf
        },
        {
            "execute",
            "cpu.core*",
            "Execution Pipes",
            "execute",
            0,
            &factories->execute_rf
        },
        {
            "dcache",
            "cpu.core*",
            "Data Cache Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->dcache_rf
        },
        {
            "mmu",
            "cpu.core*",
            "MMU Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->mmu_rf
        },
        {
            "tlb",
            "cpu.core*.mmu",
            "TLB Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->tlb_rf,
            true
        },
        {
            "lsu",
            "cpu.core*",
            "Load-Store Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->lsu_rf
        },
        {
            "l2cache",
            "cpu.core*",
            "L2Cache Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->l2cache_rf
        },
        {
            "biu",
            "cpu.core*",
            "Bus Interface Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->biu_rf
        },
        {
            "mss",
            "cpu.core*",
            "Memory Sub-System",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->mss_rf
        },
        {
            "rob",
            "cpu.core*",
            "ROB Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->rob_rf
        },
        {
            "preloader",
            "cpu.core*",
            "Preloader Facility",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->preloader_rf
        },
        {
            "mavis",
            "cpu.core*",  // Each core can have its own decoder
            "Mavis Decoding Functional Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->mavis_rf
        }
    };

    //! Instantiating ports of this topology
    port_connections = {
        {
            "cpu.core*.fetch.ports.out_fetch_queue_write",
            "cpu.core*.decode.ports.in_fetch_queue_write"
        },
        {
            "cpu.core*.fetch.ports.in_fetch_queue_credits",
            "cpu.core*.decode.ports.out_fetch_queue_credits"
        },
        {
            "cpu.core*.decode.ports.out_uop_queue_write",
            "cpu.core*.rename.ports.in_uop_queue_append"
        },
        {
            "cpu.core*.decode.ports.in_uop_queue_credits",
            "cpu.core*.rename.ports.out_uop_queue_credits"
        },
        {
            "cpu.core*.rename.ports.out_dispatch_queue_write",
            "cpu.core*.dispatch.ports.in_dispatch_queue_write"
        },
        {
            "cpu.core*.rename.ports.in_dispatch_queue_credits",
            "cpu.core*.dispatch.ports.out_dispatch_queue_credits"
        },
        {
            "cpu.core*.dispatch.ports.out_lsu_write",
            "cpu.core*.lsu.ports.in_lsu_insts"
        },
        {
            "cpu.core*.dispatch.ports.in_lsu_credits",
            "cpu.core*.lsu.ports.out_lsu_credits"
        },
        {
            "cpu.core*.dispatch.ports.out_reorder_buffer_write",
            "cpu.core*.rob.ports.in_reorder_buffer_write"
        },
        {
            "cpu.core*.dispatch.ports.in_reorder_buffer_credits",
            "cpu.core*.rob.ports.out_reorder_buffer_credits"
        },
        {
            "cpu.core*.lsu.ports.out_cache_lookup_req",
            "cpu.core*.dcache.ports.in_lsu_lookup_req"
        },
        {
            "cpu.core*.dcache.ports.out_lsu_lookup_ack",
            "cpu.core*.lsu.ports.in_cache_lookup_ack"
        },
        {
            "cpu.core*.dcache.ports.out_lsu_lookup_req",
            "cpu.core*.lsu.ports.in_cache_lookup_req"
        },
        {
            "cpu.core*.dcache.ports.out_lsu_free_req",
            "cpu.core*.lsu.ports.in_cache_free_req"
        },
        {
            "cpu.core*.dcache.ports.out_l2cache_req",
            "cpu.core*.l2cache.ports.in_dcache_l2cache_req"
        },
        {
            "cpu.core*.dcache.ports.in_l2cache_ack",
            "cpu.core*.l2cache.ports.out_l2cache_dcache_ack"
        },
        {
            "cpu.core*.dcache.ports.in_l2cache_resp",
            "cpu.core*.l2cache.ports.out_l2cache_dcache_resp"
        },
        {
            "cpu.core*.l2cache.ports.out_l2cache_biu_req",
            "cpu.core*.biu.ports.in_biu_req"
        },
        {
            "cpu.core*.biu.ports.out_biu_ack",
            "cpu.core*.l2cache.ports.in_biu_l2cache_ack"
        },
        {
            "cpu.core*.biu.ports.out_biu_resp",
            "cpu.core*.l2cache.ports.in_biu_l2cache_resp"
        },
        {
            "cpu.core*.lsu.ports.out_mmu_lookup_req",
            "cpu.core*.mmu.ports.in_lsu_lookup_req"
        },
        {
            "cpu.core*.mmu.ports.out_lsu_lookup_ack",
            "cpu.core*.lsu.ports.in_mmu_lookup_ack"
        },
        {
            "cpu.core*.mmu.ports.out_lsu_lookup_req",
            "cpu.core*.lsu.ports.in_mmu_lookup_req"
        },
        {
            "cpu.core*.mmu.ports.out_lsu_free_req",
            "cpu.core*.lsu.ports.in_mmu_free_req"
        },
        {
            "cpu.core*.biu.ports.out_mss_req_sync",
            "cpu.core*.mss.ports.in_mss_req_sync"
        },
        {
            "cpu.core*.biu.ports.in_mss_ack_sync",
            "cpu.core*.mss.ports.out_mss_ack_sync"
        },
        {
            "cpu.core*.rob.ports.out_retire_flush",
            "cpu.core*.flushmanager.ports.in_flush_request"
        },
        {
            "cpu.core*.rob.ports.out_rob_retire_ack",
            "cpu.core*.lsu.ports.in_rob_retire_ack"
        },
        {
            "cpu.core*.rob.ports.out_rob_retire_ack_rename",
            "cpu.core*.rename.ports.in_rename_retire_ack"
        },
        {
            "cpu.core*.flushmanager.ports.out_flush_upper",
            "cpu.core*.dispatch.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_flush_upper",
            "cpu.core*.decode.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_flush_lower",
            "cpu.core*.decode.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_flush_upper",
            "cpu.core*.rename.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_flush_upper",
            "cpu.core*.rob.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_flush_upper",
            "cpu.core*.lsu.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_flush_upper",
            "cpu.core*.fetch.ports.in_fetch_flush_redirect"
        },
        {
            "cpu.core*.flushmanager.ports.out_flush_lower",
            "cpu.core*.fetch.ports.in_fetch_flush_redirect"
        }
    };
}

// Called by CPUFactory
void olympia::CoreTopologySimple::bindTree(sparta::RootTreeNode* root_node)
{
    auto bind_ports =
        [root_node] (const std::string & left, const std::string & right) {
            sparta::bind(root_node->getChildAs<sparta::Port>(left),
                         root_node->getChildAs<sparta::Port>(right));
        };

    // For each core, hook up the Dispatch/FlushManager block to the Execution
    // pipes based on that core's topology.
    for(uint32_t core_num = 0; core_num < num_cores; ++core_num)
    {
        const std::string core_node = "cpu.core"+std::to_string(core_num);
        const auto dispatch_ports     = core_node + ".dispatch.ports";
        const auto flushmanager_ports = core_node + ".flushmanager.ports";

        auto execution_topology =
            olympia::coreutils::getExecutionTopology(root_node->getChild(core_node));
        for (auto exe_unit_pair : execution_topology)
        {
            const auto tgt_name   = exe_unit_pair[0];
            const auto unit_count = exe_unit_pair[1];
            const auto exe_idx    = (unsigned int) std::stoul(unit_count);
            sparta_assert(exe_idx > 0, "Expected more than 0 units! " << tgt_name);
            for(uint32_t unit_num = 0; unit_num < exe_idx; ++unit_num)
            {
                const std::string unit_name = tgt_name + std::to_string(unit_num);

                // Bind credits
                const std::string exe_credits_out =
                    core_node + ".execute." + unit_name + ".ports.out_scheduler_credits";
                const std::string disp_credits_in = dispatch_ports + ".in_" + unit_name + "_credits";
                bind_ports(exe_credits_out, disp_credits_in);

                // Bind instruction transfer
                const std::string exe_inst_in   =
                    core_node + ".execute." + unit_name + ".ports.in_execute_write";
                const std::string disp_inst_out = dispatch_ports + ".out_" + unit_name + "_write";
                bind_ports(exe_inst_in, disp_inst_out);

                // Bind flushing
                const std::string exe_flush_in =
                    core_node + ".execute." +  unit_name + ".ports.in_reorder_flush";;
                const std::string flush_manager = flushmanager_ports + ".out_flush_upper";
                bind_ports(exe_flush_in, flush_manager);

                // Bind flush requests
                const std::string exe_flush_out =
                    core_node + ".execute." +  unit_name + ".ports.out_execute_flush";;
                const std::string flush_manager_in = flushmanager_ports + ".in_flush_request";
                bind_ports(exe_flush_out, flush_manager_in);
            }
        }
    }
}

/**
 * @br0ief Static method to allocate memory for topology
 */
std::unique_ptr<olympia::CPUTopology> olympia::CPUTopology::allocateTopology(const std::string& topology)
{
    std::unique_ptr<CPUTopology> new_topology;
    if(topology == "simple"){
        new_topology.reset(new olympia::CoreTopologySimple());
    }
    else{
        throw sparta::SpartaException("This topology in unrecognized: ") << topology;
    }
    sparta_assert(nullptr != new_topology);
    return new_topology;
}
