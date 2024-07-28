project(olympia_tests)

# Enable CTset
include(CTest)

# Handy parallel processor library
include(ProcessorCount)
ProcessorCount(NUM_CORES)

if (NOT NUM_CORES EQUAL 0)
  set(CTEST_BUILD_FLAGS -j${NUM_CORES})
  set(ctest_test_args ${ctest_test_args} PARALLEL_LEVEL ${NUM_CORES})
endif()
message(STATUS "Found " ${NUM_CORES} " cores in machine (for ctest)")

# Add regress target
add_custom_target (regress)

# Add valgrind regress target
set(VALGRIND_TEST_PREFIX "^valgrind_") # Only tests with this prefix
add_custom_target (regress_valgrind)
add_custom_command(TARGET regress          POST_BUILD COMMAND ctest -E ${VALGRIND_TEST_PREFIX} -j${NUM_CORES})
add_custom_command(TARGET regress_valgrind POST_BUILD COMMAND ctest -R ${VALGRIND_TEST_PREFIX} -D ExperimentalMemCheck -j${NUM_CORES})

include_directories(${PROJECT_SOURCE_DIR}/..)

# Now the tests...
add_subdirectory(sim)
add_subdirectory(core/common)
add_subdirectory(core/dispatch)
add_subdirectory(core/l2cache)
add_subdirectory(core/rename)
add_subdirectory(core/lsu)
add_subdirectory(core/issue_queue)
add_subdirectory(core/branch_pred)
add_subdirectory(core/dcache)
add_subdirectory(fusion)
