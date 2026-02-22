project(ICache_test)

add_executable(ICache_test ICache_test.cpp)
target_link_libraries(ICache_test core_cpu core_inst core_memory core_rob core_preloader core_utils core_sim_engine fetch decode rename dispatch execute vector lsu mss common_test mavis SPARTA::sparta)

sparta_named_test(ICache_test_single_access ICache_test --testname single_access --seed 1)
sparta_named_test(ICache_test_simple ICache_test --testname simple --seed 1)
sparta_named_test(ICache_test_random ICache_test --testname random --seed 1 -p top.sink.params.miss_rate 100)
