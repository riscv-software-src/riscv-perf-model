project(olympia_tests)

# Enable CTset
include(CTest)

# Handy parallel processor library
include(ProcessorCount)
ProcessorCount(NUM_CORES)
if (NUM_CORES GREATER 8)  # Max of 8 cores
  set(NUM_CORE, 8)
endif()
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

# Now the tests...
add_subdirectory(sim)
