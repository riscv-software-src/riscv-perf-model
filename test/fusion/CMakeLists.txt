project(olympia_test)
sparta_regress (olympia)

file(CREATE_LINK ${SIM_BASE}/reports 
                 ${CMAKE_CURRENT_BINARY_DIR}/reports SYMBOLIC)

file(CREATE_LINK ${SIM_BASE}/arches
                 ${CMAKE_CURRENT_BINARY_DIR}/arches SYMBOLIC)

file(CREATE_LINK ${SIM_BASE}/mavis/json
                 ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)

file(CREATE_LINK ${SIM_BASE}/traces  
                 ${CMAKE_CURRENT_BINARY_DIR}/traces SYMBOLIC)

sparta_named_test(fusion_test olympia -i 1M
        -l top debug debug.log
        --arch-search-dir arches
        --arch fusion
        --report-all fusion.rpt text
        --workload traces/dhry_riscv.zstf)
