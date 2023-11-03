project(L2Cache_test)

add_executable(L2Cache_test L2Cache_test.cpp)

target_link_libraries(L2Cache_test core common_test mss ${STF_LINK_LIBS} SPARTA::sparta)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/expected_output  ${CMAKE_CURRENT_BINARY_DIR}/expected_output SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/single_access.json  ${CMAKE_CURRENT_BINARY_DIR}/single_access.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/hit_case.json  ${CMAKE_CURRENT_BINARY_DIR}/hit_case.json SYMBOLIC)

sparta_named_test(L2Cache_test_single_access  L2Cache_test single_access.out -c test_arches/2_src_L2Cache.yaml --input-file single_access.json)
sparta_named_test(L2Cache_test_hit_case L2Cache_test hit_case.out -c test_arches/2_src_L2Cache.yaml --input-file hit_case.json)
