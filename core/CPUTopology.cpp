// <CPUTopology.cpp> -*- C++ -*-

#include "CPUTopology.hpp"
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
            "lsu",
            "cpu.core*",
            "Load-Store Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->lsu_rf
        },
        {
            "tlb",
            "cpu.core*.lsu",
            "TLB Unit",
            sparta::TreeNode::GROUP_NAME_NONE,
            sparta::TreeNode::GROUP_IDX_NONE,
            &factories->tlb_rf,
            true
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
            "cpu.core*.lsu.ports.out_biu_req",
            "cpu.core*.biu.ports.in_biu_req"
        },
        {
            "cpu.core*.lsu.ports.in_biu_ack",
            "cpu.core*.biu.ports.out_biu_ack"
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
            "cpu.core*.flushmanager.ports.in_retire_flush"
        },
        {
            "cpu.core*.rob.ports.out_fetch_flush_redirect",
            "cpu.core*.flushmanager.ports.in_fetch_flush_redirect"
        },
        {
            "cpu.core*.rob.ports.out_rob_retire_ack",
            "cpu.core*.lsu.ports.in_rob_retire_ack"
        },
        {
            "cpu.core*.flushmanager.ports.out_retire_flush",
            "cpu.core*.dispatch.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_retire_flush",
            "cpu.core*.decode.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_retire_flush",
            "cpu.core*.rename.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_retire_flush",
            "cpu.core*.rob.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_retire_flush",
            "cpu.core*.lsu.ports.in_reorder_flush"
        },
        {
            "cpu.core*.flushmanager.ports.out_fetch_flush_redirect",
            "cpu.core*.fetch.ports.in_fetch_flush_redirect"
        }
    };
}

/**
 * @br0ief Static method to allocate memory for topology
 */
std::unique_ptr<olympia::CPUTopology> olympia::CPUTopology::allocateTopology(const std::string& topology)
{
    std::unique_ptr<CPUTopology> new_topology;
    if(topology == "core_topology_simple"){
        new_topology.reset(new olympia::CoreTopologySimple());
    }
    else{
        throw sparta::SpartaException("This topology in unrecognized.");
    }
    sparta_assert(nullptr != new_topology);
    return new_topology;
}
