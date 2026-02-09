# LSU
The design of the LSU is loosely based on the LSU implementation in [BOOM](https://docs.boom-core.org/en/latest/sections/load-store-unit.html) microarchitecture and the [E500](https://www.nxp.com/docs/en/reference-manual/E500CORERM.pdf) core.

### Ports
in_lsu_insts         <-- Input from Dispatch (Receive new memory instructions)

in_cache_lookup_ack  <-- Input from DCache (Receive acknowledgement from cache if the data is present)

in_cache_lookup_req  <-- Input from DCache (Receive data for the cache lookup request)

in_cache_free_req    <-- Input from DCache (Cache lets the lsu know that its ready to accept new lookup requests)

out_cache_lookup_req --> Output to DCache (Send a cache lookup request for a particular address)

in_mmu_lookup_ack    <-- Input from MMU (Receive acknowledgement from MMU  of particular virtual address lookup)

in_mmu_lookup_req    <-- Input from MMU (Receive physical address for the virtual address lookup request)

out_mmu_lookup_req   --> Output to DCache (Send a VA to PA address translation request)


### Configuration Parameters
`ldst_inst_queue_size` - Size of the LSU instruction queue

`allow_speculative_load_exec` - Allow loads to proceed speculatively before all older store addresses are known.

`allow_data_forwarding` - Allow loads to get data from store instead of cache, by pass mem look up.

`replay_buffer_size` - Size of the replay buffer. Defaults to the same size of the LSU instruction queue.

`replay_issue_delay` - Delay in cycles to replay the instruction.

`mmu_lookup_stage_length` - Number of cycles to complete a MMU lookup stage.

`cache_lookup_stage_length` - Number of cycles to complete a Cache lookup stage.

`cache_read_stage_length` - Number of cycles to complete a Cache read stage

### Available counters
`lsu_insts_dispatched` - Number of LSU instructions dispatched

`stores_retired` - Number of stores retired

`lsu_insts_issued` - Number of LSU instructions issued

`replay_insts`  - Number of Replay instructions issued

`lsu_insts_completed` - Number of LSU instructions completed

`lsu_flushes` - Number of instruction flushes at LSU

### Microarchitecture
The Load Unit currently has only one pipeline. It has five distinct stages.The instructions always flow through the pipeline in the order mentioned below.

- ADDRESS_CALCULATION - The virtual address is calculated
- MMU_LOOKUP - Translation of the VA to PA
- CACHE_LOOKUP - Lookup data for the PA present in the cache
- CACHE_READ - Receive data from the cache
- COMPLETION - Final stage of the pipeline to cleanup and deallocate instructions

If the MMU or CACHE responds with a miss for the lookup requests, the instruction is removed from the pipeline.Once the units respond with an `ack`, the instruction is marked as ready to added back into the pipeline.

The completion stage is responsible to remove the instruction from the issue queue once the instruction has completed executing.

##### Typical Flow
```
handleOperandIssueCheck_ -> 
    appendToReadyQueue_() -> 
        issueInst_() ->
            handleAddressCalculation_() ->
                handleMMULookupReq_() ->
                    handleCacheLookupReq_() ->
                        handleCacheRead_() ->
                            completeInst_()
```
#### Speculative Execution
By default the LSU operates with `allow_speculative_execution`, which allows loads to proceed even if older load address is not know.

The replay queue is used to store the instruction if the instruction has a corresponding miss from the mmu or cache.
The `replay_delay` specifies the delay until the request which was present in the replay queue needs to be added back into the pipeline.

Speculated Load instructions are removed from the pipeline if an older store receives its PA or there are existing older stores that are waiting in the queue, this prevents the load store hazard.

---
### Others
The LSU contains a virtual queue called the `ready_queue` to hold instructions which are ready to be pushed into the LSU pipeline.This queue is model specific queue and doesnt affect the microarchitecture of the LSU.Its used to reduce quering the LSU's instruction queue for a potentially ready instruction.
It is based on a priority queue which sorts orders based on the age of the instruction.
