project(ICache_test)

add_executable(ICache_test ICache_test.cpp)
target_link_libraries(ICache_test core common_test mss mavis SPARTA::sparta)

sparta_named_test(ICache_test_single_access ICache_test --testname single_access --seed 1)
sparta_named_test(ICache_test_simple ICache_test --testname simple --seed 1)
sparta_named_test(ICache_test_random ICache_test --testname random --seed 1 -p top.sink.params.miss_rate 100)
