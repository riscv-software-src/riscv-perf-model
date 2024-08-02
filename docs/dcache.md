# DCACHE

### Ports


in_lsu_lookup_req  <-- Input from LSU ( Receive memory request )

in_l2cache_ack     <-- Input from L2Cache ( Receive acknowledgement from cache )

in_l2cache_resp    <-- Input from L2Cache ( Receive data for the l2 cache lookup request)

out_lsu_free_req   <-- Output to LSU ( Send cache available for requests signal )

out_lsu_lookup_ack <-- Output to LSU ( Send acknowledgement for the memory reques to the LSU )

out_lsu_lookup_req <-- Output to LSU ( Send data for the LSU memory request ) 

out_l2cache_req    <-- Output to L2Cache ( Send dirty cacheline to L2cache )

### Configuration parameters

`l1_line_size`     - Size of the DL1 cache line (power of 2)

`l1_size_kb`       - Size of DL1 in KB (power of 2)

`l1_associativity` - DL1 associativity (power of 2)

`l1_always_hit`    - DL1 will always hit

`mshr_entries`     - Number of MSHR Entries

### Available counters
`dl1_cache_hits`     - Number of DL1 cache hits

`dl1_cache_misses`   - Number of DL1 cache misses

`dl1_hit_miss_ratio` - Ratio between the DL1 HIT/MISS

### Microarchitecture
The dcache has configurable number of mshr entries to handle requests in a non blocking manner.

The Dcache arbitrates requests from LSU and cache refill response from L2 Cache.
The Dcache prioritizes cache refill request over incoming memory requests from the LSU. 

The Dcache has one only pipeline with 3 different stages

| Stage      | Cache Refill                                    | Memory lookup request   |
|------------|-------------------------------------------------|-------------------------|
| LOOKUP     | Do nothing                                      | Create MSHR entry       |
| DATA READ  | Update cache with refill information            | Send L2 request if miss |
| DEALLOCATE | Deallocate mshr entries linked to the cacheline | Do nothing              |
