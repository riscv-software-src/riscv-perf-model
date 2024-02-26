project(olympia_test)

# Note: these sparta_* macros/functions get defined when find_package(Sparta) is called

# This line will make sure olympia is built before running the tests
sparta_regress (olympia)

# Create a few links like reports and arch directories for the testers
file(CREATE_LINK ${SIM_BASE}/reports ${CMAKE_CURRENT_BINARY_DIR}/reports SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches  ${CMAKE_CURRENT_BINARY_DIR}/arches SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/traces  ${CMAKE_CURRENT_BINARY_DIR}/traces SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/test/sim/json_tests  ${CMAKE_CURRENT_BINARY_DIR}/json_tests SYMBOLIC)

# Tests
sparta_named_test(olympia_json_test olympia --workload traces/example_json.json)

# Test report generation

# This command will use a report definition file to generate a text form of a report
sparta_named_test(olympia_json_test_report_text olympia
  --report-yaml-replacements OUT_BASE json_test_report OUT_FORMAT text INST_START 0
  --report-search-dir reports
  --report reports/core_report.def
  --workload traces/example_json.json)

# This command will use core_stats.yaml directly to generate a text form of a report at the "top" node
sparta_named_test(olympia_json_test_report_text_direct olympia
  --report "top" reports/core_stats.yaml core_stats.text text
  --workload traces/example_json.json)

# This command will use core_stats.yaml directly to generate an html form of a report at the "top" node
sparta_named_test(olympia_json_test_report_html olympia
  --report "top" reports/core_stats.yaml core_stats.html html
  --workload traces/example_json.json)

# This command will run the dhrystone trace (1 million instructions)
sparta_named_test(olympia_dhry_test_report_html olympia -i 1M
  --report-yaml-replacements OUT_BASE dhry_test_report OUT_FORMAT text INST_START 0
  --report-search-dir reports
  --report reports/core_report.def
  --workload traces/dhry_riscv.zstf)

# This command will run the coremark trace (1 million instructions)
sparta_named_test(olympia_coremark_test_report_html olympia -i 1M
  --report-yaml-replacements OUT_BASE coremark_test_report OUT_FORMAT text INST_START 0
  --report-search-dir reports
  --report reports/core_report.def
  --workload traces/core_riscv.zstf)

# Test missing opcodes
sparta_named_test(olympia_json_test_missing_opcodes olympia
  --workload json_tests/missing_opcodes.json)

# Test FP transfers
sparta_named_test(olympia_json_test_fp_transfers olympia
  --workload json_tests/fp_transfers.json)

# Test PEvent generation
sparta_named_test(olympia_json_test_pevents olympia
  --workload traces/dhry_riscv.zstf -i100k
  --pevents test_pevent.out RETIRE
  --pevents test_pevent.out COMPLETE)
sparta_named_test(olympia_json_test_pevents_all olympia
  --workload traces/dhry_riscv.zstf  -i100k
  --pevents test_pevent.out all)

## Test the arches
file(GLOB TEST_YAML "${SIM_BASE}/arches/*.yaml")
foreach(ARCH_FULL_PATH ${TEST_YAML})
  get_filename_component(ARCH_FILE_NAME ${ARCH_FULL_PATH} NAME)
  string(REGEX REPLACE ".yaml" "" ARCH_NAME ${ARCH_FILE_NAME})
  sparta_named_test(olympia_arch_${ARCH_NAME}_test olympia
    -i500K --workload traces/dhry_riscv.zstf
    --arch ${ARCH_NAME})
endforeach()

## Test branch misprediction flushes
foreach(ARCH_FULL_PATH ${TEST_YAML})
  get_filename_component(ARCH_FILE_NAME ${ARCH_FULL_PATH} NAME)
  string(REGEX REPLACE ".yaml" "" ARCH_NAME ${ARCH_FILE_NAME})
  sparta_named_test(olympia_arch_${ARCH_NAME}_random_branch_misprediction_test olympia
    -i500K --workload traces/dhry_riscv.zstf
    --arch ${ARCH_NAME}
    -p top.cpu.core0.execute.exe*.params.enable_random_misprediction 1)
endforeach()

set(test_params_list)
list(APPEND test_params_list "top.cpu.core0.lsu.params.mmu_lookup_stage_length 3")
list(APPEND test_params_list "top.cpu.core0.lsu.params.cache_lookup_stage_length 3")
list(APPEND test_params_list "top.cpu.core0.lsu.params.cache_read_stage_length 3")
list(APPEND test_params_list "top.cpu.core0.lsu.params.allow_speculative_load_exec false")
list(APPEND test_params_list "top.cpu.core0.lsu.params.allow_speculative_load_exec true")
list(APPEND test_params_list "top.cpu.core0.lsu.params.replay_issue_delay 5")

# Used to set a custom name for each test
set(index 1)
foreach(ARCH_FULL_PATH ${TEST_YAML})
  get_filename_component(ARCH_FILE_NAME ${ARCH_FULL_PATH} NAME)
  string(REGEX REPLACE ".yaml" "" ARCH_NAME ${ARCH_FILE_NAME})
  foreach(param ${test_params_list})
    # Split string to get the parameter and the value for it
    string(REPLACE " " ";" kv_pair ${param})
    list(GET kv_pair 0 parameter)
    list(GET kv_pair 1 value)

    sparta_named_test(olympia_arch_${ARCH_NAME}_${index}_custom_dhry_test olympia
            -i500K --workload traces/dhry_riscv.zstf -p ${parameter} ${value}
            --arch ${ARCH_NAME})

    sparta_named_test(olympia_arch_${ARCH_NAME}_${index}_custom_core olympia
            -i500K --workload traces/core_riscv.zstf -p ${parameter} ${value}
            --arch ${ARCH_NAME})

    math(EXPR index "${index} + 1")
  endforeach ()
endforeach()
