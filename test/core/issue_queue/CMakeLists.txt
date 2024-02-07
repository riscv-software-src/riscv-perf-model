project(IssueQueue_test)

add_executable(IssueQueue_test IssueQueue_test.cpp ${SIM_BASE}/sim/OlympiaSim.cpp)
target_link_libraries(IssueQueue_test core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_int_pipe.json  ${CMAKE_CURRENT_BINARY_DIR}/test_int_pipe.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_mul_pipe.json  ${CMAKE_CURRENT_BINARY_DIR}/test_mul_pipe.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_two_int_pipe.json  ${CMAKE_CURRENT_BINARY_DIR}/test_two_int_pipe.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_cores   ${CMAKE_CURRENT_BINARY_DIR}/test_cores SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/expected_output  ${CMAKE_CURRENT_BINARY_DIR}/expected_output SYMBOLIC)

# Single add per cycle
# Note: these macros get defined when find_package(Sparta) is called
sparta_named_test(IssueQueue_test_int IssueQueue_test small_core.out -c test_cores/test_big_core.yaml --input-file test_int_pipe.json)
sparta_named_test(IssueQueue_test_mul IssueQueue_test small_core.out -c test_cores/test_big_core.yaml --input-file test_mul_pipe.json)
sparta_named_test(IssueQueue_test_two_int IssueQueue_test small_core.out -c test_cores/test_big_core_full.yaml --input-file test_two_int_pipe.json)
