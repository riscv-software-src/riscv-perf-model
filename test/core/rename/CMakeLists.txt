project(Rename_test)

target_link_libraries (core mss)

add_executable(Rename_test Rename_test.cpp ${SIM_BASE}/sim/OlympiaSim.cpp)

target_link_libraries(Rename_test core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_cores   ${CMAKE_CURRENT_BINARY_DIR}/test_cores SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/expected_output  ${CMAKE_CURRENT_BINARY_DIR}/expected_output SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/rename_multiple_instructions.json  ${CMAKE_CURRENT_BINARY_DIR}/rename_multiple_instructions.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/raw_integer.json  ${CMAKE_CURRENT_BINARY_DIR}/raw_integer.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/raw_int_lsu.json  ${CMAKE_CURRENT_BINARY_DIR}/raw_int_lsu.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/raw_float_lsu.json  ${CMAKE_CURRENT_BINARY_DIR}/raw_float_lsu.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/i2f.json  ${CMAKE_CURRENT_BINARY_DIR}/i2f.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/amoadd.json  ${CMAKE_CURRENT_BINARY_DIR}/amoadd.json SYMBOLIC)

# Single add per cycle
sparta_named_test(Rename_test_Run_Small  Rename_test small_core.out -c test_cores/test_small_core.yaml --input-file rename_multiple_instructions.json)
sparta_named_test(Rename_test_Run_Medium Rename_test medium_core.out -c test_cores/test_medium_core.yaml --input-file rename_multiple_instructions.json)
sparta_named_test(Rename_test_Run_Big    Rename_test big_core.out -c test_cores/test_big_core.yaml --input-file rename_multiple_instructions.json)
sparta_named_test(Rename_test_Run_Big_Small_Rename    Rename_test big_core_small_rename.out -c test_cores/test_big_core_small_rename.yaml --input-file rename_multiple_instructions.json)
sparta_named_test(Rename_test_Run_Small_lsu_int_dependency  Rename_test small_core.out -c test_cores/test_small_core_full.yaml --input-file raw_int_lsu.json)
sparta_named_test(Rename_test_Run_Small_lsu_float_dependency  Rename_test small_core.out -c test_cores/test_small_core_full.yaml --input-file raw_float_lsu.json)
sparta_named_test(Rename_test_Run_Small_int_dependency  Rename_test small_core.out -c test_cores/test_medium_core_full.yaml --input-file raw_integer.json)
sparta_named_test(Rename_test_amoadd  Rename_test small_core.out -c test_cores/test_big_core_small_rename.yaml --input-file amoadd.json)
sparta_named_test(Rename_test_i2f  Rename_test small_core.out -c test_cores/test_medium_core_full.yaml --input-file i2f.json)
