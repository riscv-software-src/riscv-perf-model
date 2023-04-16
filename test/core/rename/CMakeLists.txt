project(Rename_test)

add_executable(Rename_test Rename_test.cpp)
target_link_libraries(Rename_test core common_test ${STF_LINK_LIBS} ${Sparta_LIBS})

include(${SPARTA_BASE}/test/TestingMacros.cmake)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_cores   ${CMAKE_CURRENT_BINARY_DIR}/test_cores SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/expected_output  ${CMAKE_CURRENT_BINARY_DIR}/expected_output SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/rename_multiple_instructions.json  ${CMAKE_CURRENT_BINARY_DIR}/rename_multiple_instructions.json SYMBOLIC)

# Single add per cycle
sparta_named_test(Rename_test_Run_Small  Rename_test small_core.out -c test_cores/test_small_core.yaml --input-file rename_multiple_instructions.json)
sparta_named_test(Rename_test_Run_Medium Rename_test medium_core.out -c test_cores/test_medium_core.yaml --input-file rename_multiple_instructions.json)
sparta_named_test(Rename_test_Run_Big    Rename_test big_core.out -c test_cores/test_big_core.yaml --input-file rename_multiple_instructions.json)
sparta_named_test(Rename_test_Run_Big_Small_Rename    Rename_test big_core_small_rename.out -c test_cores/test_big_core_small_rename.yaml --input-file rename_multiple_instructions.json)
