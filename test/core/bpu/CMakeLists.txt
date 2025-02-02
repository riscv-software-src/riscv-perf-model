set(PRJ "BPU_test")
set(EXE "BPU_test")

set(TST1 "${PRJ}_tst1")
set(TST2 "${PRJ}_json")
set(TST3 "${PRJ}_stf")

set(CFG config/config.yaml)

set(BPUDir ../../../core/fetch)

project(${PRJ})

add_executable(${EXE} BPU_testbench.cpp ${BPUDir}/BPU.cpp ${BPUDir}/BPU.hpp BPUSource.hpp BPUSink.hpp)
target_include_directories(${EXE} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${EXE} core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)

file(CREATE_LINK ${SIM_BASE}/mavis/json
                 ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)

file(CREATE_LINK ${SIM_BASE}/arches
                 ${CMAKE_CURRENT_BINARY_DIR}/arches SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/config
                 ${CMAKE_CURRENT_BINARY_DIR}/config SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/json
                 ${CMAKE_CURRENT_BINARY_DIR}/json SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/traces
                 ${CMAKE_CURRENT_BINARY_DIR}/traces SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/expected_output
                 ${CMAKE_CURRENT_BINARY_DIR}/expected_output SYMBOLIC)

sparta_named_test(${TST1} ${EXE} bpu_tb_test1.out -c ${CFG})
sparta_named_test(${TST2} ${EXE} bpu_tb_json.out  --input_file ${STF}  -c ${CFG})
sparta_named_test(${TST3} ${EXE} bpu_tb_stf.out --input_file ${JSON} -c ${CFG})