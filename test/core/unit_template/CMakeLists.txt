set(PRJ "UNIT_TMPL")
set(EXE "${PRJ}_tb") 

# There are 3 tests
# 1 - basic no options - sanity check
# 2 - (z)stf input-file
# 3 - JSON   input-file
set(TST1 "${PRJ}_test")
set(TST2 "${PRJ}_stf_test")
set(TST3 "${PRJ}_json_test")

# Test args
set(STF  ./traces/bmi_pmp.bare.stf)
set(JSON ./json/raw_integer.json)

# Test cfg : there is a single config for the unit bench
set(CFG config/config.yaml)

project(${PRJ})

add_executable(${EXE} Testbench.cpp Dut.cpp Src.cpp)
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

sparta_named_test(${TST1} ${EXE} tb.out -c ${CFG})
sparta_named_test(${TST2} ${EXE} tb_stf.out  --input_file ${STF}  -c ${CFG})
sparta_named_test(${TST3} ${EXE} tb_json.out --input_file ${JSON} -c ${CFG})
