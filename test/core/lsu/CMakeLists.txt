project(Lsu_test)

add_executable(Lsu_test Lsu_test.cpp ${SIM_BASE}/sim/OlympiaSim.cpp)

target_link_libraries(Lsu_test PRIVATE olympia_test_support)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_cores   ${CMAKE_CURRENT_BINARY_DIR}/test_cores SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/raw.json  ${CMAKE_CURRENT_BINARY_DIR}/raw.json SYMBOLIC)

sparta_named_test(Lsu_test_raw  Lsu_test small_core.out -c test_cores/test_small_core_full.yaml --input-file raw.json)