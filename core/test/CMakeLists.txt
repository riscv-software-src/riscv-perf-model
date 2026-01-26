add_executable(test_prefetcher
    test_prefetcher.cpp
)

target_link_libraries(test_prefetcher
    core
    SPARTA::sparta
)

add_test(NAME test_prefetcher COMMAND test_prefetcher)