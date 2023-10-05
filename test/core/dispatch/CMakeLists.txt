project(Dispatch_test)

add_executable(Dispatch_test Dispatch_test.cpp)
target_link_libraries(Dispatch_test core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_cores   ${CMAKE_CURRENT_BINARY_DIR}/test_cores SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/expected_output  ${CMAKE_CURRENT_BINARY_DIR}/expected_output SYMBOLIC)

# Single add per cycle
# Note: these macros get defined when find_package(Sparta) is called
sparta_named_test(Dispatch_test_Run_Small  Dispatch_test small_core.out -c test_cores/test_small_core.yaml)
sparta_named_test(Dispatch_test_Run_Medium Dispatch_test medium_core.out -c test_cores/test_medium_core.yaml)
sparta_named_test(Dispatch_test_Run_Big    Dispatch_test big_core.out -c test_cores/test_big_core.yaml)
