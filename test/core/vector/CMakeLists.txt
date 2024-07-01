project(Vector_test)

add_executable(Vector_test Vector_test.cpp ${SIM_BASE}/sim/OlympiaSim.cpp)
target_link_libraries(Vector_test core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_cores   ${CMAKE_CURRENT_BINARY_DIR}/test_cores SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetivli_vadd_lmul_4.json   ${CMAKE_CURRENT_BINARY_DIR}/vsetivli_vadd_lmul_4.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvl_vadd.json   ${CMAKE_CURRENT_BINARY_DIR}/vsetvl_vadd.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvli_vadd_sew_32.json   ${CMAKE_CURRENT_BINARY_DIR}/vsetvli_vadd_sew_32.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvli_vl_max_setting.json  ${CMAKE_CURRENT_BINARY_DIR}/vsetvli_vl_max_setting.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/multiple_vset.json  ${CMAKE_CURRENT_BINARY_DIR}/multiple_vset.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vmul_transfer.json  ${CMAKE_CURRENT_BINARY_DIR}/vmul_transfer.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/undisturbed_checking.json  ${CMAKE_CURRENT_BINARY_DIR}/undisturbed_checking.json SYMBOLIC)



sparta_named_test(Vector_test_vsetivli_lmul_4  Vector_test big_core.out -c test_cores/test_big_core_full.yaml --input-file vsetivli_vadd_lmul_4.json)
sparta_named_test(Vector_test_vsetvl_vadd  Vector_test big_core.out -c test_cores/test_big_core_full.yaml --input-file vsetvl_vadd.json)
sparta_named_test(Vector_test_vsetvli_vadd_sew_32  Vector_test big_core.out -c test_cores/test_big_core_full.yaml --input-file vsetvli_vadd_sew_32.json)
sparta_named_test(Vector_test_vsetvli_vl_max_setting  Vector_test big_core.out -c test_cores/test_big_core_full.yaml --input-file vsetvli_vl_max_setting.json)
sparta_named_test(Vector_undisturbed_test  Vector_test big_core.out -c test_cores/test_big_core_full_8_decode.yaml --input-file undisturbed_checking.json)