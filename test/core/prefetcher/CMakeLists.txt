project(Prefetcher_test)

add_executable(Prefetcher_test Prefetcher_test.cpp)
target_link_libraries(Prefetcher_test core mss mavis SPARTA::sparta)

sparta_named_test(Prefetcher_test_nextline Prefetcher_test --testname nextline)
sparta_named_test(Prefetcher_test_stride Prefetcher_test --testname stride)
sparta_named_test(Prefetcher_test_edge_cases Prefetcher_test --testname edge_cases)
