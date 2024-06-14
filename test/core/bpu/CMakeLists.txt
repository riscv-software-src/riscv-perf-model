set(PRJ "BPU")
set(EXE "${PRJ}_tb") 

project(${PRJ})

# There are 2 tests
# 1 - basic no options - sanity check
# 2 - (z)stf input-file
set(TST1 "${PRJ}_test")
set(TST2 "${PRJ}_stf_test")

# Test args
set(STF traces/basic.stf)

# Test cfg : there is a single config for the unit bench
set(CFG configs/bpu.yaml)

file(CREATE_LINK ${SIM_BASE}/mavis/json
                 ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)

file(CREATE_LINK ${SIM_BASE}/arches
                 ${CMAKE_CURRENT_BINARY_DIR}/arches SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/traces
                 ${CMAKE_CURRENT_BINARY_DIR}/traces SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/configs
                 ${CMAKE_CURRENT_BINARY_DIR}/configs SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/json
                 ${CMAKE_CURRENT_BINARY_DIR}/json SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/traces
                 ${CMAKE_CURRENT_BINARY_DIR}/traces SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/expected_output
                 ${CMAKE_CURRENT_BINARY_DIR}/expected_output SYMBOLIC)

add_executable(${EXE}
               dut.cpp
               src.cpp
               testbench.cpp
               bpuinfo.cpp
               uarch/Gem5Tage.cpp
               uarch/SimpleBTB.cpp)

target_include_directories(${EXE} PRIVATE 
                           ${CMAKE_CURRENT_SOURCE_DIR} )

target_link_libraries(${EXE}
                      core
                      common_test
                      ${STF_LINK_LIBS}
                      mavis
                      SPARTA::sparta)

sparta_named_test(${TST1} ${EXE} tb.out -c ${CFG})
sparta_named_test(${TST2} ${EXE} tb_stf.out  --input_file ${STF}  -c ${CFG})

find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxygen)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(bpu_docs
                      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      COMMENT "Generating API documentation with Doxygen"
                      VERBATIM)
else()
    message("Doxygen needs to be installed to generate documentation")
endif()

