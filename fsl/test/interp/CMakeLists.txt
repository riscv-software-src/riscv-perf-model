cmake_minimum_required(VERSION 3.10)
project(TestInterp)

# Include directories
include_directories(../../fsl_interp/inc)

set(INTERP_BIN ${CMAKE_BINARY_DIR}/bin/fsl_interp)

# Test command
add_custom_target(run_interp_tests
    COMMAND ${INTERP_BIN} 
        --verbose 
        -i syntax_tests/_1_syntax_test.fsl
        -i syntax_tests/_2_syntax_test.fsl
        -i syntax_tests/sample1.fsl
        -i syntax_tests/all_comments.fsl
        -i syntax_tests/empty_file.fsl
        -i syntax_tests/syntax1.fsl
        -i syntax_tests/syntax2.fsl
        -i syntax_tests/test3.fsl
        -i syntax_tests/test4.fsl
        -o output.txt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running interpreter tests"
)

# Ensure the interpreter binary is built before running tests
add_dependencies(run_interp_tests fsl_interp)

