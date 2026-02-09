project(Dcache_test)

add_executable(Dcache_test Dcache_test.cpp)

target_link_libraries(Dcache_test core common_test mss ${STF_LINK_LIBS} SPARTA::sparta)
file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_arches     ${CMAKE_CURRENT_BINARY_DIR}/test_arches SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/expected_output  ${CMAKE_CURRENT_BINARY_DIR}/expected_output SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/next_lvl_cache_refill.json  ${CMAKE_CURRENT_BINARY_DIR}/next_lvl_cache_refill.json SYMBOLIC)

sparta_named_test(Dcache_test_arbitrate Dcache_test arbitrate.out -c test_arches/1_src_Dcache.yaml --input-file next_lvl_cache_refill.json)