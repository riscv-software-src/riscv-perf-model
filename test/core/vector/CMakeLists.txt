project(Vector_test)

add_executable(Vector_test Vector_test.cpp ${SIM_BASE}/sim/OlympiaSim.cpp)
target_link_libraries(Vector_test core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_cores   ${CMAKE_CURRENT_BINARY_DIR}/test_cores SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetivli_vaddvv_e8m4.json        ${CMAKE_CURRENT_BINARY_DIR}/vsetivli_vaddvv_e8m4.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvli_vaddvv_e32m1ta.json      ${CMAKE_CURRENT_BINARY_DIR}/vsetvli_vaddvv_e32m1ta.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvl_vaddvv_e64m1ta.json       ${CMAKE_CURRENT_BINARY_DIR}/vsetvl_vaddvv_e64m1ta.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetivli_vaddvv_tail_e8m8ta.json ${CMAKE_CURRENT_BINARY_DIR}/vsetivli_vaddvv_tail_e8m8ta.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/multiple_vset.json               ${CMAKE_CURRENT_BINARY_DIR}/multiple_vset.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vrgather.json                    ${CMAKE_CURRENT_BINARY_DIR}/vrgather.json SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/elementwise.json ${CMAKE_CURRENT_BINARY_DIR}/elementwise.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/widening.json ${CMAKE_CURRENT_BINARY_DIR}/widening.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/widening_mixed.json ${CMAKE_CURRENT_BINARY_DIR}/widening_mixed.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/narrowing.json ${CMAKE_CURRENT_BINARY_DIR}/narrowing.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/mac.json ${CMAKE_CURRENT_BINARY_DIR}/mac.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/mac_widening.json ${CMAKE_CURRENT_BINARY_DIR}/mac_widening.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/single_dest.json ${CMAKE_CURRENT_BINARY_DIR}/single_dest.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/int_ext.json ${CMAKE_CURRENT_BINARY_DIR}/int_ext.json SYMBOLIC)

# Vset tests
sparta_named_test(Vector_test_vsetivli      Vector_test -l top info vsetivli.out      -c test_cores/test_big_core.yaml --input-file vsetivli_vaddvv_e8m4.json        --expected-num-uops 4)
sparta_named_test(Vector_test_vsetvli       Vector_test -l top info vsetvli.out       -c test_cores/test_big_core.yaml --input-file vsetvli_vaddvv_e32m1ta.json      --expected-num-uops 1)
sparta_named_test(Vector_test_vsetvl        Vector_test -l top info vsetvl.out        -c test_cores/test_big_core.yaml --input-file vsetvl_vaddvv_e64m1ta.json       --expected-num-uops 1)
sparta_named_test(Vector_test_vsetivli_tail Vector_test -l top info vsetivli_tail.out -c test_cores/test_big_core.yaml --input-file vsetivli_vaddvv_tail_e8m8ta.json --expected-num-uops 8)
sparta_named_test(Vector_test_multiple_vset Vector_test -l top info mulitple_vset.out -c test_cores/test_big_core.yaml --input-file multiple_vset.json               --expected-num-uops 15)

# Unsupported test
sparta_named_test(Vector_unsupported_test   Vector_test -l top info unsupported.out   -c test_cores/test_big_core.yaml --input-file vrgather.json)

# Vector sequencing tests
sparta_named_test(Vector_test_elementwise    Vector_test -l top info vuop_elementwise.out    -c test_cores/test_big_core.yaml --input-file elementwise.json          --expected-num-uops 8)
sparta_named_test(Vector_test_widening       Vector_test -l top info vuop_widening.out       -c test_cores/test_big_core.yaml --input-file widening.json             --expected-num-uops 8)
sparta_named_test(Vector_test_widening_mixed Vector_test -l top info vuop_widening_mixed.out -c test_cores/test_big_core.yaml --input-file widening_mixed.json       --expected-num-uops 8)
sparta_named_test(Vector_test_narrowing      Vector_test -l top info vuop_narrowing.out      -c test_cores/test_big_core.yaml --input-file narrowing.json            --expected-num-uops 8)
sparta_named_test(Vector_test_mac            Vector_test -l top info vuop_mac.out            -c test_cores/test_big_core.yaml --input-file mac.json                  --expected-num-uops 4)
sparta_named_test(Vector_test_mac_widening   Vector_test -l top info vuop_mac_widening.out   -c test_cores/test_big_core.yaml --input-file mac_widening.json         --expected-num-uops 8)
sparta_named_test(Vector_test_single_dest    Vector_test -l top info vuop_single_dest.out    -c test_cores/test_big_core.yaml --input-file single_dest.json          --expected-num-uops 4)
sparta_named_test(Vector_test_int_ext        Vector_test -l top info vuop_int_ext.out        -c test_cores/test_big_core.yaml --input-file int_ext.json              --expected-num-uops 12)
