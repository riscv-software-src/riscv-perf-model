project(Vector_test)

add_executable(Vector_test Vector_test.cpp ${SIM_BASE}/sim/OlympiaSim.cpp)
target_link_libraries(Vector_test core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)

add_executable(VLSU_test VLSU_test.cpp ${SIM_BASE}/sim/OlympiaSim.cpp)
target_link_libraries(VLSU_test core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_cores   ${CMAKE_CURRENT_BINARY_DIR}/test_cores SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetivli_vadd_lmul_4.json   ${CMAKE_CURRENT_BINARY_DIR}/vsetivli_vadd_lmul_4.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvl_vadd.json            ${CMAKE_CURRENT_BINARY_DIR}/vsetvl_vadd.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvli_vadd_sew_32.json    ${CMAKE_CURRENT_BINARY_DIR}/vsetvli_vadd_sew_32.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvli_vl_max_setting.json ${CMAKE_CURRENT_BINARY_DIR}/vsetvli_vl_max_setting.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/multiple_vset.json          ${CMAKE_CURRENT_BINARY_DIR}/multiple_vset.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vmul_transfer.json          ${CMAKE_CURRENT_BINARY_DIR}/vmul_transfer.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/undisturbed_checking.json   ${CMAKE_CURRENT_BINARY_DIR}/undisturbed_checking.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vlsu_load.json              ${CMAKE_CURRENT_BINARY_DIR}/vlsu_load.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vlsu_store.json             ${CMAKE_CURRENT_BINARY_DIR}/vlsu_store.json SYMBOLIC)



sparta_named_test(Vector_test_vsetivli      Vector_test -l top info vsetivli.out      -c test_cores/test_big_core.yaml --input-file vsetivli_vaddvv_e8m4.json)
sparta_named_test(Vector_test_vsetvli       Vector_test -l top info vsetvli.out       -c test_cores/test_big_core.yaml --input-file vsetvli_vaddvv_e32m1ta.json)
sparta_named_test(Vector_test_vsetvl        Vector_test -l top info vsetvl.out        -c test_cores/test_big_core.yaml --input-file vsetvl_vaddvv_e64m1ta.json)
sparta_named_test(Vector_test_vsetivli_tail Vector_test -l top info vsetivli_tail.out -c test_cores/test_big_core.yaml --input-file vsetivli_vaddvv_tail_e8m8ta.json)
sparta_named_test(Vector_test_multiple_vset Vector_test -l top info mulitple_vset.out -c test_cores/test_big_core.yaml --input-file multiple_vset.json)
sparta_named_test(Vector_test_vmulvx        Vector_test -l top info vmulvx.out        -c test_cores/test_big_core.yaml --input-file vmulvx_e8m4.json)
sparta_named_test(Vector_test_vmulvv        Vector_test -l top info vmulvv.out        -c test_cores/test_big_core.yaml --input-file vwmulvv_e8m4.json)
sparta_named_test(Vector_test_vmseqvv       Vector_test -l top info vmseqvv.out       -c test_cores/test_big_core.yaml --input-file vmseqvv_e8m4.json)
sparta_named_test(Vector_unsupported_test   Vector_test -l top info unsupported.out   -c test_cores/test_big_core.yaml --input-file vrgather.json)
sparta_named_test(VLSU_test_load            VLSU_test   -l top info vlsu_load.out     -c test_cores/test_big_core.yaml --input-file vlsu_load.json)
sparta_named_test(VLSU_test_store           VLSU_test   -l top info vlsu_store.out    -c test_cores/test_big_core.yaml --input-file vlsu_store.json)
