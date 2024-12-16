set(PRJ "Bpu_test")
set(EXE "${PRJ}_exec")

# sanity check
set(TST1, "${PRJ}_test")
#zstf input file
set(TST2, "${PRJ}_stf_test")

# test args
set(STF ./traces/bpu_basic_test.stf)

# configuration
set(CFG config/config.yaml)

project(${PRJ})

add_executable(${EXE} Bpu_test.cpp Bpu_unit.cpp src.cpp sink.cpp)
target_include_directories(${EXE} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${EXE} core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)


file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches ${CMAKE_CURRENT_BINARY_DIR}/arches SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/config ${CMAKE_CURRENT_BINARY_DIR}/config SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/json ${CMAKE_CURRENT_BINARY_DIR}/json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/traces ${CMAKE_CURRENT_BINARY_DIR}/traces SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/expected_output ${CMAKE_CURRENT_BINARY_DIR}/expected_output SYMBOLIC)

sparta_named_test(${TST1} ${EXE} tb.out -c ${CFG})
sparta_named_test(${TST2} ${EXE} tb_stf.out --input_file ${STF} -c ${CFG})


