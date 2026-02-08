project(Vector_test)

add_executable(Vector_test Vector_test.cpp ${SIM_BASE}/sim/OlympiaSim.cpp)
target_link_libraries(Vector_test core common_test ${STF_LINK_LIBS} mavis SPARTA::sparta)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${SIM_BASE}/scripts/generate_vector_tests.py
    WORKING_DIRECTORY ${SIM_BASE}
    RESULT_VARIABLE GEN_RESULT
)

if(NOT GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate vector test JSON files")
endif()

message(STATUS "Generated vector test JSON files in ${SIM_BASE}/jsonTests/")

file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches     ${CMAKE_CURRENT_BINARY_DIR}/arches          SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/test_cores   ${CMAKE_CURRENT_BINARY_DIR}/test_cores SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetivli_vaddvv_e8m4.json        ${CMAKE_CURRENT_BINARY_DIR}/vsetivli_vaddvv_e8m4.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvli_vaddvv_e32m1ta.json      ${CMAKE_CURRENT_BINARY_DIR}/vsetvli_vaddvv_e32m1ta.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetvl_vaddvv_e64m1ta.json       ${CMAKE_CURRENT_BINARY_DIR}/vsetvl_vaddvv_e64m1ta.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vsetivli_vaddvv_tail_e8m8ta.json ${CMAKE_CURRENT_BINARY_DIR}/vsetivli_vaddvv_tail_e8m8ta.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/multiple_vset.json               ${CMAKE_CURRENT_BINARY_DIR}/multiple_vset.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vrgather.json                    ${CMAKE_CURRENT_BINARY_DIR}/vrgather.json SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/elementwise.json    ${CMAKE_CURRENT_BINARY_DIR}/elementwise.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/widening.json       ${CMAKE_CURRENT_BINARY_DIR}/widening.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/widening_mixed.json ${CMAKE_CURRENT_BINARY_DIR}/widening_mixed.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/narrowing.json      ${CMAKE_CURRENT_BINARY_DIR}/narrowing.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/mac.json            ${CMAKE_CURRENT_BINARY_DIR}/mac.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/mac_widening.json   ${CMAKE_CURRENT_BINARY_DIR}/mac_widening.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/single_dest.json    ${CMAKE_CURRENT_BINARY_DIR}/single_dest.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/single_src.json     ${CMAKE_CURRENT_BINARY_DIR}/single_src.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/reduction.json      ${CMAKE_CURRENT_BINARY_DIR}/reduction.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/reduction_wide.json ${CMAKE_CURRENT_BINARY_DIR}/reduction_wide.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/int_ext.json        ${CMAKE_CURRENT_BINARY_DIR}/int_ext.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/slide.json          ${CMAKE_CURRENT_BINARY_DIR}/slide.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/permute.json        ${CMAKE_CURRENT_BINARY_DIR}/permute.json SYMBOLIC)

file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vload.json  ${CMAKE_CURRENT_BINARY_DIR}/vload.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vstore.json ${CMAKE_CURRENT_BINARY_DIR}/vstore.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vfirst.json ${CMAKE_CURRENT_BINARY_DIR}/vfirst.json SYMBOLIC)
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/vmv4r.json  ${CMAKE_CURRENT_BINARY_DIR}/vmv4r.json SYMBOLIC)

# Vset tests
sparta_named_test(Vector_test_vsetivli      Vector_test -l top info vsetivli.out      -c test_cores/test_big_core.yaml --input-file vsetivli_vaddvv_e8m4.json        --expected-num-uops 4)
sparta_named_test(Vector_test_vsetvli       Vector_test -l top info vsetvli.out       -c test_cores/test_big_core.yaml --input-file vsetvli_vaddvv_e32m1ta.json      --expected-num-uops 1)
sparta_named_test(Vector_test_vsetvl        Vector_test -l top info vsetvl.out        -c test_cores/test_big_core.yaml --input-file vsetvl_vaddvv_e64m1ta.json       --expected-num-uops 1)
sparta_named_test(Vector_test_vsetivli_tail Vector_test -l top info vsetivli_tail.out -c test_cores/test_big_core.yaml --input-file vsetivli_vaddvv_tail_e8m8ta.json --expected-num-uops 8)
sparta_named_test(Vector_test_multiple_vset Vector_test -l top info multiple_vset.out -c test_cores/test_big_core.yaml --input-file multiple_vset.json               --expected-num-uops 15)

# Unsupported test
sparta_named_test(Vector_unsupported_test   Vector_test -l top info unsupported.out   -c test_cores/test_big_core.yaml --input-file vrgather.json)

# Vector sequencing tests
sparta_named_test(Vector_test_elementwise    Vector_test -l top info vuop_elementwise.out    -c test_cores/test_big_core.yaml --input-file elementwise.json          --expected-num-uops 8)
sparta_named_test(Vector_test_widening       Vector_test -l top info vuop_widening.out       -c test_cores/test_big_core.yaml --input-file widening.json             --expected-num-uops 8)
sparta_named_test(Vector_test_widening_mixed Vector_test -l top info vuop_widening_mixed.out -c test_cores/test_big_core.yaml --input-file widening_mixed.json       --expected-num-uops 8)
sparta_named_test(Vector_test_narrowing      Vector_test -l top info vuop_narrowing.out      -c test_cores/test_big_core.yaml --input-file narrowing.json            --expected-num-uops 4)
sparta_named_test(Vector_test_mac            Vector_test -l top info vuop_mac.out            -c test_cores/test_big_core.yaml --input-file mac.json                  --expected-num-uops 4)
sparta_named_test(Vector_test_mac_widening   Vector_test -l top info vuop_mac_widening.out   -c test_cores/test_big_core.yaml --input-file mac_widening.json         --expected-num-uops 8)
sparta_named_test(Vector_test_single_dest    Vector_test -l top info vuop_single_dest.out    -c test_cores/test_big_core.yaml --input-file single_dest.json          --expected-num-uops 4)
sparta_named_test(Vector_test_single_src     Vector_test -l top info vuop_single_src.out     -c test_cores/test_big_core.yaml --input-file single_src.json           --expected-num-uops 4)
sparta_named_test(Vector_test_reduction      Vector_test -l top info vuop_reduction.out      -c test_cores/test_big_core.yaml --input-file reduction.json            --expected-num-uops 4)
sparta_named_test(Vector_test_reduction_wide Vector_test -l top info vuop_reduction_wide.out -c test_cores/test_big_core.yaml --input-file reduction_wide.json       --expected-num-uops 8)
sparta_named_test(Vector_test_int_ext        Vector_test -l top info vuop_int_ext.out        -c test_cores/test_big_core.yaml --input-file int_ext.json              --expected-num-uops 24)
sparta_named_test(Vector_test_slide          Vector_test -l top info vuop_slide.out          -c test_cores/test_big_core.yaml --input-file slide.json                --expected-num-uops 8)
#sparta_named_test(Vector_test_permute        Vector_test -l top info vuop_permute.out        -c test_cores/test_big_core.yaml --input-file permute.json              --expected-num-uops 19)

# Vector instruction tests
#sparta_named_test(Vector_test_vload  Vector_test -l top info vload.out  -c test_cores/test_big_core.yaml --input-file vload.json  --expected-num-uops 12)
#sparta_named_test(Vector_test_vstore Vector_test -l top info vstore.out -c test_cores/test_big_core.yaml --input-file vstore.json --expected-num-uops 8)
sparta_named_test(Vector_test_vfirst Vector_test -l top info vfirst.out -c test_cores/test_big_core.yaml --input-file vfirst.json --expected-num-uops 0)
sparta_named_test(Vector_test_vmv4r  Vector_test -l top info vmv4r.out  -c test_cores/test_big_core.yaml --input-file vmv4r.json  --expected-num-uops 4)

file(GLOB GENERATED_JSON_TESTS "${SIM_BASE}/jsonTests/*.json")

foreach(JSON_FILE ${GENERATED_JSON_TESTS})
    get_filename_component(JSON_NAME ${JSON_FILE} NAME_WE)
    
    file(CREATE_LINK ${JSON_FILE} 
         ${CMAKE_CURRENT_BINARY_DIR}/${JSON_NAME}.json 
         SYMBOLIC)
    
    sparta_named_test(
        Vector_test_gen_${JSON_NAME}
        Vector_test 
        -l top info gen_${JSON_NAME}.out 
        -c test_cores/test_big_core.yaml 
        --input-file ${JSON_NAME}.json
    )
endforeach()

list(LENGTH GENERATED_JSON_TESTS NUM_GENERATED_TESTS)
message(STATUS "Added ${NUM_GENERATED_TESTS} generated vector tests to regress target")
