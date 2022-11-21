project(Dispatch_test)

add_executable(Dispatch_test Dispatch_test.cpp)
target_link_libraries(Dispatch_test core common_test ${STF_LINK_LIBS} ${Sparta_LIBS})

include(${SPARTA_BASE}/test/TestingMacros.cmake)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/sim/arch   ${CMAKE_CURRENT_BINARY_DIR}/arch SYMBOLIC)

# Single add per cycle
sparta_named_test(Dispatch_test_Run_Single Dispatch_test single_test_output
  -r 1000 ${ONE_INST_PER_ROB_ENTRY})
